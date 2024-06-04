#include <string>

namespace distribuild::daemon {

int StartServer(int argc, char** argv) {
  setenv("LC_ALL", "en_US.utf8", true);
  unsetenv("GCC_COMPARE_DEBUG");
  unsetenv("SOURCE_DATE_EPOCH");

  // TODO: 为了安全，退出root权限

  // 禁用core dump

  // 创建临时目录

  // 清除非正常退出内容

  // 初始化单例

  // http进程

  // rpc进程

  // 启动

  // 等待关闭

}

}

int main(int argc, char** argv) {
  distribuild::daemon::StartServer(argc, argv);
}