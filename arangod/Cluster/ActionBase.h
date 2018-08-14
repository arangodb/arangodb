////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2014-2018 ArangoDB GmbH, Cologne, Germany
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

class Action;

////////////////////////////////////////////////////////////////////////////////
/// @brief Base class for db server level procedures.  These procedures were
/// previously written in javascript and migrated to C++.
///
/// The design encorporates features "desired" for future enhancement but not
/// necessarily used in the initial implementation.  Examples:  kill(), planned
/// pause(), pre and post actions.  The planned usage patterns are not tested.
///
/// The MaintenanceWorker::run() function performs the actual execution of an
/// ActionBase object.  The logical execution looks like this:
///
///     Action myAction([an array of action properties]);
///
///     if (myAction->ok() && myAction->first()) {
///       while (myAction->ok() && myAction->next()) ;
///     }
///     myAction->notifyDone();
///
/// The return boolean of first() and next() indicate whether or not another
/// iteration of the ActionBase object is needed.  ActionBase's internal Result
/// object is consulted for whether or not it is ok().
///
/// - first() is a required function for the derived classes.
/// - next() is optional, first() should return false if next() not used.
/// - notifyDone() is based upon initial javascript port, but virtualized to allow
///                future adaptation.
////////////////////////////////////////////////////////////////////////////////

class ActionBase {

 public:
  ActionBase (MaintenanceFeature&, ActionDescription const&);

  ActionBase (MaintenanceFeature&, ActionDescription&&);

  ActionBase() = delete;

  virtual ~ActionBase();

  //
  // MaintenanceWork entry points
  //

  /// @brief initial call to object to perform a unit of work.
  ///   really short tasks could do all work here and return false
  /// @return true to continue processing, false done (result() set)
  virtual bool first() = 0;

  /// @brief iterative call to perform a unit of work
  /// @return true to continue processing, false done (result() set)
  virtual bool next() { return false; }

  /// @brief routine that performs an action once state transitions to
  /// FAILED or COMPLETE.  Current performs ClusterFeature::syncDBServerStatusQuo()
  /// call on all ActionBase objects.  Slightly different from done() in that
  /// it calls immediately upon reaching target states where done() includes a
  /// a timeout against thrashing.
  virtual void notifyDone();

  //
  // common property or decription names
  //

  static const char KEY[];
  static const char FIELDS[];
  static const char TYPE[];
  static const char INDEXES[];
  static const char INDEX[];
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

  /// @brief execution finished successfully or failed ... and race timer expired
  virtual bool done() const;

  /// @brief waiting for a worker to grab it and go!
  bool runable() const {return READY==_state;}

  /// @brief did initialization have issues?
  bool ok() const {return FAILED!=_state;}

  /// @brief adjust state of object, assumes WRITE lock on _actionRegistryLock
  ActionState state() const {
    return _state;
  }

  void fail();

  virtual arangodb::Result kill(Signal const& signal);

  virtual arangodb::Result progress(double& progress);

  ActionDescription const& describe() const;

  MaintenanceFeature& feature() const;

  std::string const& get(std::string const&) const;

  VPackSlice const properties() const;

  /// @brief adjust state of object, assumes WRITE lock on _actionRegistryLock
  ActionState getState() const {
    return _state;
  }

  /// @brief adjust state of object, assumes WRITE lock on _actionRegistryLock
  void setState(ActionState state) {
    if ((FAILED == state || COMPLETE == state) && _state != state) {
      _state = state;
      endStats();
      LOG_TOPIC(DEBUG, Logger::MAINTENANCE)
        << "Action " << _description << " moved to state " << state << " after "
        << std::chrono::duration<double>(
          _actionDone.load() - _actionStarted.load()).count() << " seconds";

      notifyDone();
    } else {
      _state = state;
    }
  }

  /// @brief update incremental statistics
  void startStats();

  /// @brief update incremental statistics
  void incStats();

  /// @brief finalize statistics
  void endStats();

  /// @brief return progress statistic
  uint64_t getProgress() const {return _progress.load();}

  /// @brief Once PreAction completes, remove its pointer
  void clearPreAction() {_preAction.reset();}

  /// @brief Retrieve pointer to action that should run before this one
  std::shared_ptr<Action> getPreAction();

  /// @brief Initiate a pre action
  void createPreAction(std::shared_ptr<ActionDescription> const& description);

  /// @brief Initiate a post action
  void createPostAction(std::shared_ptr<ActionDescription> const& description);

  /// @brief Retrieve pointer to action that should run directly after this one
  std::shared_ptr<Action> getPostAction();

  /// @brief Save pointer to successor action
  void setPostAction(std::shared_ptr<ActionDescription> post) {
    _postAction=post;
  }

  /// @brief hash value of ActionDescription
  /// @return uint64_t hash
  std::string clientId() const { return _clientId; }

  /// @brief hash value of ActionDescription
  /// @return uint64_t hash
  uint64_t hash() const {return _hash;}

  /// @brief hash value of ActionDescription
  /// @return uint64_t hash
  uint64_t id() const {return _id;}

  /// @brief add VPackObject to supplied builder with info about this action
  virtual void toVelocyPack(VPackBuilder & builder) const;

  /// @brief add VPackObject to supplied builder with info about this action
  VPackBuilder toVelocyPack() const;

  /// @brief Returns json array of object contents for status reports
  ///  Thread safety of this function is questionable for some member objects
  //  virtual Result toJson(/* builder */) {return Result;}

  /// @brief Return Result object contain action specific status
  Result result() const {return _result;}

protected:

  arangodb::MaintenanceFeature& _feature;

  ActionDescription _description;

  ActionModel _model;

  uint64_t _hash;
  std::string _clientId;

  uint64_t _id;

  std::atomic<ActionState> _state;

  // NOTE: preAction should only be set within first() or post(), not construction
  std::shared_ptr<ActionDescription> _preAction;
  std::shared_ptr<ActionDescription> _postAction;

  // times for user reporting (and _actionDone used by done() to prevent
  //  race conditions of same task executing twice
  std::atomic<std::chrono::system_clock::duration> _actionCreated;
  std::atomic<std::chrono::system_clock::duration> _actionStarted;
  std::atomic<std::chrono::system_clock::duration> _actionLastStat;
  std::atomic<std::chrono::system_clock::duration> _actionDone;

  std::atomic<uint64_t> _progress;

  Result _result;



}; // class ActionBase

} // namespace maintenance

Result actionError(int errorCode, std::string const& errorMessage);
Result actionWarn(int errorCode, std::string const& errorMessage);

} // namespace arangodb


namespace std {
ostream& operator<< (
  ostream& o, arangodb::maintenance::ActionBase const& d);
}
#endif
