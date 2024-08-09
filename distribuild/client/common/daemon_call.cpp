#include "client/common/daemon_call.h"
#include "client/common/config.h"
#include "common/logging.h"
#include <Poco/Net/HTTPClientSession.h>
#include <Poco/Net/HTTPRequest.h>
#include <Poco/Exception.h>
#include <Poco/StreamCopier.h>
#include <memory>

namespace distribuild::client {

DaemonResponse DaemonHttpCall(const std::string &api, const std::vector<std::string_view> &chunks, const uint32_t timeout_seconds) {
  auto response = std::make_unique<Poco::Net::HTTPResponse>();
  try {
    // 创建 HTTP 会话，并设置超时时间
    Poco::Net::HTTPClientSession session(config::GetDaemonAddr(), config::GetDaemonPort());
    session.setTimeout(Poco::Timespan(timeout_seconds, 0));

    // 创建 HTTP 请求
    Poco::Net::HTTPRequest request(Poco::Net::HTTPRequest::HTTP_POST, api, Poco::Net::HTTPMessage::HTTP_1_1);
	request.setContentType("application/distribuild-multi-chunk");
	// 发送之前设置
	std::size_t content_length = 0;
	for (auto chunk : chunks) {
	  content_length += chunk.size();
	}
	request.setContentLength(content_length);

	// 发送请求
    std::ostream& os = session.sendRequest(request);
    for (auto chunk : chunks) {
      os.write(chunk.data(), chunk.size());
    }

    // 接收响应
	std::string body;
    std::istream& is = session.receiveResponse(*response.get());
	Poco::StreamCopier::copyToString(is, body);
	return { std::move(response), std::move(body) };

  } catch (const Poco::TimeoutException& ex) {
	LOG_ERROR("请求超时：{}", ex.displayText());
	return {nullptr, {}};
  } catch (const Poco::Exception& ex) {
    LOG_ERROR("Poco异常：{}", ex.displayText());
	return {nullptr, {}};
  }
}

DaemonResponse DaemonHttpCall(const std::string &api, const Json::Value &body, const uint32_t timeout_seconds) {
  auto response = std::make_unique<Poco::Net::HTTPResponse>();
  try {
    // 创建 HTTP 会话，并设置超时时间
    Poco::Net::HTTPClientSession session(config::GetDaemonAddr(), config::GetDaemonPort());
    session.setTimeout(Poco::Timespan(timeout_seconds, 0));

    // 创建 HTTP 请求
    Poco::Net::HTTPRequest request(Poco::Net::HTTPRequest::HTTP_POST, api, Poco::Net::HTTPMessage::HTTP_1_1);
	request.setContentType("application/application/json");

	// 序列化
	Json::StreamWriterBuilder writer;
    std::string body_string = Json::writeString(writer, body);
	request.setContentLength(body_string.length());

	// 发送请求
    session.sendRequest(request).write(body_string.data(), body_string.length());

    // 接收响应
	std::string body;
    std::istream& is = session.receiveResponse(*response.get());
	Poco::StreamCopier::copyToString(is, body);
	return { std::move(response), std::move(body) };

  } catch (const Poco::TimeoutException& ex) {
	LOG_ERROR("请求超时：{}", ex.displayText());
	return {nullptr, {}};
  } catch (const Poco::Exception& ex) {
    LOG_ERROR("Poco异常：{}", ex.displayText());
	return {nullptr, {}};
  }
}

} // namespace distribuild::client