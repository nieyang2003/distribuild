#include "temp_dir.h"

#include <chrono>
#include <Poco/Random.h>
#include <unistd.h>
#include <stdio.h>
#include <fcntl.h>
#include <fstream>

#include "distribuild/common/logging.h"
#include "distribuild/common/dir.h"

namespace distribuild::daemon::cloud {

TempDir::TempDir(const std::string& prefix) {
  path_ = fmt::format("{}/distribuild_{}_{}", prefix,
      std::time(nullptr), Poco::Random().next());
  MkDir(path_);
  alive_ = true;
}

TempDir::~TempDir() {
  Clear();
}

std::vector<std::pair<std::string, std::string>> TempDir::ReadAll(const std::string& subdir) {
  DISTBU_CHECK(alive_);
  std::vector<std::pair<std::string, std::string>> result;
  auto root_dir = fmt::format("{}/{}", path_, subdir);
  auto dir_nodes = GetDirNodesRecursively(root_dir);

  for (auto&& node : dir_nodes) {
	if (node.is_regular) {
	  auto path = fmt::format("{}/{}", root_dir, node.name);
	  std::ifstream ifs(path, std::ios::in | std::ios::binary);
	  DISTBU_CHECK(ifs, "打开文件失败");
	  std::string content((std::istreambuf_iterator<char>(ifs)), std::istreambuf_iterator<char>());
	  result.emplace_back(node.name, content);
	}
  }

  return result;
}

void TempDir::Clear() {
  if (alive_) {
	RemoveDir(path_);
	alive_ = false;
  }
}
const std::string& GetTempDir() {
  static const std::string result = "tmp";
  return result;
}

}  // namespace distribuild::daemon::cloud