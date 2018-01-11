////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2017-2018 ArangoDB GmbH, Cologne, Germany
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

#include "ActionDescription.h"

#include "lib/Basics/Result.h"

#include <chrono>

namespace arangodb {
namespace maintenance {

  /**
   * Threading notes:
   * MaintenanceAction is managed by a MaintenanceWorker object.  The
   * execution could be on a worker pool thread or the Action creator's thread.
   * Rest API and supervisory code may read status information at
   * any time.  Similarly, the Rest API might change the state to PAUSED
   * or FAILED (for delete) at any time.
   */

typedef std::map<std::string, std::string> ActionDescription_t;

class MaintenanceAction {

public:

  /// @brief what is this action doing right now
  enum State {
    ANYSTATE,      // used for calls with a state comparison, NOT a valid state for object
    READY,         // Action is ready to start/resume execution
    EXECUTING,     // thread working the task
    WAITING,       // a predecessor Action must complete first
    PAUSED,        // this Action was paused (maybe by UI)
    COMPLETE,      // Action completed successfully, awaiting removal from deque
    FAILED         // Action had an error or was canceled, awaiting removal from deque
  };



  /// @brief construct with parameter description
  MaintenanceAction(ActionDescription_t && description, uint64_t id);

  /// @brief clean up
  virtual ~MaintenanceAction();

  /// @brief let external users and inherited classes adjust state
  void setState(State NewState) noexcept;

  /// @brief external routine, calls firstWrapped() virtual function.
  ///  MaintenanceWorker calls this for first piece of work for this action.
  ///  Short / atomic actions might perform all work in this call.  If so,
  ///  set state to COMPLETE  or FAILED during such atomic calls.
  ///  Wrapper will set FAILED state if result is not ok().
  arangodb::Result first() noexcept;

  /// @brief external routine, calls nextWrapped() virtual function.
  ///  For continued iteration work.  Set state to COMPLETE or FAILED
  ///  once last iteration completed.  Wrapper will set FAILED state if
  ///  result is not ok().
  arangodb::Result next() noexcept;

  /// @brief thread safe mechanism for changing state.  Optional precondition
  ///  is to prevent a Rest API from changing a COMPLETE to FAILED (or similar)
  ///  in a race condition between external thread and internal thread making changes
  arangodb::Result setState(State NewState, State PreconditionState=ANYSTATE) noexcept;


  /// @brief external routine, puts state information into builder then calls
  ///  toJsonWrapped for inherited classes to added whatever.  No exceptions,
  ///  set a bad Result code
  arangodb::Result toJson(/* vpackbuilder & ToDo */) noexcept;

protected:
  /// @brief map of options needed to execute this action
  ActionDescription_t _description;

  /// @brief unique, process specific identifier set at construction
  uint64_t _id;

  //
  // state variables
  //

  /// @brief protection of all state variables
  basics::ReadWriteLock _stateLock;

  /// @brief what is this action doing right now
  State _state;

  /// @brief when object was created
  std::chrono::steady_clock::time_point _timeCreated;

  /// @brief when object was completed or failed
  std::chrono::steady_clock::time_point _timeStopped;

  /// @brief when last activity finished: first(), next(), or paused
  std::chrono::steady_clock::time_point _timeLastActive;

  /// @brief activity counter incremented on first() and next()
  uint64_t _activityCount;

  //
  // methods used by inheriting classes
  //

  /// @brief code to set up iteration and/or perform initial
  ///  assessment of server state, might push a predecessor task
  ///  Note: only called when _activityCount is zero (non-zero
  ///  occurs on restart of PAUSED or WAITING tasks)
  ///  Default implementation returns an error Result.
  virtual Result firstWrapped() noexcept;

  /// @brief actual iteration for long running tasks.  This can
  ///  call could push a predecessor task, but why?
  ///  Default implementation returns an error Result.
  virtual Result nextWrapped() noexcept;

  /// @brief Optional.  Allows inheriting classes to add information
  ///  to status json.  No throws.  Send back errors via Result.
  ///  Default implementation is a no-op returning ok() Result.
  virtual Result toJsonWrapped() noexcept;

};

}}
#endif
