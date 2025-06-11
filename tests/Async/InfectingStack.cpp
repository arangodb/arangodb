#include "Async/async.h"
#include "Futures/Future.h"
#include <iostream>
#include <chrono>
#include <thread>

using namespace arangodb;
using namespace arangodb::futures;

std::jthread additional_thread;

auto do_request() -> Future<int> {
  Promise<int> p;
  Future<int> f = p.getFuture();
  additional_thread = std::jthread([promise = std::move(p)]() mutable {
    std::this_thread::sleep_for(std::chrono::seconds(1));
    promise.setValue(1);
  });
  return f;
}

auto request_coro() -> Future<Unit> {
  return do_request().thenValue([](auto item) {
    std::cout << "main | we got the result: " << item << std::endl;
    return;
  });
}

int main() {
  Promise<int> promise;
  std::cout << "main | start" << std::endl;

  std::ignore = request_coro();

  std::cout << "main | do other stuff" << std::endl;

  additional_thread.join();
  std::cout << "main | stop" << std::endl;
}

// TODO: coawait do_request instead of waiting

// infecting stack:
// have a waiting future stacked inside 2 calls
// but we want to continue at top level
// e.g. in making RestHandlers async (Johann)
