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

using Metadata = std::unordered_map<std::string, std::string>;

struct RootActivity {
  bool operator==(RootActivity const&) const = default;
};
template<typename Inspector>
auto inspect(Inspector& f, RootActivity& x) {
  return f.object(x).fields();
}

struct ActivityId {
  void* id{nullptr};
  bool operator==(ActivityId const&) const = default;
};
template<typename Inspector>
auto inspect(Inspector& f, ActivityId& x) {
  return f.object(x).fields(f.field("id", fmt::format("{}", x.id)));
}

enum class State { Active = 0, Deleted };
template<typename Inspector>
auto inspect(Inspector& f, State& x) {
  return f.enumeration(x).values(State::Active, "Active", State::Deleted,
                                 "Deleted");
}

/**
   A snapshot of an activity in the registry
 */
struct ActivityInRegistrySnapshot {
  std::string name;
  State state;
  ActivityId id;
  ActivityId parent;
  Metadata metadata;
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
      f.field("parent", x.parent), f.field("metadata", x.metadata));
}

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

  std::string const name;
  std::atomic<State> state;
  ActivityId parent;
  Metadata metadata;
};

/**
   This is a scope for an active activity.

   It adds an node to the activity registry on construction and marks it for
   deletion on destruction. The added node itself is deleted later via the
   registry garbage collection.
*/
struct Activity {
  Activity(Activity&& other) = delete;
  Activity& operator=(Activity&& other) = delete;
  Activity(Activity const&) = delete;
  Activity& operator=(Activity const&) = delete;

  Activity(std::string name, Metadata metadata);
  Activity(std::string name, Metadata metadata, ActivityId parent);
  ~Activity();

  auto id() const noexcept -> ActivityId;

 private:
  // no automatic deletion when unique_ptr is destroyed, deletion is done by
  // registry garbadge collection
  struct noop {
    void operator()(void*) {}
  };
  std::unique_ptr<containers::ThreadOwnedList<ActivityInRegistry>::Node, noop>
      _node_in_registry = nullptr;
};

}  // namespace arangodb::activity_registry

auto operator<<(
    std::ostream& out,
    arangodb::activity_registry::ActivityInRegistrySnapshot const& activity)
    -> std::ostream&;
