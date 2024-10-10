////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2014-2024 ArangoDB GmbH, Cologne, Germany
/// Copyright 2004-2014 triAGENS GmbH, Cologne, Germany
///
/// Licensed under the Business Source License 1.1 (the "License");
/// you may not use this file except in compliance with the License.
/// You may obtain a copy of the License at
///
///     https://github.com/arangodb/arangodb/blob/devel/LICENSE
///
/// Unless required by applicable law or agreed to in writing, software
/// distributed under the License is distributed on an "AS IS" BASIS,
/// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
/// See the License for the specific language governing permissions and
/// limitations under the License.
///
/// Copyright holder is ArangoDB GmbH, Cologne, Germany
///
/// @author Julia Volmer
////////////////////////////////////////////////////////////////////////////////
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

struct Thread {
  std::string name;
  std::thread::id id;
};
template<typename Inspector>
auto inspect(Inspector& f, Thread& x) {
  return f.object(x).fields(f.field("name", x.name),
                            f.field("id", fmt::format("{}", x.id)));
}

struct Promise {
  Promise(Promise* next, std::shared_ptr<ThreadRegistry> registry,
          std::source_location location);
  ~Promise() = default;

  auto mark_for_deletion() noexcept -> void;

  Thread thread;
  std::source_location entry_point;
  // identifies the promise list it belongs to
  std::shared_ptr<ThreadRegistry> registry;
  Promise* next = nullptr;
  // this needs to be an atomic because it is accessed during garbage collection
  // which can happen in a different thread. This thread will load the value.
  // Since there is only one transition, i.e. from nullptr to non-null ptr,
  // any missed update will result in a pessimistic execution and not an error.
  // More precise, the item might not be deleted, although it is not in head
  // position and can be deleted. It will be deleted next round.
  std::atomic<Promise*> previous = nullptr;
  // only needed to garbage collect promises
  Promise* next_to_free = nullptr;
};
template<typename Inspector>
auto inspect(Inspector& f, Promise& x) {
  // perhaps just use for saving
  return f.object(x).fields(
      f.field("thread", x.thread),
      f.field(
          "source_location",
          fmt::format("{}:{} {}", x.entry_point.file_name(),
                      x.entry_point.line(), x.entry_point.function_name())));
}

struct AddToAsyncRegistry {
  AddToAsyncRegistry() = default;
  AddToAsyncRegistry(std::source_location loc);
  AddToAsyncRegistry(AddToAsyncRegistry const&) = delete;
  AddToAsyncRegistry& operator=(AddToAsyncRegistry const&) = delete;
  AddToAsyncRegistry(AddToAsyncRegistry&&) = delete;
  AddToAsyncRegistry& operator=(AddToAsyncRegistry&&) = delete;
  ~AddToAsyncRegistry();

 private:
  struct noop {
    void operator()(void*) {}
  };
  std::unique_ptr<Promise, noop> promise = nullptr;
};

}  // namespace arangodb::async_registry
