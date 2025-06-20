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
#include "Containers/Concurrent/shared.h"
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

struct PromiseId {
  void* id;
  bool operator==(PromiseId const&) const = default;
};
template<typename Inspector>
auto inspect(Inspector& f, PromiseId& x) {
  return f.object(x).fields(f.field("id", fmt::format("{}", x.id)));
}

struct Requester : std::variant<basics::ThreadInfo, PromiseId> {
  static auto from(std::variant<basics::ThreadInfo, void*> var) -> Requester {
    return std::visit(
        overloaded{
            [](basics::ThreadInfo const& info) { return Requester{info}; },
            [](void* ptr) { return Requester{PromiseId{ptr}}; }},
        var);
  }
};
template<typename Inspector>
auto inspect(Inspector& f, Requester& x) {
  return f.variant(x).unqualified().alternatives(
      inspection::inlineType<PromiseId>(),
      inspection::inlineType<basics::ThreadInfo>());
}

struct PromiseSnapshot {
  PromiseId id;
  basics::ThreadInfo owning_thread;
  Requester requester;
  State state;
  std::optional<basics::ThreadId> thread;
  basics::SourceLocationSnapshot source_location;
  bool operator==(PromiseSnapshot const&) const = default;
};
template<typename Inspector>
auto inspect(Inspector& f, PromiseSnapshot& x) {
  return f.object(x).fields(
      f.embedFields(x.id), f.field("owning_thread", x.owning_thread),
      f.field("requester", x.requester), f.field("state", x.state),
      f.field("running_thread", x.thread),
      f.field("source_location", x.source_location));
}

using CurrentRequester =
    std::variant<containers::SharedPtr<basics::ThreadInfo>, PromiseId>;
struct AtomicRequester
    : containers::AtomicSharedOrRawPtr<basics::ThreadInfo, void> {
  static auto from(CurrentRequester req) -> AtomicRequester {
    return std::visit(
        overloaded{[](containers::SharedPtr<basics::ThreadInfo> const& ptr) {
                     return AtomicRequester{ptr};
                   },
                   [](PromiseId const& id) { return AtomicRequester{id.id}; }},
        req);
  }
};

/**
   Promise in the registry
 */
struct Promise {
  using Snapshot = PromiseSnapshot;
  Promise(CurrentRequester requester, std::source_location location);
  ~Promise() = default;

  auto id() -> PromiseId { return PromiseId{this}; }
  auto snapshot() -> Snapshot {
    return PromiseSnapshot{
        .id = id(),
        .owning_thread =
            owning_thread.get_ref().value(),  // owning_thread is never changed,
                                              // can therefore never be nullopt
        .requester = Requester::from(requester.load()),
        .state = state.load(),
        .thread = running_thread.load(std::memory_order_acquire),
        .source_location = source_location.snapshot()};
  }
  auto set_to_deleted() -> void {
    state.store(State::Deleted, std::memory_order_relaxed);
  }

  containers::SharedPtr<basics::ThreadInfo> owning_thread;
  AtomicRequester requester;  // can be used here because PromiseId includes a
                              // ptr to a Promise, which has the proper
                              // alignment to be used with AtomicSharedOrRawPtr
  std::atomic<State> state = State::Running;
  std::atomic<std::optional<basics::ThreadId>> running_thread;
  basics::VariableSourceLocation source_location;
};

/**
   Gives a ptr to the currently running coroutine on the thread or to the
   current thread if no coroutine is running at the moment.
 */
auto get_current_coroutine() noexcept -> CurrentRequester*;

/**
   Wrapper promise for easier usage in the code

   This is a wrapper around the promise: On construction, it creates a promise
   and registers it in the global register. On destruction, it marks the promise
   for deletion in the register. Therefore it has a shorter lifetime than the
   promise itself.
 */
struct AddToAsyncRegistry {
  AddToAsyncRegistry() = default;  // can be created without a node
  AddToAsyncRegistry(std::source_location loc);
  AddToAsyncRegistry(AddToAsyncRegistry const&) = delete;
  AddToAsyncRegistry& operator=(AddToAsyncRegistry const&) = delete;
  AddToAsyncRegistry(AddToAsyncRegistry&&) = delete;
  AddToAsyncRegistry& operator=(AddToAsyncRegistry&&) = delete;
  ~AddToAsyncRegistry();

  auto id() -> std::optional<PromiseId>;
  auto update_source_location(std::source_location loc) -> void;
  auto update_state(State state) -> std::optional<State>;
  auto update_requester(std::optional<PromiseId> promise) -> void;
  auto update_requester_to_current_thread() -> void;

 private:
  struct noop {
    void operator()(void*) {}
  };
  std::unique_ptr<containers::ThreadOwnedList<Promise>::Node, noop>
      node_in_registry = nullptr;
};

}  // namespace arangodb::async_registry
