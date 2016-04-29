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
/// @author Jan Steemann
////////////////////////////////////////////////////////////////////////////////

#include "HeartbeatThread.h"

#include "Basics/ConditionLocker.h"
#include "Basics/JsonHelper.h"
#include "Logger/Logger.h"
#include "Basics/MutexLocker.h"
#include "Basics/VelocyPackHelper.h"
#include "Cluster/ClusterComm.h"
#include "Cluster/ClusterInfo.h"
#include "Cluster/ClusterMethods.h"
#include "Cluster/ServerJob.h"
#include "Cluster/ServerState.h"
#include "Dispatcher/Dispatcher.h"
#include "Dispatcher/DispatcherFeature.h"
#include "Dispatcher/Job.h"
#include "V8/v8-globals.h"
#include "VocBase/auth.h"
#include "VocBase/server.h"
#include "VocBase/vocbase.h"
#include <functional>
#include <iostream>

#include <velocypack/Iterator.h>
#include <velocypack/velocypack-aliases.h>

using namespace arangodb;

volatile sig_atomic_t HeartbeatThread::HasRunOnce = 0;

////////////////////////////////////////////////////////////////////////////////
/// @brief constructs a heartbeat thread
////////////////////////////////////////////////////////////////////////////////

HeartbeatThread::HeartbeatThread(TRI_server_t* server,
                                 AgencyCallbackRegistry* agencyCallbackRegistry,
                                 uint64_t interval,
                                 uint64_t maxFailsBeforeWarning)
    : Thread("Heartbeat"),
      _server(server),
      _agencyCallbackRegistry(agencyCallbackRegistry),
      _statusLock(),
      _agency(),
      _condition(),
      _refetchUsers(),
      _myId(ServerState::instance()->getId()),
      _interval(interval),
      _maxFailsBeforeWarning(maxFailsBeforeWarning),
      _numFails(0),
      _lastSuccessfulVersion(0),
      _dispatchedVersion(0),
      _currentPlanVersion(0),
      _ready(false),
      _wasNotified(false) {}

////////////////////////////////////////////////////////////////////////////////
/// @brief destroys a heartbeat thread
////////////////////////////////////////////////////////////////////////////////

HeartbeatThread::~HeartbeatThread() { shutdown(); }

////////////////////////////////////////////////////////////////////////////////
/// @brief heartbeat main loop
/// the heartbeat thread constantly reports the current server status to the
/// agency. it does so by sending the current state string to the key
/// "Sync/ServerStates/" + my-id.
/// after transferring the current state to the agency, the heartbeat thread
/// will wait for changes on the "Sync/Commands/" + my-id key. If no changes
/// occur,
/// then the request it aborted and the heartbeat thread will go on with
/// reporting its state to the agency again. If it notices a change when
/// watching the command key, it will wake up and apply the change locally.
////////////////////////////////////////////////////////////////////////////////

void HeartbeatThread::run() {
  if (ServerState::instance()->isCoordinator()) {
    runCoordinator();
  } else {
    runDBServer();
  }
}

////////////////////////////////////////////////////////////////////////////////
/// @brief heartbeat main loop, dbserver version
////////////////////////////////////////////////////////////////////////////////

void HeartbeatThread::runDBServer() {
  LOG(TRACE) << "starting heartbeat thread (DBServer version)";

  // convert timeout to seconds
  double const interval = (double)_interval / 1000.0 / 1000.0;

  // value of Sync/Commands/my-id at startup
  uint64_t lastCommandIndex = getLastCommandIndex();

  std::function<bool(VPackSlice const& result)> updatePlan = [&](
      VPackSlice const& result) {
    if (!result.isNumber()) {
      LOG(ERR) << "Version is not a number! " << result.toJson();
      return false;
    }
    uint64_t version = result.getNumber<uint64_t>();
    
    bool mustChangePlan = false;
    uint64_t versionToChange = 0;
    {
      MUTEX_LOCKER(mutexLocker, _statusLock);
      if (version > _currentPlanVersion) {
        _currentPlanVersion = version;

        mustChangePlan = _lastSuccessfulVersion < _currentPlanVersion;
        versionToChange = _currentPlanVersion;
      }
    }
    if (mustChangePlan) {
      LOG(TRACE) << "Dispatching " << versionToChange;
      handlePlanChangeDBServer(versionToChange);
    } else {
      LOG(TRACE) << "not dispatching";
    }

    return true;
  };

  auto agencyCallback = std::make_shared<AgencyCallback>(
      _agency, "Plan/Version", updatePlan, true);

  bool registered = false;
  while (!registered) {
    registered = _agencyCallbackRegistry->registerCallback(agencyCallback);
    if (!registered) {
      LOG(ERR) << "Couldn't register plan change in agency!";
      sleep(1);
    }
  }
  
  while (!isStopping()) {
    LOG(DEBUG) << "sending heartbeat to agency";

    double const start = TRI_microtime();

    // send our state to the agency.
    // we don't care if this fails
    sendState();

    if (isStopping()) {
      break;
    }

    {
      // send an initial GET request to Sync/Commands/my-id
    
      AgencyCommResult result =
        _agency.getValues("Sync/Commands/" + _myId, false);
      
      if (result.successful()) {
        handleStateChange(result, lastCommandIndex);
      }
    }
    
    if (isStopping()) {
      break;
    }
    // XXX execute at least once
    double remain = interval - (TRI_microtime() - start);
    while (remain > 0) {
      LOG(TRACE) << "Entering update loop";
      
      bool wasNotified;
      {
        CONDITION_LOCKER(locker, _condition);
        wasNotified = _wasNotified;
        if (!wasNotified) {
          locker.wait(static_cast<uint64_t>(remain * 1000000.0));
          wasNotified = _wasNotified;
        }
        _wasNotified = false;
      }

      if (!wasNotified) {
        LOG(TRACE) << "Lock reached timeout";
        agencyCallback->refetchAndUpdate();
      } else {
        // mop: a plan change returned successfully...check if we are up-to-date
        bool mustChangePlan;
        uint64_t versionToChange;
        {
          MUTEX_LOCKER(mutexLocker, _statusLock);
          mustChangePlan = _lastSuccessfulVersion < _currentPlanVersion;
          versionToChange = _currentPlanVersion;
        }
        if (mustChangePlan) {
          LOG(TRACE) << "Dispatching " << versionToChange;
          handlePlanChangeDBServer(versionToChange);
        }
      }
      remain = interval - (TRI_microtime() - start);
    }
  }

  _agencyCallbackRegistry->unregisterCallback(agencyCallback);
  int count = 0;
  while (++count < 3000) {
    bool isInPlanChange;
    {
      MUTEX_LOCKER(mutexLocker, _statusLock);
      isInPlanChange = _dispatchedVersion > 0;
    }
    if (!isInPlanChange) {
      break;
    }
    usleep(1000);
  }
  LOG(TRACE) << "stopped heartbeat thread (DBServer version)";
}

////////////////////////////////////////////////////////////////////////////////
/// @brief heartbeat main loop, coordinator version
////////////////////////////////////////////////////////////////////////////////

void HeartbeatThread::runCoordinator() {
  LOG(TRACE) << "starting heartbeat thread (coordinator version)";

  uint64_t oldUserVersion = 0;

  // convert timeout to seconds
  double const interval = (double)_interval / 1000.0 / 1000.0;

  // last value of plan which we have noticed:
  uint64_t lastPlanVersionNoticed = 0;
  // last value of current which we have noticed:
  uint64_t lastCurrentVersionNoticed = 0;

  // value of Sync/Commands/my-id at startup
  uint64_t lastCommandIndex = getLastCommandIndex();

  setReady();

  while (!isStopping()) {
    LOG(TRACE) << "sending heartbeat to agency";

    double const start = TRI_microtime();
    // send our state to the agency.
    // we don't care if this fails
    sendState();

    if (isStopping()) {
      break;
    }

    {
    
      // send an initial GET request to Sync/Commands/my-id
      AgencyCommResult result =
          _agency.getValues("Sync/Commands/" + _myId, false);

      if (result.successful()) {
        handleStateChange(result, lastCommandIndex);
      }
    }

    if (isStopping()) {
      break;
    }

    bool shouldSleep = true;

    // get the current version of the Plan
  
    AgencyCommResult result = _agency.getValues("Plan/Version", false);

    if (result.successful()) {
      result.parse("", false);

      std::map<std::string, AgencyCommResultEntry>::iterator it =
          result._values.begin();

      if (it != result._values.end()) {
        // there is a plan version

        uint64_t planVersion = arangodb::basics::VelocyPackHelper::stringUInt64(
            it->second._vpack->slice());

        if (planVersion > lastPlanVersionNoticed) {
          if (handlePlanChangeCoordinator(planVersion)) {
            lastPlanVersionNoticed = planVersion;
          }
        }
      }
    }

    result.clear();

  
    result = _agency.getValues2("Sync/UserVersion", false);
    if (result.successful()) {

      velocypack::Slice slice =
        result._vpack->slice()[0].get(AgencyComm::prefixStripped())
        .get("Sync").get("UserVersion");

      if (slice.isNumber()) {
        // there is a UserVersion
        uint64_t userVersion = slice.getUInt();
        if (userVersion != oldUserVersion) {
          // reload user cache for all databases
          std::vector<DatabaseID> dbs =
              ClusterInfo::instance()->listDatabases(true);
          std::vector<DatabaseID>::iterator i;
          bool allOK = true;
          for (i = dbs.begin(); i != dbs.end(); ++i) {
            TRI_vocbase_t* vocbase =
                TRI_UseCoordinatorDatabaseServer(_server, i->c_str());

            if (vocbase != nullptr) {
              LOG(INFO) << "Reloading users for database " << vocbase->_name
                        << ".";

              if (!fetchUsers(vocbase)) {
                // something is wrong... probably the database server
                // with the _users collection is not yet available
                TRI_InsertInitialAuthInfo(vocbase);
                allOK = false;
                // we will not set oldUserVersion such that we will try this
                // very same exercise again in the next heartbeat
              }
              TRI_ReleaseVocBase(vocbase);
            }
          }
          if (allOK) {
            oldUserVersion = userVersion;
          }
        }
      }
    }

    result = _agency.getValues("Current/Version", false);
    if (result.successful()) {
      result.parse("", false);

      std::map<std::string, AgencyCommResultEntry>::iterator it =
          result._values.begin();

      if (it != result._values.end()) {
        // there is a plan version
        uint64_t currentVersion =
            arangodb::basics::VelocyPackHelper::stringUInt64(
                it->second._vpack->slice());

        if (currentVersion > lastCurrentVersionNoticed) {
          lastCurrentVersionNoticed = currentVersion;

          ClusterInfo::instance()->invalidateCurrent();
        }
      }
    }

    if (shouldSleep) {
      double remain = interval - (TRI_microtime() - start);

      // sleep for a while if appropriate, on some systems usleep does not
      // like arguments greater than 1000000
      while (remain > 0.0) {
        if (remain >= 0.5) {
          usleep(500000);
          remain -= 0.5;
        } else {
          usleep((unsigned long)(remain * 1000.0 * 1000.0));
          remain = 0.0;
        }
      }
    }
    
  }

  LOG(TRACE) << "stopped heartbeat thread";
}

////////////////////////////////////////////////////////////////////////////////
/// @brief initializes the heartbeat
////////////////////////////////////////////////////////////////////////////////

bool HeartbeatThread::init() {
  // send the server state a first time and use this as an indicator about
  // the agency's health
  if (!sendState()) {
    return false;
  }

  return true;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief finished plan change
////////////////////////////////////////////////////////////////////////////////

void HeartbeatThread::removeDispatchedJob(bool success) {
  LOG(TRACE) << "Dispatched job returned!";
  {
    MUTEX_LOCKER(mutexLocker, _statusLock);
    if (success) {
      _lastSuccessfulVersion = _dispatchedVersion;
    } else {
      LOG(WARN) << "Updating plan to " << _dispatchedVersion << " failed!";
    }
    _dispatchedVersion = 0;
  }
  CONDITION_LOCKER(guard, _condition);
  _wasNotified = true;
  _condition.signal();
}

////////////////////////////////////////////////////////////////////////////////
/// @brief fetch the index id of the value of Sync/Commands/my-id from the
/// agency this index value is determined initially and it is passed to the
/// watch command (we're waiting for an entry with a higher id)
////////////////////////////////////////////////////////////////////////////////

uint64_t HeartbeatThread::getLastCommandIndex() {
  // get the initial command state

  AgencyCommResult result = _agency.getValues("Sync/Commands/" + _myId, false);

  if (result.successful()) {
    result.parse("Sync/Commands/", false);

    std::map<std::string, AgencyCommResultEntry>::iterator it =
        result._values.find(_myId);

    if (it != result._values.end()) {
      // found something
      LOG(TRACE) << "last command index was: '" << (*it).second._index << "'";
      return (*it).second._index;
    }
  }

  if (result._index > 0) {
    // use the value returned in header X-Etcd-Index
    return result._index;
  }

  // nothing found. this is not an error
  return 0;
}

// We need to sort the _system database to the beginning:
static bool myDBnamesComparer(std::string const& a, std::string const& b) {
  if (a == "_system") {
    return true;
  }
  if (b == "_system") {
    return false;
  }
  return a < b;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief handles a plan version change, coordinator case
/// this is triggered if the heartbeat thread finds a new plan version number
////////////////////////////////////////////////////////////////////////////////

static std::string const prefixPlanChangeCoordinator = "Plan/Databases";
bool HeartbeatThread::handlePlanChangeCoordinator(uint64_t currentPlanVersion) {

  bool fetchingUsersFailed = false;
  LOG(TRACE) << "found a plan update";
  AgencyCommResult result;
  
  {
    AgencyCommLocker locker("Plan", "READ");
    if (locker.successful()) {
      result = _agency.getValues2(prefixPlanChangeCoordinator, true);
    }
  }
  
  if (result.successful()) {
    
    std::vector<TRI_voc_tick_t> ids;
    velocypack::Slice databases =
      result._vpack->slice()[0]
      .get(AgencyComm::prefix().substr(1,AgencyComm::prefix().size()-2))
      .get("Plan").get("Databases");
    
    // loop over all database names we got and create a local database
    // instance if not yet present:
    
    for (auto const& options : VPackObjectIterator (databases)) {
      
      std::string const name = options.value.get("name").copyString();
      TRI_voc_tick_t id = 0;
      
      if (options.value.hasKey("id")) {
        VPackSlice const v = options.value.get("id");
        if (v.isNumber()) {
          id = v.getUInt();
        }
      }
      
      if (id > 0) {
        ids.push_back(id);
      }
      
      TRI_vocbase_t* vocbase =
        TRI_UseCoordinatorDatabaseServer(_server, name.c_str());
      
      if (vocbase == nullptr) {
        // database does not yet exist, create it now
        
        if (id == 0) {
          // verify that we have an id
          id = ClusterInfo::instance()->uniqid();
        }
        
        TRI_vocbase_defaults_t defaults;
        TRI_GetDatabaseDefaultsServer(_server, &defaults);
        
        // create a local database object...
        TRI_CreateCoordinatorDatabaseServer(_server, id, name.c_str(),
                                            &defaults, &vocbase);
        
        if (vocbase != nullptr) {
          HasRunOnce = 1;
          
          // insert initial user(s) for database
          if (!fetchUsers(vocbase)) {
            TRI_ReleaseVocBase(vocbase);
            return false;  // We give up, we will try again in the
                           // next heartbeat
          }
        }
      } else {
        if (_refetchUsers.find(vocbase) != _refetchUsers.end()) {
          // must re-fetch users for an existing database
          if (!fetchUsers(vocbase)) {
            fetchingUsersFailed = true;
          }
        }
        
        TRI_ReleaseVocBase(vocbase);
      }
      
    }
    
    // get the list of databases that we know about locally
    std::vector<TRI_voc_tick_t> localIds =
      TRI_GetIdsCoordinatorDatabaseServer(_server);
    
    for (auto id : localIds) {
      auto r = std::find(ids.begin(), ids.end(), id);
      
      if (r == ids.end()) {
        // local database not found in the plan...
        TRI_DropByIdCoordinatorDatabaseServer(_server, id, false);
      }
    }
    
  } else {
    return false;
  }
  
  if (fetchingUsersFailed) {
    return false;
  }
  
  // invalidate our local cache
  ClusterInfo::instance()->flush();
  
  // turn on error logging now
  if (!ClusterComm::instance()->enableConnectionErrorLogging(true)) {
    LOG(INFO) << "created coordinator databases for the first time";
  }

  return true;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief handles a plan version change, DBServer case
/// this is triggered if the heartbeat thread finds a new plan version number
////////////////////////////////////////////////////////////////////////////////

bool HeartbeatThread::handlePlanChangeDBServer(uint64_t currentPlanVersion) {
  LOG(TRACE) << "found a plan update";

  // schedule a job for the change
  std::unique_ptr<arangodb::rest::Job> job(new ServerJob(this));
  
  auto dispatcher = DispatcherFeature::DISPATCHER;
  {
    MUTEX_LOCKER(mutexLocker, _statusLock);
    // mop: only dispatch one at a time
    if (_dispatchedVersion > 0) {
      return false;
    }
    _dispatchedVersion = currentPlanVersion;
  }
  if (dispatcher->addJob(job) == TRI_ERROR_NO_ERROR) {
    LOG(TRACE) << "scheduled plan update handler";
    return true;
  }
  MUTEX_LOCKER(mutexLocker, _statusLock);
  _dispatchedVersion = 0;


  LOG(ERR) << "could not schedule plan update handler";

  return false;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief handles a state change
/// this is triggered if the watch command reports a change
/// when this is called, it will update the index value of the last command
/// (we'll pass the updated index value to the next watches so we don't get
/// notified about this particular change again).
////////////////////////////////////////////////////////////////////////////////

bool HeartbeatThread::handleStateChange(AgencyCommResult& result,
                                        uint64_t& lastCommandIndex) {
  result.parse("Sync/Commands/", false);

  std::map<std::string, AgencyCommResultEntry>::const_iterator it =
      result._values.find(_myId);

  if (it != result._values.end()) {
    lastCommandIndex = (*it).second._index;

    std::string command = "";
    VPackSlice const slice = it->second._vpack->slice();
    if (slice.isString()) {
      command = slice.copyString();
    }
    ServerState::StateEnum newState = ServerState::stringToState(command);

    if (newState != ServerState::STATE_UNDEFINED) {
      // state change.
      ServerState::instance()->setState(newState);
      return true;
    }
  }

  return false;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief sends the current server's state to the agency
////////////////////////////////////////////////////////////////////////////////

bool HeartbeatThread::sendState() {
  const AgencyCommResult result = _agency.sendServerState(0.0);
//      8.0 * static_cast<double>(_interval) / 1000.0 / 1000.0);

  if (result.successful()) {
    _numFails = 0;
    return true;
  }

  if (++_numFails % _maxFailsBeforeWarning == 0) {
    std::string const endpoints = AgencyComm::getEndpointsString();

    LOG(WARN) << "heartbeat could not be sent to agency endpoints ("
              << endpoints << "): http code: " << result.httpCode()
              << ", body: " << result.body();
    _numFails = 0;
  }

  return false;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief fetch users for a database (run on coordinator only)
////////////////////////////////////////////////////////////////////////////////

bool HeartbeatThread::fetchUsers(TRI_vocbase_t* vocbase) {
  bool result = false;
  VPackBuilder builder;
  builder.openArray();

  LOG(TRACE) << "fetching users for database '" << vocbase->_name << "'";

  int res = usersOnCoordinator(std::string(vocbase->_name), builder, 10.0);

  if (res == TRI_ERROR_NO_ERROR) {
    builder.close();
    VPackSlice users = builder.slice();
    // we were able to read from the _users collection
    TRI_ASSERT(users.isArray());

    if (users.length() == 0) {
      // no users found, now insert initial default user
      TRI_InsertInitialAuthInfo(vocbase);
    } else {
      // users found in collection, insert them into cache
      TRI_PopulateAuthInfo(vocbase, users);
    }

    result = true;
  } else if (res == TRI_ERROR_ARANGO_COLLECTION_NOT_FOUND) {
    // could not access _users collection, probably the cluster
    // was just created... insert initial default user
    TRI_InsertInitialAuthInfo(vocbase);
    result = true;
  } else if (res == TRI_ERROR_INTERNAL) {
    // something is wrong... probably the database server with the
    // _users collection is not yet available
    // try again next time
    result = false;
  }

  if (result) {
    LOG(TRACE) << "fetching users for database '" << vocbase->_name
               << "' successful";
    _refetchUsers.erase(vocbase);
  } else {
    LOG(TRACE) << "fetching users for database '" << vocbase->_name
               << "' failed with error: " << TRI_errno_string(res);
    _refetchUsers.insert(vocbase);
  }

  return result;
}
