#include "compilers.h"

#include "distribuild/common/string.h"
#include "distribuild/common/logging.h"
#include "distribuild/daemon/cloud/config.h"

namespace distribuild::daemon::cloud {

namespace {


std::vector<std::string> SearchCompiler(const std::string_view& dir) {
  std::vector<std::string> result;
  // TODO:
  return result;
}

void AddEnvTo(const std::string_view& path,
              std::unordered_map<std::string, std::string>& temp_paths,
              std::vector<EnviromentDesc>& temp_envs) {
  
}

} // namespace

Compilers* Compilers::Instance() {
  static Compilers instance;
  return &instance;
}

Compilers::Compilers() {
  // TODO: 启动定时器：OnTimerRescan
}

Compilers::~Compilers() {
  // TODO: 关闭定时器
}

std::vector<EnviromentDesc> Compilers::GetAll() const {
  std::shared_lock<std::shared_mutex> rlock(mutex_);
  return compilers_;
}

std::optional<std::string> Compilers::TryGetPath(
    const EnviromentDesc& env) const {
  std::shared_lock<std::shared_mutex> rlock(mutex_);
  if (auto iter = compiler_paths_.find(env.compiler_digest());
         iter != compiler_paths_.end()) {
    return iter->second;
  } else {
	return std::nullopt;
  }
}

void Compilers::Stop() {
  // TODO: 关闭定时器OnTimerRescan
}

void Compilers::Join() { }

void Compilers::OnTimerRescan() {
  std::unique_lock<std::shared_mutex> wlock(mutex_); // 一把大写锁

  std::unordered_map<std::string, std::string> temp_paths;
  std::vector<EnviromentDesc> temp_envs;

  // 查找PATH中的编译器
  for (auto&& dir : Split(getenv("PATH"), ":", true)) {
    for (auto&& e : SearchCompiler(dir)) {
	  AddEnvTo(e, temp_paths, temp_envs);
	}
  }

  // 查找用户的上传的编译包
  for (auto&& dir : Split(config::GetCompilerDirs(), ":", true)) {
	for (auto&& e : SearchCompiler(dir)) {
	  AddEnvTo(e, temp_paths, temp_envs);
	}
  }

  // TODO: 自定义编译器

  // 更新环境
  compiler_paths_.swap(temp_paths);
  compilers_.swap(temp_envs);
}

} // namespace distribuild::daemon::cloud