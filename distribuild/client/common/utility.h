#pragma once
#include <string>
#include <functional>

namespace distribuild::client {

const std::string& GetSelfExecutable();

/// @brief 获得程序不含路径的基本名
std::string GetBaseName(const std::string& name);

/// @brief 获得绝对程序路径名
std::string GetRealPath(const std::string& name);

/// @brief 遍历"PATH"环境变量下的目录寻找目标可执行文件的绝对路径
/// @param executable 目标可执行文件
/// @param filter 过滤器，过滤路径
std::string FindExecutableInPath(const std::string& executable,
	const std::function<bool(const std::string& path)>& filter);

/// @brief 获得文件修改时间和大小
/// @param file 
/// @return 
std::pair<std::uint64_t, std::uint64_t>
  GetFileModifytimeAndSize(const std::string& file);

} // namespace distribuild::client