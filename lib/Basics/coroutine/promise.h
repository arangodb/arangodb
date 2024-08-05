#pragma once

#include <iostream>
#include <source_location>
#include <string>
#include <atomic>

namespace arangodb::coroutine {

struct ThreadRegistry;

struct Observables {
  Observables(std::source_location loc) : _where(std::move(loc)) {}

  std::source_location _where;
};

struct PromiseInList : Observables {
  PromiseInList(std::source_location loc) : Observables(std::move(loc)) {}

  virtual auto destroy() noexcept -> void = 0;
  virtual ~PromiseInList() = default;

  // identifies the promise list it belongs to
  ThreadRegistry* registry;
  std::atomic<PromiseInList*> next;
  // only needed to remove an item
  std::atomic<PromiseInList*> previous;
  // only needed to garbage collect promises
  std::atomic<PromiseInList*> next_to_free;
};

}  // namespace arangodb::coroutine
