////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2014-2018 ArangoDB GmbH, Cologne, Germany
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

#ifndef ARANGOD_CLUSTER_MAINTENANCE_ACTION
#define ARANGOD_CLUSTER_MAINTENANCE_ACTION 1

#include <chrono>
#include <map>
#include <memory>

#include <velocypack/vpack.h>
#include <velocypack/velocypack-aliases.h>

#include "Basics/Common.h"
#include "Basics/ReadWriteLock.h"
#include "Basics/Result.h"

namespace arangodb {

class MaintenanceFeature;

namespace maintenance {


typedef std::map<std::string, std::string> ActionDescription_t;
typedef std::shared_ptr<class MaintenanceAction> MaintenanceActionPtr_t;

class MaintenanceAction {
 public:
  MaintenanceAction(arangodb::MaintenanceFeature & feature,
                    std::shared_ptr<ActionDescription_t> const & description,
                    std::shared_ptr<VPackBuilder> const & properties);

  MaintenanceAction() = delete;

  virtual ~MaintenanceAction() {};

 public:

  //
  // MaintenanceWork entry points
  //

  /// @brief initial call to object to perform a unit of work.
  ///   really short tasks could do all work here and return false
  /// @return true to continue processing, false done (result() set)
  virtual bool first() = 0 ;

  /// @brief iterative call to perform a unit of work
  /// @return true to continue processing, false done (result() set)
  virtual bool next() {return false;};

  //
  // common property or decription names
  //

  static const char KEY[];
  static const char FIELDS[];
  static const char TYPE[];
  static const char INDEXES[];
  static const char SHARDS[];
  static const char DATABASE[];
  static const char COLLECTION[];
  static const char EDGE[];
  static const char NAME[];
  static const char ID[];
  static const char LEADER[];
  static const char LOCAL_LEADER[];
  static const char GLOB_UID[];
  static const char OBJECT_ID[];


  //
  // state accessor and set functions
  //  (some require time checks and/or combination tests)
  //
  enum ActionState {
    READY = 1,     // waiting for a worker on the deque
    EXECUTING = 2, // user or worker thread currently executing
    WAITING = 3,   // initiated a pre-task, waiting for its completion
    PAUSED = 4,    // (not implemented) user paused task
    COMPLETE = 5,  // task completed successfully
    FAILED = 6,    // task failed, no longer executing
  };

  /// @brief execution finished successfully or failed ... and race timer expired
  bool done() const;

  /// @brief waiting for a worker to grab it and go!
  bool runable() const {return READY==_state;};

  /// @brief adjust state of object, assumes WRITE lock on _actionRegistryLock
  ActionState getState() const {return _state;};

  /// @brief adjust state of object, assumes WRITE lock on _actionRegistryLock
  void setState(ActionState state) {_state = state;};

  /// @brief update incremental statistics
  void startStats();

  /// @brief update incremental statistics
  void incStats();

  /// @brief finalize statistics
  void endStats();

  /// @brief return progress statistic
  uint64_t getProgress() const {return _progress.load();};

  /// @brief Once PreAction completes, remove its pointer
  void clearPreAction() {_preAction.reset();};

  /// @brief Retrieve pointer to action that should run before this one
  MaintenanceActionPtr_t getPreAction() {return _preAction;};

  /// @brief Retrieve pointer to action that should run directly after this one
  MaintenanceActionPtr_t getNextAction() {return _nextAction;};

  /// @brief Save pointer to successor action
  void setNextAction(MaintenanceActionPtr_t next) {_nextAction=next;}

  /// @brief hash value of ActionDescription_t
  /// @return uint64_t hash
  uint64_t hash() const {return _hash;};

  /// @brief hash value of ActionDescription_t
  /// @return uint64_t hash
  uint64_t id() const {return _id;};

  /// @brief Returns json array of object contents for status reports
  ///  Thread safety of this function is questionable for some member objects
//  virtual Result toJson(/* builder */) {return Result;};

  /// @brief Return Result object contain action specific status
  Result result() const {return _result;}

  /// @brief access action description object
  ActionDescription_t const * getDescription() const {return _description.get();};

  /// @brief access properties builder / slice
  VPackBuilder const * getProperties() const {return _properties.get();};

protected:
  arangodb::MaintenanceFeature & _feature;

  std::shared_ptr<ActionDescription_t> const _description;
  std::shared_ptr<VPackBuilder> const _properties;

  uint64_t _hash;

  uint64_t _id;

  std::atomic<ActionState> _state;

  // NOTE: preAction should only be set within first() or next(), not construction
  MaintenanceActionPtr_t _preAction;
  MaintenanceActionPtr_t _nextAction;

  // times for user reporting (and _actionDone used by done() to prevent
  //  race conditions of same task executing twice
  std::chrono::system_clock::time_point _actionCreated;
  std::chrono::system_clock::time_point _actionStarted;
  std::chrono::system_clock::time_point _actionLastStat;
  std::chrono::system_clock::time_point _actionDone;

  std::atomic<uint64_t> _progress;

  Result _result;

};// class MaintenanceAction

} // namespace maintenance
} // namespace arangodb

#endif
