#include "daemon_call.h"

namespace distribuild::client {

DaemonResponse DaemonCall(const std::string& api, const std::vector<std::string>& headers, const std::vector<std::string_view>& bodies, std::chrono::nanoseconds timeout) {
  // 构建头部（body已经构建好）
  // 使用ASIO？
  // 连接
  // 发送
  // 等待响应（超时）
}

}