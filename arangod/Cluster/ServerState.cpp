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

#include "ServerState.h"

#include <iomanip>
#include <sstream>

#include <boost/uuid/uuid.hpp>
#include <boost/uuid/uuid_generators.hpp>
#include <boost/uuid/uuid_io.hpp>

#include "Agency/AgencyComm.h"
#include "ApplicationFeatures/ApplicationServer.h"
#include "Basics/ReadLocker.h"
#include "Basics/VelocyPackHelper.h"
#include "Basics/WriteLocker.h"
#include "Cluster/ClusterInfo.h"

#include "Logger/Logger.h"
#include "RestServer/DatabasePathFeature.h"

using namespace arangodb;
using namespace arangodb::basics;

////////////////////////////////////////////////////////////////////////////////
/// @brief single instance of ServerState - will live as long as the server is
/// running
////////////////////////////////////////////////////////////////////////////////

static ServerState Instance;

ServerState::ServerState()
    : _id(),
      _dataPath(),
      _logPath(),
      _arangodPath(),
      _dbserverConfig(),
      _coordinatorConfig(),
      _address(),
      _lock(),
      _role(),
      _idOfPrimary(""),
      _state(STATE_UNDEFINED),
      _initialized(false),
      _clusterEnabled(false),
      _foxxmaster(""),
      _foxxmasterQueueupdate(false) {
  storeRole(ROLE_UNDEFINED);
}

ServerState::~ServerState() {}

////////////////////////////////////////////////////////////////////////////////
/// @brief create the (sole) instance
////////////////////////////////////////////////////////////////////////////////

ServerState* ServerState::instance() { return &Instance; }

////////////////////////////////////////////////////////////////////////////////
/// @brief get the string representation of a role
////////////////////////////////////////////////////////////////////////////////

const std::vector<std::string> ServerState::RoleStr ({
    "NONE", "SNGL", "PRMR", "SCND", "CRDN", "AGNT"
      });

std::string ServerState::roleToString(ServerState::RoleEnum role) {
  switch (role) {
    case ROLE_UNDEFINED:
      return "UNDEFINED";
    case ROLE_SINGLE:
      return "SINGLE";
    case ROLE_PRIMARY:
      return "PRIMARY";
    case ROLE_SECONDARY:
      return "SECONDARY";
    case ROLE_COORDINATOR:
      return "COORDINATOR";
    case ROLE_AGENT:
      return "AGENT";
  }

  TRI_ASSERT(false);
  return "";
}

////////////////////////////////////////////////////////////////////////////////
/// @brief convert a string to a role
////////////////////////////////////////////////////////////////////////////////

ServerState::RoleEnum ServerState::stringToRole(std::string const& value) {
  if (value == "SINGLE") {
    return ROLE_SINGLE;
  } else if (value == "PRIMARY") {
    return ROLE_PRIMARY;
  } else if (value == "SECONDARY") {
    return ROLE_SECONDARY;
  } else if (value == "COORDINATOR") {
    return ROLE_COORDINATOR;
  }

  return ROLE_UNDEFINED;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief convert a string representation to a state
////////////////////////////////////////////////////////////////////////////////

ServerState::StateEnum ServerState::stringToState(std::string const& value) {
  if (value == "SHUTDOWN") {
    return STATE_SHUTDOWN;
  }
  // TODO MAX: do we need to understand other states, too?

  return STATE_UNDEFINED;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief get the string representation of a state
////////////////////////////////////////////////////////////////////////////////

std::string ServerState::stateToString(StateEnum state) {
  // TODO MAX: cleanup
  switch (state) {
    case STATE_UNDEFINED:
      return "UNDEFINED";
    case STATE_STARTUP:
      return "STARTUP";
    case STATE_SERVINGASYNC:
      return "SERVING";
    case STATE_SERVINGSYNC:
      return "SERVING";
    case STATE_STOPPING:
      return "STOPPING";
    case STATE_STOPPED:
      return "STOPPED";
    case STATE_SYNCING:
      return "SYNCING";
    case STATE_INSYNC:
      return "INSYNC";
    case STATE_LOSTPRIMARY:
      return "LOSTPRIMARY";
    case STATE_SERVING:
      return "SERVING";
    case STATE_SHUTDOWN:
      return "SHUTDOWN";
  }

  TRI_ASSERT(false);
  return "";
}

////////////////////////////////////////////////////////////////////////////////
/// @brief find and set our role
////////////////////////////////////////////////////////////////////////////////
void ServerState::findAndSetRoleBlocking() {
  while (true) {
    auto role = determineRole(_localInfo, _id);
    std::string roleString = roleToString(role);
    LOG_TOPIC(DEBUG, Logger::CLUSTER) << "Found my role: " << roleString;

    if (storeRole(role)) {
      break;
    }
    sleep(1);
  }
}

////////////////////////////////////////////////////////////////////////////////
/// @brief flush the server state (used for testing)
////////////////////////////////////////////////////////////////////////////////

void ServerState::flush() { findAndSetRoleBlocking(); }

////////////////////////////////////////////////////////////////////////////////
/// @brief get the server role
////////////////////////////////////////////////////////////////////////////////

ServerState::RoleEnum ServerState::getRole() {
  auto role = loadRole();

  if (role != ServerState::ROLE_UNDEFINED || !_clusterEnabled) {
    return role;
  }

  //TRI_ASSERT(!_id.empty());
  findAndSetRoleBlocking();
  return loadRole();
}

bool ServerState::unregister() {
  TRI_ASSERT(!getId().empty());

  std::string const& id = getId();

  std::string localInfoEncoded = StringUtils::urlEncode(_localInfo);
  AgencyOperation deleteLocalIdMap("Target/MapLocalToID/" + localInfoEncoded,
                                   AgencySimpleOperationType::DELETE_OP);

  std::vector<AgencyOperation> operations = {deleteLocalIdMap};

  auto role = loadRole();
  const std::string agencyKey = roleToAgencyKey(role);
  TRI_ASSERT(isClusterRole(role));
  if (role == ROLE_COORDINATOR || role == ROLE_PRIMARY) {
    operations.push_back(AgencyOperation("Plan/" + agencyKey + "/" + id,
                                         AgencySimpleOperationType::DELETE_OP));
    operations.push_back(AgencyOperation("Current/" + agencyKey + "/" + id,
                                         AgencySimpleOperationType::DELETE_OP));
  }

  AgencyWriteTransaction unregisterTransaction(operations);

  AgencyComm comm;
  AgencyCommResult result;

  result = comm.sendTransactionWithFailover(unregisterTransaction);
  return result.successful();
}

////////////////////////////////////////////////////////////////////////////////
/// @brief try to register with a role
////////////////////////////////////////////////////////////////////////////////
bool ServerState::registerWithRole(ServerState::RoleEnum role,
                                   std::string const& myAddress) {

  setLocalInfo(RoleStr.at(role) + ":" + myAddress);
  
  if (!getId().empty()) {
    LOG_TOPIC(INFO, Logger::CLUSTER)
        << "Registering with role and localinfo. Supplied id is being ignored";
    return false;
  }

  AgencyComm comm;
  AgencyCommResult result;
  std::string localInfoEncoded = StringUtils::urlEncode(getLocalInfo());
  result = comm.getValues("Target/MapLocalToID/" + localInfoEncoded);

  std::string id;
  bool found = true;
  if (!result.successful()) {
    found = false;
  } else {
    VPackSlice idSlice = result.slice()[0].get(
        std::vector<std::string>({AgencyCommManager::path(), "Target",
                                  "MapLocalToID", localInfoEncoded}));
    if (!idSlice.isString()) {
      found = false;
    } else {
      id = idSlice.copyString();
    }
  }
  if (!found) {
    LOG_TOPIC(DEBUG, Logger::CLUSTER)
        << "Determining id from localinfo failed."
        << "Continuing with registering ourselves for the first time";
    id = createIdForRole(comm, role);
  }

  const std::string agencyKey = roleToAgencyKey(role);
  const std::string planKey = "Plan/" + agencyKey + "/" + id;
  const std::string currentKey = "Current/" + agencyKey + "/" + id;

  auto builder = std::make_shared<VPackBuilder>();
  result = comm.getValues(planKey);
  found = true;
  if (!result.successful()) {
    found = false;
  } else {
    VPackSlice plan = result.slice()[0].get(std::vector<std::string>(
        {AgencyCommManager::path(), "Plan", agencyKey, id}));
    if (!plan.isString()) {
      found = false;
    } else {
      builder->add(plan);
    }
  }
  if (!found) {
    // mop: hmm ... we are registered but not part of the Plan :O
    // create a plan for ourselves :)
    builder->add(VPackValue("none"));

    VPackSlice plan = builder->slice();

    comm.setValue(planKey, plan, 0.0);
    if (!result.successful()) {
      LOG_TOPIC(ERR, Logger::CLUSTER) << "Couldn't create plan "
                                      << result.errorMessage();
      return false;
    }
  }

  result = comm.setValue(currentKey, builder->slice(), 0.0);

  if (!result.successful()) {
    LOG_TOPIC(ERR, Logger::CLUSTER) << "Could not talk to agency! "
                                    << result.errorMessage();
    return false;
  }

  _id = id;

  findAndSetRoleBlocking();
  LOG_TOPIC(DEBUG, Logger::CLUSTER) << "We successfully announced ourselves as "
                                    << roleToString(role) << " and our id is "
                                    << id;

  return true;
}

//////////////////////////////////////////////////////////////////////////////
/// @brief get the key for a role in the agency
//////////////////////////////////////////////////////////////////////////////
std::string ServerState::roleToAgencyKey(ServerState::RoleEnum role) {
  switch (role) {
    case ROLE_PRIMARY:
      return "DBServers";
    case ROLE_COORDINATOR:
      return "Coordinators";

    case ROLE_SECONDARY:
    case ROLE_UNDEFINED:
    case ROLE_SINGLE:
    case ROLE_AGENT: {
    }
  }
  return "INVALID_CLUSTER_ROLE";
}

//////////////////////////////////////////////////////////////////////////////
/// @brief create an id for a specified role
//////////////////////////////////////////////////////////////////////////////
std::string ServerState::createIdForRole(AgencyComm comm,
                                         ServerState::RoleEnum role) {
  
  typedef std::pair<AgencyOperation,AgencyPrecondition> operationType;
  std::string const agencyKey = roleToAgencyKey(role);
  

  VPackBuilder builder;
  builder.add(VPackValue("none"));
  
  AgencyCommResult createResult;
  
  auto dbpath =
    application_features::ApplicationServer::getFeature<DatabasePathFeature>(
      "DatabasePath");
  TRI_ASSERT(dbpath != nullptr);
  auto filePath = dbpath->directory() + "/UUID";
  std::ifstream ifs(filePath);
  std::string id;
  
  if (ifs.is_open()) {
    std::getline(ifs, id);
    ifs.close();
    LOG_TOPIC(INFO, Logger::CLUSTER)
      << "Restarting with persisted UUID " << id;
  } else {
    std::ofstream ofs(filePath);
    id = RoleStr.at(role) + "-" + to_string(boost::uuids::random_generator()());
    ofs << id << std::endl;
    ofs.close();
    LOG_TOPIC(INFO, Logger::CLUSTER)
      << "Fresh start. Persisting new UUID " << id;
  }
  
  AgencyCommResult result = comm.getValues("Plan/" + agencyKey);
  if (!result.successful()) {
    LOG(FATAL) << "Couldn't fetch Plan/" << agencyKey
               << " from agency. Agency is not initialized?";
    FATAL_ERROR_EXIT();
  }
  VPackSlice servers = result.slice()[0].get(
    std::vector<std::string>({AgencyCommManager::path(), "Plan", agencyKey}));
  if (!servers.isObject()) {
    LOG(FATAL) << "Plan/" << agencyKey << " in agency is no object. "
               << "Agency not initialized?";
    FATAL_ERROR_EXIT();
  }
  
  VPackSlice entry = servers.get(id);
  LOG_TOPIC(TRACE, Logger::STARTUP)
    << id << " found in existing keys: " << (!entry.isNone());
  
  std::string targetIdStr =
    (role == ROLE_COORDINATOR) ?
    "Target/LatestCoordinatorId" : "Target/LatestDBServerId";
  std::string planUrl = "Plan/" + agencyKey + "/" + id;
  std::string targetUrl = "Target/MapUniqueToShortID/" + id;

  VPackBuilder empty;
  { VPackObjectBuilder preconditionDefinition(&empty); }
  
  AgencyGeneralTransaction reg;
  reg.operations.push_back( // Plan entry if not exists
    operationType(
      AgencyOperation(planUrl, AgencyValueOperationType::SET, builder.slice()),
      AgencyPrecondition(planUrl, AgencyPrecondition::Type::EMPTY, true)));
  reg.operations.push_back( // ShortID incrementif not already got one
    operationType(
      AgencyOperation(targetIdStr, AgencySimpleOperationType::INCREMENT_OP),
      AgencyPrecondition(targetUrl, AgencyPrecondition::Type::EMPTY, true)));
  reg.operations.push_back( // Get shortID
    operationType(AgencyOperation(targetIdStr), AgencyPrecondition()));
  result = comm.sendTransactionWithFailover(reg, 0.0);

  VPackSlice latestId = result.slice()[2].get(
    std::vector<std::string>(
      {AgencyCommManager::path(), "Target",
          (role == ROLE_COORDINATOR) ?
          "LatestCoordinatorId" : "LatestDBServerId"}));
  
  VPackBuilder localIdBuilder;
  {
    VPackObjectBuilder b(&localIdBuilder);
    localIdBuilder.add("TransactionID", latestId);
    std::stringstream ss; // ShortName
    ss << ((role == ROLE_COORDINATOR) ? "Coordinator" : "DBServer")
       << std::setw(4) << std::setfill('0') << latestId.getNumber<uint32_t>();
    std::string shortName = ss.str();
    localIdBuilder.add("ShortName", VPackValue(shortName));
  }
  
  AgencyWriteTransaction shortId( // Set new shortID if not got one already
    {AgencyOperation(
        targetUrl, AgencyValueOperationType::SET, localIdBuilder.slice())},
    AgencyPrecondition(targetUrl, AgencyPrecondition::Type::EMPTY, true)
    );
  result = comm.sendTransactionWithFailover(shortId, 0.0);
  
  return id;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief set the server role
////////////////////////////////////////////////////////////////////////////////

void ServerState::setRole(ServerState::RoleEnum role) { storeRole(role); }

////////////////////////////////////////////////////////////////////////////////
/// @brief get the server local info
////////////////////////////////////////////////////////////////////////////////

std::string ServerState::getLocalInfo() {
  READ_LOCKER(readLocker, _lock);
  return _localInfo;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief set the server local info
////////////////////////////////////////////////////////////////////////////////

void ServerState::setLocalInfo(std::string const& localInfo) {
  if (localInfo.empty()) {
    return;
  }

  WRITE_LOCKER(writeLocker, _lock);
  _localInfo = localInfo;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief get the server id
////////////////////////////////////////////////////////////////////////////////

std::string ServerState::getId() {
  READ_LOCKER(readLocker, _lock);
  return _id;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief get the server id
////////////////////////////////////////////////////////////////////////////////

std::string ServerState::getPrimaryId() {
  READ_LOCKER(readLocker, _lock);
  return _idOfPrimary;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief set the server id
////////////////////////////////////////////////////////////////////////////////

void ServerState::setId(std::string const& id) {
  if (id.empty()) {
    return;
  }

  WRITE_LOCKER(writeLocker, _lock);
  _id = id;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief get the server description
////////////////////////////////////////////////////////////////////////////////

std::string ServerState::getDescription() {
  READ_LOCKER(readLocker, _lock);
  return _description;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief set the server description
////////////////////////////////////////////////////////////////////////////////

void ServerState::setDescription(std::string const& description) {
  if (description.empty()) {
    return;
  }

  WRITE_LOCKER(writeLocker, _lock);
  _description = description;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief get the server address
////////////////////////////////////////////////////////////////////////////////

std::string ServerState::getAddress() {
  READ_LOCKER(readLocker, _lock);
  return _address;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief set the server address
////////////////////////////////////////////////////////////////////////////////

void ServerState::setAddress(std::string const& address) {
  if (address.empty()) {
    return;
  }

  WRITE_LOCKER(writeLocker, _lock);
  _address = address;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief get the current state
////////////////////////////////////////////////////////////////////////////////

ServerState::StateEnum ServerState::getState() {
  READ_LOCKER(readLocker, _lock);
  return _state;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief set the current state
////////////////////////////////////////////////////////////////////////////////

void ServerState::setState(StateEnum state) {
  bool result = false;
  auto role = loadRole();

  WRITE_LOCKER(writeLocker, _lock);

  if (state == _state) {
    return;
  }

  if (role == ROLE_PRIMARY) {
    result = checkPrimaryState(state);
  } else if (role == ROLE_SECONDARY) {
    result = checkSecondaryState(state);
  } else if (role == ROLE_COORDINATOR) {
    result = checkCoordinatorState(state);
  }

  if (result) {
    LOG_TOPIC(DEBUG, Logger::CLUSTER)
        << "changing state of " << ServerState::roleToString(role)
        << " server from " << ServerState::stateToString(_state) << " to "
        << ServerState::stateToString(state);

    _state = state;
  } else {
    LOG_TOPIC(ERR, Logger::CLUSTER)
        << "invalid state transition for " << ServerState::roleToString(role)
        << " server from " << ServerState::stateToString(_state) << " to "
        << ServerState::stateToString(state);
  }
}

////////////////////////////////////////////////////////////////////////////////
/// @brief gets the data path
////////////////////////////////////////////////////////////////////////////////

std::string ServerState::getDataPath() {
  READ_LOCKER(readLocker, _lock);
  return _dataPath;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief sets the data path
////////////////////////////////////////////////////////////////////////////////

void ServerState::setDataPath(std::string const& value) {
  WRITE_LOCKER(writeLocker, _lock);
  _dataPath = value;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief gets the log path
////////////////////////////////////////////////////////////////////////////////

std::string ServerState::getLogPath() {
  READ_LOCKER(readLocker, _lock);
  return _logPath;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief sets the log path
////////////////////////////////////////////////////////////////////////////////

void ServerState::setLogPath(std::string const& value) {
  WRITE_LOCKER(writeLocker, _lock);
  _logPath = value;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief gets the arangod path
////////////////////////////////////////////////////////////////////////////////

std::string ServerState::getArangodPath() {
  READ_LOCKER(readLocker, _lock);
  return _arangodPath;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief sets the arangod path
////////////////////////////////////////////////////////////////////////////////

void ServerState::setArangodPath(std::string const& value) {
  WRITE_LOCKER(writeLocker, _lock);
  _arangodPath = value;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief gets the JavaScript startup path
////////////////////////////////////////////////////////////////////////////////

std::string ServerState::getJavaScriptPath() {
  READ_LOCKER(readLocker, _lock);
  return _javaScriptStartupPath;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief sets the arangod path
////////////////////////////////////////////////////////////////////////////////

void ServerState::setJavaScriptPath(std::string const& value) {
  WRITE_LOCKER(writeLocker, _lock);
  _javaScriptStartupPath = value;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief gets the DBserver config
////////////////////////////////////////////////////////////////////////////////

std::string ServerState::getDBserverConfig() {
  READ_LOCKER(readLocker, _lock);
  return _dbserverConfig;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief sets the DBserver config
////////////////////////////////////////////////////////////////////////////////

void ServerState::setDBserverConfig(std::string const& value) {
  WRITE_LOCKER(writeLocker, _lock);
  _dbserverConfig = value;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief gets the coordinator config
////////////////////////////////////////////////////////////////////////////////

std::string ServerState::getCoordinatorConfig() {
  READ_LOCKER(readLocker, _lock);
  return _coordinatorConfig;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief sets the coordinator config
////////////////////////////////////////////////////////////////////////////////

void ServerState::setCoordinatorConfig(std::string const& value) {
  WRITE_LOCKER(writeLocker, _lock);
  _coordinatorConfig = value;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief redetermine the server role, we do this after a plan change.
/// This is needed for automatic failover. This calls determineRole with
/// previous values of _info and _id. In particular, the _id will usually
/// already be set. If the current role cannot be determined from the
/// agency or is not unique, then the system keeps the old role.
/// Returns true if there is a change and false otherwise.
////////////////////////////////////////////////////////////////////////////////

bool ServerState::redetermineRole() {
  std::string saveIdOfPrimary = _idOfPrimary;
  RoleEnum role = determineRole(_localInfo, _id);
  std::string roleString = roleToString(role);
  LOG_TOPIC(INFO, Logger::CLUSTER) << "Redetermined role from agency: "
                                   << roleString;
  if (role == ServerState::ROLE_UNDEFINED) {
    return false;
  }
  RoleEnum oldRole = loadRole();
  if (role != oldRole) {
    LOG_TOPIC(INFO, Logger::CLUSTER) << "Changed role to: " << roleString;
    if (!storeRole(role)) {
      return false;
    }
    return true;
  }
  if (_idOfPrimary != saveIdOfPrimary) {
    LOG_TOPIC(INFO, Logger::CLUSTER) << "The ID of our primary has changed!";
    return true;
  }
  return false;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief determine the server role by fetching data from the agency
/// Note: this method must be called under the _lock
////////////////////////////////////////////////////////////////////////////////

ServerState::RoleEnum ServerState::determineRole(std::string const& info,
                                                 std::string& id) {
  if (id.empty()) {
    int res = lookupLocalInfoToId(info, id);
    if (res != TRI_ERROR_NO_ERROR) {
      LOG_TOPIC(ERR, Logger::CLUSTER) << "Could not lookupLocalInfoToId";
      return ServerState::ROLE_UNDEFINED;
    }
    // When we get here, we have have successfully looked up our id
    LOG_TOPIC(DEBUG, Logger::CLUSTER) << "Learned my own Id: " << id;
    setId(id);
  }

  ServerState::RoleEnum role = checkCoordinatorsList(id);
  if (role == ServerState::ROLE_UNDEFINED) {
    role = checkServersList(id);
  }
  // mop: role might still be undefined
  return role;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief validate a state transition for a primary server
////////////////////////////////////////////////////////////////////////////////

bool ServerState::checkPrimaryState(StateEnum state) {
  if (state == STATE_STARTUP) {
    // startup state can only be set once
    return (_state == STATE_UNDEFINED);
  } else if (state == STATE_SERVINGASYNC) {
    return (_state == STATE_STARTUP || _state == STATE_STOPPED);
  } else if (state == STATE_SERVINGSYNC) {
    return (_state == STATE_STARTUP || _state == STATE_SERVINGASYNC ||
            _state == STATE_STOPPED);
  } else if (state == STATE_STOPPING) {
    return (_state == STATE_SERVINGSYNC || _state == STATE_SERVINGASYNC);
  } else if (state == STATE_STOPPED) {
    return (_state == STATE_STOPPING);
  } else if (state == STATE_SHUTDOWN) {
    return (_state == STATE_STARTUP || _state == STATE_STOPPED ||
            _state == STATE_SERVINGSYNC || _state == STATE_SERVINGASYNC);
  }

  // anything else is invalid
  return false;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief validate a state transition for a secondary server
////////////////////////////////////////////////////////////////////////////////

bool ServerState::checkSecondaryState(StateEnum state) {
  if (state == STATE_STARTUP) {
    // startup state can only be set once
    return (_state == STATE_UNDEFINED);
  } else if (state == STATE_SYNCING) {
    return (_state == STATE_STARTUP || _state == STATE_LOSTPRIMARY);
  } else if (state == STATE_INSYNC) {
    return (_state == STATE_SYNCING);
  } else if (state == STATE_LOSTPRIMARY) {
    return (_state == STATE_SYNCING || _state == STATE_INSYNC);
  } else if (state == STATE_SERVING) {
    return (_state == STATE_STARTUP);
  } else if (state == STATE_SHUTDOWN) {
    return (_state == STATE_STARTUP || _state == STATE_SYNCING ||
            _state == STATE_INSYNC || _state == STATE_LOSTPRIMARY);
  }

  // anything else is invalid
  return false;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief validate a state transition for a coordinator server
////////////////////////////////////////////////////////////////////////////////

bool ServerState::checkCoordinatorState(StateEnum state) {
  if (state == STATE_STARTUP) {
    // startup state can only be set once
    return (_state == STATE_UNDEFINED);
  } else if (state == STATE_SERVING) {
    return (_state == STATE_STARTUP);
  } else if (state == STATE_SHUTDOWN) {
    return (_state == STATE_STARTUP || _state == STATE_SERVING);
  }

  // anything else is invalid
  return false;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief lookup the server role by scanning Plan/Coordinators for our id
////////////////////////////////////////////////////////////////////////////////

ServerState::RoleEnum ServerState::checkCoordinatorsList(
    std::string const& id) {
  // fetch value at Plan/Coordinators
  // we need to do this to determine the server's role

  std::string const key = "Plan/Coordinators";

  AgencyComm comm;
  AgencyCommResult result = comm.getValues(key);

  if (!result.successful()) {
    std::string const endpoints = AgencyCommManager::MANAGER->endpointsString();

    LOG_TOPIC(TRACE, Logger::CLUSTER)
        << "Could not fetch configuration from agency endpoints (" << endpoints
        << "): got status code " << result._statusCode
        << ", message: " << result.errorMessage() << ", key: " << key;

    return ServerState::ROLE_UNDEFINED;
  }

  VPackSlice coordinators = result.slice()[0].get(std::vector<std::string>(
      {AgencyCommManager::path(), "Plan", "Coordinators"}));
  if (!coordinators.isObject()) {
    LOG_TOPIC(TRACE, Logger::CLUSTER)
        << "Got an invalid JSON response for Plan/Coordinators";
    return ServerState::ROLE_UNDEFINED;
  }

  // check if we can find ourselves in the list returned by the agency
  VPackSlice me = coordinators.get(id);
  if (!me.isNone()) {
    // we are in the list. this means we are a primary server
    return ServerState::ROLE_COORDINATOR;
  }

  return ServerState::ROLE_UNDEFINED;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief lookup the server id by using the local info
////////////////////////////////////////////////////////////////////////////////

int ServerState::lookupLocalInfoToId(std::string const& localInfo,
                                     std::string& id) {
  // fetch value at Plan/DBServers
  // we need to do this to determine the server's role

  std::string const key = "Target/MapLocalToID";

  int count = 0;
  while (++count <= 600) {
    AgencyComm comm;
    AgencyCommResult result = comm.getValues(key);

    if (!result.successful()) {
      std::string const endpoints = AgencyCommManager::MANAGER->endpointsString();

      LOG_TOPIC(DEBUG, Logger::STARTUP)
          << "Could not fetch configuration from agency endpoints ("
          << endpoints << "): got status code " << result._statusCode
          << ", message: " << result.errorMessage() << ", key: " << key;
    } else {
      VPackSlice slice = result.slice()[0].get(std::vector<std::string>(
          {AgencyCommManager::path(), "Target", "MapLocalToID"}));

      if (!slice.isObject()) {
        LOG_TOPIC(DEBUG, Logger::STARTUP) << "Target/MapLocalToID corrupt: "
                                          << "no object.";
      } else {
        slice = slice.get(localInfo);
        if (slice.isObject()) {
          id = arangodb::basics::VelocyPackHelper::getStringValue(slice, "ID",
                                                                  "");
          if (id.empty()) {
            LOG_TOPIC(ERR, Logger::STARTUP) << "ID not set!";
            return TRI_ERROR_CLUSTER_COULD_NOT_DETERMINE_ID;
          }
          std::string description =
              arangodb::basics::VelocyPackHelper::getStringValue(
                  slice, "Description", "");
          if (!description.empty()) {
            setDescription(description);
          }
          return TRI_ERROR_NO_ERROR;
        }
      }
    }
    sleep(1);
  };
  return TRI_ERROR_CLUSTER_COULD_NOT_DETERMINE_ID;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief lookup the server role by scanning Plan/DBServers for our id
////////////////////////////////////////////////////////////////////////////////

ServerState::RoleEnum ServerState::checkServersList(std::string const& id) {
  // fetch value at Plan/DBServers
  // we need to do this to determine the server's role

  std::string const key = "Plan/DBServers";

  AgencyComm comm;
  AgencyCommResult result = comm.getValues(key);

  if (!result.successful()) {
    std::string const endpoints = AgencyCommManager::MANAGER->endpointsString();

    LOG_TOPIC(TRACE, Logger::CLUSTER)
        << "Could not fetch configuration from agency endpoints (" << endpoints
        << "): got status code " << result._statusCode
        << ", message: " << result.errorMessage() << ", key: " << key;

    return ServerState::ROLE_UNDEFINED;
  }

  ServerState::RoleEnum role = ServerState::ROLE_UNDEFINED;

  VPackSlice dbservers = result.slice()[0].get(std::vector<std::string>(
      {AgencyCommManager::path(), "Plan", "DBServers"}));
  if (!dbservers.isObject()) {
    LOG_TOPIC(TRACE, Logger::CLUSTER)
        << "Got an invalid JSON response for Plan/DBServers";
    return ServerState::ROLE_UNDEFINED;
  }

  // check if we can find ourselves in the list returned by the agency
  VPackSlice me = dbservers.get(id);
  if (!me.isNone()) {
    // we are in the list. this means we are a primary server
    role = ServerState::ROLE_PRIMARY;
  } else {
    // check if we are a secondary...
    for (auto const& s : VPackObjectIterator(dbservers)) {
      VPackSlice slice = s.value;
      std::string name =
          arangodb::basics::VelocyPackHelper::getStringValue(slice, "");

      if (name == id) {
        role = ServerState::ROLE_SECONDARY;
        _idOfPrimary = s.key.copyString();
        break;
      }
    }
  }

  return role;
}

//////////////////////////////////////////////////////////////////////////////
/// @brief store the server role
//////////////////////////////////////////////////////////////////////////////

bool ServerState::storeRole(RoleEnum role) {
  if (isClusterRole(role)) {
    AgencyComm comm;
    AgencyCommResult result;

    if (role == ServerState::ROLE_COORDINATOR) {
      VPackBuilder builder;
      try {
        builder.add(VPackValue("none"));
      } catch (...) {
        LOG(FATAL) << "out of memory";
        FATAL_ERROR_EXIT();
      }

      // register coordinator
      AgencyCommResult result =
          comm.setValue("Current/Coordinators/" + _id, builder.slice(), 0.0);

      if (!result.successful()) {
        LOG(FATAL) << "unable to register coordinator in agency";
        FATAL_ERROR_EXIT();
      }
    } else if (role == ServerState::ROLE_PRIMARY) {
      VPackBuilder builder;
      try {
        builder.add(VPackValue("none"));
      } catch (...) {
        LOG(FATAL) << "out of memory";
        FATAL_ERROR_EXIT();
      }

      // register server
      AgencyCommResult result =
          comm.setValue("Current/DBServers/" + _id, builder.slice(), 0.0);

      if (!result.successful()) {
        LOG(FATAL) << "unable to register db server in agency";
        FATAL_ERROR_EXIT();
      }
    } else if (role == ServerState::ROLE_SECONDARY) {
      std::string keyName = _id;
      VPackBuilder builder;
      try {
        builder.add(VPackValue(keyName));
      } catch (...) {
        LOG(FATAL) << "out of memory";
        FATAL_ERROR_EXIT();
      }

      std::string myId("Current/DBServers/" +
                       ServerState::instance()->getPrimaryId());
      AgencyOperation addMe(myId, AgencyValueOperationType::SET,
                            builder.slice());
      AgencyOperation incrementVersion("Plan/Version",
                                       AgencySimpleOperationType::INCREMENT_OP);
      AgencyPrecondition precondition(myId, AgencyPrecondition::Type::EMPTY, true);
      AgencyWriteTransaction trx({addMe, incrementVersion}, precondition);

      // register server
      AgencyCommResult result = comm.sendTransactionWithFailover(trx, 0.0);

      if (!result.successful()) {
        // mop: fail gracefully (allow retry)
        return false;
      }
    }
  }
  _role.store(role, std::memory_order_release);
  return true;
}

bool ServerState::isFoxxmaster() {
  return !isRunningInCluster() || _foxxmaster == getId();
}

std::string const& ServerState::getFoxxmaster() { return _foxxmaster; }

void ServerState::setFoxxmaster(std::string const& foxxmaster) {
  if (_foxxmaster != foxxmaster) {
    setFoxxmasterQueueupdate(true);
  }
  _foxxmaster = foxxmaster;
}

bool ServerState::getFoxxmasterQueueupdate() { return _foxxmasterQueueupdate; }

void ServerState::setFoxxmasterQueueupdate(bool value) {
  _foxxmasterQueueupdate = value;
}
