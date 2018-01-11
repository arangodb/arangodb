////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2014-2017 ArangoDB GmbH, Cologne, Germany
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

#ifndef ARANGOD_CLUSTER_MAINTENANCE_FEATURE
#define ARANGOD_CLUSTER_MAINTENANCE_FEATURE 1

#include "Basics/Common.h"
#include "lib/Basics/Result.h"

#include "ApplicationFeatures/ApplicationFeature.h"
#include "ProgramOptions/ProgramOptions.h"

namespace arangodb {

class MaintenanceFeature : public application_features::ApplicationFeature {
 public:
  explicit MaintenanceFeature(application_features::ApplicationServer*);

 public:
  void collectOptions(std::shared_ptr<options::ProgramOptions>) override;

  // preparation phase for feature in the preparation phase, the features must
  // not start any threads. furthermore, they must not write any files under
  // elevated privileges if they want other features to access them, or if they
  // want to access these files with dropped privileges
  virtual void prepare() override;

  // start the feature
  virtual void start() override;

  // notify the feature about a shutdown request
  virtual void beginShutdown() override {_isShuttingDown=true;};

  // stop the feature
  virtual void stop() override {};

  // shut down the feature
  virtual void unprepare() override {};


  //
  // api features
  //

  /// @brief This is the  API for creating an Action and executing it.
  ///  Execution can be immediate by calling thread, or asynchronous via thread pool.
  ///  ActionDescription parameter will be MOVED to new object.
  Result addAction(ActionDescription_t && description, bool executeNow=false) noexcept;

  /// @brief This is the API for MaintenanceAction objects to call to create and
  ///  start a preprocess Action.  The Action executes on the caller's thread AFTER
  ///  returning to the MaintenanceWorker object.
  ///  ActionDescription parameter will be COPIED to new object.
  Result addPreprocess(ActionDescription_t & description, std::shared_ptr<class MaintenanceAction> existingAction) noexcept;

  /// @brief This API will attempt to fail an existing Action that is waiting
  ///  or executing.  Will not fail Actions that have already succeeded or failed.
  Result deleteAction(uint64_t id) noexcept;

  /// @brief Returns json array of all MaintenanceActions within the deque
  Result toJson(/* builder */) noexcept;


protected:
  /// @brief tunable option for thread pool size
  int32_t _maintenanceThreadsMax;

  /// @brief tunable option for number of seconds COMPLETE or FAILED actions block
  ///  duplicates from adding to _actionRegistry
  int32_t _secondsActionsBlock;

  /// @brief tunable option for number of seconds COMPLETE and FAILE actions remain
  ///  within _actionRegistry
  int32_t _secondsActionsLinger;

  /// @brief flag to indicate when it is time to stop thread pool
  std::atomic<bool> _isShuttingDown;

  /// @brief simple counter for creating MaintenanceAction id.  Ok for it to roll over.
  uint64_t _nextActionId;

  /// @brief all actions executing, waiting, and done
  std::deque<std::shared_ptr<MaintenanceAction>> _actionRegistry;

};

}

#endif
