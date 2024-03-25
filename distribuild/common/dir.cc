#include "dir.h"

#include <dirent.h>
#include <sys/stat.h>
#include <string.h>
#include <unistd.h>

#include "distribuild/common/logging.h"

namespace distbu {

void MkDir(const std::string &path, mode_t mode) {
    std::string copy = path;
    const char* dir = copy.data();
    for (size_t pos = copy.find('/', 0); pos != std::string::npos; pos = copy.find('/', pos + 1)) {
        copy[pos] = '\0';
        if (mkdir(dir, mode) == -1) {
            DISTBU_CHECK(errno == EEXIST, "创建目录[{}]失败", dir);
        }
        copy[pos] = '/';
    }
    if (mkdir(dir, mode) == -1) {
        DISTBU_CHECK(errno == EEXIST, "创建目录[{}]失败", dir);
    }
}

void RemoveDir(const std::string &path) {
    std::unique_ptr<DIR, void(*)(DIR*)> dir(opendir(path.c_str()), [](auto ptr){ closedir(ptr); });
    DISTBU_CHECK(dir, "打开目录[{}]失败", path);
    while (auto dr = readdir(dir.get())) {
        if (dr->d_name == std::string(".") || dr->d_name == std::string("..")) {
            continue;
        }
        auto fullname = fmt::format("{}/{}", path, dr->d_name);
        if (unlink(fullname.c_str()) != 0) {
            DISTBU_CHECK(errno == EISDIR, "删除目录[{}]失败", fullname);
            RemoveDir(fullname);
        } else {
            LOG_INFO("删除目录", fullname);
        }
    }
    DISTBU_CHECK(rmdir(path.c_str()) == 0);
}

} // namespace distribuild