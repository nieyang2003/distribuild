#include "compilition.h"

#include "distribuild/common/logging.h"
#include "distribuild/client/common/multi_chunk.h"
#include "distribuild/client/common/daemon_call.h"
#include "distribuild/client/common/utility.h"

#include "thirdparty/jsoncpp/include/json/json.h"

using namespace std::literals;

namespace distribuild::client {

/// @brief 提交编译任务
/// @param args 
/// @param rewritten_source 
/// @return 
std::optional<std::string> SubmitComileTask(const CompilerArgs& args, RewriteResult rewritten_source) {
  auto&& compiler = args.GetCompiler();
  auto&& [mtime, size] = GetFileModifytimeAndSize(compiler);
  Json::Value task_req;

  task_req["requestor_process_id"] = getpid();
  task_req["source_path"] = rewritten_source.source_path;
  task_req["source_digest"] = rewritten_source.source_digest;
  task_req["cache_control"] = static_cast<int>(rewritten_source.cache_control);
  task_req["compiler"]["path"] = compiler;
  task_req["compiler"]["size"] = static_cast<Json::UInt64>(size);
  task_req["compiler"]["mtime"] = static_cast<Json::UInt64>(mtime);
  
  // 重写到云端
  submit_task_req["compiler_invocation_arguments"] = args.Rewrite();

  /// 构造报文数据块
  std::vector<std::string_view> parts;
  auto req = Json::FastWriter().write(task_req);
  parts.push_back(req);
  parts.push_back(rewritten_source.zstd_rewritten);
  std::string header = MakeMultiChunkHeader(parts);
  parts.insert(parts.begin(), header);

  /// 发送请求
  DaemonResponse response = DaemonCall("", {""}, parts, 5s);
  
  /// 响应异常
  if (response.status == 400) {

  }

  if (response.status != 200) {
    LOG_ERROR("请求被拒绝, status = {}, body: {}", response.status, response.body);
    return {};
  }


  /// 解析返回
  Json::Value json_value;
  if (!Json::Reader().parse(response.body, json_value)) {
	LOG_ERROR("收到错误响应");
	return {};
  }
  return json_value["task_id"].asString();
}

CompileResult WaitForCloudCompileResult(const CompilerArgs& args, const std::string task_id) {
  // TODO:
  return CompileResult();
}

CompileResult CompileOnCloud(const CompilerArgs& args, RewriteResult rewritten_source) {
  DISTBU_CHECK(!rewritten_source.zstd_rewritten.empty(), "没有被压缩");
  LOG_TRACE("压缩后文件大小为 {} 字节", rewritten_source.zstd_rewritten.size());
  
  LOG_TRACE("开始提交");
  auto task_id = SubmitComileTask(args, std::move(rewritten_source));
  if (!task_id) {
	LOG_WARN("无法提交编译任务");
	return {-1};
  }
  LOG_TRACE("提交成功, task id: {}", *task_id);

  // 返回结果
  return WaitForCloudCompileResult(args, *task_id);
}

}