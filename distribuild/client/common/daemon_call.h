#pragma once
#include <Poco/Net/HTTPResponse.h>
#include <json/json.h>
#include <memory>

namespace distribuild::client {

/// @brief http响应
struct DaemonResponse {
  std::unique_ptr<Poco::Net::HTTPResponse> resp;
  std::string body;        // 消息体
};

/// @brief 发起分块数据格式http请求
DaemonResponse DaemonHttpCall(const std::string& api, const std::vector<std::string_view>& chunks, const uint32_t timeout_seconds);

/// @brief 发起json格式http请求
/// @param api 
/// @param body 
/// @param timeout_seconds 
/// @return 
DaemonResponse DaemonHttpCall(const std::string& api, const Json::Value& body, const uint32_t timeout_seconds);

} // namespace distribuild::client