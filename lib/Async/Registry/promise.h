#pragma once

#include <iostream>
#include <source_location>
#include <string>
#include <memory>

namespace arangodb::async_registry {

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
  std::shared_ptr<ThreadRegistry> registry = nullptr;
  PromiseInList* next = nullptr;
  // only needed to remove an item
  PromiseInList* previous = nullptr;
  // only needed to garbage collect promises
  PromiseInList* next_to_free = nullptr;
};

}  // namespace arangodb::async_registry
