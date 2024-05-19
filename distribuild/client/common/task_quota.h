#pragma once

#include <memory>
#include <chrono>

namespace distribuild::client {

using TaskQuota = std::shared_ptr<void>;

/**
 * @brief 向服务端发送http请求尝试获得任务配额
 * 
 * @return 智能指针，如果服务端返回http:200，则在析构时，
 *         自动调用释放配额的函数
 */
TaskQuota TryAcquireTaskQuota(bool lightweight, std::chrono::nanoseconds timeout);

/**
 * @brief 获得任务配额
 * 
 * 在超时时间内调用TryAcquireTaskQuota获得配额并返回
 */
TaskQuota AcquireTaskQuota(bool lightweight);

} // namespace distribuild::client