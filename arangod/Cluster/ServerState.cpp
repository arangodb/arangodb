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
#include <iostream>
#include <sstream>

#include <boost/uuid/uuid.hpp>
#include <boost/uuid/uuid_generators.hpp>
#include <boost/uuid/uuid_io.hpp>

#include "Agency/AgencyComm.h"
#include "ApplicationFeatures/ApplicationServer.h"
#include "Basics/FileUtils.h"
#include "Basics/ReadLocker.h"
#include "Basics/VelocyPackHelper.h"
#include "Basics/WriteLocker.h"
#include "Cluster/ClusterInfo.h"

#include "Logger/Logger.h"
#include "RestServer/DatabaseFeature.h"
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

std::string ServerState::roleToString(ServerState::RoleEnum role) {
  switch (role) {
    case ROLE_UNDEFINED:
      return "UNDEFINED";
    case ROLE_SINGLE:
      return "SINGLE";
    case ROLE_PRIMARY:
      return "PRIMARY";
    case ROLE_COORDINATOR:
      return "COORDINATOR";
    case ROLE_AGENT:
      return "AGENT";
  }

  TRI_ASSERT(false);
  return "";
}

std::string ServerState::roleToShortString(ServerState::RoleEnum role) {
  switch (role) {
    case ROLE_UNDEFINED:
      return "NONE";
    case ROLE_SINGLE:
      return "SNGL";
    case ROLE_PRIMARY:
      return "PRMR";
    case ROLE_COORDINATOR:
      return "CRDN";
    case ROLE_AGENT:
      return "AGNT";
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
  } else if (value == "COORDINATOR") {
    return ROLE_COORDINATOR;
  } else if (value == "AGENT") {
    return ROLE_AGENT;
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
    case STATE_STOPPING:
      return "STOPPING";
    case STATE_STOPPED:
      return "STOPPED";
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
    LOG_TOPIC(DEBUG, Logger::CLUSTER) << "Found my role: " << role;

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
  TRI_ASSERT(AgencyCommManager::isEnabled());

  std::string const& id = getId();
  std::string localInfoEncoded = StringUtils::urlEncode(_localInfo);
  AgencyOperation deleteLocalIdMap("Target/MapLocalToID/" + localInfoEncoded,
                                   AgencySimpleOperationType::DELETE_OP);

  std::vector<AgencyOperation> operations = {deleteLocalIdMap};
  const std::string agencyListKey = roleToAgencyListKey(loadRole());
  operations.push_back(AgencyOperation("Plan/" + agencyListKey + "/" + id,
                                       AgencySimpleOperationType::DELETE_OP));
  operations.push_back(AgencyOperation("Current/" + agencyListKey + "/" + id,
                                       AgencySimpleOperationType::DELETE_OP));

  AgencyWriteTransaction unregisterTransaction(operations);
  AgencyComm comm;
  AgencyCommResult r = comm.sendTransactionWithFailover(unregisterTransaction);
  return r.successful();
}

////////////////////////////////////////////////////////////////////////////////
/// @brief try to integrate into a cluster
////////////////////////////////////////////////////////////////////////////////
bool ServerState::integrateIntoCluster(ServerState::RoleEnum role,
                                       std::string const& myAddress) {

  AgencyComm comm;
  AgencyCommResult result;

  std::string id;
  if (!hasPersistedId()) {
    id = generatePersistedId(role);
    LOG_TOPIC(INFO, Logger::CLUSTER)
      << "Fresh start. Persisting new UUID " << id;
  } else {
    id = getPersistedId();
    LOG_TOPIC(DEBUG, Logger::CLUSTER)
      << "Restarting with persisted UUID " << id;
  }
  setId(id);

  if (!registerAtAgency(comm, role, id)) {
    FATAL_ERROR_EXIT();
  }

  findAndSetRoleBlocking();
  LOG_TOPIC(DEBUG, Logger::CLUSTER) << "We successfully announced ourselves as "
    << roleToString(role) << " and our id is "
    << id;

  return true;
}

//////////////////////////////////////////////////////////////////////////////
/// @brief get the key for a role in the agency
//////////////////////////////////////////////////////////////////////////////
std::string ServerState::roleToAgencyListKey(ServerState::RoleEnum role) {
  return roleToAgencyKey(role) + "s"; // lol
}

std::string ServerState::roleToAgencyKey(ServerState::RoleEnum role) {
  switch (role) {
    case ROLE_PRIMARY:
      return "DBServer";
    case ROLE_COORDINATOR:
      return "Coordinator";
    case ROLE_SINGLE:
      return "Single";
      
    case ROLE_UNDEFINED:
    case ROLE_AGENT: {
      TRI_ASSERT(false);
    }
  }
  return "INVALID_CLUSTER_ROLE";
}

void mkdir (std::string const& path) {
  if (!TRI_IsDirectory(path.c_str())) {
    if (!arangodb::basics::FileUtils::createDirectory(path)) {
      LOG_TOPIC(FATAL, arangodb::Logger::FIXME) << "Couldn't create file directory " << path << " (UUID)";
      FATAL_ERROR_EXIT();
    }
  }
}

std::string ServerState::getUuidFilename() {
  auto dbpath =
    application_features::ApplicationServer::getFeature<DatabasePathFeature>(
      "DatabasePath");
  TRI_ASSERT(dbpath != nullptr);
  mkdir (dbpath->directory());

  return dbpath->directory() + "/UUID";
}

bool ServerState::hasPersistedId() {
  std::string uuidFilename = getUuidFilename(); 
  return FileUtils::exists(uuidFilename);
}

bool ServerState::writePersistedId(std::string const& id) {
  std::string uuidFilename = getUuidFilename();
  std::ofstream ofs(uuidFilename);
  if (!ofs.is_open()) {
    LOG_TOPIC(FATAL, Logger::CLUSTER)
      << "Couldn't write id file " << getUuidFilename();
    FATAL_ERROR_EXIT();
    return false;
  }
  ofs << id << std::endl;
  ofs.close();

  return true;
}

std::string ServerState::generatePersistedId(RoleEnum const& role) {
  std::string id = roleToShortString(role) + "-" +
    to_string(boost::uuids::random_generator()());
  writePersistedId(id);
  return id;
}

std::string ServerState::getPersistedId() {
  std::string uuidFilename = getUuidFilename(); 
  std::ifstream ifs(uuidFilename);

  std::string id;
  if (ifs.is_open()) {
    std::getline(ifs, id);
    ifs.close();
  } else {
    LOG_TOPIC(FATAL, Logger::STARTUP) << "Couldn't open " << uuidFilename;
    FATAL_ERROR_EXIT();
  }
  return id;
}

//////////////////////////////////////////////////////////////////////////////
/// @brief create an id for a specified role
//////////////////////////////////////////////////////////////////////////////

bool ServerState::registerAtAgency(AgencyComm& comm,
                                   const ServerState::RoleEnum& role,
                                   std::string const& id) {

  typedef std::pair<AgencyOperation,AgencyPrecondition> operationType;
  std::string agencyListKey = roleToAgencyListKey(role);
  std::string idKey = "Latest" + roleToAgencyKey(role) + "Id";

  VPackBuilder builder;
  builder.add(VPackValue("none"));

  AgencyCommResult createResult;

  AgencyCommResult result = comm.getValues("Plan/" + agencyListKey);
  if (!result.successful()) {
    LOG_TOPIC(FATAL, Logger::STARTUP) << "Couldn't fetch Plan/"
                                      << agencyListKey << " from agency. "
                                      << " Agency is not initialized?";
    return false;
  }

  VPackSlice servers = result.slice()[0].get(
    std::vector<std::string>({AgencyCommManager::path(), "Plan", agencyListKey}));
  if (!servers.isObject()) {
    LOG_TOPIC(FATAL, Logger::STARTUP) << "Plan/" << agencyListKey
      << " in agency is no object. "
      << "Agency not initialized?";
    return false;
  }

  std::string planUrl = "Plan/" + agencyListKey + "/" + id;
  std::string currentUrl = "Current/" + agencyListKey + "/" + id;

  AgencyGeneralTransaction reg;
  reg.push_back( // Plan entry if not exists
      operationType(
        AgencyOperation(planUrl, AgencyValueOperationType::SET, builder.slice()),
        AgencyPrecondition(planUrl, AgencyPrecondition::Type::EMPTY, true)));

  reg.push_back( // Current entry if not exists
      operationType(
        AgencyOperation(currentUrl, AgencyValueOperationType::SET, builder.slice()),
        AgencyPrecondition(currentUrl, AgencyPrecondition::Type::EMPTY, true)));
  
  // ok to fail..if it failed we are already registered
  comm.sendTransactionWithFailover(reg, 0.0);

  std::string targetIdStr = "Target/" + idKey;
  std::string targetUrl = "Target/MapUniqueToShortID/" + id;

  size_t attempts {0};
  while (attempts++ < 300) {
    AgencyReadTransaction readValueTrx(std::vector<std::string>{AgencyCommManager::path(targetIdStr),
                                                                AgencyCommManager::path(targetUrl)});
    AgencyCommResult result = comm.sendTransactionWithFailover(readValueTrx, 0.0);
    
    if (!result.successful()) {
      LOG_TOPIC(WARN, Logger::CLUSTER) << "Couldn't fetch " << targetIdStr
        << " and " << targetUrl;
      sleep(1);
      continue;
    }

    VPackSlice mapSlice = result.slice()[0].get(
    std::vector<std::string>(
      {AgencyCommManager::path(), "Target", "MapUniqueToShortID", id}));

    // already registered
    if (!mapSlice.isNone()) {
      return true;
    }

    VPackSlice latestId = result.slice()[0].get(
    std::vector<std::string>(
      {AgencyCommManager::path(), "Target", idKey}));

    uint32_t num = 0;
    std::unique_ptr<AgencyPrecondition> latestIdPrecondition;
    VPackBuilder latestIdBuilder;
    if (latestId.isNumber()) {
      num = latestId.getNumber<uint32_t>();
      latestIdBuilder.add(VPackValue(num));
      latestIdPrecondition.reset(new AgencyPrecondition(targetIdStr,
                  AgencyPrecondition::Type::VALUE, latestIdBuilder.slice()));
    } else {
      latestIdPrecondition.reset(new AgencyPrecondition(targetIdStr,
                  AgencyPrecondition::Type::EMPTY, true));
    }

    VPackBuilder localIdBuilder;
    {
      VPackObjectBuilder b(&localIdBuilder);
      localIdBuilder.add("TransactionID", VPackValue(num + 1));
      std::stringstream ss; // ShortName
      ss << roleToAgencyKey(role) << std::setw(4) << std::setfill('0') << num + 1;
      localIdBuilder.add("ShortName", VPackValue(ss.str()));
    }

    std::vector<AgencyOperation> operations;
    std::vector<AgencyPrecondition> preconditions;

    operations.push_back(
      AgencyOperation(targetIdStr, AgencySimpleOperationType::INCREMENT_OP)
    );
    operations.push_back(
      AgencyOperation(targetUrl, AgencyValueOperationType::SET, localIdBuilder.slice())
    );

    preconditions.push_back(*(latestIdPrecondition.get()));
    preconditions.push_back(
      AgencyPrecondition(targetUrl, AgencyPrecondition::Type::EMPTY, true)
    );

    AgencyWriteTransaction trx(operations, preconditions);
    result = comm.sendTransactionWithFailover(trx, 0.0);

    if (result.successful()) {
      return true;
    }
    sleep(1);
  }

  LOG_TOPIC(FATAL, Logger::STARTUP) << "Couldn't register shortname for " << id;
  return false;
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
  } else if (role == ROLE_COORDINATOR) {
    result = checkCoordinatorState(state);
  } else if (role == ROLE_SINGLE) {
    result = true;
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

void ServerState::forceRole(ServerState::RoleEnum role) {
  TRI_ASSERT(role != ServerState::ROLE_UNDEFINED);
  TRI_ASSERT(_role == ServerState::ROLE_UNDEFINED);

  auto expected = _role.load(std::memory_order_relaxed);

  while (true) {
    if (expected != ServerState::ROLE_UNDEFINED) {
      THROW_ARANGO_EXCEPTION_MESSAGE(TRI_ERROR_INTERNAL, "invalid role found");
    }
    if (_role.compare_exchange_weak(expected, role,
                                    std::memory_order_release,
                                    std::memory_order_relaxed)) {
      return;
    }
    expected = _role.load(std::memory_order_relaxed);
  }
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
  LOG_TOPIC(INFO, Logger::CLUSTER) << "Redetermined role from agency: "
                                   << role;
  if (role == ServerState::ROLE_UNDEFINED) {
    return false;
  }
  RoleEnum oldRole = loadRole();
  TRI_ASSERT(oldRole == ROLE_UNDEFINED || role == oldRole);
  if (role != oldRole) {
    LOG_TOPIC(INFO, Logger::CLUSTER) << "Changed role to: " << role;
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

  ServerState::RoleEnum role = ServerState::ROLE_UNDEFINED;// loadRole();
/*
  if (role == ServerState::ROLE_SINGLE) {
    // role of single server does never change
    return role;
  }
*/
  if (isInServerList(ServerState::ROLE_COORDINATOR, id)) {
    role = ServerState::ROLE_COORDINATOR;
  } else if (isInServerList(ServerState::ROLE_PRIMARY, id)) {
    role = ServerState::ROLE_PRIMARY;
  } else if (isInServerList(ServerState::ROLE_SINGLE, id)) {
    role = ServerState::ROLE_SINGLE;
  }
  // TODO: does not hold true anymore FIXME
  // TRI_ASSERT(role == loadRole());
  return role;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief validate a state transition for a primary server
////////////////////////////////////////////////////////////////////////////////

bool ServerState::checkPrimaryState(StateEnum state) {
  if (state == STATE_STARTUP) {
    // startup state can only be set once
    return (_state == STATE_UNDEFINED);
  } else if (state == STATE_SERVING) {
    return (_state == STATE_STARTUP ||  _state == STATE_STOPPED);
  } else if (state == STATE_STOPPING) {
    return _state == STATE_SERVING;
  } else if (state == STATE_STOPPED) {
    return (_state == STATE_STOPPING);
  } else if (state == STATE_SHUTDOWN) {
    return (_state == STATE_STARTUP || _state == STATE_STOPPED ||
            _state == STATE_SERVING);
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
/// @brief lookup the server role by scanning Plan/[DBServers] for our id
////////////////////////////////////////////////////////////////////////////////

bool ServerState::isInServerList(ServerState::RoleEnum role,
                                 std::string const& id) {
  // fetch value at Plan/DBServers
  // we need to do this to determine the server's role

  std::string agencyListKey = roleToAgencyListKey(role);
  std::string const key = "Plan/" + agencyListKey;

  AgencyComm comm;
  AgencyCommResult result = comm.getValues(key);
  if (!result.successful()) {
    std::string const endpoints = AgencyCommManager::MANAGER->endpointsString();
    LOG_TOPIC(TRACE, Logger::CLUSTER)
        << "Could not fetch configuration from agency endpoints (" << endpoints
        << "): got status code " << result._statusCode
        << ", message: " << result.errorMessage() << ", key: " << key;
    return false;
  }

  VPackSlice servers = result.slice()[0].get(std::vector<std::string>(
      {AgencyCommManager::path(), "Plan", agencyListKey}));

  // check if we can find ourselves in the list returned by the agency
  if (servers.isObject() && !servers.get(id).isNone()) {
    // we are in the list. this means we are a primary server
    return true;
  } else {
    LOG_TOPIC(TRACE, Logger::CLUSTER)
      << "Got an invalid JSON response for Plan/DBServers";
  }

  return false;
}

//////////////////////////////////////////////////////////////////////////////
/// @brief store the server role
//////////////////////////////////////////////////////////////////////////////

bool ServerState::storeRole(RoleEnum role) {
  if (AgencyCommManager::isEnabled()) {// isClusterRole(role)
    try {
      VPackBuilder builder;
      builder.add(VPackValue("none"));
    
      AgencyOperation op("Current/" + roleToAgencyListKey(role) + "/" + _id,
                       AgencyValueOperationType::SET, builder.slice());
      std::unique_ptr<AgencyTransaction> trx(new AgencyWriteTransaction(op));

      AgencyComm comm; // should not throw anything
      AgencyCommResult res = comm.sendTransactionWithFailover(*trx.get(), 0.0);
      if (!res.successful()) {
          return false;
      }
    } catch (...) {
      LOG_TOPIC(FATAL, arangodb::Logger::FIXME) << __FUNCTION__
        << " out of memory storing server role";
      FATAL_ERROR_EXIT();
    }
  }

  Logger::setRole(roleToString(role)[0]);
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

std::ostream& operator<<(std::ostream& stream, arangodb::ServerState::RoleEnum role) {
  stream << arangodb::ServerState::roleToString(role);
  return stream;
}
