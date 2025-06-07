#include "Async/async.h"
#include "Futures/Future.h"
#include <iostream>
#include <chrono>
#include <thread>

using namespace arangodb;
using namespace arangodb::futures;

auto request(Future<int>&& future) -> async<int> {
  std::cout << "request | start" << std::endl; // 2
  
  co_await std::move(future);
  
  std::cout << "request | after coawait" << std::endl; // 4 
  co_return 1;
}

int main() {
  Promise<int> promise;
  std::cout << "main | start" << std::endl; // 1
  
  std::ignore = request(promise.getFuture());
  
  std::cout << "main | do other work" << std::endl; // 3
  
  // std::this_thread::sleep_for(std::chrono::seconds(1));
  // promise.setValue(1);
  
  std::cout << "main | stop" << std::endl; // 5
}
