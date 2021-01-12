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

#ifndef ARANGOD_CLUSTER_MAINTENANCE_WORKER
#define ARANGOD_CLUSTER_MAINTENANCE_WORKER 1

#include "Basics/Thread.h"
#include "Cluster/Action.h"
#include "RestServer/Metrics.h"

namespace arangodb {

class MaintenanceFeature;

namespace maintenance {

class MaintenanceWorker : public Thread {
 public:
  explicit MaintenanceWorker(MaintenanceFeature& feature,
                    std::unordered_set<std::string> const& labels = std::unordered_set<std::string>());

  MaintenanceWorker(MaintenanceFeature& feature, std::shared_ptr<Action>& directAction);

  virtual ~MaintenanceWorker() { shutdown(); }

  //
  // MaintenanceWorker entry points
  //

  /// @brief Thread object entry point
  virtual void run() override;

  /// @brief Share internal result state, likely derived from most recent action
  /// @returns Result object
  Result result() const { return _lastResult; }

 protected:
  enum WorkerState {
    eSTOP = 1,
    eFIND_ACTION = 2,
    eRUN_FIRST = 3,
    eRUN_NEXT = 4,

  };

  /// @brief Determine next loop state
  /// @param true if action wants to continue, false if action done
  void nextState(bool);

  /// @brief Find an action that is ready to process within feature's deque
  /// @return action to process, or nullptr

  arangodb::MaintenanceFeature& _feature;

  std::shared_ptr<Action> _curAction;

  WorkerState _loopState;

  bool _directAction;

  Result _lastResult;

  const std::unordered_set<std::string> _labels;

 private:
  MaintenanceWorker(MaintenanceWorker const&) = delete;

  void recordJobStats(bool failed);

};  // class MaintenanceWorker

}  // namespace maintenance
}  // namespace arangodb

#endif
