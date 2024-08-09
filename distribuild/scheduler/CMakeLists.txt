# 查找源码
file(GLOB SOURCES "*.cpp")
# 静态库
add_library(lib_scheduler STATIC ${SOURCES})
# 可执行文件
add_executable(scheduler main.cpp)
# 链接
target_link_libraries(scheduler PRIVATE 
	fmt::fmt
	lib_scheduler
	gflags
	proto
	Poco::Foundation
	Poco::Util
)