#pragma once

#include "observables.hpp"
#include <atomic>
#include <mutex>
#include <source_location>
#include <unordered_set>

namespace arangodb::coroutine {

struct PromiseInList : Observables {
  PromiseInList(std::source_location loc) : Observables(std::move(loc)) {}
  // atomic ptr to next promise in this thread
  std::atomic<PromiseInList*> next;
  // thread id
};

std::ostream& operator<<(std::ostream& out, const PromiseInList& promise) {
  return out << static_cast<Observables>(promise);
}

struct PromiseList {
  auto add(PromiseInList* promise) -> void;
  auto remove(std::unordered_set<PromiseInList*> promises_to_delete) -> void;
  template<typename F>
  auto for_promise(F& function) -> void;

  // atomic pointer to some promise that was startet by specific thread
  std::atomic<PromiseInList*> head = nullptr;
  std::mutex mutex;
};

// perhaps only need relaxed everywhere because we don't need any
// synchronization

auto PromiseList::add(PromiseInList* promise) -> void {
  auto current_head = head.load(std::memory_order_relaxed);
  do {
    // set promise -> current_head
    promise->next.store(current_head, std::memory_order_relaxed);
    // set head = promise
    // (1) sets value read by (2)
  } while (not head.compare_exchange_weak(
      current_head, promise,
      std::memory_order_release,  // makes sure that promise->next.store
                                  // happens before head is set to promise in
                                  // all other threads as well
      std::memory_order_relaxed));
}

auto PromiseList::remove(std::unordered_set<PromiseInList*> promises_to_delete)
    -> void {
  auto guard = std::lock_guard(mutex);
  auto current = head.load(std::memory_order_relaxed);
  if (promises_to_delete.contains(current)) {
    auto next_after_head = current->next.load(std::memory_order_relaxed);
    // set head = next_after_head
    if (head.compare_exchange_strong(current, next_after_head,
                                     std::memory_order_relaxed)) {
      // if a new element was added in parallel, it cannot be in the
      // promises_to_delete set
      current = next_after_head;
    }
  }
  while (current != nullptr) {
    auto next = current->next.load();
    if (promises_to_delete.contains(next)) {
      auto next_to_next = next->next.load(
          std::memory_order_acquire);  // make sure that this happens before
                                       // the store below
      current->next.store(next_to_next,
                          std::memory_order_release);  // make sure this happens
                                                       // after the load above
      current = next_to_next;
    } else {
      current = next;
    }
  }
  // TODO delete promises
}

// TODO requires
template<typename F>
auto PromiseList::for_promise(F& function) -> void {
  auto guard = std::lock_guard(mutex);
  // (2) reads value set by (1)
  for (auto current = head.load(
           std::memory_order_acquire);  // makes sure that all following loads
                                        // happen after this one
       current != nullptr;
       current = current->next.load(
           std::memory_order_acquire)) {  // makes sure that all following
                                          // loads happen after this one
    function(current);
  }
}

// TODO global variable
PromiseList promises;

}  // namespace arangodb::coroutine
