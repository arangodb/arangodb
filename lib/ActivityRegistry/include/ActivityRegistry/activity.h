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

#include "Containers/Concurrent/ThreadOwnedList.h"
#include "Containers/Concurrent/source_location.h"
#include "Containers/Concurrent/thread.h"
#include "Inspection/Types.h"
#include "fmt/format.h"

#include <atomic>
#include <cstdint>
#include <optional>
#include <source_location>
#include <string>

namespace arangodb::activity_registry {

struct RootActivity {
  bool operator==(RootActivity const&) const = default;
};
template<typename Inspector>
auto inspect(Inspector& f, RootActivity& x) {
  return f.object(x).fields();
}

struct ActivityId {
  void* id;
  bool operator==(ActivityId const&) const = default;
};
template<typename Inspector>
auto inspect(Inspector& f, ActivityId& x) {
  return f.object(x).fields(f.field("id", fmt::format("{}", x.id)));
}

/**
   An activity can either have no parent because it is a root activity or its
   parent is another activity, defined via its id.
 */
struct ParentSnapshot : std::variant<RootActivity, ActivityId> {};
template<typename Inspector>
auto inspect(Inspector& f, ParentSnapshot& x) {
  return f.variant(x).unqualified().alternatives(
      inspection::inlineType<RootActivity>(),
      inspection::inlineType<ActivityId>());
}

enum class State { Created = 0, Running, Finished, Deleted };
template<typename Inspector>
auto inspect(Inspector& f, State& x) {
  return f.enumeration(x).values(State::Created, "Created", State::Running,
                                 "Running", State::Finished, "Finished",
                                 State::Deleted, "Deleted");
}

/**
   A snapshot of an activity in the registry
 */
struct ActivityInRegistrySnapshot {
  std::string name;
  State state;
  ActivityId id;
  ParentSnapshot parent;
  std::optional<basics::ThreadId> thread;
  basics::SourceLocationSnapshot source_location;
  bool operator==(ActivityInRegistrySnapshot const&) const = default;
  auto update_state(State new_state) -> ActivityInRegistrySnapshot& {
    state = new_state;
    return *this;
  }
};
template<typename Inspector>
auto inspect(Inspector& f, ActivityInRegistrySnapshot& x) {
  return f.object(x).fields(
      f.embedFields(x.id), f.field("name", x.name), f.field("state", x.state),
      f.field("parent", x.parent), f.field("thread", x.thread),
      f.field("source_location", x.source_location));
}

/**
   A node is the high-level entry in the activity registry.
 */
struct Node;

/**
   A shared reference to a node in the registry.
 */
using NodeReference = std::shared_ptr<Node>;

/**
   An activity can either have no parent because it is a root activity or its
   parent is another activity, defined as a shared reference to its node.

   The shared reference makes sure that a node lives as long as it is
   referenced, e.g. by a child node.
 */
struct Parent : std::variant<RootActivity, NodeReference> {};

/**
   The activity item inside the registry.

   A node in the registry owns such an ActivityInRegistry.
 */
struct ActivityInRegistry {
  using Snapshot = ActivityInRegistrySnapshot;
  auto id() -> ActivityId { return ActivityId{this}; }
  auto snapshot() -> ActivityInRegistrySnapshot;
  auto set_to_deleted() -> void {
    state.store(State::Deleted, std::memory_order_release);
  }
  static auto root(std::string name, std::source_location loc)
      -> ActivityInRegistry {
    return ActivityInRegistry{.name = std::move(name),
                              .state = State::Running,
                              .parent = Parent{RootActivity{}},
                              .running_thread = basics::ThreadId::current(),
                              .source_location = std::move(loc)};
  }
  static auto child(std::string name, NodeReference parent,
                    std::source_location loc) -> ActivityInRegistry {
    return ActivityInRegistry{.name = std::move(name),
                              .state = State::Running,
                              .parent = Parent{std::move(parent)},
                              .running_thread = basics::ThreadId::current(),
                              .source_location = std::move(loc)};
  }

  std::string const name;
  std::atomic<State> state;
  Parent parent;
  std::optional<basics::ThreadId>
      running_thread;  // probably has to also be atomic because
                       // changes for scheduled activity
  std::source_location const source_location;
  // possibly interesting other properties:
  // std::chrono::time_point<std::chrono::steady_clock> creation = std:;
};

/**
   A node in the activity registry.

   Uses inheritance to circumvent problems with non-satified constraints for
   Node when used in ParentActivity in ActivityInRegistry
 */
struct Node : public containers::ThreadOwnedList<ActivityInRegistry>::Node {};

/**
   This is a scope for an active activity.

   It adds an node to the activity registry on construction and sets its
   state to finished on destruction.

   An activity registry node is marked for deletion (and will then be garbage
   collected at some point) when all its shared references are gone. A shared
   reference to an activity registry node is owned by a activity and its
   children (via their parent member. Therefore a node in the registry lives at
   least as long as its activity scope or its longest living child.
*/
struct Activity {
  Activity(Activity&& other) = delete;
  Activity& operator=(Activity&& other) = delete;
  Activity(Activity const&) = delete;
  Activity& operator=(Activity const&) = delete;

  Activity(std::string name,
           std::source_location loc = std::source_location::current());
  ~Activity();

  auto id() -> ActivityId;

 private:
  Activity* parent;
  NodeReference _node_in_registry;
};

/**
   The currently active activity that is executed on the current thread.

   Can be used to get and set the current activity. Points to nullptr when there
   is no active activity on the current thread.
 */
auto get_current_activity() -> Activity**;

}  // namespace arangodb::activity_registry

auto operator<<(
    std::ostream& out,
    arangodb::activity_registry::ActivityInRegistrySnapshot const& activity)
    -> std::ostream&;
