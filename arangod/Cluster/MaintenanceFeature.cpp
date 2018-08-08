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

#include "MaintenanceFeature.h"

#include "Basics/ConditionLocker.h"
#include "Basics/ReadLocker.h"
#include "Basics/WriteLocker.h"
#include "Cluster/ActionDescription.h"
#include "Cluster/CreateDatabase.h"
#include "Cluster/Action.h"
#include "Cluster/MaintenanceWorker.h"

using namespace arangodb;
using namespace arangodb::application_features;
using namespace arangodb::options;
using namespace arangodb::maintenance;

MaintenanceFeature::MaintenanceFeature(ApplicationServer* server)
  : ApplicationFeature(server, "Maintenance") {

//  startsAfter("EngineSelector");    // ??? what should this be
//  startsBefore("StorageEngine");

  init();

} // MaintenanceFeature::MaintenanceFeature


MaintenanceFeature::MaintenanceFeature()
  : ApplicationFeature(nullptr, "Maintenance") {

  // must not use startsAfter/Before since nullptr given to ApplicationFeature
  init();

  return;

} // MaintenanceFeature::MaintenanceFeature


void MaintenanceFeature::init() {
  _isShuttingDown=false;
  _nextActionId=1;

  setOptional(true);
  requiresElevatedPrivileges(false); // ??? this mean admin priv?

  // these parameters might be updated by config and/or command line options
  _maintenanceThreadsMax = static_cast<int32_t>(TRI_numberProcessors() +1);
  _secondsActionsBlock = 30;
  _secondsActionsLinger = 300;

  return;

} // MaintenanceFeature::init


void MaintenanceFeature::collectOptions(std::shared_ptr<ProgramOptions> options) {
  options->addSection("server", "Server features");

  options->addHiddenOption(
    "--server.maintenance-threads",
    "maximum number of threads available for maintenance actions",
    new Int32Parameter(&_maintenanceThreadsMax));

  options->addHiddenOption(
    "--server.maintenance-actions-block",
    "minimum number of seconds finished Actions block duplicates",
    new Int32Parameter(&_secondsActionsBlock));

  options->addHiddenOption(
    "--server.maintenance-actions-linger",
    "minimum number of seconds finished Actions remain in deque",
    new Int32Parameter(&_secondsActionsLinger));

} // MaintenanceFeature::collectOptions


/// do not start threads in prepare
void MaintenanceFeature::prepare() {
} // MaintenanceFeature::prepare


void MaintenanceFeature::start() {
  int loop;
  bool flag;

  // start threads
  for (loop=0; loop<_maintenanceThreadsMax; ++loop) {
    maintenance::MaintenanceWorker * newWorker = new maintenance::MaintenanceWorker(*this);
    _activeWorkers.push_back(newWorker);
    flag = newWorker->start(&_workerCompletion);

    if (!flag) {
      LOG_TOPIC(ERR, Logger::MAINTENANCE)
        << "MaintenanceFeature::start:  newWorker start failed";
    } // if
  } // for
} // MaintenanceFeature::start


void MaintenanceFeature::beginShutdown() {

    _isShuttingDown=true;
    CONDITION_LOCKER(cLock, _actionRegistryCond);
    _actionRegistryCond.broadcast();

    return;

} // MaintenanceFeature


void MaintenanceFeature::stop() {

  for (auto itWorker : _activeWorkers ) {
    CONDITION_LOCKER(cLock, _workerCompletion);

    // loop on each worker, retesting at 10ms just in case
    if (itWorker->isRunning()) {
      _workerCompletion.wait(10000);
    } // if
  } // for

  return;

} // MaintenanceFeature::stop


/// @brief Move an incomplete action to failed state
Result MaintenanceFeature::deleteAction(uint64_t action_id) {
  Result result;

  // pointer to action, or nullptr
  auto action = findActionId(action_id);

  if (action) {
    if (maintenance::COMPLETE != action->getState()) {
      action->setState(maintenance::FAILED);
    } else {
      result.reset(TRI_ERROR_BAD_PARAMETER,"deleteAction called after action complete.");
    } // else
  } else {
    result.reset(TRI_ERROR_BAD_PARAMETER,"deleteAction could not find action to delete.");
  } // else

  return result;

} // MaintenanceFeature::deleteAction



/// @brief This is the  API for creating an Action and executing it.
///  Execution can be immediate by calling thread, or asynchronous via thread pool.
///  not yet:  ActionDescription parameter will be MOVED to new object.
Result MaintenanceFeature::addAction(
  std::shared_ptr<maintenance::Action> newAction, bool executeNow) {

  Result result;

  // the underlying routines are believed to be safe and throw free,
  //  but just in case
  try {

    size_t action_hash = newAction->hash();
    WRITE_LOCKER(wLock, _actionRegistryLock);

    std::shared_ptr<Action> curAction = findActionHashNoLock(action_hash);

    // similar action not in the queue (or at least no longer viable)
    if (curAction == nullptr || curAction->done()) {

      createAction(newAction, executeNow);

      if (!newAction || !newAction->ok()) {
        /// something failed in action creation ... go check logs
        result.reset(TRI_ERROR_BAD_PARAMETER, "createAction rejected parameters.");
      } // if
    } else {
      // action already exist, need write lock to prevent race
      result.reset(TRI_ERROR_BAD_PARAMETER, "addAction called while similar action already processing.");
    } //else

    // executeNow process on this thread, right now!
    if (result.ok() && executeNow) {
      maintenance::MaintenanceWorker worker(*this, newAction);
      worker.run();
      result = worker.result();
    } // if
  } catch (...) {
    result.reset(TRI_ERROR_INTERNAL, "addAction experience an unexpected throw.");
  } // catch

  return result;

} // MaintenanceFeature::addAction



/// @brief This is the  API for creating an Action and executing it.
///  Execution can be immediate by calling thread, or asynchronous via thread pool.
///  not yet:  ActionDescription parameter will be MOVED to new object.
Result MaintenanceFeature::addAction(
  std::shared_ptr<maintenance::ActionDescription> const & description,
  bool executeNow) {

  Result result;

  // the underlying routines are believed to be safe and throw free,
  //  but just in case
  try {
    std::shared_ptr<Action> newAction;

    // is there a known name field
    auto find_it = description->get("name");

    size_t action_hash = description->hash();
    WRITE_LOCKER(wLock, _actionRegistryLock);

    std::shared_ptr<Action> curAction = findActionHashNoLock(action_hash);

    // similar action not in the queue (or at least no longer viable)
    if (!curAction || curAction->done()) {
      newAction = createAction(description, executeNow);

      if (!newAction || !newAction->ok()) {
        /// something failed in action creation ... go check logs
        result.reset(TRI_ERROR_BAD_PARAMETER, "createAction rejected parameters.");
      } // if
    } else {
      // action already exist, need write lock to prevent race
      result.reset(TRI_ERROR_BAD_PARAMETER, "addAction called while similar action already processing.");
    } //else

    // executeNow process on this thread, right now!
    if (result.ok() && executeNow) {
      maintenance::MaintenanceWorker worker(*this, newAction);
      worker.run();
      result = worker.result();
    } // if
  } catch (...) {
    result.reset(TRI_ERROR_INTERNAL, "addAction experience an unexpected throw.");
  } // catch

  return result;

} // MaintenanceFeature::addAction


std::shared_ptr<Action> MaintenanceFeature::preAction(
  std::shared_ptr<ActionDescription> const & description) {

  return createAction(description, true);

} // MaintenanceFeature::preAction


std::shared_ptr<Action> MaintenanceFeature::postAction(
  std::shared_ptr<ActionDescription> const & description) {

  return createAction(description, true);

} // MaintenanceFeature::postAction


void MaintenanceFeature::createAction(
  std::shared_ptr<Action> action, bool executeNow) {

  // mark as executing so no other workers accidentally grab it
  if (executeNow) {
    action->setState(maintenance::EXECUTING);
  } // if

  // WARNING: holding write lock to _actionRegistry and about to
  //   lock condition variable
  {
    _actionRegistry.push_back(action);

    if (!executeNow) {
      CONDITION_LOCKER(cLock, _actionRegistryCond);
      _actionRegistryCond.signal();
    } // if
  } // lock

}


std::shared_ptr<Action> MaintenanceFeature::createAction(
  std::shared_ptr<ActionDescription> const & description,
  bool executeNow) {

  // write lock via _actionRegistryLock is assumed held
  std::shared_ptr<Action> newAction;

  // name should already be verified as existing ... but trust no one
  std::string name = description->get(NAME);

  // call factory
  newAction = std::make_shared<Action>(*this, *description);

  // if a new action constructed successfully
  if (newAction->ok()) {

    createAction(newAction, executeNow);

  } else {
    LOG_TOPIC(ERR, Logger::MAINTENANCE)
      << "createAction:  unknown action name given, \"" << name.c_str() << "\", or other construction failure.";
  } // else

  return newAction;

} // if


std::shared_ptr<Action> MaintenanceFeature::findAction(
  std::shared_ptr<ActionDescription> const description) {
  return findActionHash(description->hash());
}


std::shared_ptr<Action> MaintenanceFeature::findActionHash(size_t hash) {
  READ_LOCKER(rLock, _actionRegistryLock);

  return(findActionHashNoLock(hash));

} // MaintenanceFeature::findActionHash


std::shared_ptr<Action> MaintenanceFeature::findActionHashNoLock(size_t hash) {
  // assert to test lock held?

  std::shared_ptr<Action> ret_ptr;

  for (auto action_it=_actionRegistry.begin();
       _actionRegistry.end() != action_it && !ret_ptr; ++action_it) {
    if ((*action_it)->hash() == hash) {
      ret_ptr=*action_it;
    } // if
  } // for

  return ret_ptr;

} // MaintenanceFeature::findActionHashNoLock


std::shared_ptr<Action> MaintenanceFeature::findActionId(uint64_t id) {
  READ_LOCKER(rLock, _actionRegistryLock);

  return(findActionIdNoLock(id));

} // MaintenanceFeature::findActionId


std::shared_ptr<Action> MaintenanceFeature::findActionIdNoLock(uint64_t id) {
  // assert to test lock held?

  std::shared_ptr<Action> ret_ptr;

  for (auto action_it=_actionRegistry.begin();
       _actionRegistry.end() != action_it && !ret_ptr; ++action_it) {
    if ((*action_it)->id() == id) {
      ret_ptr=*action_it;
    } // if
  } // for

  return ret_ptr;

} // MaintenanceFeature::findActionIdNoLock


std::shared_ptr<Action> MaintenanceFeature::findReadyAction() {
  std::shared_ptr<Action> ret_ptr;

  while(!_isShuttingDown && !ret_ptr) {

    // scan for ready action (and purge any that are done waiting)
    {
      WRITE_LOCKER(wLock, _actionRegistryLock);

      for (auto loop=_actionRegistry.begin(); _actionRegistry.end()!=loop && !ret_ptr; ) {
        auto state = (*loop)->getState();
        if (state == maintenance::READY) {
          ret_ptr=*loop;
          ret_ptr->setState(maintenance::EXECUTING);
        } else if (state == maintenance::COMPLETE || state == maintenance::FAILED ) {
          loop = _actionRegistry.erase(loop);
        } else {
          ++loop;
        } // else
      } // for
    } // WRITE

    // no pointer ... wait 1 second
    if (!_isShuttingDown && !ret_ptr) {
    CONDITION_LOCKER(cLock, _actionRegistryCond);
      _actionRegistryCond.wait(100000);
    } // if

  } // while

  return ret_ptr;

} // MaintenanceFeature::findReadyAction


VPackBuilder  MaintenanceFeature::toVelocyPack() const {
  VPackBuilder vb;
  READ_LOCKER(rLock, _actionRegistryLock);

  { VPackArrayBuilder ab(&vb);
    for (auto const& action : _actionRegistry ) {
      action->toVelocyPack(vb);
    } // for

  }
  return vb;

} // MaintenanceFeature::toVelocyPack
#if 0
std::string MaintenanceFeature::toJson(VPackBuilder & builder) {
} // MaintenanceFeature::toJson
#endif
