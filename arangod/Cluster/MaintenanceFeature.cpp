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
#include "Cluster/MaintenanceAction.h"
#include "Cluster/MaintenanceWorker.h"

using namespace arangodb::options;

namespace arangodb {

MaintenanceFeature::MaintenanceFeature(application_features::ApplicationServer* server)
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
  _maintenanceThreadsMax = static_cast<int32_t>(TRI_numberProcessors()/4 +1);
  _secondsActionsBlock = 30;
  _secondsActionsLinger = 300;

  return;

} // MaintenanceFeature::init


void MaintenanceFeature::collectOptions(std::shared_ptr<ProgramOptions> options) {
  options->addSection("server", "Server features");

  options->addHiddenOption("--server.maintenance-threads",
                           "maximum number of threads available for maintenance actions",
                     new Int32Parameter(&_maintenanceThreadsMax));

  options->addHiddenOption("--server.maintenance-actions-block",
                           "minimum number of seconds finished Actions block duplicates",
                     new Int32Parameter(&_secondsActionsBlock));

  options->addHiddenOption("--server.maintenance-actions-linger",
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
      LOG_TOPIC(ERR, Logger::CLUSTER)
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
    if (maintenance::MaintenanceAction::COMPLETE != action->getState()) {
      action->setState(maintenance::MaintenanceAction::FAILED);
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
Result MaintenanceFeature::addAction(std::shared_ptr<maintenance::ActionDescription_t> const & description,
                                     std::shared_ptr<VPackBuilder> const & properties,
                                     bool executeNow) {
  Result result;

  // the underlying routines are believed to be safe and throw free,
  //  but just in case
  try {
    maintenance::MaintenanceActionPtr_t newAction;

    // is there a known name field
    auto find_it = description->find("name");
    if (description->end()!=find_it) {
      size_t action_hash = maintenance::ActionDescription::hash(*description);
      WRITE_LOCKER(wLock, _actionRegistryLock);

      maintenance::MaintenanceActionPtr_t curAction = findActionHashNoLock(action_hash);

      // similar action not in the queue (or at least no longer viable)
      if (!curAction || curAction->done()) {
        newAction = createAction(description, properties, executeNow);

        if (!newAction) {
          /// something failed in action creation ... go check logs
          result.reset(TRI_ERROR_BAD_PARAMETER, "createAction rejected parameters.");
        } // if
      } else {
        // action already exist, need write lock to prevent race
        result.reset(TRI_ERROR_BAD_PARAMETER, "addAction called while similar action already processing.");
      } //else
    } else {
      // description lacks mandatory "name" field
      result.reset(TRI_ERROR_BAD_PARAMETER, "addAction called without required \"name\" field.");
    } // else

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


maintenance::MaintenanceActionPtr_t MaintenanceFeature::preAction(std::shared_ptr<maintenance::ActionDescription_t> const & description,
                                                                  std::shared_ptr<VPackBuilder> const & properties) {

  return createAction(description, properties, true);

} // MaintenanceFeature::preAction


maintenance::MaintenanceActionPtr_t MaintenanceFeature::createAction(std::shared_ptr<maintenance::ActionDescription_t> const & description,
                                                                     std::shared_ptr<VPackBuilder> const & properties,
                                                                     bool executeNow) {
  // write lock via _actionRegistryLock is assumed held
  maintenance::MaintenanceActionPtr_t newAction;

  // name should already be verified as existing ... but trust no one
  auto find_it = description->find("name");
  if (description->end()!=find_it) {
    std::string name = find_it->second;

    // call factory
    newAction = actionFactory(name, description, properties);

    // if a new action created
    if (newAction) {

      // mark as executing so no other workers accidentally grab it
      if (executeNow) {
        newAction->setState(maintenance::MaintenanceAction::EXECUTING);
      } // if

      // WARNING: holding write lock to _actionRegistry and about to
      //   lock condition variable
      {
        CONDITION_LOCKER(cLock, _actionRegistryCond);
        _actionRegistry.push_back(newAction);

        if (!executeNow) {
          _actionRegistryCond.signal();
        } // if
      } // lock
    } else {
      LOG_TOPIC(ERR, Logger::CLUSTER)
        << "createAction:  unknown action name given, \"" << name.c_str() << "\".";
    } // else
  } // if

  return newAction;

} // MaintenanceFeature::createAction


// All action creators should go here.
//  (actionFactory is a virtual function to allow unit tests to
//   quietly create specialty actions for testing)
#if 0
  void * MaintenanceFeature::actionFactory(std::string & name) {
    return nullptr;
  }
#else
maintenance::MaintenanceActionPtr_t MaintenanceFeature::actionFactory(std::string & name,
                                                                      std::shared_ptr<maintenance::ActionDescription_t> const & description,
                                                                      std::shared_ptr<VPackBuilder> const & properties) {
  maintenance::MaintenanceActionPtr_t newAction;

    // walk list until we find the object of our desire
    if (name == "CreateDatabase") {
      newAction.reset(new maintenance::CreateDatabase(*this, description, properties));
    }

  return newAction;

} // MaintenanceFeature::actionFactory
#endif

maintenance::MaintenanceActionPtr_t MaintenanceFeature::findActionHash(size_t hash) {
  READ_LOCKER(rLock, _actionRegistryLock);

  return(findActionHashNoLock(hash));

} // MaintenanceFeature::findActionHash


maintenance::MaintenanceActionPtr_t MaintenanceFeature::findActionHashNoLock(size_t hash) {
  // assert to test lock held?

  maintenance::MaintenanceActionPtr_t ret_ptr;

  for (auto action_it=_actionRegistry.begin();
       _actionRegistry.end() != action_it && !ret_ptr; ++action_it) {
    if ((*action_it)->hash() == hash) {
      ret_ptr=*action_it;
    } // if
  } // for

  return ret_ptr;

} // MaintenanceFeature::findActionHashNoLock


maintenance::MaintenanceActionPtr_t MaintenanceFeature::findActionId(uint64_t id) {
  READ_LOCKER(rLock, _actionRegistryLock);

  return(findActionIdNoLock(id));

} // MaintenanceFeature::findActionId


maintenance::MaintenanceActionPtr_t MaintenanceFeature::findActionIdNoLock(uint64_t id) {
  // assert to test lock held?

  maintenance::MaintenanceActionPtr_t ret_ptr;

  for (auto action_it=_actionRegistry.begin();
       _actionRegistry.end() != action_it && !ret_ptr; ++action_it) {
    if ((*action_it)->id() == id) {
      ret_ptr=*action_it;
    } // if
  } // for

  return ret_ptr;

} // MaintenanceFeature::findActionIdNoLock


maintenance::MaintenanceActionPtr_t MaintenanceFeature::findReadyAction() {
  maintenance::MaintenanceActionPtr_t ret_ptr;

  while(!_isShuttingDown && !ret_ptr) {
    CONDITION_LOCKER(cLock, _actionRegistryCond);

    // scan for ready action (and purge any that are done waiting)
    {
      WRITE_LOCKER(wLock, _actionRegistryLock);

      for (auto loop=_actionRegistry.begin(); _actionRegistry.end()!=loop && !ret_ptr; ) {
        if ((*loop)->runable()) {
          ret_ptr=*loop;
          ret_ptr->setState(maintenance::MaintenanceAction::EXECUTING);
        } else if ((*loop)->done()) {
          loop = _actionRegistry.erase(loop);
        } else {
          ++loop;
        } // else
      } // for
    } // WRITE

    // no pointer ... wait 1 second
    if (!_isShuttingDown && !ret_ptr) {
      _actionRegistryCond.wait(1000000);
    } // if

  } // while

  return ret_ptr;

} // MaintenanceFeature::findReadyAction


VPackBuilder  MaintenanceFeature::toVelocityPack() const {
  VPackBuilder vb;
  READ_LOCKER(rLock, _actionRegistryLock);

  { VPackArrayBuilder ab(&vb);
    for (auto action : _actionRegistry ) {
      action->toVelocityPack(vb);
    } // for

  }
  return vb;

} // MaintenanceFeature::toVelocityPack
#if 0
std::string MaintenanceFeature::toJson(VPackBuilder & builder) {
} // MaintenanceFeature::toJson
#endif
} // namespace arangodb
