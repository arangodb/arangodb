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
#include "Basics/Logger.h"
#include "Basics/MutexLocker.h"
#include "Basics/VelocyPackHelper.h"
#include "Cluster/ClusterComm.h"
#include "Cluster/ClusterInfo.h"
#include "Cluster/ClusterMethods.h"
#include "Cluster/ServerJob.h"
#include "Cluster/ServerState.h"
#include "Dispatcher/ApplicationDispatcher.h"
#include "Dispatcher/Dispatcher.h"
#include "Dispatcher/Job.h"
#include "V8Server/ApplicationV8.h"
#include "V8/v8-globals.h"
#include "V8Server/v8-vocbase.h"
#include "VocBase/auth.h"
#include "VocBase/server.h"
#include "VocBase/vocbase.h"

using namespace arangodb;

volatile sig_atomic_t HeartbeatThread::HasRunOnce = 0;

////////////////////////////////////////////////////////////////////////////////
/// @brief constructs a heartbeat thread
////////////////////////////////////////////////////////////////////////////////

HeartbeatThread::HeartbeatThread(
    TRI_server_t* server, arangodb::rest::ApplicationDispatcher* dispatcher,
    ApplicationV8* applicationV8, uint64_t interval,
    uint64_t maxFailsBeforeWarning)
    : Thread("Heartbeat"),
      _server(server),
      _dispatcher(dispatcher),
      _applicationV8(applicationV8),
      _statusLock(),
      _agency(),
      _condition(),
      _refetchUsers(),
      _myId(ServerState::instance()->getId()),
      _interval(interval),
      _maxFailsBeforeWarning(maxFailsBeforeWarning),
      _numFails(0),
      _numDispatchedJobs(0),
      _lastDispatchedJobResult(false),
      _versionThatTriggeredLastJob(0),
      _ready(false),
      _stop(0) {
  TRI_ASSERT(_dispatcher != nullptr);
  allowAsynchronousCancelation();
}

////////////////////////////////////////////////////////////////////////////////
/// @brief destroys a heartbeat thread
////////////////////////////////////////////////////////////////////////////////

HeartbeatThread::~HeartbeatThread() {}

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

  // last value of plan which we have noticed:
  uint64_t lastPlanVersionNoticed = 0;

  // last value of plan for which a job was successfully completed
  // on a coordinator, only this is used and not lastPlanVersionJobScheduled
  uint64_t lastPlanVersionJobSuccess = 0;

  // value of Sync/Commands/my-id at startup
  uint64_t lastCommandIndex = getLastCommandIndex();

  uint64_t agencyIndex = 0;

  while (!_stop) {
    LOG(TRACE) << "sending heartbeat to agency";

    double const start = TRI_microtime();

    // send our state to the agency.
    // we don't care if this fails
    sendState();

    if (_stop) {
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

    if (_stop) {
      break;
    }

    // The following loop will run until the interval has passed, at which
    // time a break will be called.
    while (true) {
      double remain = interval - (TRI_microtime() - start);

      if (remain <= 0.0) {
        break;
      }

      // First see whether a previously scheduled job has done some good:
      double timeout = remain;
      {
        MUTEX_LOCKER(mutexLocker, _statusLock);
        if (_numDispatchedJobs == -1) {
          if (_lastDispatchedJobResult) {
            lastPlanVersionJobSuccess = _versionThatTriggeredLastJob;
            LOG(INFO) << "Found result of successful handleChangesDBServer job, have now version " << lastPlanVersionJobSuccess << ".";
          }
          _numDispatchedJobs = 0;
        } else if (_numDispatchedJobs > 0) {
          timeout = (std::min)(0.1, remain);
          // Only wait for at most this much, because
          // we want to see the result of the running job
          // in time
        }
      }

      // get the current version of the Plan, or watch for a change:
      AgencyCommResult result;
      result.clear();

      if (agencyIndex != 0) {
        // If a job is scheduled and is still running, the timeout is at most
        // 0.1s, otherwise we wait up to the remainder of the interval:
        result =
            _agency.watchValue("Plan/Version", agencyIndex + 1, timeout, false);
      } else {
        result = _agency.getValues("Plan/Version", false);
      }

      if (result.successful()) {
        agencyIndex = result.index();
        result.parse("", false);

        std::map<std::string, AgencyCommResultEntry>::iterator it =
            result._values.begin();

        if (it != result._values.end()) {
          // there is a plan version
          uint64_t planVersion =
              arangodb::basics::VelocyPackHelper::stringUInt64(
                  it->second._vpack->slice());

          if (planVersion > lastPlanVersionNoticed) {
            lastPlanVersionNoticed = planVersion;
          }
        }
      } else {
        agencyIndex = 0;
      }

      if (lastPlanVersionNoticed > lastPlanVersionJobSuccess &&
          !hasPendingJob()) {
        handlePlanChangeDBServer(lastPlanVersionNoticed);
      }

      if (_stop) {
        break;
      }
    }
  }

  // another thread is waiting for this value to appear in order to shut down
  // properly
  _stop = 2;

  // Wait until any pending job is finished
  int count = 0;
  while (count++ < 10000) {
    {
      MUTEX_LOCKER(mutexLocker, _statusLock);
      if (_numDispatchedJobs <= 0) {
        break;
      }
    }
    usleep(1000);
  }
  LOG(TRACE) << "stopped heartbeat thread (DBServer version)";
}

////////////////////////////////////////////////////////////////////////////////
/// @brief check whether a job is still running or does not have reported yet
////////////////////////////////////////////////////////////////////////////////

bool HeartbeatThread::hasPendingJob() {
  MUTEX_LOCKER(mutexLocker, _statusLock);
  return _numDispatchedJobs != 0;
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

  // value of Sync/Commands/my-id at startup
  uint64_t lastCommandIndex = getLastCommandIndex();

  setReady();

  while (!_stop) {
    LOG(TRACE) << "sending heartbeat to agency";

    double const start = TRI_microtime();

    // send our state to the agency.
    // we don't care if this fails
    sendState();

    if (_stop) {
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

    if (_stop) {
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

    result = _agency.getValues("Sync/UserVersion", false);
    if (result.successful()) {
      result.parse("", false);
      std::map<std::string, AgencyCommResultEntry>::iterator it =
          result._values.begin();
      if (it != result._values.end()) {
        // there is a UserVersion
        uint64_t userVersion = arangodb::basics::VelocyPackHelper::stringUInt64(
            it->second._vpack->slice());
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
              LOG(INFO) << "Reloading users for database " << vocbase->_name << ".";

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

  // another thread is waiting for this value to appear in order to shut down
  // properly
  _stop = 2;

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
/// @brief whether or not the thread is ready
////////////////////////////////////////////////////////////////////////////////

bool HeartbeatThread::isReady() {
  MUTEX_LOCKER(mutexLocker, _statusLock);

  return _ready;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief set the thread status to ready
////////////////////////////////////////////////////////////////////////////////

void HeartbeatThread::setReady() {
  MUTEX_LOCKER(mutexLocker, _statusLock);
  _ready = true;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief decrement the counter for dispatched jobs
////////////////////////////////////////////////////////////////////////////////

void HeartbeatThread::removeDispatchedJob(bool success) {
  MUTEX_LOCKER(mutexLocker, _statusLock);
  TRI_ASSERT(_numDispatchedJobs > 0);
  _numDispatchedJobs = -1;
  _lastDispatchedJobResult = success;
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
      result = _agency.getValues(prefixPlanChangeCoordinator, true);
    }
  }

  if (result.successful()) {
    result.parse(prefixPlanChangeCoordinator + "/", false);

    std::vector<TRI_voc_tick_t> ids;

    // When we run through the databases, we need to do the _system database
    // first, otherwise, we cannot handle incoming requests from DBservers
    // as they are for example received when we ask for users of a database!
    std::map<std::string, AgencyCommResultEntry>::iterator it;
    std::vector<std::string> names;
    for (it = result._values.begin(); it != result._values.end(); ++it) {
      names.push_back(it->first);
    }
    std::sort(names.begin(), names.end(), &myDBnamesComparer);

    // loop over all database names we got and create a local database
    // instance if not yet present:
    for (std::string const& name : names) {
      it = result._values.find(name);
      VPackSlice const options = it->second._vpack->slice();

      TRI_voc_tick_t id = 0;
      if (options.hasKey("id")) {
        VPackSlice const v = options.get("id");
        if (v.isString()) {
          id = arangodb::basics::StringUtils::uint64(v.copyString());
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

      ++it;
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

  MUTEX_LOCKER(mutexLocker, _statusLock);
  if (_numDispatchedJobs > 0) {
    // do not flood the dispatcher queue with multiple server jobs
    // as this may lead to all dispatcher threads being blocked
    return false;
  }

  // schedule a job for the change
  std::unique_ptr<arangodb::rest::Job> job(
      new ServerJob(this, _server, _applicationV8));

  if (_dispatcher->dispatcher()->addJob(job) == TRI_ERROR_NO_ERROR) {
    ++_numDispatchedJobs;
    _versionThatTriggeredLastJob = currentPlanVersion;

    LOG(TRACE) << "scheduled plan update handler";
    return true;
  }

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
  const AgencyCommResult result = _agency.sendServerState(
      8.0 * static_cast<double>(_interval) / 1000.0 / 1000.0);

  if (result.successful()) {
    _numFails = 0;
    return true;
  }

  if (++_numFails % _maxFailsBeforeWarning == 0) {
    std::string const endpoints = AgencyComm::getEndpointsString();

    LOG(WARN) << "heartbeat could not be sent to agency endpoints (" << endpoints.c_str() << "): http code: " << result.httpCode() << ", body: " << result.body().c_str();
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
    LOG(TRACE) << "fetching users for database '" << vocbase->_name << "' successful";
    _refetchUsers.erase(vocbase);
  } else {
    LOG(TRACE) << "fetching users for database '" << vocbase->_name << "' failed with error: " << TRI_errno_string(res);
    _refetchUsers.insert(vocbase);
  }

  return result;
}
