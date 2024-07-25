#include "promise_registry.hpp"

namespace arangodb::coroutine {

struct ThreadRegistryForPromises {
  // all threads can call this
  auto add(std::shared_ptr<PromiseRegistryOnThread> list) -> void {
    // add list to lists
  }

  // all thrads can call this
  // is called when a thread is destroyed. Q: does this happen?
  auto garbage_collect(PromiseRegistryOnThread* list) -> void {
    // remove list from lists
    // iterate over list and
    // - call list.remove on each item
    // - delete each promise
  }

  template<typename F>
  requires requires(F f, PromiseInList* promise) { {f(promise)}; }
  auto for_promise(F& function) -> void {
    // iterate over all lists and call list.for_promise on each list
  }
};

}  // namespace arangodb::coroutine
