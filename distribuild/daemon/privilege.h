#pragma once

#include <sys/prctl.h>
#include <sys/resource.h>
#include <unistd.h>
#include <pwd.h>
#include "common/spdlogging.h"

namespace {

inline bool IsRunningAsRoot() {
  return getuid() == 0 || geteuid() == 0;
}

inline std::pair<uid_t, gid_t> GetAvailableUser() {
  for (auto&& name : {"distribuild", "daemon", "nobody"}) {
    auto pw = getpwnam(name);
	if (pw != nullptr) {
	  return {pw->pw_uid, pw->pw_gid};
	}
  }
  LOG_WARN("无法找到合适用户，使用65534");
  return {65534, 65534};
}

} // namespace

namespace distribuild::daemon {

/// @brief 放弃root权限
void DropPrivilege() {
  if (!IsRunningAsRoot()) {
	LOG_INFO("未以root用户权限运行");
	return;
  }
}

void DisableCoreDump() {
  rlimit limit = {};
  if (setrlimit(RLIMIT_CORE, &limit) != 0) {
    LOG_FATAL("禁用core dump失败");
	return;
  }
  LOG_INFO("禁用core dump");
}

} // namespace distribuild::daemon