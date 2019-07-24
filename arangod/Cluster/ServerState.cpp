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
/// @author Jan Steemann
////////////////////////////////////////////////////////////////////////////////

#include "ServerState.h"

#include <algorithm>
#include <iomanip>

#include <boost/uuid/uuid.hpp>
#include <boost/uuid/uuid_generators.hpp>
#include <boost/uuid/uuid_io.hpp>

#include "Agency/AgencyComm.h"
#include "Agency/AgencyStrings.h"
#include "ApplicationFeatures/ApplicationServer.h"
#include "Basics/FileUtils.h"
#include "Basics/ReadLocker.h"
#include "Basics/VelocyPackHelper.h"
#include "Basics/WriteLocker.h"
#include "Cluster/ClusterInfo.h"
#include "Logger/Logger.h"
#include "Rest/Version.h"
#include "RestServer/DatabaseFeature.h"
#include "RestServer/DatabasePathFeature.h"
#include "StorageEngine/EngineSelectorFeature.h"
#include "VocBase/ticks.h"

using namespace arangodb;
using namespace arangodb::basics;
using namespace arangodb::consensus;

////////////////////////////////////////////////////////////////////////////////
/// @brief single instance of ServerState - will live as long as the server is
/// running
////////////////////////////////////////////////////////////////////////////////

static ServerState Instance;

ServerState::ServerState()
    : _role(RoleEnum::ROLE_UNDEFINED),
      _lock(),
      _id(),
      _shortId(0),
      _javaScriptStartupPath(),
      _myEndpoint(),
      _advertisedEndpoint(),
      _host(),
      _state(STATE_UNDEFINED),
      _initialized(false),
      _foxxmaster(),
      _foxxmasterQueueupdate(false) {
  setRole(ROLE_UNDEFINED);
}

void ServerState::findHost(std::string const& fallback) {
  // Compute a string identifying the host on which we are running, note
  // that this is more complicated than immediately obvious, because we
  // could sit in a container which is deployed by Kubernetes or Mesos or
  // some other orchestration framework:

  // the following is set by Mesos or by an administrator:
  char* p = getenv("HOST");
  if (p != nullptr) {
    _host = p;
    return;
  }

  // the following is set by Kubernetes when using the downward API:
  p = getenv("NODE_NAME");
  if (p != nullptr) {
    _host = p;
    return;
  }

  // Now look at the contents of the file /etc/machine-id, if it exists:
  std::string name = "/etc/machine-id";
  if (arangodb::basics::FileUtils::exists(name)) {
    try {
      _host = arangodb::basics::FileUtils::slurp(name);
      while (!_host.empty() &&
             (_host.back() == '\r' || _host.back() == '\n' || _host.back() == ' ')) {
        _host.erase(_host.size() - 1);
      }
      if (!_host.empty()) {
        return;
      }
    } catch (...) {
    }
  }

#ifdef __APPLE__
  static_assert(sizeof(uuid_t) == 16, "");
  uuid_t localUuid;
  struct timespec timeout;
  timeout.tv_sec = 5;
  timeout.tv_nsec = 0;
  int res = gethostuuid(localUuid, &timeout);
  if (res == 0) {
    _host = StringUtils::encodeHex(reinterpret_cast<char*>(localUuid), sizeof(uuid_t));
    return;
  }
#endif

  // Finally, as a last resort, take the fallback, coming from
  // the ClusterFeature with the value of --cluster.my-address
  // or by the AgencyFeature with the value of --agency.my-address:
  _host = fallback;
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
    case ROLE_DBSERVER:
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
    case ROLE_DBSERVER:
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
  } else if (value == "PRIMARY" || value == "DBSERVER") {
    // note: DBSERVER is an alias for PRIMARY
    // internally and in all API values returned we will still use PRIMARY
    // for compatibility reasons
    return ROLE_DBSERVER;
  } else if (value == "COORDINATOR") {
    return ROLE_COORDINATOR;
  } else if (value == "AGENT") {
    return ROLE_AGENT;
  }

  return ROLE_UNDEFINED;
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
/// @brief convert a mode to string
////////////////////////////////////////////////////////////////////////////////

std::string ServerState::modeToString(Mode mode) {
  switch (mode) {
    case Mode::DEFAULT:
      return "default";
    case Mode::MAINTENANCE:
      return "maintenance";
    case Mode::TRYAGAIN:
      return "tryagain";
    case Mode::REDIRECT:
      return "redirect";
    case Mode::INVALID:
      return "invalid";
  }

  TRI_ASSERT(false);
  return "";
}

////////////////////////////////////////////////////////////////////////////////
/// @brief convert string to mode
////////////////////////////////////////////////////////////////////////////////

ServerState::Mode ServerState::stringToMode(std::string const& value) {
  if (value == "default") {
    return Mode::DEFAULT;
  } else if (value == "maintenance") {
    return Mode::MAINTENANCE;
  } else if (value == "tryagain") {
    return Mode::TRYAGAIN;
  } else if (value == "redirect") {
    return Mode::REDIRECT;
  } else {
    return Mode::INVALID;
  }
}

////////////////////////////////////////////////////////////////////////////////
/// @brief current server mode
////////////////////////////////////////////////////////////////////////////////
static std::atomic<ServerState::Mode> _serverstate_mode(ServerState::Mode::DEFAULT);

////////////////////////////////////////////////////////////////////////////////
/// @brief atomically load current server mode
////////////////////////////////////////////////////////////////////////////////
ServerState::Mode ServerState::mode() {
  return _serverstate_mode.load(std::memory_order_acquire);
}

////////////////////////////////////////////////////////////////////////////////
/// @brief change server mode, returns previously set mode
////////////////////////////////////////////////////////////////////////////////
ServerState::Mode ServerState::setServerMode(ServerState::Mode value) {
  //_serverMode.store(value, std::memory_order_release);
  if (_serverstate_mode.load(std::memory_order_acquire) != value) {
    return _serverstate_mode.exchange(value, std::memory_order_release);
  }
  return value;
}

static std::atomic<bool> _serverstate_readonly(false);

bool ServerState::readOnly() {
  return _serverstate_readonly.load(std::memory_order_acquire);
}

/// @brief set server read-only
bool ServerState::setReadOnly(bool ro) {
  return _serverstate_readonly.exchange(ro, std::memory_order_release);
}

// ============ Instance methods =================

/// @brief unregister this server with the agency
bool ServerState::unregister() {
  TRI_ASSERT(!getId().empty());
  TRI_ASSERT(AgencyCommManager::isEnabled());

  std::string const& id = getId();
  std::vector<AgencyOperation> operations;
  const std::string agencyListKey = roleToAgencyListKey(loadRole());

  operations.push_back(AgencyOperation(concatPath({PLAN, agencyListKey, id}),
                                       AgencySimpleOperationType::DELETE_OP));
  operations.push_back(AgencyOperation(concatPath({CURRENT, agencyListKey, id}),
                                       AgencySimpleOperationType::DELETE_OP));
  operations.push_back(AgencyOperation(PLAN_VERSION, AgencySimpleOperationType::INCREMENT_OP));
  operations.push_back(AgencyOperation(CURRENT_VERSION,
                                       AgencySimpleOperationType::INCREMENT_OP));

  AgencyWriteTransaction unregisterTransaction(operations);
  AgencyComm comm;
  AgencyCommResult r = comm.sendTransactionWithFailover(unregisterTransaction);
  return r.successful();
}

////////////////////////////////////////////////////////////////////////////////
/// @brief try to integrate into a cluster
////////////////////////////////////////////////////////////////////////////////

bool ServerState::integrateIntoCluster(ServerState::RoleEnum role,
                                       std::string const& myEndpoint,
                                       std::string const& advEndpoint) {
  WRITE_LOCKER(writeLocker, _lock);

  AgencyComm comm;
  if (!checkEngineEquality(comm)) {
    LOG_TOPIC("1e2da", FATAL, arangodb::Logger::ENGINES)
        << "the usage of different storage engines in the "
        << "cluster is unsupported and may cause issues";
    return false;
  }

  std::string id;
  if (!hasPersistedId()) {
    id = generatePersistedId(role);

    LOG_TOPIC("0d924", INFO, Logger::CLUSTER) << "Fresh start. Persisting new UUID " << id;
  } else {
    id = getPersistedId();
    LOG_TOPIC("db3ce", DEBUG, Logger::CLUSTER) << "Restarting with persisted UUID " << id;
  }
  setId(id);
  _myEndpoint = myEndpoint;
  _advertisedEndpoint = advEndpoint;
  TRI_ASSERT(!_myEndpoint.empty());

  if (!registerAtAgencyPhase1(comm, role)) {
    return false;
  }

  Logger::setRole(roleToString(role)[0]);
  _role.store(role, std::memory_order_release);

  LOG_TOPIC("61a39", DEBUG, Logger::CLUSTER) << "We successfully announced ourselves as "
                                    << roleToString(role) << " and our id is " << id;

  // now overwrite the entry in /Current/ServersRegistered/<myId>
  return registerAtAgencyPhase2(comm);
}

//////////////////////////////////////////////////////////////////////////////
/// @brief get the key for a role in the agency
//////////////////////////////////////////////////////////////////////////////
std::string ServerState::roleToAgencyListKey(ServerState::RoleEnum role) {
  return roleToAgencyKey(role) + "s";
}

std::string ServerState::roleToAgencyKey(ServerState::RoleEnum role) {
  switch (role) {
    case ROLE_DBSERVER:
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

std::string ServerState::getUuidFilename() {
  auto dbpath = application_features::ApplicationServer::getFeature<DatabasePathFeature>(
      "DatabasePath");
  TRI_ASSERT(dbpath != nullptr);
  return FileUtils::buildFilename(dbpath->directory(), "UUID");
}

bool ServerState::hasPersistedId() {
  std::string uuidFilename = getUuidFilename();
  return FileUtils::exists(uuidFilename);
}

bool ServerState::writePersistedId(std::string const& id) {
  std::string uuidFilename = getUuidFilename();
  // try to create underlying directory
  int error;
  FileUtils::createDirectory(FileUtils::dirname(uuidFilename), &error);

  try {
    arangodb::basics::FileUtils::spit(uuidFilename, id, true);
  } catch (arangodb::basics::Exception const& ex) {
    LOG_TOPIC("f2f70", FATAL, arangodb::Logger::FIXME)
        << "Cannot write UUID file '" << uuidFilename << "': " << ex.what();
    FATAL_ERROR_EXIT();
  }

  return true;
}

std::string ServerState::generatePersistedId(RoleEnum const& role) {
  std::string id =
      roleToShortString(role) + "-" + to_string(boost::uuids::random_generator()());
  writePersistedId(id);
  return id;
}

std::string ServerState::getPersistedId() {
  std::string uuidFilename = getUuidFilename();
  if (hasPersistedId()) {
    try {
      auto uuidBuf = arangodb::basics::FileUtils::slurp(uuidFilename);
      basics::StringUtils::trimInPlace(uuidBuf);
      if (!uuidBuf.empty()) {
        return uuidBuf;
      }
    } catch (arangodb::basics::Exception const& ex) {
      LOG_TOPIC("8dd60", FATAL, arangodb::Logger::CLUSTER)
          << "Couldn't read UUID file '" << uuidFilename << "' - " << ex.what();
      FATAL_ERROR_EXIT();
    }
  }

  LOG_TOPIC("b3923", FATAL, Logger::STARTUP) << "Couldn't open UUID file '" << uuidFilename << "'";
  FATAL_ERROR_EXIT();
}

/// @brief check equality of engines with other registered servers
bool ServerState::checkEngineEquality(AgencyComm& comm) {
  std::string engineName = EngineSelectorFeature::engineName();

  AgencyCommResult result = comm.getValues(concatPath({CURRENT, SERVERS_REGISTERED}));
  if (result.successful()) {  // no error if we cannot reach agency directly
    VPackSlice servers = result.slice()[0].get(std::vector<std::string>({ARANGO, CURRENT, SERVERS_REGISTERED}));
    if (!servers.isObject()) {
      return true;  // do not do anything harsh here
    }

    for (VPackObjectIterator::ObjectPair pair : VPackObjectIterator(servers)) {
      if (pair.value.isObject()) {
        VPackSlice engineStr = pair.value.get("engine");
        if (engineStr.isString() && !engineStr.isEqualString(engineName)) {
          return false;
        }
      }
    }
  }

  return true;
}

bool ServerState::checkIfAgencyInitialized(AgencyComm& comm,
                                           ServerState::RoleEnum const& role) {
  std::string const agencyListKey = roleToAgencyListKey(role);

  AgencyCommResult result = comm.getValues(concatPath({PLAN, agencyListKey}));
  if (!result.successful()) {
    LOG_TOPIC("0f327", FATAL, Logger::STARTUP)
        << "Couldn't fetch " << PLAN << "/" << agencyListKey << " from agency. "
        << " Agency is not initialized?";
    return false;
  }

  VPackSlice servers = result.slice()[0].get(
      std::vector<std::string>({AgencyCommManager::path(), PLAN, agencyListKey}));
  if (!servers.isObject()) {
    LOG_TOPIC("6507f", FATAL, Logger::STARTUP)
        << PLAN << "/" << agencyListKey << " in agency is not an object. "
        << "Agency not initialized?";
    return false;
  }

  return true;
}

//////////////////////////////////////////////////////////////////////////////
/// @brief create an id for a specified role
//////////////////////////////////////////////////////////////////////////////

bool ServerState::registerAtAgencyPhase1(AgencyComm& comm, const ServerState::RoleEnum& role) {
  std::string const agencyListKey = roleToAgencyListKey(role);
  std::string const latestIdKey = "Latest" + roleToAgencyKey(role) + "Id";

  VPackBuilder builder;
  builder.add(VPackValue("none"));

  std::string planUrl = concatPath({PLAN, agencyListKey,  _id});
  std::string currentUrl = concatPath({CURRENT, agencyListKey, _id});

  AgencyWriteTransaction preg(
      {AgencyOperation(planUrl, AgencyValueOperationType::SET, builder.slice()),
       AgencyOperation(PLAN_VERSION, AgencySimpleOperationType::INCREMENT_OP)},
      AgencyPrecondition(planUrl, AgencyPrecondition::Type::EMPTY, true));
  // ok to fail..if it failed we are already registered
  comm.sendTransactionWithFailover(preg, 0.0);
  AgencyWriteTransaction creg(
      {AgencyOperation(currentUrl, AgencyValueOperationType::SET, builder.slice()),
       AgencyOperation(CURRENT_VERSION, AgencySimpleOperationType::INCREMENT_OP)},
      AgencyPrecondition(currentUrl, AgencyPrecondition::Type::EMPTY, true));
  // ok to fail..if it failed we are already registered
  AgencyCommResult res = comm.sendTransactionWithFailover(creg, 0.0);

  // coordinator is already/still registered from an previous unclean shutdown;
  // must establish a new short ID
  bool forceChangeShortId = isCoordinator(role);

  std::string targetIdPath = concatPath({TARGET, latestIdKey});
  std::string targetUrl = concatPath({TARGET, MAP_UNIQUE_TO_SHORT_ID, _id});

  size_t attempts{0};
  while (attempts++ < 300) {
    AgencyReadTransaction readValueTrx(
        std::vector<std::string>{AgencyCommManager::path(targetIdPath),
                                 AgencyCommManager::path(targetUrl)});
    AgencyCommResult result = comm.sendTransactionWithFailover(readValueTrx, 0.0);

    if (!result.successful()) {
      LOG_TOPIC("8d5ff", WARN, Logger::CLUSTER)
          << "Couldn't fetch " << targetIdPath << " and " << targetUrl;
      std::this_thread::sleep_for(std::chrono::seconds(1));
      continue;
    }

    VPackSlice mapSlice = result.slice()[0].get(std::vector<std::string>(
        {AgencyCommManager::path(), TARGET, MAP_UNIQUE_TO_SHORT_ID, _id}));

    // already registered
    if (!mapSlice.isNone() && !forceChangeShortId) {
      VPackSlice s = mapSlice.get("TransactionID");
      if (s.isNumber()) {
        uint32_t shortId = s.getNumericValue<uint32_t>();
        setShortId(shortId);
        LOG_TOPIC("c6fb2", DEBUG, Logger::CLUSTER) << "restored short id " << shortId << " from agency";
      } else {
        LOG_TOPIC("13c13", WARN, Logger::CLUSTER)
            << "unable to restore short id from agency";
      }
      return true;
    }

    VPackSlice latestIdSlice = result.slice()[0].get(
        std::vector<std::string>({AgencyCommManager::path(), TARGET, latestIdKey}));

    uint32_t num = 0;
    std::unique_ptr<AgencyPrecondition> latestIdPrecondition;
    VPackBuilder latestIdBuilder;
    if (latestIdSlice.isNumber()) {
      num = latestIdSlice.getNumber<uint32_t>();
      latestIdBuilder.add(VPackValue(num));
      latestIdPrecondition.reset(
          new AgencyPrecondition(targetIdPath, AgencyPrecondition::Type::VALUE,
                                 latestIdBuilder.slice()));
    } else {
      latestIdPrecondition.reset(
          new AgencyPrecondition(targetIdPath, AgencyPrecondition::Type::EMPTY, true));
    }

    VPackBuilder localIdBuilder;
    {
      VPackObjectBuilder b(&localIdBuilder);
      localIdBuilder.add("TransactionID", VPackValue(num + 1));
      std::stringstream ss;  // ShortName
      size_t width = std::max(std::to_string(num + 1).size(), static_cast<size_t>(4));
      ss << roleToAgencyKey(role) << std::setw(width) << std::setfill('0') << num + 1;
      localIdBuilder.add("ShortName", VPackValue(ss.str()));
    }

    std::vector<AgencyOperation> operations;
    std::vector<AgencyPrecondition> preconditions;

    operations.push_back(AgencyOperation(targetIdPath, AgencySimpleOperationType::INCREMENT_OP));
    operations.push_back(AgencyOperation(targetUrl, AgencyValueOperationType::SET,
                                         localIdBuilder.slice()));

    preconditions.push_back(*(latestIdPrecondition.get()));
    preconditions.push_back(AgencyPrecondition(targetUrl, AgencyPrecondition::Type::EMPTY,
                                               mapSlice.isNone()));

    AgencyWriteTransaction trx(operations, preconditions);
    result = comm.sendTransactionWithFailover(trx, 0.0);

    if (result.successful()) {
      setShortId(num + 1);  // save short ID for generating server-specific ticks
      return true;
    }

    std::this_thread::sleep_for(std::chrono::seconds(1));
  }

  LOG_TOPIC("309d7", FATAL, Logger::STARTUP) << "Couldn't register shortname for " << _id;
  return false;
}

bool ServerState::registerAtAgencyPhase2(AgencyComm& comm) {
  TRI_ASSERT(!_id.empty() && !_myEndpoint.empty());

  while (!application_features::ApplicationServer::isStopping()) {
    VPackBuilder builder;
    try {
      VPackObjectBuilder b(&builder);
      builder.add("endpoint", VPackValue(_myEndpoint));
      builder.add("advertisedEndpoint", VPackValue(_advertisedEndpoint));
      builder.add("host", VPackValue(getHost()));
      builder.add("version", VPackValue(rest::Version::getNumericServerVersion()));
      builder.add("versionString", VPackValue(rest::Version::getServerVersion()));
      builder.add("engine", VPackValue(EngineSelectorFeature::engineName()));
    } catch (...) {
      LOG_TOPIC("de625", FATAL, arangodb::Logger::CLUSTER) << "out of memory";
      FATAL_ERROR_EXIT();
    }

    auto result = comm.setValue(concatPath({CURRENT, SERVERS_REGISTERED, _id}), builder.slice(), 0.0);

    if (result.successful()) {
      return true;
    } else {
      LOG_TOPIC("ba205", WARN, arangodb::Logger::CLUSTER)
          << "failed to register server in agency: http code: " << result.httpCode()
          << ", body: '" << result.body() << "', retrying ...";
    }

    std::this_thread::sleep_for(std::chrono::seconds(1));
  }

  return false;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief set the server role
////////////////////////////////////////////////////////////////////////////////

void ServerState::setRole(ServerState::RoleEnum role) {
  Logger::setRole(roleToString(role)[0]);
  _role.store(role, std::memory_order_release);
}

////////////////////////////////////////////////////////////////////////////////
/// @brief get the server id
////////////////////////////////////////////////////////////////////////////////

std::string ServerState::getId() const {
  std::lock_guard<std::mutex> guard(_idLock);
  return _id;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief set the server id
////////////////////////////////////////////////////////////////////////////////

void ServerState::setId(std::string const& id) {
  if (id.empty()) {
    return;
  }

  std::lock_guard<std::mutex> guard(_idLock);
  _id = id;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief get the short server id
////////////////////////////////////////////////////////////////////////////////

uint32_t ServerState::getShortId() {
  return _shortId.load(std::memory_order_relaxed);
}

////////////////////////////////////////////////////////////////////////////////
/// @brief set the short server id
////////////////////////////////////////////////////////////////////////////////

void ServerState::setShortId(uint32_t id) {
  if (id == 0) {
    return;
  }

  _shortId.store(id, std::memory_order_relaxed);
}

////////////////////////////////////////////////////////////////////////////////
/// @brief get the server address
////////////////////////////////////////////////////////////////////////////////

std::string ServerState::getEndpoint() {
  READ_LOCKER(readLocker, _lock);
  return _myEndpoint;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief get the server advertised endpoint
////////////////////////////////////////////////////////////////////////////////

std::string ServerState::getAdvertisedEndpoint() {
  READ_LOCKER(readLocker, _lock);
  return _advertisedEndpoint;
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

  WRITE_LOCKER(writeLocker, _lock);

  if (state == _state) {
    return;
  }

  auto role = getRole();
  if (role == ROLE_DBSERVER) {
    result = checkPrimaryState(state);
  } else if (role == ROLE_COORDINATOR) {
    result = checkCoordinatorState(state);
  } else if (role == ROLE_SINGLE) {
    result = true;
  }

  if (result) {
    LOG_TOPIC("bd5f1", DEBUG, Logger::CLUSTER)
        << "changing state of " << ServerState::roleToString(role)
        << " server from " << ServerState::stateToString(_state) << " to "
        << ServerState::stateToString(state);

    _state = state;
  } else {
    LOG_TOPIC("fb1f9", ERR, Logger::CLUSTER)
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

////////////////////////////////////////////////////////////////////////////////
/// @brief validate a state transition for a primary server
////////////////////////////////////////////////////////////////////////////////

bool ServerState::checkPrimaryState(StateEnum state) {
  if (state == STATE_STARTUP) {
    // startup state can only be set once
    return (_state == STATE_UNDEFINED);
  } else if (state == STATE_SERVING) {
    return (_state == STATE_STARTUP || _state == STATE_STOPPED);
  } else if (state == STATE_STOPPING) {
    return _state == STATE_SERVING;
  } else if (state == STATE_STOPPED) {
    return (_state == STATE_STOPPING);
  } else if (state == STATE_SHUTDOWN) {
    return (_state == STATE_STARTUP || _state == STATE_STOPPED || _state == STATE_SERVING);
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

bool ServerState::isFoxxmaster() const {
  return /*!isRunningInCluster() ||*/ _foxxmaster == getId();
}

std::string const& ServerState::getFoxxmaster() { return _foxxmaster; }

void ServerState::setFoxxmaster(std::string const& foxxmaster) {
  if (_foxxmaster != foxxmaster) {
    setFoxxmasterQueueupdate(true);
    _foxxmaster = foxxmaster;

    // We're the new foxxmaster, set this once.
    if (isFoxxmaster()) {
      setFoxxmasterSinceNow();
    }
  }
}

bool ServerState::getFoxxmasterQueueupdate() const noexcept {
  return _foxxmasterQueueupdate;
}

void ServerState::setFoxxmasterQueueupdate(bool value) {
  _foxxmasterQueueupdate = value;
}

TRI_voc_tick_t ServerState::getFoxxmasterSince() const noexcept {
  return _foxxmasterSince;
}

void ServerState::setFoxxmasterSinceNow() {
  ServerState::_foxxmasterSince = TRI_HybridLogicalClock();
}

std::ostream& operator<<(std::ostream& stream, arangodb::ServerState::RoleEnum role) {
  stream << arangodb::ServerState::roleToString(role);
  return stream;
}

Result ServerState::propagateClusterReadOnly(bool mode) {
  // Agency enabled will work for single server replication as well as cluster
  if (AgencyCommManager::isEnabled()) {
    std::vector<AgencyOperation> operations;
    VPackBuilder builder;
    builder.add(VPackValue(mode));
    operations.push_back(AgencyOperation("Readonly", AgencyValueOperationType::SET,
                                         builder.slice()));

    AgencyWriteTransaction readonlyMode(operations);
    AgencyComm comm;
    AgencyCommResult r = comm.sendTransactionWithFailover(readonlyMode);
    if (!r.successful()) {
      return Result(TRI_ERROR_CLUSTER_AGENCY_COMMUNICATION_FAILED, r.errorMessage());
    }
    // This is propagated to all servers via the heartbeat, which happens
    // once per second. So to ensure that every server has taken note of
    // the change, we delay here for 2 seconds.
    std::this_thread::sleep_for(std::chrono::seconds(2));
  }
  setReadOnly(mode);
  return Result();
}
