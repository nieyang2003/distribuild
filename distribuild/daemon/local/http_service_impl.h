#pragma once

#include <Poco/Net/HTTPRequestHandlerFactory.h>
#include <Poco/Net/HTTPRequestHandler.h>
#include <Poco/Net/HTTPServerRequest.h>
#include <Poco/Net/HTTPServerResponse.h>
#include <Poco/URI.h>
#include <unordered_map>
#include <functional>
#include <string>

namespace distribuild::daemon::local {

/// @brief http服务实现
/// 用于与用户通信
class HttpServiceImpl : public Poco::Net::HTTPRequestHandler {
 public:
  using Handler = std::function<void(Poco::Net::HTTPServerRequest&, Poco::Net::HTTPServerResponse&)>;

  HttpServiceImpl();
  virtual ~HttpServiceImpl() override = default;
  
  void handleRequest(Poco::Net::HTTPServerRequest& request, Poco::Net::HTTPServerResponse& response) override;

 private:
  /// @brief 用户获取服务版本号
  void GetVersion   (Poco::Net::HTTPServerRequest& request, Poco::Net::HTTPServerResponse& response);

  /// @brief 获取配额
  /// @param request 超时时间、是否是轻量级任务、请求者pid的json报文
  /// @param response http响应
  /// 由TaskMonitor控制任务的数量
  void AcquireQuota (Poco::Net::HTTPServerRequest& request, Poco::Net::HTTPServerResponse& response);

  /// @brief 释放配额
  /// @param request 请求者pid的json报文
  /// @param response http 200
  void ReleaseQuota (Poco::Net::HTTPServerRequest& request, Poco::Net::HTTPServerResponse& response);

  /// @brief 
  /// @param request 
  /// @param response 
  void SetFileDigest(Poco::Net::HTTPServerRequest& request, Poco::Net::HTTPServerResponse& response);

  /// @brief 提交C++编译任务
  /// @param request 
  /// @param response 
  void SubmitCxxTask(Poco::Net::HTTPServerRequest& request, Poco::Net::HTTPServerResponse& response);

  /// @brief 
  /// @param request 
  /// @param response 
  void WaitForTask  (Poco::Net::HTTPServerRequest& request, Poco::Net::HTTPServerResponse& response);

  /// @brief 
  /// @param request 
  /// @param response 
  void AskToLeave   (Poco::Net::HTTPServerRequest& request, Poco::Net::HTTPServerResponse& response);

 private:
  std::unordered_map<std::string, Handler> get_router_;
  std::unordered_map<std::string, Handler> post_router_;
};

class HttpFactory : public Poco::Net::HTTPRequestHandlerFactory {
 public:
  HttpFactory() {}
  ~HttpFactory() {};

  Poco::Net::HTTPRequestHandler* createRequestHandler(const Poco::Net::HTTPServerRequest& request) override {
      return new HttpServiceImpl;
  }
};

} // namespace distribuild::daemon::local