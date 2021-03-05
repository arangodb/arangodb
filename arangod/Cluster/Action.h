////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2014-2021 ArangoDB GmbH, Cologne, Germany
/// Copyright 2004-2014 triAGENS GmbH, Cologne, Germany
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
/// @author Kaveh Vahedipour
/// @author Matthew Von-Maszewski
////////////////////////////////////////////////////////////////////////////////

#ifndef ARANGODB_CLUSTER_MAINTENANCE_ACTION_H
#define ARANGODB_CLUSTER_MAINTENANCE_ACTION_H

#include "ActionBase.h"
#include "ActionDescription.h"
#include "Basics/Result.h"

#include <chrono>

namespace arangodb {

class MaintenanceFeature;

namespace maintenance {

class Action {
 public:
  /// @brief construct with description
  Action(MaintenanceFeature&, ActionDescription const&);

  /// @brief construct with description
  Action(MaintenanceFeature&, ActionDescription&&);

  /// @brief construct with description
  Action(MaintenanceFeature&, std::shared_ptr<ActionDescription> const&);

  Action(Action const&) = delete;
  Action(Action&&) = delete;
  Action() = delete;
  Action& operator=(Action const&) = delete;

  /// @brief for sorting in a priority queue:
  bool operator<(Action const& other) const;

  /**
   * @brief construct with concrete action base
   * @param feature  Maintenance feature
   * @param action   Concrete action
   */
  explicit Action(std::unique_ptr<ActionBase> action);

  /// @brief clean up
  virtual ~Action();

  /// @brief run for some time and tell, if need more time or done
  bool next();

  /// @brief run for some time and tell, if need more time or done
  arangodb::Result result();

  /// @brief run for some time and tell, if need more time or done
  bool first();

  /// @brief is object in a usable condition
  bool ok() const { return (nullptr != _action.get() && _action->ok()); };

  /// @brief check progress
  arangodb::Result progress(double& progress);

  /// @brief describe action
  ActionDescription const& describe() const;

  /// @brief describe action
  MaintenanceFeature& feature() const;

  // @brief get properties
  std::shared_ptr<VPackBuilder> const properties() const;

  ActionState getState() const;

  void setState(ActionState state) { _action->setState(state); }

  /// @brief update incremental statistics
  void startStats();

  /// @brief update incremental statistics
  void incStats();

  /// @brief finalize statistics
  void endStats();

  /// @brief check if action matches worker options
  bool matches(std::unordered_set<std::string> const& labels) const;

  /// @brief return progress statistic
  uint64_t getProgress() const { return _action->getProgress(); }

  /// @brief Once PreAction completes, remove its pointer
  void clearPreAction() { _action->clearPreAction(); }

  /// @brief Retrieve pointer to action that should run before this one
  std::shared_ptr<Action> getPreAction() { return _action->getPreAction(); }

  /// @brief Initiate a pre action
  void createPreAction(ActionDescription const& description);

  /// @brief Initiate a post action
  void createPostAction(ActionDescription const& description);

  /// @brief Retrieve pointer to action that should run directly after this one
  std::shared_ptr<Action> getPostAction() { return _action->getPostAction(); }

  /// @brief Save pointer to successor action
  void setPostAction(std::shared_ptr<ActionDescription> post) {
    _action->setPostAction(post);
  }

  /// @brief hash value of ActionDescription
  /// @return uint64_t hash
  uint64_t hash() const { return _action->hash(); }

  /// @brief hash value of ActionDescription
  /// @return uint64_t hash
  uint64_t id() const { return _action->id(); }

  /// @brief add VPackObject to supplied builder with info about this action
  void toVelocyPack(VPackBuilder& builder) const;

  /// @brief add VPackObject to supplied builder with info about this action
  VPackBuilder toVelocyPack() const { return _action->toVelocyPack(); }

  /// @brief Returns json array of object contents for status reports
  ///  Thread safety of this function is questionable for some member objects
  //  virtual Result toJson(/* builder */) {return Result;};

  /// @brief Return Result object contain action specific status
  Result result() const { return _action->result(); }

  /// @brief execution finished successfully or failed ... and race timer
  /// expired
  bool done() const { return _action->done(); }

  /// @brief waiting for a worker to grab it and go!
  bool runnable() const { return _action->runnable(); }

  /// @brief When object was constructed
  std::chrono::system_clock::time_point getCreateTime() const {
    return _action->getCreateTime();
  }

  /// @brief When object was first started
  std::chrono::system_clock::time_point getStartTime() const {
    return _action->getStartTime();
  }

  /// @brief When object most recently iterated
  std::chrono::system_clock::time_point getLastStatTime() const {
    return _action->getLastStatTime();
  }

  /// @brief When object finished executing
  std::chrono::system_clock::time_point getDoneTime() const {
    return _action->getDoneTime();
  }

  auto getRunDuration() const {
    return _action->getDoneTime() - _action->getStartTime();
  }

  auto getQueueDuration() const {
    return _action->getStartTime() - _action->getCreateTime();
  }

  /// @brief fastTrack
  bool fastTrack() const {
    return _action->fastTrack();
  }

  /// @brief priority
  int priority() const {
    return _action->priority();
  }

 private:
  /// @brief actually create the concrete action
  void create(MaintenanceFeature&, ActionDescription const&);

  /// @brief concrete action
  std::unique_ptr<ActionBase> _action;
};

}  // namespace maintenance
}  // namespace arangodb

namespace std {
ostream& operator<<(ostream& o, arangodb::maintenance::Action const& d);
}
#endif
