#include <coroutine>
#include <string>
#include <iostream>
#include <chrono>
#include <thread>

using namespace std::literals;

template<typename T>
class CoTask {
 public:
  // --------------------------------------------------------- //
  struct promise_type {
  	T value_;
  	/// @brief 构造成功
  	auto get_return_object() { return CoTask<T>{ std::coroutine_handle<promise_type>::from_promise(*this) }; }
    /// @brief 创建后立即执行
    auto initial_suspend() noexcept { return std::suspend_never{}; }
  	/// @brief 协程结束前暂停，在析构中释放
  	auto final_suspend() noexcept { return std::suspend_always{}; }
  	/// @brief 异常
  	void unhandled_exception() { throw; }
  	/// @brief co_return
  	auto return_value(T v) { value_ = v; }
  	/// @brief co_yield 调用
  	auto yield_value(T v) { this->value_ = v; return std::suspend_always{}; }
  };

  // --------------------------------------------------------- //
  struct Awaiter {
    CoTask& task_;
    Awaiter(CoTask& task) : task_(task) {}
    /// @brief 操作是否已经完成，false表示要暂停
    bool await_ready() const { return true; }
    /// @brief 返回值就是 co_await 操作符的返回值
    auto await_resume() const { return task_.Get(); }
    /// @brief 控制协程是否暂停
    void await_suspend(std::coroutine_handle<>) const {
	  if (!task_.handle_.done())
  	    task_.handle_.resume();
    }
  };

  Awaiter operator co_await() {
    return Awaiter{*this};
  }

  CoTask(std::coroutine_handle<promise_type> h) : handle_(h) {}

  ~CoTask() {
	if (handle_) handle_.destroy();
  }

  T Get() const { return handle_.promise().value_; }

 private:
  std::coroutine_handle<promise_type> handle_;

};

CoTask<int> Hello() {
  std::cout << "Hello\n";
  std::this_thread::sleep_for(std::chrono::seconds(1));
  co_return 1;
}

CoTask<int> World() {
  std::cout << "World\n";
  std::this_thread::sleep_for(std::chrono::seconds(1));
  co_return 1;
}

CoTask<int> HelloWorld() {
  std::cout << "Before Hello\n";
  auto hello = Hello();
  std::cout << "After Hello\n";

  std::cout << "Before World\n";
  auto world = World();
  std::cout << "After World\n";

  auto rt1 = co_await hello;
  auto rt2 = co_await world;
  std::cout << "返回了" << rt1 << " " << rt2 << "\n";
  co_return 0;
}

int main() {
  auto task = HelloWorld();
  while (!task.Get()) {
    std::this_thread::sleep_for(std::chrono::milliseconds(10));
  }
  return 0;
}