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
TaskQuota TryAcquireTaskQuota(bool lightweight, std::chrono::seconds timeout);

/**
 * @brief 获得任务配额
 * 
 * 在超时时间内调用TryAcquireTaskQuota使服务分配配额，
 * 成功则返回一个释放配额的函数
 */
TaskQuota AcquireTaskQuota(bool lightweight);

} // namespace distribuild::client