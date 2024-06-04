#include "privilege.h"

#include <sys/prctl.h>
#include <unistd.h>
#include <pwd.h>

#include "distribuild/common/logging.h"

namespace distribuild::daemon {

namespace {

bool IsRunningAsRoot() {
  return getuid() == 0 || geteuid() == 0;
}

std::pair<uid_t, gid_t> GetAvailableUser() {
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

void DropPrivilege() {
  if (!IsRunningAsRoot()) {
	LOG_INFO("未以root用户权限运行");
	return;
  }
}

} // namespace distribuild::daemon