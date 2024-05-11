# 源文件
set(PROTO_FILES
  env_info.proto
  patch.proto
  scheduler.proto
)

# 静态库
add_library(proto ${PROTO_FILES})
# 链接库
target_link_libraries(proto PUBLIC protobuf::libprotobuf gRPC::grpc gRPC::grpc++)
# 将编译生成的静态库的包含目录设置为当前源代码目录和生成的二进制目录
target_include_directories(proto PUBLIC ${PROTOBUF_INCLUDE_DIRS} ${CMAKE_CURRENT_BINARY_DIR})
# grpc cpp插件
get_target_property(grpc_cpp_plugin_location gRPC::grpc_cpp_plugin LOCATION)
# proto buffer 文件
protobuf_generate(TARGET proto LANGUAGE cpp)
# grpc 文件
protobuf_generate(TARGET proto LANGUAGE grpc GENERATE_EXTENSIONS .grpc.pb.h .grpc.pb.cc PLUGIN "protoc-gen-grpc=${grpc_cpp_plugin_location}")
