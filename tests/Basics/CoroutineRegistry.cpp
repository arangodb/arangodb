#include "Basics/async/async.h"
#include "Assertions/Assert.h"
#include <iostream>

using namespace arangodb;

auto foo() -> async<int> { co_return 1; }

// auto print(PromiseInList *promise) -> void {
//   std::cout << *promise << std::endl;
// }

int main() {
  std::cout << "Hallo" << std::endl;
  auto f = foo();
  // promises.for_promise(print);
}
