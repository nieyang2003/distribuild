# 查找源码
file(GLOB SOURCES "./*.cpp" "./*/*.cpp" "./*/*/*.cpp")
list(REMOVE_ITEM SOURCES "${CMAKE_CURRENT_SOURCE_DIR}/main.cpp")
# 静态库
add_library(lib_daemon STATIC ${SOURCES})
# 可执行文件
add_executable(daemon main.cpp)
# 链接
target_link_libraries(daemon PRIVATE
	lib_daemon
    spdlog::spdlog
	gflags
	proto
	Poco::Foundation
	Poco::Net
	Poco::JSON
	Poco::Util
	blake3
	zstd
)