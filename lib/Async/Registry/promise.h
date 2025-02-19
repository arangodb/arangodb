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
#include <optional>
#include <source_location>
#include <string>
#include <thread>
#include "Basics/threads-posix.h"
#include "Inspection/Format.h"
#include "Inspection/Types.h"
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

struct ThreadRegistry;

struct ThreadId {
  static auto current() noexcept -> ThreadId;
  auto name() -> std::string;
  TRI_tid_t posix_id;
  pid_t kernel_id;
  bool operator==(ThreadId const&) const = default;
};
template<typename Inspector>
auto inspect(Inspector& f, ThreadId& x) {
  return f.object(x).fields(f.field("LWPID", x.kernel_id),
                            f.field("name", x.name()));
}

struct SourceLocationSnapshot {
  std::string_view file_name;
  std::string_view function_name;
  std::uint_least32_t line;
  bool operator==(SourceLocationSnapshot const&) const = default;
};
template<typename Inspector>
auto inspect(Inspector& f, SourceLocationSnapshot& x) {
  return f.object(x).fields(f.field("file_name", x.file_name),
                            f.field("line", x.line),
                            f.field("function_name", x.function_name));
}
struct SourceLocation {
  auto snapshot() -> SourceLocationSnapshot {
    return SourceLocationSnapshot{.file_name = file_name,
                                  .function_name = function_name,
                                  .line = line.load()};
  }
  const std::string_view file_name;
  const std::string_view function_name;
  std::atomic<std::uint_least32_t> line;
};

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
  ThreadId item;
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
struct Requester : std::variant<ThreadId, PromiseId> {
  static auto current_thread() -> Requester;
};
template<typename Inspector>
auto inspect(Inspector& f, Requester& x) {
  if constexpr (!Inspector::isLoading) {  // only serialize
    RequesterWrapper tmp =
        std::visit(overloaded{
                       [&](PromiseId waiter) {
                         return RequesterWrapper{PromiseIdWrapper{waiter}};
                       },
                       [&](ThreadId waiter) {
                         return RequesterWrapper{ThreadIdWrapper{waiter}};
                       },
                   },
                   x);
    return f.apply(tmp);
  }
}

struct PromiseSnapshot {
  void* id;
  ThreadId thread;
  SourceLocationSnapshot source_location;
  Requester requester;
  State state;
  bool operator==(PromiseSnapshot const&) const = default;
};
template<typename Inspector>
auto inspect(Inspector& f, PromiseSnapshot& x) {
  return f.object(x).fields(f.field("owning_thread", x.thread),
                            f.field("source_location", x.source_location),
                            f.field("id", fmt::format("{}", x.id)),
                            f.field("requester", x.requester),
                            f.field("state", x.state));
}
struct Promise {
  Promise(Promise* next, std::shared_ptr<ThreadRegistry> registry,
          Requester requester, std::source_location location);
  ~Promise() = default;

  auto mark_for_deletion() noexcept -> void;
  auto id() -> void* { return this; }
  auto snapshot() -> PromiseSnapshot {
    return PromiseSnapshot{.id = id(),
                           .thread = thread,
                           .source_location = source_location.snapshot(),
                           .requester = requester.load(),
                           .state = state.load()};
  }

  ThreadId thread;

  SourceLocation source_location;
  std::atomic<Requester> requester;
  std::atomic<State> state = State::Running;
  // identifies the promise list it belongs to
  std::shared_ptr<ThreadRegistry> registry;
  Promise* next = nullptr;
  // this needs to be an atomic because it is accessed during garbage
  // collection which can happen in a different thread. This thread will
  // load the value. Since there is only one transition, i.e. from nullptr
  // to non-null ptr, any missed update will result in a pessimistic
  // execution and not an error. More precise, the item might not be
  // deleted, although it is not in head position and can be deleted. It
  // will be deleted next round.
  std::atomic<Promise*> previous = nullptr;
  // only needed to garbage collect promises
  Promise* next_to_free = nullptr;
};

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

 public:
  std::unique_ptr<Promise, noop> promise_in_registry = nullptr;
};

}  // namespace arangodb::async_registry

template<>
struct fmt::formatter<arangodb::async_registry::ThreadId>
    : arangodb::inspection::inspection_formatter {};
