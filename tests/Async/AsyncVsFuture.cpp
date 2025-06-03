#include "Async/async.h"

#include "Futures/Future.h"
#include <chrono>
#include <coroutine>
#include <iostream>
#include <thread>

using namespace arangodb;
using namespace arangodb::futures;

struct WaitSlot {
  void resume() {
    ready = true;
    if (_continuation) {
      _continuation.resume();
    }
  }

  bool await_ready() { return ready; }
  void await_resume() {}
  void await_suspend(std::coroutine_handle<> continuation) {
    _continuation = continuation;
  }

  std::coroutine_handle<> _continuation;
  bool ready = false;
};

std::jthread additional_thread;
std::jthread additional_thread_2;

auto long_running() -> async<int> {
  std::cout << "long_running | start" << std::endl;  // 1

  WaitSlot slot;
  additional_thread = std::jthread([&]() {  // after 1 and before 5
    std::cout << "long_running | new thread does some heavy work" << std::endl;
    std::cout << "thread: " << std::this_thread::get_id() << std::endl;
    std::this_thread::sleep_for(std::chrono::seconds(1));
    std::cout << "long_running | new thread finished work" << std::endl;
    slot.resume();
  });

  std::cout << "long_running | suspend" << std::endl;  // 2
  co_await slot;

  std::cout << "long_running | resumed" << std::endl;  // 5
  std::cout << "thread: " << std::this_thread::get_id() << std::endl;
  co_return 1;
}
auto long_running_future() -> Future<int> {
  std::cout << "long_running | start" << std::endl;  // 1

  Promise<int> p;
  Future<int> f = p.getFuture();
  additional_thread =
      std::jthread([promise = std::move(p)]() mutable {  // after 2 and before 6
        std::cout << "long_running | new thread does some heavy work"
                  << std::endl;
        std::cout << "thread: " << std::this_thread::get_id() << std::endl;
        std::this_thread::sleep_for(std::chrono::seconds(1));
        std::cout << "long_running | new thread finished work" << std::endl;
        promise.setValue(1);
      });

  std::cout << "long_running | suspend" << std::endl;  // 2

  return f;
}
auto long_running_future_2() -> Future<int> {
  std::cout << "long_running | start" << std::endl;  // 1

  Promise<int> p;
  Future<int> f = p.getFuture();
  additional_thread_2 =
      std::jthread([promise = std::move(p)]() mutable {  // after 2 and before 6
        std::cout << "long_running | new thread does some heavy work"
                  << std::endl;
        std::cout << "thread: " << std::this_thread::get_id() << std::endl;
        std::this_thread::sleep_for(std::chrono::seconds(1));
        std::cout << "long_running | new thread finished work" << std::endl;
        promise.setValue(1);
      });

  std::cout << "long_running | suspend" << std::endl;  // 2

  return f;
}

int main() {
  // std::cout << "thread: " << std::this_thread::get_id() << std::endl;

  // // coro
  // {
  //   std::ignore = long_running();

  //   std::cout << "main | do other work" << std::endl;  // 3
  //   // ...

  //   std::cout << "main | join thread" << std::endl;  // 4
  //   additional_thread.join();

  //   std::cout << "main | stop" << std::endl;  // 6
  // }

  // std::cout << "-------------------" << std::endl;

  std::cout << "thread: " << std::this_thread::get_id() << std::endl;
  // future
  {
    std::ignore = long_running_future().then([&](Try<int>&& x) {
      std::cout << "long_running | resumed" << std::endl;  // 5
      std::cout << "thread: " << std::this_thread::get_id() << std::endl;
      return long_running_future_2();
    });

    std::cout << "main | do other work" << std::endl;  // 3
    // ...

    std::cout << "main | join thread" << std::endl;  // 4
    additional_thread.join();
    std::cout << "main | join thread" << std::endl;  // 4
    additional_thread_2.join();

    std::cout << "main | stop" << std::endl;  // 6
  }
}
