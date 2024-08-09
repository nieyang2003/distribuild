#pragma once
#include <sys/types.h>
#include <vector>
#include <string>
#include <queue>
#include <memory>
#include <dirent.h>
#include <sys/stat.h>
#include <unistd.h>
#include "common/logging.h"

namespace distribuild {

using namespace std::literals;

struct DirNode {
  std::string name;
  std::uint64_t inode;
  bool is_block_dev: 1;
  bool is_char_dev: 1;
  bool is_dir: 1;
  bool is_symlink: 1;
  bool is_regular: 1;
  bool is_unix_socket: 1;
};

// inline void MkDir(const std::string &path, mode_t mode = 0755) {
//     std::string copy = path;
//     const char* dir = copy.c_str();
//     for (size_t pos = copy.find('/', 0); pos != std::string::npos; pos = copy.find('/', pos + 1)) {
//         copy[pos] = '\0';
//         if (mkdir(dir, mode) == -1) {
//             DISTBU_CHECK_FORMAT(errno == EEXIST, "创建目录[{}]失败", dir);
//         }
//         copy[pos] = '/';
//     }
//     if (mkdir(dir, mode) == -1) {
//         DISTBU_CHECK_FORMAT(errno == EEXIST, "创建目录[{}]失败", dir);
//     }
// 	LOG_DEBUG("创建目录 `{}`", path);
// }

inline void Mkdirs(const std::string& path, mode_t mode = 0755) {
  auto copy = path;
  auto dir_path = copy.data();
  for (char* p = strchr(dir_path + 1, '/'); p; p = strchr(p + 1, '/')) {
    *p = '\0';
    if (mkdir(dir_path, mode) == -1) {
      DISTBU_CHECK_FORMAT(errno == EEXIST, "创建目录`{}`失败", dir_path);
    }
    *p = '/';
  }
  if (mkdir(dir_path, mode) == -1) {
    DISTBU_CHECK_FORMAT(errno == EEXIST, "创建目录[{}]失败", dir_path);
  }
}

inline void RemoveDir(const std::string &path) {
  std::unique_ptr<DIR, void(*)(DIR*)> dir(opendir(path.c_str()), [](auto ptr){ closedir(ptr); });
  DISTBU_CHECK_FORMAT(dir, "打开目录[{}]失败", path);
  while (auto dr = readdir(dir.get())) {
      if (dr->d_name == std::string(".") || dr->d_name == std::string("..")) {
          continue;
      }
      auto fullname = fmt::format("{}/{}", path, dr->d_name);
      if (unlink(fullname.c_str()) != 0) {
          DISTBU_CHECK_FORMAT(errno == EISDIR, "删除目录[{}]失败", fullname);
          RemoveDir(fullname);
      } else {
          LOG_INFO("删除目录", fullname);
      }
  }
  DISTBU_CHECK(rmdir(path.c_str()) == 0);
  LOG_DEBUG("删除目录 `{}`", path);
}

inline std::vector<DirNode> GetDirNodes(const std::string &path) {
  std::unique_ptr<DIR, int(*)(DIR*)> dir(opendir(path.c_str()), &closedir);
  std::vector<DirNode> result;

  DISTBU_CHECK_FORMAT(dir != nullptr, "打开目录{}失败", path);
  while (auto dir_ent = readdir(dir.get())) { // 遍历目录
	if (dir_ent->d_name == "."sv || dir_ent->d_name == ".."sv) {
	  continue;
	}

	auto type = dir_ent->d_type;
	DirNode node = {
      .name           = dir_ent->d_name,
	  .inode          = dir_ent->d_ino,
      .is_block_dev   = !!(type & DT_BLK),
      .is_char_dev    = !!(type & DT_CHR),
      .is_dir         = !!(type & DT_DIR),
      .is_symlink     = !!(type & DT_LNK),
      .is_regular     = !!(type & DT_REG),
      .is_unix_socket = !!(type & DT_SOCK),
	};
    result.push_back(node);
  }
  return result;
}

inline std::vector<DirNode> GetDirNodesRecursively(const std::string &path) {
  std::vector<DirNode> result;
  std::queue<std::string> queue({""});

  while (!queue.empty()) {
	auto temp = GetDirNodes(path + "/" + queue.front());
	if (!queue.front().empty()) {
	  for (auto&& e : temp) {
		e.name = queue.front() + "/" + e.name;
	  }
	}
	queue.pop();
	result.insert(result.end(), temp.begin(), temp.end());
	for (auto&& e : temp) {
	  if (e.is_dir) {
	    queue.push(e.name);
	  }
	}
  }

  return result;
}

} // namespace distribuild
