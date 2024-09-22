#include "http_service_impl.h"
#include <Poco/JSON/Object.h>
#include <Poco/JSON/Stringifier.h>
#include <Poco/JSON/Parser.h>
#include <Poco/JSON/Array.h>
#include <Poco/StreamCopier.h>
#include <google/protobuf/util/json_util.h>
#include <sys/signal.h>
#include <optional>
#include "daemon/version.h"
#include "daemon/local/dist_task/cxx_task.h"
#include "daemon/local/task_monitor.h"
#include "daemon/local/task_dispatcher.h"
#include "daemon/local/file_cache.h"
#include "common/multi_chunk.h"
#include "common/spdlogging.h"
#include "../build/distribuild/proto/http_service.grpc.pb.h"
#include "../build/distribuild/proto/http_service.pb.h"

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
  google::protobuf::util::JsonPrintOptions options;
  options.add_whitespace = true;
  options.always_print_primitive_fields = true;
  options.preserve_proto_field_names = true;

  std::string result;
  auto status = google::protobuf::util::MessageToJsonString(message, &result, options);
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
  LOG_DEBUG("收到来自`{}`的请求，请求路径：{}", request.clientAddress().toString(), request.getURI());

  if (request.getMethod() == Poco::Net::HTTPServerRequest::HTTP_GET) {
    if (auto iter = get_router_.find(request.getURI()); iter != get_router_.end()) {
	  return iter->second(request, response);
	}
  } else if (request.getMethod() == Poco::Net::HTTPServerRequest::HTTP_POST) {
    if (auto iter = post_router_.find(request.getURI()); iter != post_router_.end()) {
	  return iter->second(request, response);
	}
  }

  spdlog::error("404 Not Found: {}", request.getURI());
  response.setStatus(Poco::Net::HTTPServerResponse::HTTP_NOT_FOUND);
  response.send() << "Distribuild: 404 Not Found";
}

void HttpServiceImpl::GetVersion(Poco::Net::HTTPServerRequest& request,
                                 Poco::Net::HTTPServerResponse& response) {
  Poco::JSON::Object json;
  json.set("version", DISTRIBUILD_VERSION);
  json.set("time", "2024/5/26");

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
	if (!json->has("ms_to_wait") || !json->get("ms_to_wait").isInteger() ||
	    !json->has("lightweight") || !json->get("lightweight").isBoolean() ||
		!json->has("requestor_pid") || !json->get("requestor_pid").isInteger()) {
	  response.setStatus(Poco::Net::HTTPResponse::HTTP_BAD_REQUEST);
	  response.send() << "非法参数";
	}

	// 获取参数
	auto ms_to_wait = json->get("ms_to_wait").convert<uint32_t>() * 1ms;
	auto lightweight = json->get("lightweight").convert<bool>();
	auto requestor_pid = json->get("requestor_pid").convert<uint32_t>();

	// 请求任务监视器，等待获得许可
	if (!TaskMonitor::Instance()->WaitForNewTask(requestor_pid, lightweight, ms_to_wait)) {
	  LOG_DEBUG("HttpServiceImpl::AcquireQuota");
	  response.setStatus(Poco::Net::HTTPResponse::HTTP_SERVICE_UNAVAILABLE);
	}

	response.send();
  } catch (const Poco::JSON::JSONException& e) {
    response.setStatus(Poco::Net::HTTPResponse::HTTP_BAD_REQUEST);
	response.send() << "400 Bad Request: Invalid JSON format";
  } catch (const std::exception& e) {
    response.setStatus(Poco::Net::HTTPResponse::HTTP_INTERNAL_SERVER_ERROR);
    response.send() << "500 Internal Server Error: " << e.what();
  }
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
	response.send();
  } catch (const Poco::JSON::JSONException& e) {
    response.setStatus(Poco::Net::HTTPResponse::HTTP_BAD_REQUEST);
	response.send() << "400 Bad Request: Invalid JSON format";
  } catch (const std::exception& e) {
    response.setStatus(Poco::Net::HTTPResponse::HTTP_INTERNAL_SERVER_ERROR);
    response.send() << "500 Internal Server Error: " << e.what();
  }
}

void HttpServiceImpl::SetFileDigest(Poco::Net::HTTPServerRequest& request,
                                    Poco::Net::HTTPServerResponse& response) {
  // 解析内容
  std::string body(std::istreambuf_iterator<char>(request.stream()), {});
  http_service::SetFileDigestRequest req;
  auto status = google::protobuf::util::JsonStringToMessage(body, &req);
  // 解析失败
  if (!status.ok()) {
	response.setStatus(Poco::Net::HTTPResponse::HTTP_BAD_REQUEST);
	LOG_DEBUG("解析请求失败：{}", status.message().as_string());
	response.send() << "解析请求失败";
  }
  // 解析成功，设置缓存
  FileCache::Instance()->Set(req.file_desc().path(), req.file_desc().size(), req.file_desc().mtime(), req.digest());
  // 默认 200 OK
  response.send();
}

void HttpServiceImpl::SubmitCxxTask(Poco::Net::HTTPServerRequest& request,
                                    Poco::Net::HTTPServerResponse& response) {
  try {
	// 获得字符串
	std::string body(std::istreambuf_iterator<char>(request.stream()), {});

    // 解析分块
	auto parts = TryParseMultiChunk(body);
    if (!parts || parts->empty()) {
	  LOG_INFO("解析分块失败");
	  response.setStatus(Poco::Net::HTTPResponse::HTTP_BAD_REQUEST);
	  response.setContentType("text/plain");
	  response.send() << "解析分块失败";
	  return;
	}

    // 解析message
	http_service::SubmitCxxTaskRequest req;
	auto rt = google::protobuf::util::JsonStringToMessage(parts->front(), &req);
	if (!rt.ok()) {
	  LOG_INFO("解析json失败：{}", rt.ToString());
	  response.setStatus(Poco::Net::HTTPResponse::HTTP_BAD_REQUEST);
	  response.setContentType("text/plain");
	  response.send() << "解析json失败";
	  return;
	}

    // 解析出task
	auto task = std::make_unique<CxxDistTask>();
	auto status = task->Prepare(req, std::vector<std::string_view>(parts->begin() + 1, parts->end()));
	if (!status.ok()) {
	  LOG_INFO("prepare失败：{}", status.error_message());
	  response.setStatus(Poco::Net::HTTPResponse::HTTP_BAD_REQUEST);
	  response.send() << "prepare失败";
	  return;
	}

    // 放入TaskDispatcher
	http_service::SubmitCxxTaskResponse resp;
	resp.set_task_id(TaskDispatcher::Instance()->QueueTask(std::move(task), std::chrono::steady_clock::now() + 5min));
    LOG_INFO("放入任务 task_id = {}", resp.task_id());

	// 序列化结果为json
	response.setContentType("application/json");
	response.send() << MessageToJson(resp);;
  } catch (const Poco::JSON::JSONException& e) {
    response.setStatus(Poco::Net::HTTPResponse::HTTP_BAD_REQUEST);
	response.send() << "400 Bad Request: 非法json格式";
  } catch (const std::exception& e) {
    response.setStatus(Poco::Net::HTTPResponse::HTTP_INTERNAL_SERVER_ERROR);
    response.send() << "500 Internal Server Error: " << e.what();
  }
}

void HttpServiceImpl::WaitForTask(Poco::Net::HTTPServerRequest& request,
                                  Poco::Net::HTTPServerResponse& response) {
  constexpr auto kMaxWaitSeconds = 10s; // 用户设定的最长等待时间

  try {
	// 将 JSON 对象转换为字符串
	std::string body(std::istreambuf_iterator<char>(request.stream()), {});

    // 解析message
	http_service::WaitForCXXTaskRequest req;
	auto rt = google::protobuf::util::JsonStringToMessage(body, &req);
	if (!rt.ok()) {
	  response.setStatus(Poco::Net::HTTPResponse::HTTP_BAD_REQUEST);
	  response.setContentType("text/plain");
	  response.send() << "解析请求失败: " << rt.ToString();
	  return;
	}
    
	// 检查超时时间
	if (req.ms_to_wait() * 1ms > kMaxWaitSeconds) {
	  response.setStatus(Poco::Net::HTTPResponse::HTTP_BAD_REQUEST);
	  response.setContentType("text/plain");
	  response.send() << "等待时间过长 `ms_to_wait`";
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
	  response.send();
	  return;
	}

    auto cxx_task = static_cast<CxxDistTask*>(result.first.get());
	DISTBU_CHECK(cxx_task);

    // 设置结果
    auto output = cxx_task->GetOutput();
	if (output) {
	  auto&& files = output->second;
	  files.insert(files.begin(), MessageToJson(output->first));
	  response.send() << distribuild::MakeMultiChunk(files);
	} else {
      response.setStatus(Poco::Net::HTTPResponse::HTTP_EXPECTATION_FAILED);
	  response.send();
	}
  } catch (const Poco::JSON::JSONException& e) {
    response.setStatus(Poco::Net::HTTPResponse::HTTP_BAD_REQUEST);
	response.send() << "400 Bad Request: json格式错误";
  } catch (const std::exception& e) {
    response.setStatus(Poco::Net::HTTPResponse::HTTP_INTERNAL_SERVER_ERROR);
    response.send() << "500 Internal Server Error: " << e.what();
  }
}

void HttpServiceImpl::AskToLeave(Poco::Net::HTTPServerRequest& request,
                                 Poco::Net::HTTPServerResponse& response) {
  kill(getpid(), SIGINT);
  response.send();
}

}  // namespace distribuild::daemon::local