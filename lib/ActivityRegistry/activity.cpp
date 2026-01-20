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
#include "ActivityRegistry/activity.h"

#include "Assertions/ProdAssert.h"
#include "Containers/Concurrent/source_location.h"
#include "Containers/Concurrent/thread.h"
#include "Inspection/Format.h"
#include "ActivityRegistry/activity_registry_variable.h"
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
using namespace arangodb::activity_registry;

auto operator<<(
    std::ostream& out,
    arangodb::activity_registry::ActivityInRegistrySnapshot const& activity)
    -> std::ostream& {
  return out << inspection::json(activity);
}

auto ActivityInRegistry::snapshot() -> ActivityInRegistrySnapshot {
  return ActivityInRegistrySnapshot{
      .name = name, .state = state, .id = id(), .parent = parent};
}

Activity::Activity(std::string name)
    : _node_in_registry{get_thread_registry().add([&]() {
        return ActivityInRegistry{.name = std::move(name),
                                  .state = State::Active,
                                  .parent = {RootActivity{}}};
      })} {}

Activity::Activity(std::string name, ActivityId parent)
    : _node_in_registry{get_thread_registry().add([&]() {
        return ActivityInRegistry{.name = std::move(name),
                                  .state = State::Active,
                                  .parent = {Parent{parent}}};
      })} {}

Activity::~Activity() {
  if (_node_in_registry != nullptr) {
    _node_in_registry->list->mark_for_deletion(_node_in_registry.get());
  }
}

auto Activity::id() -> ActivityId { return _node_in_registry->data.id(); }
