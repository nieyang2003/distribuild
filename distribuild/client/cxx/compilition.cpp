#include "client/cxx/compilition.h"
#include "common/logging.h"
#include "common/multi_chunk.h"
#include "common/crypto/zstd.h"
#include "common/crypto/blake3.h"
#include "common/encode.h"
#include "client/common/daemon_call.h"
#include "client/common/utility.h"
#include <json/json.h>
#include <fmt/format.h>
#include <fstream>

using namespace std::literals;

namespace distribuild::client {

namespace {

const std::unordered_set<std::string_view> kIgnoreCloudArgs = {"-MMD", "-MF", "-MD", "-MT", "-MP", "-o", "-Wmissing-include-dirs"};
const std::vector<std::string_view> kIgnoreCloudPrefixes = {"-Wp,-MMD", "-Wp,-MF", "-Wp,-MD",  "-Wp,-MD", "-Wp,-MP",  "-I",      "-include", "-isystem"};

/// @brief 读取文件二进制数据制作digest
std::string GetFileDigest(const std::string_view& path) {
  std::ifstream input(std::string(path), std::ios::binary);
  if (!input) {
	LOG_FATAL("打开文件'{}'失败", path);
  }
  return EncodeHex(Blake3(std::string(std::istreambuf_iterator<char>(input), {})));
}

} // namespace

/// @brief 提交编译任务
/// @param args 
/// @param rewritten_source 
/// @return 
std::optional<uint64_t> SubmitComileTask(const CompilerArgs& args, RewriteResult rewritten_source) {
  auto&& compiler = args.GetCompiler();
  auto&& [mtime, size] = GetFileModifytimeAndSize(compiler);
  Json::Value task_req;

  task_req["requestor_pid"] = getpid();
  task_req["source_path"] = rewritten_source.source_path;
  task_req["source_digest"] = rewritten_source.source_digest;
  task_req["cache_control"] = static_cast<int>(rewritten_source.cache_control);
  task_req["compiler_args"] = args.Rewrite(kIgnoreCloudArgs,
                              kIgnoreCloudPrefixes,
                              {"-fpreprocessed", rewritten_source.directives_only ? "-fdirectives-only" : "", "-x", rewritten_source.language, "-"},
							  false).ToCommandLine(false);
  task_req["compiler"]["path"] = std::string(compiler);
  task_req["compiler"]["size"] = static_cast<Json::UInt64>(size);
  task_req["compiler"]["mtime"] = static_cast<Json::UInt64>(mtime);

  /// 构造报文数据块
  std::vector<std::string_view> parts;
  auto req = Json::FastWriter().write(task_req);
  parts.push_back(req);
  parts.push_back(rewritten_source.zstd_rewritten);
  std::string header_line = MakeMultiChunkHeaderLine(parts);
  parts.insert(parts.begin(), header_line);

  /// 发送请求
  DaemonResponse response = DaemonHttpCall("/local/submit_cxx_task", parts, 5);
  if (!response.resp) { return std::nullopt; }

  if (response.resp->getStatus() == 400) {
	LOG_TRACE("Submit失败：status = {} {}\n{}", (int)response.resp->getStatus(), response.resp->getReason(), response.body);

    Json::Value req_body;
	req_body["file_desc"]["path"]  = compiler;
	req_body["file_desc"]["size"]  = size;
	req_body["file_desc"]["mtime"] = mtime;
	req_body["digest"] = GetFileDigest(compiler);

	auto&& [r, b] = DaemonHttpCall("/local/set_file_digest", req_body, 1);
	if (!r || r->getStatus() != 200) {
	  return std::nullopt;
	}

    LOG_DEBUG("重试");
	response = DaemonHttpCall("/local/submit_cxx_task", parts, 5);
	if (!response.resp) { return std::nullopt; }
  }

  if (response.resp->getStatus() == 200) {
	Json::Value json_value;
    if (!Json::Reader().parse(response.body, json_value)) {
      LOG_ERROR("收到无效响应：\n{}", response.body);
      return std::nullopt;
    }
	if (!json_value.isMember("task_id") || !json_value["task_id"].isString()) {
      LOG_ERROR("收到无效响应：\n{}", response.body);
	  return std::nullopt;
	}
	return std::stoul(json_value["task_id"].asString());
  }

  LOG_ERROR("Submit被拒绝，status = {} {}\n{}", (int)response.resp->getStatus(), response.resp->getReason(), response.body);
  return std::nullopt;
}

CompileResult WaitForCloudCompileResult(const CompilerArgs& args, const uint64_t task_id) {
  do {
	Json::Value req_body;
	req_body["task_id"] = task_id;
	req_body["ms_to_wait"] = 10000; // 10s
    auto&& [response, body] = DaemonHttpCall("/local/wait_for_cxx_task", req_body, 15);

	if (!response) {
	  return {-1};
	} else if (response->getStatus() == 503) {
	  LOG_DEBUG("503");
	  continue;
	} else if (response->getStatus() == 404) {
	  LOG_WARN("守护进程遗忘了任务：statu: {} {}\n{}", (int)response->getStatus(), response->getReason(), body);
	  return {-1};
	} else if (response->getStatus() != 200) {
	  LOG_ERROR("失败：status: {} {}\n{}", (int)response->getStatus(), response->getReason(), body);
	}

	// 解析请求为分块
	Json::Value json_value;
	auto parsed = TryParseMultiChunk(body);
	if (!parsed || parsed->empty() || !Json::Reader().parse(parsed->front().data(), parsed->front().data() + parsed->front().size(), json_value)) {
		LOG_ERROR("从守护进程收到的响应格式错误");
		return {-1};
	}
	LOG_DEBUG("编译结果👍👍👍\n{}", json_value.toStyledString());

	std::vector<std::pair<std::string, std::string>> output_files;
	std::size_t output_file_bytes = 0;

	for (Json::Value::ArrayIndex i = 0; i != json_value["file_extensions"].size(); ++i) {
	  auto path = json_value["file_extensions"][i].asString();
	  auto decompressed = ZSTDDecompress(parsed->at(i + 1));
	  if (!decompressed) {
		LOG_ERROR("zstd解压缩失败");
		return {-1};
	  }
	  output_files.emplace_back(path, std::move(*decompressed));
	  output_file_bytes += output_files.back().second.size();
	}

	// TODO: 其它选项

	CompileResult result = {
      .exit_code = json_value["exit_code"].asInt(),
	  .std_out   = json_value["std_out"].asString(),
	  .std_err   = json_value["std_err"].asString(),
	  .output_files = std::move(output_files),
	};
	LOG_DEBUG("云端编译结果: exit_code {}, stdout {} bytes, stderr {} bytes, {} output files ({} bytes in total).",
               result.exit_code, result.std_out.size(), result.std_err.size(),result.output_files.size(), output_file_bytes);

	return result;

  } while(true);
}

CompileResult CompileOnCloud(const CompilerArgs& args, RewriteResult rewritten_source) {
  DISTBU_CHECK_FORMAT(!rewritten_source.zstd_rewritten.empty(), "没有被压缩");
  LOG_TRACE("压缩后文件大小为 {} 字节", rewritten_source.zstd_rewritten.size());
  
  LOG_TRACE("开始提交");
  auto task_id = SubmitComileTask(args, std::move(rewritten_source));
  if (!task_id) {
	LOG_WARN("无法提交编译任务到云端");
	return {-1};
  }
  LOG_TRACE("提交成功, task id: {}", *task_id);

  // 返回结果
  return WaitForCloudCompileResult(args, *task_id);
}

}