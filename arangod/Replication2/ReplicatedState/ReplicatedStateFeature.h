////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2021-2021 ArangoDB GmbH, Cologne, Germany
///
/// Licensed under the Apache License, Version 2.0 (the "License");
/// you may not use this file except in compliance with the License.
/// You may obtain a copy of the License at
///
///     http://www.apache.org/licenses/LICENSE-2.0
///
/// Unless required by applicable law or agreed to in writing, software
/// distributed under the License is distributed on an "AS IS" BASIS,
/// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
/// See the License for the specific language governing permissions and
/// limitations under the License.
///
/// Copyright holder is ArangoDB GmbH, Cologne, Germany
///
/// @author Lars Maier
////////////////////////////////////////////////////////////////////////////////

#pragma once

#include <memory>
#include <unordered_map>

#include "Replication2/ReplicatedState/ReplicatedState.h"
#include "Replication2/ReplicatedState/ReplicatedStateTraits.h"

namespace arangodb::replication2::replicated_log {
struct ReplicatedLog;
class LogFollower;
class LogLeader;
}  // namespace arangodb::replication2::replicated_log

namespace arangodb::replication2::replicated_state {

struct ReplicatedStateFeature {
  /**
   * Registers a new State implementation with the given name.
   * @tparam S State Machine
   * @param name Name of the implementation
   * @param args Arguments forwarded to the constructor of the factory type,
   * i.e. ReplicatedStateTraits<S>::FactoryType.
   */
  template<typename S, typename... Args>
  void registerStateType(std::string name, Args&&... args) {
    using Factory = typename ReplicatedStateTraits<S>::FactoryType;
    static_assert(std::is_constructible_v<Factory, Args...>);
    auto factory = std::make_shared<InternalFactory<S, Factory>>(
        std::in_place, std::forward<Args>(args)...);
    auto [iter, wasInserted] =
        factories.try_emplace(std::move(name), std::move(factory));
    assertWasInserted(name, wasInserted);
  }

  /**
   * Create a new replicated state using the implementation specified by the
   * name.
   * @param name Which implementation to use.
   * @param log ReplicatedLog to use.
   * @return
   */
  auto createReplicatedState(std::string_view name,
                             std::shared_ptr<replicated_log::ReplicatedLog> log)
      -> std::shared_ptr<ReplicatedStateBase>;

 private:
  static void assertWasInserted(std::string_view name, bool wasInserted);
  struct InternalFactoryBase
      : std::enable_shared_from_this<InternalFactoryBase> {
    virtual ~InternalFactoryBase() = default;
    virtual auto createReplicatedState(
        std::shared_ptr<replicated_log::ReplicatedLog>)
        -> std::shared_ptr<ReplicatedStateBase> = 0;
  };

  template<typename S, typename Factory>
  struct InternalFactory;

  std::unordered_map<std::string, std::shared_ptr<InternalFactoryBase>>
      factories;
};

template<typename S,
         typename Factory = typename ReplicatedStateTraits<S>::FactoryType>
struct ReplicatedStateFeature::InternalFactory : InternalFactoryBase,
                                                 private Factory {
  template<typename... Args>
  explicit InternalFactory(std::in_place_t, Args&&... args)
      : Factory(std::forward<Args>(args)...) {}

  auto createReplicatedState(std::shared_ptr<replicated_log::ReplicatedLog> log)
      -> std::shared_ptr<ReplicatedStateBase> override {
    return std::make_shared<ReplicatedState<S>>(std::move(log),
                                                getStateFactory());
  }

  auto getStateFactory() -> std::shared_ptr<Factory> {
    return {shared_from_this(), static_cast<Factory*>(this)};
  }
};

}  // namespace arangodb::replication2::replicated_state
