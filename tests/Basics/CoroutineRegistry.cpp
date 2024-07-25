#include "Basics/async/async.h"
#include "Assertions/Assert.h"
#include "Basics/async/promise_registry.hpp"
#include "Basics/async/thread_registry.hpp"
#include <iostream>
#include <memory>
#include <thread>

using namespace std::chrono_literals;
using namespace arangodb;

coroutine::ThreadRegistryForPromises thread_registry;

auto foo() -> async<int> { co_return 1; }
auto bar() -> async<int> { co_return 4; }
auto baz() -> async<int> { co_return 2; }

auto print(coroutine::PromiseInList* promise) -> void {
  std::cout << *promise << std::endl;
}

int main() {
  thread_registry.create();

  auto f = foo();
  auto b = bar();

  auto thread = std::jthread([]() {
    thread_registry.create();
    auto z = baz();
    // we are sure that all threads still exist
    thread_registry.for_promise(print);
  });
}

// TODO implement free list
// add is called by different threads
// only need simple list
// exchange with nullptr on list head to get it
// remove can possibly look a bit similar to old remove on PromiseList

// TODO finish memory orders (ask Manuel)
