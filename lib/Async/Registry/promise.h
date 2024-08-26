#pragma once

#include <iostream>
#include <memory>
#include <source_location>
#include <string>
#include <thread>
#include "fmt/format.h"
#include "fmt/std.h"

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

  std::string thread_name;
  std::thread::id thread_id;

  PromiseInList* next = nullptr;
  // only needed to remove an item
  PromiseInList* previous = nullptr;
  // only needed to garbage collect promises
  PromiseInList* next_to_free = nullptr;
};

template<typename Inspector>
auto inspect(Inspector& f, PromiseInList& x) {
  // perhaps just use for saving
  return f.object(x).fields(
      f.field("source_location",
              fmt::format("{}:{} {}", x._where.file_name(), x._where.line(),
                          x._where.function_name())),
      f.field("thread_name", x.thread_name),
      f.field("thread_id", fmt::format("{}", x.thread_id)));
}

}  // namespace arangodb::async_registry
