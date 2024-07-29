#include <functional>
#include "Basics/async/promise_registry.hpp"
#include "Basics/async/feature.hpp"

namespace arangodb::coroutine {

// the single owner of all promise registries
struct ThreadRegistryForPromises {
  // all threads can call this
  auto create() -> void {
    promise_registry = std::make_shared<coroutine::PromiseRegistryOnThread>();
    auto current_head = head.load(std::memory_order_relaxed);
    do {
      promise_registry->next.store(current_head, std::memory_order_relaxed);
      // (1) sets value read by (2)
    } while (not head.compare_exchange_weak(current_head, promise_registry,
                                            std::memory_order_release,
                                            std::memory_order_relaxed));
  }

  // next step to implmement this
  // with singly linked list similar how we did it before for PromiseRegistry
  // all thrads can call this
  // auto erase(PromiseInList* list) -> void;

  auto for_promise(std::function<void(PromiseInList*)> function) -> void {
    // (2) reads value set by (1)
    for (auto current = head.load(std::memory_order_acquire);
         current != nullptr;
         current = current->next.load(std::memory_order_acquire)) {
      current->for_promise(function);
    }
  }

  // is there a better way to make this not shared but unique?
  std::atomic<std::shared_ptr<PromiseRegistryOnThread>> head = nullptr;
};

}  // namespace arangodb::coroutine
