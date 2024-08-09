# 分布式编译系统
## 编译
前提：
C++20、fmt、JsonCpp、zstd、blake3、Poco、openssl、gflags、protobuf、grpc、gtest
...()
```
mkdir build && cd build
cmake ..
make
```
## 使用
```
# shell-1
./bin/scheduler
```
```
# shell-2
./bin/daemon
```
```
# shell-3
./bin/distribuild g++ -c ./bin/example.cpp -o test.o
# 生成了test.o，可以`g++ test.o`验证
```
## 目的
单机上限（典型分布式问题），提高系统资源利用率
自己在学习与实习时接触的C++项目源码数量巨大，编译时间过长
单一故障影响全部
## 设计图

TODO

## 相关项目
[distcc](https://github.com/distcc/distcc)
[yadcc](https://github.com/Tencent/yadcc)