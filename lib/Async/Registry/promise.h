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

#include <atomic>
#include <iostream>
#include <memory>
#include <optional>
#include <source_location>
#include <string>
#include <thread>
#include "Containers/Concurrent/ThreadOwnedList.h"
#include "Containers/Concurrent/thread.h"
#include "Containers/Concurrent/source_location.h"
#include "fmt/format.h"
#include "fmt/std.h"

namespace {
// helper type for the visitor
template<class... Ts>
struct overloaded : Ts... {
  using Ts::operator()...;
};
template<class... Ts>
overloaded(Ts...) -> overloaded<Ts...>;
}  // namespace

namespace arangodb::async_registry {

enum class State { Running = 0, Suspended, Resolved, Deleted };
template<typename Inspector>
auto inspect(Inspector& f, State& x) {
  return f.enumeration(x).values(State::Running, "Running", State::Suspended,
                                 "Suspended", State::Resolved, "Resolved",
                                 State::Deleted, "Deleted");
}

using PromiseId = void*;

struct PromiseIdWrapper {
  PromiseId item;
};
template<typename Inspector>
auto inspect(Inspector& f, PromiseIdWrapper& x) {
  return f.object(x).fields(f.field("promise", fmt::format("{}", x.item)));
}
struct ThreadIdWrapper {
  basics::ThreadId item;
};
template<typename Inspector>
auto inspect(Inspector& f, ThreadIdWrapper& x) {
  return f.object(x).fields(f.field("thread", x.item));
}
struct RequesterWrapper : std::variant<ThreadIdWrapper, PromiseIdWrapper> {};
template<typename Inspector>
auto inspect(Inspector& f, RequesterWrapper& x) {
  return f.variant(x).unqualified().alternatives(
      inspection::inlineType<PromiseIdWrapper>(),
      inspection::inlineType<ThreadIdWrapper>());
}
struct Requester : std::variant<basics::ThreadId, PromiseId> {
  static auto current_thread() -> Requester {
    return {basics::ThreadId::current()};
  }
};
template<typename Inspector>
auto inspect(Inspector& f, Requester& x) {
  if constexpr (!Inspector::isLoading) {  // only serialize
    RequesterWrapper tmp =
        std::visit(overloaded{
                       [&](PromiseId waiter) {
                         return RequesterWrapper{PromiseIdWrapper{waiter}};
                       },
                       [&](basics::ThreadId waiter) {
                         return RequesterWrapper{ThreadIdWrapper{waiter}};
                       },
                   },
                   x);
    return f.apply(tmp);
  }
}

struct PromiseSnapshot {
  void* id;
  Requester requester;
  State state;
  std::optional<basics::ThreadId> thread;
  basics::SourceLocationSnapshot source_location;
  bool operator==(PromiseSnapshot const&) const = default;
};
template<typename Inspector>
auto inspect(Inspector& f, PromiseSnapshot& x) {
  return f.object(x).fields(
      f.field("id", fmt::format("{}", x.id)), f.field("requester", x.requester),
      f.field("state", x.state), f.field("running_thread", x.thread),
      f.field("source_location", x.source_location));
}

/**
   Promise in the registry
 */
struct Promise {
  using Snapshot = PromiseSnapshot;
  Promise(Requester requester, std::source_location location);
  ~Promise() = default;

  auto id() -> void* { return this; }
  auto snapshot() -> Snapshot {
    return PromiseSnapshot{
        .id = id(),
        .requester = requester.load(),
        .state = state.load(),
        .thread = running_thread.load(std::memory_order_acquire),
        .source_location = source_location.snapshot()};
  }
  auto set_to_deleted() -> void {
    state.store(State::Deleted, std::memory_order_relaxed);
  }

  basics::ThreadId owning_thread;
  std::atomic<Requester> requester;
  std::atomic<State> state = State::Running;
  std::atomic<std::optional<basics::ThreadId>> running_thread;
  basics::VariableSourceLocation source_location;
};

/**
   Gives a ptr to the currently running coroutine on the thread or to the
   current thread if no coroutine is running at the moment.
 */
auto get_current_coroutine() noexcept -> Requester*;

/**
   Wrapper promise for easier usage in the code

   This is a wrapper around the promise: On construction, it creates a promise
   and registers it in the global register. On destruction, it marks the promise
   for deletion in the register. Therefore it has a shorter lifetime than the
   promise itself.
 */
struct AddToAsyncRegistry {
  AddToAsyncRegistry() = default;
  AddToAsyncRegistry(std::source_location loc);
  AddToAsyncRegistry(AddToAsyncRegistry const&) = delete;
  AddToAsyncRegistry& operator=(AddToAsyncRegistry const&) = delete;
  AddToAsyncRegistry(AddToAsyncRegistry&&) = delete;
  AddToAsyncRegistry& operator=(AddToAsyncRegistry&&) = delete;
  ~AddToAsyncRegistry();

  auto id() -> void*;
  auto update_source_location(std::source_location loc) -> void;
  auto update_state(State state) -> std::optional<State>;
  auto update_requester(Requester requester) -> void;

 private:
  struct noop {
    void operator()(void*) {}
  };
  std::unique_ptr<containers::ThreadOwnedList<Promise>::Node, noop>
      node_in_registry = nullptr;
};

}  // namespace arangodb::async_registry
