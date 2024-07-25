#include "Basics/async/async.h"
#include "Assertions/Assert.h"
#include "Basics/async/promise_registry.hpp"
#include "Basics/async/thread_registry.hpp"
#include <iostream>
#include <memory>

using namespace arangodb;

coroutine::ThreadRegistryForPromises all_promises;

auto foo() -> async<int> { co_return 1; }

auto print(coroutine::PromiseInList* promise) -> void {
  std::cout << *promise << std::endl;
}

int main() {
  // for this thread
  coroutine::promise_registry =
      std::make_shared<coroutine::PromiseRegistryOnThread>();
  // all_promises.add(thread_promises); // TODO

  auto f = foo();
  coroutine::promise_registry->for_promise(print);
  // TODO instead: all_promises.for_promise(print);
}
