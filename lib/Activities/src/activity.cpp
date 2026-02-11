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
#include "Activities/activity.h"

#include "Assertions/ProdAssert.h"
#include "Containers/Concurrent/source_location.h"
#include "Containers/Concurrent/thread.h"
#include "Inspection/Format.h"
#include "Activities/activity_registry_variable.h"
#include <atomic>
#include <optional>
#include <source_location>
#include <variant>
#include <optional>

// helper type for the visitor
namespace {
template<class... Ts>
struct overloaded : Ts... {
  using Ts::operator()...;
};
template<class... Ts>
overloaded(Ts...) -> overloaded<Ts...>;
}  // namespace

using namespace arangodb;
using namespace arangodb::activities;

auto operator<<(
    std::ostream& out,
    arangodb::activities::ActivityInRegistrySnapshot const& activity)
    -> std::ostream& {
  return out << inspection::json(activity);
}

auto ActivityInRegistry::snapshot() -> ActivityInRegistrySnapshot {
  return ActivityInRegistrySnapshot{.type = type,
                                    .state = state,
                                    .id = id(),
                                    .parent = parent,
                                    .metadata = metadata.copy()};
}

Activity::Activity(std::string type, Metadata metadata)
    : _node_in_registry{get_thread_registry().add([&]() {
        return ActivityInRegistry{
            .type = std::move(type),
            .state = State::Active,
            .parent = {Registry::currentlyExecutingActivity()},
            .metadata = Guarded<Metadata>(metadata)};
      })} {}

Activity::Activity(std::string type, Metadata metadata, ActivityId parent)
    : _node_in_registry{get_thread_registry().add([&]() {
        return ActivityInRegistry{.type = std::move(type),
                                  .state = State::Active,
                                  .parent = parent,
                                  .metadata = Guarded<Metadata>(metadata)};
      })} {}

Activity::~Activity() {
  if (_node_in_registry != nullptr) {
    _node_in_registry->list->mark_for_deletion(_node_in_registry.get());
  }
}

auto Activity::id() const noexcept -> ActivityId {
  return _node_in_registry->data.id();
}
