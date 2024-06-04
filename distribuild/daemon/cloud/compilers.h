#pragma once

#include <shared_mutex>
#include <optional>

#include "env_desc.pb.h"

namespace distribuild::daemon::cloud {

class Compilers {
 public:
  static Compilers* Instance();
  Compilers();
  ~Compilers();

  // 获得所有编译器
  std::vector<EnviromentDesc> GetAll() const;

  // 获得编译的路径
  std::optional<std::string>  TryGetPath(const EnviromentDesc& env) const;

  void Stop();

  void Join();

 private:
  void OnTimerRescan();

 private:
  mutable std::shared_mutex mutex_;

  /// @brief 本机已有编译器
  std::vector<EnviromentDesc> compilers_;

  /// @brief 本机编译器所在路径
  std::unordered_map<std::string, std::string> compiler_paths_;
};

} // namespace distribuild::daemon::cloud