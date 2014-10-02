////////////////////////////////////////////////////////////////////////////////
/// @brief heartbeat thread
///
/// @file
///
/// DISCLAIMER
///
/// Copyright 2014 ArangoDB GmbH, Cologne, Germany
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
/// @author Copyright 2014, ArangoDB GmbH, Cologne, Germany
/// @author Copyright 2013, triagens GmbH, Cologne, Germany
////////////////////////////////////////////////////////////////////////////////

#include "HeartbeatThread.h"
#include "Basics/ConditionLocker.h"
#include "Basics/JsonHelper.h"
#include "Basics/logging.h"
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

using namespace triagens::arango;

// -----------------------------------------------------------------------------
// --SECTION--                                                   HeartbeatThread
// -----------------------------------------------------------------------------
        
volatile sig_atomic_t HeartbeatThread::HasRunOnce = 0;

// -----------------------------------------------------------------------------
// --SECTION--                                      constructors and destructors
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @brief constructs a heartbeat thread
////////////////////////////////////////////////////////////////////////////////

HeartbeatThread::HeartbeatThread (TRI_server_t* server,
                                  triagens::rest::ApplicationDispatcher* dispatcher,
                                  ApplicationV8* applicationV8,
                                  uint64_t interval,
                                  uint64_t maxFailsBeforeWarning)
  : Thread("heartbeat"),
    _server(server),
    _dispatcher(dispatcher),
    _applicationV8(applicationV8),
    _agency(),
    _condition(),
    _refetchUsers(),
    _myId(ServerState::instance()->getId()),
    _interval(interval),
    _maxFailsBeforeWarning(maxFailsBeforeWarning),
    _numFails(0),
    _stop(0),
    _ready(0) {

  TRI_ASSERT(_dispatcher != nullptr);
  allowAsynchronousCancelation();
}

////////////////////////////////////////////////////////////////////////////////
/// @brief destroys a heartbeat thread
////////////////////////////////////////////////////////////////////////////////

HeartbeatThread::~HeartbeatThread () {
}

// -----------------------------------------------------------------------------
// --SECTION--                                                    Thread methods
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @brief heartbeat main loop
/// the heartbeat thread constantly reports the current server status to the
/// agency. it does so by sending the current state string to the key
/// "Sync/ServerStates/" + my-id.
/// after transferring the current state to the agency, the heartbeat thread
/// will wait for changes on the "Sync/Commands/" + my-id key. If no changes occur,
/// then the request it aborted and the heartbeat thread will go on with
/// reporting its state to the agency again. If it notices a change when
/// watching the command key, it will wake up and apply the change locally.
////////////////////////////////////////////////////////////////////////////////

void HeartbeatThread::run () {
  LOG_TRACE("starting heartbeat thread");

  uint64_t oldUserVersion = 0;

  // convert timeout to seconds
  const double interval = (double) _interval / 1000.0 / 1000.0;

  // last value of plan that we fetched
  uint64_t lastPlanVersion = 0;

  // value of Sync/Commands/my-id at startup
  uint64_t lastCommandIndex = getLastCommandIndex();
  const bool isCoordinator = ServerState::instance()->isCoordinator();

  if (isCoordinator) {
    ready(true);
  }


  while (! _stop) {
    LOG_TRACE("sending heartbeat to agency");

    const double start = TRI_microtime();

    // send our state to the agency.
    // we don't care if this fails
    sendState();

    if (_stop) {
      break;
    }

    {
      // send an initial GET request to Sync/Commands/my-id
      AgencyCommResult result = _agency.getValues("Sync/Commands/" + _myId, false);

      if (result.successful()) {
        handleStateChange(result, lastCommandIndex);
      }
    }

    if (_stop) {
      break;
    }

    bool shouldSleep = true;

    if (isCoordinator) {
      // isCoordinator
      // --------------------

      // get the current version of the Plan
      AgencyCommResult result = _agency.getValues("Plan/Version", false);

      if (result.successful()) {
        result.parse("", false);

        std::map<std::string, AgencyCommResultEntry>::iterator it = result._values.begin();

        if (it != result._values.end()) {
          // there is a plan version
          uint64_t planVersion = triagens::basics::JsonHelper::stringUInt64((*it).second._json);

          if (planVersion > lastPlanVersion) {
            handlePlanChangeCoordinator(planVersion, lastPlanVersion);
          }
        }
      }

      result.clear();

      result = _agency.getValues("Sync/UserVersion", false);
      if (result.successful()) {
        result.parse("", false);
        std::map<std::string, AgencyCommResultEntry>::iterator it
            = result._values.begin();
        if (it != result._values.end()) {
          // there is a UserVersion
          uint64_t userVersion = triagens::basics::JsonHelper::stringUInt64((*it).second._json);
          if (userVersion != oldUserVersion) {
            // reload user cache for all databases
            vector<DatabaseID> dbs
                = ClusterInfo::instance()->listDatabases(true);
            vector<DatabaseID>::iterator i;
            bool allOK = true;
            for (i = dbs.begin(); i != dbs.end(); ++i) {
              TRI_vocbase_t* vocbase = TRI_UseCoordinatorDatabaseServer(_server,
                                                    i->c_str());

              if (vocbase != nullptr) {
                LOG_INFO("Reloading users for database %s.",vocbase->_name);

                if (! fetchUsers(vocbase)) {
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
    }
    else {
      // ! isCoordinator
      // --------------------

      // get the current version of the Plan
      AgencyCommResult result = _agency.getValues("Plan/Version", false);

      if (result.successful()) {
        const uint64_t agencyIndex = result.index();
        result.parse("", false);
        bool changed = false;

        std::map<std::string, AgencyCommResultEntry>::iterator it = result._values.begin();

        if (it != result._values.end()) {
          // there is a plan version
          uint64_t planVersion = triagens::basics::JsonHelper::stringUInt64((*it).second._json);

          if (planVersion > lastPlanVersion) {
            handlePlanChangeDBServer(planVersion, lastPlanVersion);
            changed = true;
          }
        }

        if (_stop) {
          break;
        }

        if (! changed) {
          const double remain = interval - (TRI_microtime() - start);

          if (remain > 0.0) {
            // watch Plan/Version for changes
            result.clear();

            result = _agency.watchValue("Plan/Version",
                                        agencyIndex + 1,
                                        remain,
                                        false);

            if (result.successful()) {
              result.parse("", false);
              it = result._values.begin();

              if (it != result._values.end()) {
                // there is a plan version
                uint64_t planVersion = triagens::basics::JsonHelper::stringUInt64((*it).second._json);

                if (planVersion > lastPlanVersion) {
                  handlePlanChangeDBServer(planVersion, lastPlanVersion);
                  shouldSleep = false;
                }
              }
            }
          }
        }
      }
    }

    if (shouldSleep) {
      double const remain = interval - (TRI_microtime() - start);

      // sleep for a while if apropriate
      if (remain > 0.0) {
        usleep((unsigned long) (remain * 1000.0 * 1000.0));
      }
    }
  }

  // another thread is waiting for this value to appear in order to shut down properly
  _stop = 2;

  LOG_TRACE("stopped heartbeat thread");
}

// -----------------------------------------------------------------------------
// --SECTION--                                                    public methods
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @brief initialises the heartbeat
////////////////////////////////////////////////////////////////////////////////

bool HeartbeatThread::init () {
  // send the server state a first time and use this as an indicator about
  // the agency's health
  if (! sendState()) {
    return false;
  }

  return true;
}

// -----------------------------------------------------------------------------
// --SECTION--                                                   private methods
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @brief fetch the index id of the value of Sync/Commands/my-id from the
/// agency this index value is determined initially and it is passed to the
/// watch command (we're waiting for an entry with a higher id)
////////////////////////////////////////////////////////////////////////////////

uint64_t HeartbeatThread::getLastCommandIndex () {
  // get the initial command state
  AgencyCommResult result = _agency.getValues("Sync/Commands/" + _myId, false);

  if (result.successful()) {
    result.parse("Sync/Commands/", false);

    std::map<std::string, AgencyCommResultEntry>::iterator it = result._values.find(_myId);

    if (it != result._values.end()) {
      // found something
      LOG_TRACE("last command index was: '%llu'", (unsigned long long) (*it).second._index);
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
static bool myDBnamesComparer (std::string const& a, std::string const& b) {
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

bool HeartbeatThread::handlePlanChangeCoordinator (uint64_t currentPlanVersion,
                                                   uint64_t& remotePlanVersion) {
  static const string prefix = "Plan/Databases";

  bool fetchingUsersFailed = false;
  LOG_TRACE("found a plan update");

  // invalidate our local cache
  ClusterInfo::instance()->flush();

  AgencyCommResult result;

  {
    AgencyCommLocker locker("Plan", "READ");

    if (locker.successful()) {
      result = _agency.getValues(prefix, true);
    }
  }

  if (result.successful()) {
    result.parse(prefix + "/", false);

    vector<TRI_voc_tick_t> ids;

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
    std::vector<std::string>::iterator it1;
    for (it1 = names.begin(); it1 != names.end(); ++it1) {
      it = result._values.find(*it1);
      string const& name = *it1;
      TRI_json_t const* options = (*it).second._json;

      TRI_voc_tick_t id = 0;
      TRI_json_t const* v = TRI_LookupArrayJson(options, "id");
      if (TRI_IsStringJson(v)) {
        id = triagens::basics::StringUtils::uint64(v->_value._string.data);
      }

      if (id > 0) {
        ids.push_back(id);
      }

      TRI_vocbase_t* vocbase = TRI_UseCoordinatorDatabaseServer(_server, name.c_str());

      if (vocbase == nullptr) {
        // database does not yet exist, create it now

        if (id == 0) {
          // verify that we have an id
          id = ClusterInfo::instance()->uniqid();
        }

        TRI_vocbase_defaults_t defaults;
        TRI_GetDatabaseDefaultsServer(_server, &defaults);

        // create a local database object...
        TRI_CreateCoordinatorDatabaseServer(_server, id, name.c_str(), &defaults, &vocbase);

        if (vocbase != nullptr) {
          HasRunOnce = 1;

          // insert initial user(s) for database
          if (! fetchUsers(vocbase)) {
            TRI_ReleaseVocBase(vocbase);
            return false;  // We give up, we will try again in the
                           // next heartbeat, because we did not
                           // touch remotePlanVersion
          }
        }
      }
      else {
        if (_refetchUsers.find(vocbase) != _refetchUsers.end()) {
          // must re-fetch users for an existing database
          if (! fetchUsers(vocbase)) {
            fetchingUsersFailed = true;
          }
        }

        TRI_ReleaseVocBase(vocbase);
      }

      ++it;
    }

    TRI_voc_tick_t* localIds = TRI_GetIdsCoordinatorDatabaseServer(_server);

    if (localIds != nullptr) {
      TRI_voc_tick_t* p = localIds;

      while (*p != 0) {
        vector<TRI_voc_tick_t>::const_iterator r = std::find(ids.begin(), ids.end(), *p);

        if (r == ids.end()) {
          TRI_DropByIdCoordinatorDatabaseServer(_server, *p, false);
        }

        ++p;
      }

      TRI_Free(TRI_UNKNOWN_MEM_ZONE, localIds);
    }
  }
  else {
    return false;
  }

  if (fetchingUsersFailed) {
    return false;
  }


  remotePlanVersion = currentPlanVersion;

  // turn on error logging now
  if (! ClusterComm::instance()->enableConnectionErrorLogging(true)) {
    LOG_INFO("created coordinator databases for the first time");
  }

  return true;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief handles a plan version change, DBServer case
/// this is triggered if the heartbeat thread finds a new plan version number
////////////////////////////////////////////////////////////////////////////////

bool HeartbeatThread::handlePlanChangeDBServer (uint64_t currentPlanVersion,
                                                uint64_t& remotePlanVersion) {
  LOG_TRACE("found a plan update");

  // invalidate our local cache
  ClusterInfo::instance()->flush();

  // schedule a job for the change
  triagens::rest::Job* job = new ServerJob(this, _server, _applicationV8);

  TRI_ASSERT(job != nullptr);

  if (_dispatcher->dispatcher()->addJob(job) == TRI_ERROR_NO_ERROR) {
    remotePlanVersion = currentPlanVersion;

    LOG_TRACE("scheduled plan update handler");
  }
  else {
    LOG_ERROR("could not schedule plan update handler");
  }

  return true;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief handles a state change
/// this is triggered if the watch command reports a change
/// when this is called, it will update the index value of the last command
/// (we'll pass the updated index value to the next watches so we don't get
/// notified about this particular change again).
////////////////////////////////////////////////////////////////////////////////

bool HeartbeatThread::handleStateChange (AgencyCommResult& result,
                                         uint64_t& lastCommandIndex) {
  result.parse("Sync/Commands/", false);

  std::map<std::string, AgencyCommResultEntry>::const_iterator it = result._values.find(_myId);

  if (it != result._values.end()) {
    lastCommandIndex = (*it).second._index;

    const std::string command = triagens::basics::JsonHelper::getStringValue((*it).second._json, "");
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

bool HeartbeatThread::sendState () {
  const AgencyCommResult result = _agency.sendServerState(
                 8.0 * static_cast<double>(_interval) / 1000.0 / 1000.0);

  if (result.successful()) {
    _numFails = 0;
    return true;

  }

  if (++_numFails % _maxFailsBeforeWarning == 0) {
    const std::string endpoints = AgencyComm::getEndpointsString();

    LOG_WARNING("heartbeat could not be sent to agency endpoints (%s): http code: %d, body: %s",
                endpoints.c_str(),
                result.httpCode(),
                result.body().c_str());
    _numFails = 0;
  }

  return false;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief fetch users for a database (run on coordinator only)
////////////////////////////////////////////////////////////////////////////////

bool HeartbeatThread::fetchUsers (TRI_vocbase_t* vocbase) {
  bool result = false;
  TRI_json_t* json = nullptr;
  
  LOG_TRACE("fetching users for database '%s'", vocbase->_name);  

  int res = usersOnCoordinator(string(vocbase->_name), json, 10.0);
  
  if (res == TRI_ERROR_NO_ERROR) {
    // we were able to read from the _users collection
    TRI_ASSERT(TRI_IsListJson(json));

    if (json->_value._objects._length == 0) {
      // no users found, now insert initial default user
      TRI_InsertInitialAuthInfo(vocbase);
    }
    else {
      // users found in collection, insert them into cache
      TRI_PopulateAuthInfo(vocbase, json);
    }
    
    result = true;
  }
  else if (res == TRI_ERROR_ARANGO_COLLECTION_NOT_FOUND) {
    // could not access _users collection, probably the cluster
    // was just created... insert initial default user
    TRI_InsertInitialAuthInfo(vocbase);
    result = true;
  }
  else if (res == TRI_ERROR_INTERNAL) {
    // something is wrong... probably the database server with the
    // _users collection is not yet available
    // try again next time
    result = false;
  }

  if (json != nullptr) {
    TRI_FreeJson(TRI_UNKNOWN_MEM_ZONE, json);
  }
    
  if (result) {
    LOG_TRACE("fetching users for database '%s' successful", vocbase->_name);
    _refetchUsers.erase(vocbase);
  }
  else {
    LOG_TRACE("fetching users for database '%s' failed with error: %s", TRI_errno_string(res));
    _refetchUsers.insert(vocbase);
  }

  return result;
}

// -----------------------------------------------------------------------------
// --SECTION--                                                       END-OF-FILE
// -----------------------------------------------------------------------------

// Local Variables:
// mode: outline-minor
// outline-regexp: "/// @brief\\|/// {@inheritDoc}\\|/// @page\\|// --SECTION--\\|/// @\\}"
// End:
