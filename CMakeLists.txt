# 编译器所需版本
cmake_minimum_required(VERSION 3.20)

# 名称
project(distribuild C CXX)

# 设置构建类型为调试模式
set(CMAKE_BUILD_TYPE "Debug")

# 输出详细构建信息
set(CMAKE_VERBOSE_MAKEFILE ON)

# 设置C++编译器编译选项
set(CMAKE_CXX_FLAGS "$ENV{CXXFLAGS} -rdynamic -O3 -fPIC -ggdb -std=c++2a -Wall -Wno-deprecated -Werror -Wno-unused-function -Wno-builtin-macro-redefined -Wno-deprecated-declarations")
# 设置C编译编译选项
set(CMAKE_C_FLAGS "$ENV{CXXFLAGS} -rdynamic -O3 -fPIC -ggdb -std=c11 -Wall -Wno-deprecated -Werror -Wno-unused-function -Wno-builtin-macro-redefined -Wno-deprecated-declarations")

# 设置搜索路径
# include_directories(.)

# 创建源码变量
set(SRC_LIST)

# # 搜索源文件
# file(GLOB SRC_LIST distribuild/*.cc distribuild/*.h distribuild/*/*.cc distribuild/*/*.h)

# 设置动态库生成路径
set(EXECUTABLE_OUTPUT_PATH ${PROJECT_SOURCE_DIR}/lib)

# 设置可执行文件生成路径
SET(EXECUTABLE_OUTPUT_PATH ${PROJECT_SOURCE_DIR}/bin)

# # 制作动态库
# add_library(distribuild SHARED ${SRC_LIST})

# 添加第三方库
find_package(fmt REQUIRED)
find_package(GTest REQUIRED)
find_package(jsoncpp REQUIRED)
find_package(OpenSSL REQUIRED)
find_package(protobuf REQUIRED)
find_package(gRPC REQUIRED)
find_package(Zstd REQUIRED)
find_package(Blake3 REQUIRED)


set(LIBS
    distribuild
    fmt::fmt
    gtest_main
    GTest::gtest
)

# 构建测试文件
# option(BUILD_TEST "ON for complile test" ON)
# if(BUILD_TEST)

# add_executable(test_common "tests/common_test.cc")
# target_link_libraries(test_common ${LIBS})

# endif()

add_subdirectory(distribuild)
add_subdirectory(tests)