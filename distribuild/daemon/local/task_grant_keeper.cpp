#include <grpc/grpc.h>
#include <grpcpp/grpcpp.h>
#include <Poco/Thread.h>
#include <grpcpp/create_channel.h>
#include <Poco/ThreadPool.h>
#include <Poco/TaskManager.h>
#include "daemon/local/task_grant_keeper.h"
#include "common/spdlogging.h"
#include "common/tools.h"
#include "daemon/config.h"
#include "daemon/version.h"

using namespace std::literals;

namespace distribuild::daemon::local {

TaskGrantKeeper::TaskGrantKeeper()
  : task_manager_(Poco::ThreadPool::defaultPool()) {
  auto channel = grpc::CreateChannel(FLAGS_scheduler_location, grpc::InsecureChannelCredentials());
  scheduler_stub_ = scheduler::SchedulerService::NewStub(channel);
  DISTBU_CHECK(scheduler_stub_);
}

std::optional<TaskGrantKeeper::GrantDesc> TaskGrantKeeper::Get(const EnviromentDesc& desc, const std::chrono::nanoseconds& timeout) {
  // 获得keeper
  EnvGrantKeeper* keeper = nullptr;
  {
	std::scoped_lock lock(mutex_);
    auto&& new_keeper = keepers_[desc.compiler_digest()];
	if (!new_keeper) {
	  new_keeper = std::make_unique<EnvGrantKeeper>();
	  new_keeper->env_desc = desc;
	  new_keeper->task = new GrantFetcherPocoTask(this, new_keeper.get());
	  task_manager_.start(new_keeper->task);
	}
	keeper = new_keeper.get();
  }

  // 丢弃过期的授权
  std::unique_lock lock(keeper->mutex);
  while (!keeper->remaining.empty() && keeper->remaining.front().expire_tp < std::chrono::steady_clock::now()) {
	keeper->remaining.pop();
  }

  // 还有则直接出队一个授权
  if (!keeper->remaining.empty()) {
	auto result = keeper->remaining.front();
    keeper->remaining.pop();
    return result;
  }

  ++keeper->waiters;

  // 通知任务去申请grant
  keeper->need_more_cv.notify_all();
  if (!keeper->available_cv.wait_for(lock, timeout, [&] { return !keeper->remaining.empty(); })) {
	DISTBU_CHECK(--keeper->waiters == 0);
    return std::nullopt; // 申请失败
  }

  // 申请成功
  LOG_DEBUG("申请成功");
  auto result = keeper->remaining.front();
  keeper->remaining.pop();
  DISTBU_CHECK(--keeper->waiters == 0);
  return result;
}

void TaskGrantKeeper::Free(std::uint64_t grant_id) {
  grpc::ClientContext context;
  scheduler::FreeTaskRequst req;
  req.set_token(FLAGS_scheduler_token);
  req.add_task_grant_ids(grant_id);
  SetTimeout(&context, 5s);

  grpc::CompletionQueue cq;

  // !
  scheduler_stub_->AsyncFreeTask(&context, req, &cq);
}

void TaskGrantKeeper::Stop() {
  std::scoped_lock lock(mutex_);
  leaving_.store(true, std::memory_order_relaxed);
  for (auto&& [_, v] : keepers_) {
	v->need_more_cv.notify_all(); // 通知所有keeper退出
  }
}

void TaskGrantKeeper::Join() {
  for (auto&& [_, keeper] : keepers_) {
    keeper->task->cancel();
  }
}

void TaskGrantKeeper::GrantFetcherProc(EnvGrantKeeper* keeper) {
  LOG_DEBUG("开始FetcherProcTask");
  constexpr auto kMaxWait = 5s;
  constexpr auto kNetworkDelayTolerance = 5s;
  constexpr auto kExpiresIn = 15s;

  while (!leaving_.load(std::memory_order_relaxed)) {
	// 等待，直到 leaving_ 或 remaining 队列为空
	std::unique_lock lock(keeper->mutex);
    keeper->need_more_cv.wait(lock, [&] { // 等待需要授权
      return leaving_.load(std::memory_order_relaxed) || keeper->remaining.empty();
    });

    // 要退出了
	if (leaving_.load(std::memory_order_relaxed)) {
	  break;
	}

	grpc::ClientContext context;
	scheduler::WaitForStaringTaskRequest req;
	scheduler::WaitForStaringTaskReponse resp;
    
	SetTimeout(&context, 5s + kMaxWait);
	req.set_token(FLAGS_scheduler_token);
    req.set_mills_to_wait(kMaxWait / 1ms);
    req.set_next_keep_alive_in_ms(kExpiresIn / 1ms);
    *req.mutable_env_desc() = keeper->env_desc;
    req.set_immeadiate_reqs(keeper->waiters);
    req.set_prefetch_reqs(1);
    req.set_min_version(DISTRIBUILD_VERSION);

	lock.unlock();
	auto status = scheduler_stub_->WaitForStaringTask(&context, req, &resp); // 阻塞调用，不加锁
	lock.lock();

	if (status.ok()) {
	  // 存储授权信息
	  for (int i = 0; i < resp.grants_size(); ++i) {
        keeper->remaining.push(GrantDesc{
		  .expire_tp = std::chrono::steady_clock::now() + kMaxWait - kNetworkDelayTolerance,
		  .grant_id = resp.grants(i).task_grant_id(),
		  .servant_location = resp.grants(i).servant_location(),
		});
	  }
	  keeper->available_cv.notify_all(); // 通知已经获得授权
	} else {
	  LOG_WARN("启动任务失败，错误信息：{}", status.error_message());
	  Poco::Thread::sleep(100);
	}
  }
}

}  // namespace distribuild::daemon::local