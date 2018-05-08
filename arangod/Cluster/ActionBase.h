////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2014-2016 ArangoDB GmbH, Cologne, Germany
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

#ifndef ARANGODB_MAINTENANCE_ACTION_BASE_H
#define ARANGODB_MAINTENANCE_ACTION_BASE_H

#include "ActionDescription.h"

#include "Basics/Common.h"
#include "Basics/Result.h"

#include <chrono>

namespace arangodb {

class MaintenanceFeature;

namespace maintenance {

typedef std::shared_ptr<class ActionBase> ActionPtr_t;

enum ActionModel { BACKGROUND, FOREGROUND };

class ActionBase {

 public:
  ActionBase (std::shared_ptr<arangodb::MaintenanceFeature> feature,
              ActionDescription const& description);

  ActionBase() = delete;

  virtual ~ActionBase();

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
    WAITINGPRE = 4,// parent task created, about to execute on parent's thread
    WAITINGPOST = 5,// parent task created, will execute after parent's success
    PAUSED = 6,    // (not implemented) user paused task
    COMPLETE = 7,  // task completed successfully
    FAILED = 8,    // task failed, no longer executing
  };
  
  /// @brief execution finished successfully or failed ... and race timer expired
  bool done() const;

  /// @brief waiting for a worker to grab it and go!
  bool runable() const {return READY==_state;};

  /// @brief adjust state of object, assumes WRITE lock on _actionRegistryLock
  ActionState state() const {return _state;};

  virtual arangodb::Result run(
    std::chrono::duration<double> const&, bool& finished) = 0;
  
  virtual arangodb::Result kill(Signal const& signal) = 0;
  
  virtual arangodb::Result progress(double& progress) = 0;

  ActionDescription describe() const;

  std::string const& get(std::string const&) const;
  VPackSlice const& properties() const;

protected:

  std::shared_ptr<arangodb::MaintenanceFeature> _feature;

  ActionDescription _description;

  ActionModel _model;

  uint64_t _hash;

  uint64_t _id;

  std::atomic<ActionState> _state;

  // NOTE: preAction should only be set within first() or post(), not construction
  ActionPtr_t _preAction;
  ActionPtr_t _postAction;

  // times for user reporting (and _actionDone used by done() to prevent
  //  race conditions of same task executing twice
  std::chrono::system_clock::time_point _actionCreated;
  std::chrono::system_clock::time_point _actionStarted;
  std::chrono::system_clock::time_point _actionLastStat;
  std::chrono::system_clock::time_point _actionDone;

  std::atomic<uint64_t> _progress;

  Result _result;



}; // class ActionBase

} // namespace maintenance

} // namespace arangodb

#endif

