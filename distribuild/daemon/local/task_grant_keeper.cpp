#include "task_grant_keeper.h"

#include <grpc/grpc.h>
#include <grpcpp/grpcpp.h>
#include <grpcpp/create_channel.h>

#include "distribuild/common/logging.h"

using namespace std::literals;

namespace distribuild::daemon::local {

TaskGrantKeeper::TaskGrantKeeper() {
  auto channel = grpc::CreateChannel("127.0.0.1:10000", grpc::InsecureChannelCredentials());
  auto scheduler_stub_ = scheduler::SchedulerService::NewStub(channel);
  DISTBU_CHECK(scheduler_stub_);
}

std::optional<TaskGrantKeeper::GrantDesc> TaskGrantKeeper::Get(
    const EnviromentDesc& desc, const std::chrono::nanoseconds& timeout) {
  // 获得keeper
  EnvGrantKeeper* keeper;
  {
	std::scoped_lock lock(mutex_);
    auto&& new_keeper = keepers_[desc.compiler_digest()];
	if (!new_keeper) {
	  new_keeper = std::make_unique<EnvGrantKeeper>();
	  new_keeper->env_desc = desc;
	  keeper = new_keeper.get();
	}
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

  ++keeper->waiters; // ! 是否要加锁

  // 通知协程去申请grant
  keeper->need_more_cv.notify_all();
  if (!keeper->available_cv.wait_for(lock, timeout, [&] { return !keeper->remaining.empty(); })) {
	DISTBU_CHECK(--keeper->waiters == 0);
    return {}; // 申请失败
  }

  // 申请成功
  auto result = keeper->remaining.front();
  keeper->remaining.pop();
  DISTBU_CHECK(--keeper->waiters == 0);
  return result;
}

void TaskGrantKeeper::Free(std::uint64_t grant_id) {
  grpc::ClientContext context;
  context.set_deadline(std::chrono::system_clock::now() + 5s);

  scheduler::FreeTaskRequst req;
  req.set_token("123456");
  req.add_task_grant_ids(grant_id);

  grpc::CompletionQueue cq;

  // TODO: 设置异步等待结果函数
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
  for (auto&& [compiler_digest, keeper] : keepers_) {
	// TODO: 回收所有协程
  }
}

void TaskGrantKeeper::GrantFetcherProc(EnvGrantKeeper* keeper) {
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

	auto now = std::chrono::steady_clock::now();
	grpc::ClientContext context;
	scheduler::WaitForStaringTaskRequest req;
	scheduler::WaitForStaringTaskReponse res;
    
	context.set_deadline(now + kMaxWait + 5s);
	req.set_token("123456");
    req.set_mills_to_wait(kMaxWait / 1ms);
    req.set_next_keep_alive_in_ms(kExpiresIn / 1ms);
    *req.mutable_env_desc() = keeper->env_desc;
    req.set_immeadiate_reqs(keeper->waiters);
    req.set_prefetch_reqs(1);
    req.set_min_version(1);
    
	lock.unlock();
	auto result = scheduler_stub_->WaitForStaringTask(&context, req, &res); // 阻塞调用，不加锁
	lock.lock();

	if (result.ok()) {
	  // 存储授权信息
	  for (int i = 0; i < res.grants_size(); ++i) {
        keeper->remaining.push(GrantDesc{
		  .expire_tp = now + kMaxWait - kNetworkDelayTolerance,
		  .grant_id = res.grants(i).task_grant_id(),
		  .servant_location = res.grants(i).servant_location(),
		});
	  }
	  keeper->available_cv.notify_all(); // 通知已经获得授权
	} else {
	  LOG_WARN("启动任务失败，错误信息：", result.error_details());
	  // TODO: 等待100ms
	}
  }
}

}  // namespace distribuild::daemon::local