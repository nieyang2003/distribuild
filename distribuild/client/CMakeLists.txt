# 查找源码
file(GLOB SOURCES "common/*.cpp" "cxx/*.cpp")
# 静态库
add_library(lib_client STATIC ${SOURCES})
# 可执行文件
add_executable(distribuild client.cpp)
# 链接
target_link_libraries(distribuild PRIVATE 
	lib_client
    spdlog::spdlog
	JsonCpp::JsonCpp
	zstd
	blake3
	Poco::Net
)