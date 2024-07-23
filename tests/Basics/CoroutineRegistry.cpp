#include "Basics/async/async.h"
#include "Assertions/Assert.h"
#include "Basics/async/registry.hpp"
#include <iostream>

using namespace arangodb;

auto foo() -> async<int> { co_return 1; }

auto print(coroutine::PromiseInList* promise) -> void {
  std::cout << *promise << std::endl;
}

int main() {
  auto f = foo();
  coroutine::promises.for_promise(print);
}
