#include "http_service_impl.h"

#include <Poco/JSON/Object.h>
#include <Poco/JSON/Stringifier.h>
#include <Poco/JSON/Parser.h>
#include <Poco/JSON/Array.h>
#include <Poco/StreamCopier.h>
#include <google/protobuf/util/json_util.h>
#include <sys/signal.h>
#include <optional>

#include "distribuild/daemon/local/dist_task/cxx_task.h"
#include "distribuild/daemon/local/task_monitor.h"
#include "distribuild/daemon/local/task_dispatcher.h"
#include "distribuild/daemon/local/file_cache.h"
#include "distribuild/common/multi_chunk.h"
#include "distribuild/common/logging.h"

#include "http_service.grpc.pb.h"
#include "http_service.pb.h"

using namespace std::literals;

namespace distribuild::daemon::local {

namespace {

template<class T>
std::optional<T> TryParseJson(const std::string& str) {
  T message;
  auto result = google::protobuf::util::JsonStringToMessage(str, &message);
  if (result.ok()) {
	return message;
  } else {
	return std::nullopt;
  }
}

std::string MessageToJson(const google::protobuf::Message& message) {
  std::string result;
  auto status = google::protobuf::util::MessageToJsonString(message, &result);
  DISTBU_CHECK(status.ok());
  return result;
}

} // namespace

HttpServiceImpl::HttpServiceImpl() {
  get_router_ = {
    {"/local/get_version", std::bind(&HttpServiceImpl::GetVersion, this, std::placeholders::_1, std::placeholders::_2)}
  };
  post_router_ = {
    {"/local/acquire_quota", std::bind(&HttpServiceImpl::AcquireQuota, this, std::placeholders::_1, std::placeholders::_2)},
	{"/local/release_quota", std::bind(&HttpServiceImpl::ReleaseQuota, this, std::placeholders::_1, std::placeholders::_2)},
	{"/local/set_file_digest", std::bind(&HttpServiceImpl::SetFileDigest, this, std::placeholders::_1, std::placeholders::_2)},
	{"/local/submit_cxx_task", std::bind(&HttpServiceImpl::SubmitCxxTask, this, std::placeholders::_1, std::placeholders::_2)},
	{"/local/wait_for_cxx_task", std::bind(&HttpServiceImpl::WaitForTask, this, std::placeholders::_1, std::placeholders::_2)},
	{"/local/ask_to_leave", std::bind(&HttpServiceImpl::AskToLeave, this, std::placeholders::_1, std::placeholders::_2)},
  };
}

void HttpServiceImpl::handleRequest(Poco::Net::HTTPServerRequest& request, Poco::Net::HTTPServerResponse& response) {
  if (request.getMethod() == Poco::Net::HTTPServerRequest::HTTP_GET) {
    if (auto iter = get_router_.find(request.getURI()); iter != get_router_.end()) {
	  return iter->second(request, response);
	}
  } else if (request.getMethod() == Poco::Net::HTTPServerRequest::HTTP_POST) {
    if (auto iter = post_router_.find(request.getURI()); iter != post_router_.end()) {
	  return iter->second(request, response);
	}
  }
  
  response.setStatus(Poco::Net::HTTPServerResponse::HTTP_NOT_FOUND);
}

void HttpServiceImpl::GetVersion(Poco::Net::HTTPServerRequest& request,
                                 Poco::Net::HTTPServerResponse& response) {
  Poco::JSON::Object json;
  json.set("version", "2024/5/26");
  json.set("message", 1);

  response.setContentType("application/json");
  std::ostream& ostr = response.send();
  Poco::JSON::Stringifier::stringify(json, ostr);
}

void HttpServiceImpl::AcquireQuota(Poco::Net::HTTPServerRequest& request,
                                   Poco::Net::HTTPServerResponse& response) {
  try {
	// 解析json
	Poco::JSON::Parser parser;
	Poco::Dynamic::Var result = parser.parse(request.stream());
	Poco::JSON::Object::Ptr json = result.extract<Poco::JSON::Object::Ptr>();
    
    // 判断字段
	if (!json->has("ms_to_wait") || !json->get("ms_to_wait").isBoolean() ||
	    !json->has("lightweight") || !json->get("lightweight").isInteger() ||
		!json->has("requestor_pid") || !json->get("requestor_pid").isInteger()) {
	  response.setStatus(Poco::Net::HTTPResponse::HTTP_BAD_REQUEST);
	  response.send() << "400 Bad Request: Invalid arguments";
	}
    
	// 获取参数
	auto ms_to_wait = json->get("ms_to_wait").convert<uint32_t>() * 1ms;
	auto lightweight = json->get("lightweight").convert<bool>();
	auto requestor_pid = json->get("requestor_pid").convert<uint32_t>();
    
	// 请求任务监视器，等待获得许可
	if (!TaskMonitor::Instance()->WaitForNewTask(requestor_pid, lightweight, ms_to_wait)) {
	  response.setStatus(Poco::Net::HTTPResponse::HTTP_SERVICE_UNAVAILABLE);
	}
  } catch (const Poco::JSON::JSONException& e) {
    response.setStatus(Poco::Net::HTTPResponse::HTTP_BAD_REQUEST);
	response.send() << "400 Bad Request: Invalid JSON format";
  } catch (const std::exception& e) {
    response.setStatus(Poco::Net::HTTPResponse::HTTP_INTERNAL_SERVER_ERROR);
    response.send() << "500 Internal Server Error: " << e.what();
  }
  // 默认 200 OK
}

void HttpServiceImpl::ReleaseQuota(Poco::Net::HTTPServerRequest& request,
                                   Poco::Net::HTTPServerResponse& response) {
  try {
	// 解析http json报文
	Poco::JSON::Parser parser;
	Poco::Dynamic::Var result = parser.parse(request.stream());
	Poco::JSON::Object::Ptr json = result.extract<Poco::JSON::Object::Ptr>();

    // 获取参数
    if (!json->has("requestor_pid") || !json->get("requestor_pid").isInteger()) {
	  response.setStatus(Poco::Net::HTTPResponse::HTTP_BAD_REQUEST);
	  response.send() << "400 Bad Request: Invalid arguments";
	}

    // 释放任务
	TaskMonitor::Instance()->DropTask(json->get("requestor_pid").convert<uint32_t>());
  } catch (const Poco::JSON::JSONException& e) {
    response.setStatus(Poco::Net::HTTPResponse::HTTP_BAD_REQUEST);
	response.send() << "400 Bad Request: Invalid JSON format";
  } catch (const std::exception& e) {
    response.setStatus(Poco::Net::HTTPResponse::HTTP_INTERNAL_SERVER_ERROR);
    response.send() << "500 Internal Server Error: " << e.what();
  }
  // 默认 200 OK
}

void HttpServiceImpl::SetFileDigest(Poco::Net::HTTPServerRequest& request,
                                    Poco::Net::HTTPServerResponse& response) {
  // 解析内容
  std::stringstream ss;
  Poco::StreamCopier::copyStream(request.stream(), ss);
  http_service::SetFileDigestRequest req;
  auto status = google::protobuf::util::JsonStringToMessage(ss.str(), &req);
  // 解析失败
  if (!status.ok()) {
	response.setStatus(Poco::Net::HTTPResponse::HTTP_BAD_REQUEST);
	response.send() << "Failed to parse request" << status.message();
  }
  // 解析成功，设置缓存
  FileCache::Instance()->Set(req.file_desc().path(), req.file_desc().size(), req.file_desc().timestamp(), req.digest());
  // 默认 200 OK
}

void HttpServiceImpl::SubmitCxxTask(Poco::Net::HTTPServerRequest& request,
                                    Poco::Net::HTTPServerResponse& response) {
  try {
	// 获得字符串
	std::ostringstream stream;
	Poco::StreamCopier::copyStream(request.stream(), stream);

    // 解析分块
	auto parts = TryParseMultiChunk(stream.str());
    if (!parts || parts->empty()) {
	  response.setStatus(Poco::Net::HTTPResponse::HTTP_BAD_REQUEST);
	  response.setContentType("text/plain");
	  response.send() << "Failed to parse request";
	  return;
	}

    // 解析message
	http_service::SubmitCxxTaskRequest req;
	auto rt = google::protobuf::util::JsonStringToMessage(parts->front(), &req);
	if (!rt.ok()) {
	  response.setStatus(Poco::Net::HTTPResponse::HTTP_BAD_REQUEST);
	  response.setContentType("text/plain");
	  response.send() << "Failed to parse request: " << rt.ToString();
	  return;
	}

    // 解析出task
	auto task = std::make_unique<CxxDistTask>();
	auto status = task->Prepare(req, std::vector<std::string_view>(parts->begin() + 1, parts->end()));
	if (!status.ok()) {
	  response.setStatus(Poco::Net::HTTPResponse::HTTP_BAD_REQUEST);
	  response.send() << status.error_message();
	  return;
	}

    // 放入TaskDispatcher
	http_service::SubmitCxxTaskResponse res;
	res.set_task_id(TaskDispatcher::Instance()->QueueTask(std::move(task), std::chrono::steady_clock::now() + 5min));
	
	// 序列化结果为json
	std::string json_result;
	google::protobuf::util::MessageToJsonString(res, &json_result);
	response.setContentType("application/json");
	response.send() << json_result;
  } catch (const Poco::JSON::JSONException& e) {
    response.setStatus(Poco::Net::HTTPResponse::HTTP_BAD_REQUEST);
	response.send() << "400 Bad Request: Invalid JSON format";
  } catch (const std::exception& e) {
    response.setStatus(Poco::Net::HTTPResponse::HTTP_INTERNAL_SERVER_ERROR);
    response.send() << "500 Internal Server Error: " << e.what();
  }
  // 默认 200 OK
}

void HttpServiceImpl::WaitForTask(Poco::Net::HTTPServerRequest& request,
                                  Poco::Net::HTTPServerResponse& response) {
  constexpr auto kMaxWaitSeconds = 10s; // 用户设定的最长等待时间

  try {
	// 将 JSON 对象转换为字符串
    std::ostringstream jsonStream;
	Poco::StreamCopier::copyStream(request.stream(), jsonStream);

    // 解析message
	http_service::WaitForCXXTaskRequest req;
	auto rt = google::protobuf::util::JsonStringToMessage(jsonStream.str(), &req);
	if (!rt.ok()) {
	  response.setStatus(Poco::Net::HTTPResponse::HTTP_BAD_REQUEST);
	  response.setContentType("text/plain");
	  response.send() << "Failed to parse request: " << rt.ToString();
	  return;
	}
    
	// 检查超时时间
	if (req.ms_to_wait() * 1ms > kMaxWaitSeconds) {
	  response.setStatus(Poco::Net::HTTPResponse::HTTP_BAD_REQUEST);
	  response.setContentType("text/plain");
	  response.send() << "Unacceptable `ms_to_wait`";
	  return;
	}

    // 分派任务
	auto result = TaskDispatcher::Instance()->WaitForTask(req.task_id(), req.ms_to_wait() * 1ms);
	if (!result.first) {
	  if (result.second == TaskDispatcher::WaitStatus::Timeout) {
		response.setStatus(Poco::Net::HTTPResponse::HTTP_SERVICE_UNAVAILABLE);
	  } else if (result.second == TaskDispatcher::WaitStatus::NotFound) {
        response.setStatus(Poco::Net::HTTPResponse::HTTP_NOT_FOUND);
	  }
	  return;
	}

    auto cxx_task = dynamic_cast<CxxDistTask*>(result.first.get());
	DISTBU_CHECK(cxx_task);

    // 设置结果
    auto output = cxx_task->GetOutput();
	if (output) {
	  auto&& files = output->second;
	  files.insert(files.begin(), MessageToJson(output->first));
	  response.send() << MakeMultiChunk(files);
	} else {
      response.setStatus(Poco::Net::HTTPResponse::HTTP_EXPECTATION_FAILED);
	}
  } catch (const Poco::JSON::JSONException& e) {
    response.setStatus(Poco::Net::HTTPResponse::HTTP_BAD_REQUEST);
	response.send() << "400 Bad Request: Invalid JSON format";
  } catch (const std::exception& e) {
    response.setStatus(Poco::Net::HTTPResponse::HTTP_INTERNAL_SERVER_ERROR);
    response.send() << "500 Internal Server Error: " << e.what();
  }
  // 默认 200 OK
}

void HttpServiceImpl::AskToLeave(Poco::Net::HTTPServerRequest& request,
                                 Poco::Net::HTTPServerResponse& response) {
  kill(getpid(), SIGINT);
}

}  // namespace distribuild::daemon::local