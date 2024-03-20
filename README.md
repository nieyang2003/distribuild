# 分布式编译系统
### 初衷
单机上限（典型分布式问题），提高系统资源利用率
自己在学习与实习时接触的C++项目源码数量巨大，编译时间过长
单一故障影响全部

# 设计


# 相关项目
[distcc](https://github.com/distcc/distcc)
[yadcc](https://github.com/Tencent/yadcc)

# 技术
### 语言
C++20
### 通信
grpc
sylar / Boost.Asio / 不确定中...
...
### 测试/调试
glog
gtest
benchmark
gperftools
...
### 序列化
json-cpp
yaml-cpp
rapidxml
protobuf
### 压缩加密
zstd
openssl
...
### 其它
fmt