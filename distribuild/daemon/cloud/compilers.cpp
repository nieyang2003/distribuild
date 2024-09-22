#include <sys/stat.h>
#include <unistd.h>
#include <fstream>
#include "daemon/cloud/compilers.h"
#include "common/tools.h"
#include "common/spdlogging.h"
#include "daemon/config.h"
#include "common/crypto/blake3.h"
#include "common/encode.h"

namespace distribuild::daemon::cloud {

namespace {

/// @brief 编译器是否存在
/// @param path 
/// @return 
std::optional<std::string> GetPathHasCompiler(const std::string& path) {
  struct stat st;
  if (lstat(path.c_str(), &st) == -1) {
    return std::nullopt; // 路径错误或文件不存在
  }
  char buf[PATH_MAX + 1];
  if (!realpath(path.c_str(), buf)) {
    return std::nullopt; // 转换为绝对路径
  }
  // 检查编译器是否对当前用户、组或其他用户可执行
  auto compiler = (geteuid() == st.st_uid && (st.st_mode & S_IXUSR)) ||
                  (getegid() == st.st_gid && (st.st_mode & S_IXGRP)) ||
                  (st.st_mode & S_IXOTH);
  if (!compiler) {
    return std::nullopt;
  }
  return buf;
}

/// @brief 在指定目录下寻找编译器
/// @param dir 
/// @return 
std::vector<std::string> SearchCompiler(const std::string_view& dir) {
  static const std::unordered_set<std::string> kSupportedCompilers = {"gcc", "g++"};

  std::vector<std::string> result;

  for (auto&& compiler : kSupportedCompilers) {
	std::optional<std::string> path;

    if (dir.empty() || dir.back() == '/') {
	  path = GetPathHasCompiler(fmt::format("{}{}", dir, compiler));
	} else {
	  path = GetPathHasCompiler(fmt::format("{}/{}", dir, compiler));
	}
	if (!path) continue;

	result.push_back(*path);
  }

  return result;
}

/// @brief 读取文件二进制数据制作digest
std::optional<std::string> TryGetFileDigest(const std::string_view& path) {
  std::ifstream input(std::string(path), std::ios::binary);
  if (!input) {
	LOG_ERROR("打开文件'{}'失败", path);
	return std::nullopt;
  }
  return EncodeHex(Blake3(std::string(std::istreambuf_iterator<char>(input), {})));
}

/// @brief 添加编译环境，忽略相同编译器
void AddEnvTo(const std::string_view& path, std::unordered_map<std::string, std::string>& found_compiler_paths, std::vector<EnviromentDesc>& found_compilers) {
  EnviromentDesc env_desc;

  auto digest = TryGetFileDigest(path);
  if (!digest) return;
  
  env_desc.set_compiler_digest(*digest);
  if (found_compiler_paths.count(*digest) == 0) {
	// 忽略重复的编译器
    found_compiler_paths[*digest] = path;
	found_compilers.push_back(env_desc);
	LOG_DEBUG("添加：`{}`，摘要：`{}`", path, *digest);
  }
}

} // namespace

Compilers* Compilers::Instance() {
  static Compilers instance;
  return &instance;
}

Compilers::Compilers()
  : timer_(0, FLAGS_compilers_rescan_timer_intervals) {
  LOG_DEBUG("启动定时器 OnTimerRescan");
  timer_.start(Poco::TimerCallback<Compilers>(*this, &Compilers::OnTimerRescan));
}

Compilers::~Compilers() {
}

std::vector<EnviromentDesc> Compilers::GetAll() const {
  std::shared_lock<std::shared_mutex> rlock(mutex_);
  return compilers_;
}

std::optional<std::string> Compilers::TryGetPath(const EnviromentDesc& env) const {
  std::shared_lock<std::shared_mutex> rlock(mutex_);
  if (auto iter = compiler_paths_.find(env.compiler_digest()); iter != compiler_paths_.end()) {
    return iter->second;
  } else {
	return std::nullopt;
  }
}

void Compilers::Stop() {
  timer_.stop();
}

void Compilers::Join() { }

void Compilers::OnTimerRescan(Poco::Timer& timer) {
  LOG_INFO("重新扫描编译器");
  std::unique_lock<std::shared_mutex> wlock(mutex_); // 一把大写锁

  std::unordered_map<std::string, std::string> found_compiler_paths;
  std::vector<EnviromentDesc> found_compilers;

  // 查找PATH中的编译器
  for (auto&& dir : Split(getenv("PATH"), ":", true)) {
    for (auto&& e : SearchCompiler(dir)) {
	  AddEnvTo(e, found_compiler_paths, found_compilers);
	}
  }

  // 查找用户的编译器
  for (auto&& dir : Split(FLAGS_compiler_dir_path, ":", true)) {
	for (auto&& e : SearchCompiler(dir)) {
	  AddEnvTo(e, found_compiler_paths, found_compilers);
	}
  }

  // 更新环境
  compiler_paths_.swap(found_compiler_paths);
  compilers_.swap(found_compilers);
}

} // namespace distribuild::daemon::cloud