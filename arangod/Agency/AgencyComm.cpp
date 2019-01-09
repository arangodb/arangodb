///////////////////////////////////////////////////////////////////////////////
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
/// @author Max Neunhoeffer
////////////////////////////////////////////////////////////////////////////////

#include "AgencyComm.h"
#include "ApplicationFeatures/ApplicationServer.h"
#include "RestServer/ServerFeature.h"

#include <thread>
#ifdef DEBUG_SYNC_REPLICATION
#include <atomic>
#endif

#include <velocypack/Iterator.h>
#include <velocypack/Sink.h>
#include <velocypack/velocypack-aliases.h>

#include "Basics/ReadLocker.h"
#include "Basics/StringBuffer.h"
#include "Basics/StringUtils.h"
#include "Basics/VelocyPackHelper.h"
#include "Basics/WriteLocker.h"
#include "Cluster/ClusterComm.h"
#include "Cluster/ServerState.h"
#include "Endpoint/Endpoint.h"
#include "GeneralServer/AuthenticationFeature.h"
#include "Logger/Logger.h"
#include "Random/RandomGenerator.h"
#include "Rest/HttpRequest.h"
#include "Rest/HttpResponse.h"
#include "SimpleHttpClient/GeneralClientConnection.h"
#include "SimpleHttpClient/SimpleHttpClient.h"
#include "SimpleHttpClient/SimpleHttpResult.h"

#include <boost/uuid/uuid.hpp>
#include <boost/uuid/uuid_generators.hpp>
#include <boost/uuid/uuid_io.hpp>

using namespace arangodb;
using namespace arangodb::application_features;
using namespace arangodb::httpclient;
using namespace arangodb::rest;

#ifdef DEBUG_SYNC_REPLICATION
static std::atomic<uint64_t> debugUniqId(1);
bool AgencyComm::syncReplDebug = false;
#endif

static void addEmptyVPackObject(std::string const& name, VPackBuilder& builder) {
  builder.add(VPackValue(name));
  VPackObjectBuilder c(&builder);
}

const std::vector<std::string> AgencyTransaction::TypeUrl(
    {"/read", "/write", "/transact", "/transient", "/config"});

// -----------------------------------------------------------------------------
// --SECTION--                                                AgencyPrecondition
// -----------------------------------------------------------------------------

AgencyPrecondition::AgencyPrecondition()
    : type(AgencyPrecondition::Type::NONE), empty(true) {}

AgencyPrecondition::AgencyPrecondition(std::string const& key, Type t, bool e)
    : key(AgencyCommManager::path(key)), type(t), empty(e) {}

AgencyPrecondition::AgencyPrecondition(std::string const& key, Type t, VPackSlice const& s)
    : key(AgencyCommManager::path(key)), type(t), empty(false), value(s) {}

void AgencyPrecondition::toVelocyPack(VPackBuilder& builder) const {
  if (type != AgencyPrecondition::Type::NONE) {
    builder.add(VPackValue(key));
    {
      VPackObjectBuilder preconditionDefinition(&builder);
      switch (type) {
        case AgencyPrecondition::Type::EMPTY:
          builder.add("oldEmpty", VPackValue(empty));
          break;
        case AgencyPrecondition::Type::VALUE:
          builder.add("old", value);
          break;
          // mop: useless compiler warning :S
        case AgencyPrecondition::Type::NONE:
          break;
      }
    }
  }
}

void AgencyPrecondition::toGeneralBuilder(VPackBuilder& builder) const {
  if (type != AgencyPrecondition::Type::NONE) {
    VPackObjectBuilder preconditionDefinition(&builder);
    builder.add(VPackValue(key));
    {
      VPackObjectBuilder preconditionDefinition(&builder);
      switch (type) {
        case AgencyPrecondition::Type::EMPTY:
          builder.add("oldEmpty", VPackValue(empty));
          break;
        case AgencyPrecondition::Type::VALUE:
          builder.add("old", value);
          break;
          // mop: useless compiler warning :S
        case AgencyPrecondition::Type::NONE:
          break;
      }
    }
  } else {
    VPackObjectBuilder guard(&builder);
  }
}

// -----------------------------------------------------------------------------
// --SECTION--                                                   AgencyOperation
// -----------------------------------------------------------------------------

AgencyOperation::AgencyOperation(std::string const& key)
    : _key(AgencyCommManager::path(key)), _opType() {
  _opType.type = AgencyOperationType::Type::READ;
}

AgencyOperation::AgencyOperation(std::string const& key, AgencySimpleOperationType opType)
    : _key(AgencyCommManager::path(key)), _opType() {
  _opType.type = AgencyOperationType::Type::SIMPLE;
  _opType.simple = opType;
}

AgencyOperation::AgencyOperation(std::string const& key,
                                 AgencyValueOperationType opType, VPackSlice value)
    : _key(AgencyCommManager::path(key)), _opType(), _value(value) {
  _opType.type = AgencyOperationType::Type::VALUE;
  _opType.value = opType;
}

AgencyOperation::AgencyOperation(std::string const& key, AgencyValueOperationType opType,
                                 VPackSlice newValue, VPackSlice oldValue)
    : _key(AgencyCommManager::path(key)), _opType(), _value(newValue), _value2(oldValue) {
  _opType.type = AgencyOperationType::Type::VALUE;
  _opType.value = opType;
}

AgencyOperationType AgencyOperation::type() const { return _opType; }

void AgencyOperation::toVelocyPack(VPackBuilder& builder) const {
  builder.add(VPackValue(_key));
  {
    VPackObjectBuilder valueOperation(&builder);
    builder.add("op", VPackValue(_opType.toString()));
    if (_opType.type == AgencyOperationType::Type::VALUE) {
      if (_opType.value == AgencyValueOperationType::OBSERVE ||
          _opType.value == AgencyValueOperationType::UNOBSERVE) {
        builder.add("url", _value);
      } else if (_opType.value == AgencyValueOperationType::ERASE) {
        builder.add("val", _value);
      } else if (_opType.value == AgencyValueOperationType::REPLACE) {
        builder.add("new", _value);
        builder.add("val", _value2);
      } else {
        builder.add("new", _value);
      }
      if (_ttl > 0) {
        builder.add("ttl", VPackValue(_ttl));
      }
    }
  }
}

void AgencyOperation::toGeneralBuilder(VPackBuilder& builder) const {
  if (_opType.type == AgencyOperationType::Type::READ) {
    builder.add(VPackValue(_key));
  } else {
    VPackObjectBuilder operation(&builder);
    builder.add(VPackValue(_key));
    VPackObjectBuilder valueOperation(&builder);
    builder.add("op", VPackValue(_opType.toString()));
    if (_opType.type == AgencyOperationType::Type::VALUE) {
      if (_opType.value == AgencyValueOperationType::OBSERVE ||
          _opType.value == AgencyValueOperationType::UNOBSERVE) {
        builder.add("url", _value);
      } else {
        builder.add("new", _value);
      }
      if (_ttl > 0) {
        builder.add("ttl", VPackValue(_ttl));
      }
    }
  }
}

// -----------------------------------------------------------------------------
// --SECTION--                                                 AgencyTransaction
// -----------------------------------------------------------------------------

std::string AgencyTransaction::toJson() const {
  VPackBuilder builder;
  builder.openArray();
  toVelocyPack(builder);
  builder.close();
  return builder.toJson();
}

// -----------------------------------------------------------------------------
// --SECTION--                                            AgencyWriteTransaction
// -----------------------------------------------------------------------------

void AgencyWriteTransaction::toVelocyPack(VPackBuilder& builder) const {
  VPackArrayBuilder guard(&builder);
  {
    VPackObjectBuilder guard2(&builder);  // Writes
    for (AgencyOperation const& operation : operations) {
      operation.toVelocyPack(builder);
    }
  }

  if (preconditions.size() > 0) {
    VPackObjectBuilder guard3(&builder);  // Preconditions
    for (AgencyPrecondition const& precondition : preconditions) {
      precondition.toVelocyPack(builder);
    }
  } else {
    VPackObjectBuilder guard3(&builder);
  }

  builder.add(VPackValue(clientId));  // Transactions
}

bool AgencyWriteTransaction::validate(AgencyCommResult const& result) const {
  return (result.slice().isObject() && result.slice().hasKey("results") &&
          result.slice().get("results").isArray());
}

// -----------------------------------------------------------------------------
// --SECTION-- AgencyTransientTransaction
// -----------------------------------------------------------------------------

void AgencyTransientTransaction::toVelocyPack(VPackBuilder& builder) const {
  VPackArrayBuilder guard(&builder);
  {
    VPackObjectBuilder guard2(&builder);
    for (AgencyOperation const& operation : operations) {
      operation.toVelocyPack(builder);
    }
  }
  if (preconditions.size() > 0) {
    VPackObjectBuilder guard3(&builder);
    for (AgencyPrecondition const& precondition : preconditions) {
      precondition.toVelocyPack(builder);
    }
  }
}

bool AgencyTransientTransaction::validate(AgencyCommResult const& result) const {
  return (result.slice().isArray() && result.slice().length() > 0 &&
          result.slice()[0].isBool() && result.slice()[0].getBool() == true);
}

// -----------------------------------------------------------------------------
// --SECTION--                                          AgencyGeneralTransaction
// -----------------------------------------------------------------------------
/*
void AgencyGeneralTransaction::toVelocyPack(VPackBuilder& builder) const {
  for (auto const& trx : transactions) {
    auto opers = std::get<0>(trx);
    auto precs = std::get<1>(trx);
    TRI_ASSERT(!opers.empty());
    if (!opers.empty()) {
      if (opers[0].type().type == AgencyOperationType::Type::READ) {
        for (auto const& op : opers) {
          VPackArrayBuilder guard(&builder);
          op.toGeneralBuilder(builder);
        }
      } else {
          VPackArrayBuilder guard(&builder);
        { VPackObjectBuilder o(&builder);  // Writes
          for (AgencyOperation const& oper : opers) {
            oper.toVelocyPack(builder);
          }}
        { VPackObjectBuilder p(&builder);  // Preconditions
          if (!precs.empty()) {
            for (AgencyPrecondition const& prec : precs) {
              prec.toVelocyPack(builder);
            }}}
        builder.add(VPackValue(clientId)); // Transactions
      }
    }
  }
}

void AgencyGeneralTransaction::push_back(AgencyOperation const& op) {
  transactions.emplace_back(
    TransactionType(std::vector<AgencyOperation>(1, op),
                    std::vector<AgencyPrecondition>(0)));
}

void AgencyGeneralTransaction::push_back(
  std::pair<AgencyOperation,AgencyPrecondition> const& oper) {
  transactions.emplace_back(
    TransactionType(std::vector<AgencyOperation>(1,oper.first),
                    std::vector<AgencyPrecondition>(1,oper.second)));
}

bool AgencyGeneralTransaction::validate(AgencyCommResult const& result) const {
  return (result.slice().isArray() &&
          result.slice().length() >= 1); // >= transactions.size()
}*/

// -----------------------------------------------------------------------------
// --SECTION--                                             AgencyReadTransaction
// -----------------------------------------------------------------------------

void AgencyReadTransaction::toVelocyPack(VPackBuilder& builder) const {
  VPackArrayBuilder guard2(&builder);
  for (std::string const& key : keys) {
    builder.add(VPackValue(key));
  }
}

bool AgencyReadTransaction::validate(AgencyCommResult const& result) const {
  return (result.slice().isArray() && result.slice().length() == 1);
}

// -----------------------------------------------------------------------------
// --SECTION--                                                  AgencyCommResult
// -----------------------------------------------------------------------------

AgencyCommResult::AgencyCommResult()
    : _location(), _message(), _body(), _values(), _statusCode(0), _connected(false), _sent(false) {}

AgencyCommResult::AgencyCommResult(int code, std::string const& message,
                                   std::string const& clientId)
    : _location(),
      _message(message),
      _body(),
      _values(),
      _statusCode(code),
      _connected(false),
      _sent(false) {}

void AgencyCommResult::set(int code, std::string const& message) {
  _message = message;
  _statusCode = code;
  _location.clear();
  _body.clear();
  _values.clear();
  _vpack.reset();
}

bool AgencyCommResult::connected() const { return _connected; }

int AgencyCommResult::httpCode() const { return _statusCode; }

bool AgencyCommResult::sent() const { return _sent; }

int AgencyCommResult::errorCode() const {
  try {
    if (!_body.empty()) {
      std::shared_ptr<VPackBuilder> bodyBuilder = VPackParser::fromJson(_body);
      VPackSlice body = bodyBuilder->slice();
      if (!body.isObject()) {
        return 0;
      }
      // get "errorCode" attribute (0 if not exist)
      return basics::VelocyPackHelper::getNumericValue<int>(body, "errorCode", 0);
    }
    return 0;
  } catch (VPackException const&) {
    return 0;
  }
}

std::string AgencyCommResult::errorMessage() const {
  if (!_message.empty()) {
    // return stored message first if set
    return _message;
  }

  if (!_connected) {
    return std::string("unable to connect to agency");
  }

  try {
    std::shared_ptr<VPackBuilder> bodyBuilder = VPackParser::fromJson(_body);

    VPackSlice body = bodyBuilder->slice();
    if (!body.isObject()) {
      return "";
    }
    // get "message" attribute ("" if not exist)
    return arangodb::basics::VelocyPackHelper::getStringValue(body, "message",
                                                              "");
  } catch (VPackException const& e) {
    std::string message("VPackException parsing body (" + _body + "): " + e.what());
    return std::string(message);
  }
}

std::string AgencyCommResult::errorDetails() const {
  std::string const errorMessage = this->errorMessage();

  if (errorMessage.empty()) {
    return _message;
  }

  return _message + " (" + errorMessage + ")";
}

void AgencyCommResult::clear() {
  // clear existing values. They free themselves
  _values.clear();

  _location = "";
  _message = "";
  _body = "";
  _vpack.reset();
  _statusCode = 0;
  _sent = false;
  _connected = false;
}

VPackSlice AgencyCommResult::slice() const { return _vpack->slice(); }

// -----------------------------------------------------------------------------
// --SECTION--                                                 AgencyCommManager
// -----------------------------------------------------------------------------

std::unique_ptr<AgencyCommManager> AgencyCommManager::MANAGER;

AgencyConnectionOptions AgencyCommManager::CONNECTION_OPTIONS(15.0, 120.0, 120.0, 100);

void AgencyCommManager::initialize(std::string const& prefix) {
  MANAGER.reset(new AgencyCommManager(prefix));
}

void AgencyCommManager::shutdown() { MANAGER.reset(); }

std::string AgencyCommManager::path() {
  if (MANAGER == nullptr) {
    return "";
  }

  return MANAGER->_prefix;
}

std::string AgencyCommManager::path(std::string const& p1) {
  if (MANAGER == nullptr) {
    return "";
  }

  return MANAGER->_prefix + "/" + basics::StringUtils::trim(p1, "/");
}

std::string AgencyCommManager::path(std::string const& p1, std::string const& p2) {
  if (MANAGER == nullptr) {
    return "";
  }

  return MANAGER->_prefix + "/" + basics::StringUtils::trim(p1, "/") + "/" +
         basics::StringUtils::trim(p2, "/");
}

std::vector<std::string> AgencyCommManager::slicePath(std::string const& p1) {
  std::string const p2 = basics::StringUtils::trim(p1, "/");
  std::vector<std::string> split = basics::StringUtils::split(p2, '/');
  if (split.size() > 0 && split[0] != AgencyCommManager::path()) {
    split.insert(split.begin(), AgencyCommManager::path());
  }
  return split;
}

std::string AgencyCommManager::generateStamp() {
  time_t tt = time(nullptr);
  struct tm tb;
  char buffer[21];

  TRI_gmtime(tt, &tb);

  size_t len = ::strftime(buffer, sizeof(buffer), "%Y-%m-%dT%H:%M:%SZ", &tb);

  return std::string(buffer, len);
}

bool AgencyCommManager::start() {
  AgencyComm comm;
  bool ok = comm.ensureStructureInitialized();

  LOG_TOPIC(DEBUG, Logger::AGENCYCOMM)
      << "structures " << (ok ? "are" : "failed to") << " initialize";

  return ok;
}

void AgencyCommManager::stop() {
  MUTEX_LOCKER(locker, _lock);

  for (auto& i : _unusedConnections) {
    i.second.clear();
  }
  _unusedConnections.clear();
}

std::unique_ptr<GeneralClientConnection> AgencyCommManager::acquire(std::string& endpoint) {
  std::unique_ptr<GeneralClientConnection> connection;

  MUTEX_LOCKER(locker, _lock);

  if (_endpoints.empty()) {
    return nullptr;
  } else {
    if (endpoint.empty()) {
      endpoint = _endpoints.front();
      LOG_TOPIC(DEBUG, Logger::AGENCYCOMM)
          << "Using endpoint " << endpoint << " for agency communication, full selection:";
    }
    if (!_unusedConnections[endpoint].empty()) {
      connection.reset(_unusedConnections[endpoint].back().release());
      _unusedConnections[endpoint].pop_back();
    } else {
      connection = createNewConnection();
    }
  }

  LOG_TOPIC(TRACE, Logger::AGENCYCOMM)
      << "acquiring agency connection '" << connection.get()
      << "' for endpoint '" << endpoint << "'";

  return connection;
}

void AgencyCommManager::release(std::unique_ptr<httpclient::GeneralClientConnection> connection,
                                std::string const& endpoint) {
  MUTEX_LOCKER(locker, _lock);
  releaseNonLocking(std::move(connection), endpoint);
}

void AgencyCommManager::releaseNonLocking(std::unique_ptr<httpclient::GeneralClientConnection> connection,
                                          std::string const& endpoint) {
  if (_endpoints.front() == endpoint) {
    LOG_TOPIC(TRACE, Logger::AGENCYCOMM)
        << "releasing agency connection '" << connection.get()
        << "', active endpoint '" << endpoint << "'";

  } else {
    LOG_TOPIC(TRACE, Logger::AGENCYCOMM)
        << "releasing agency connection '" << connection.get()
        << "', inactive endpoint '" << endpoint << "'";
  }

  _unusedConnections[endpoint].emplace_back(std::move(connection));
}

void AgencyCommManager::failed(std::unique_ptr<httpclient::GeneralClientConnection> connection,
                               std::string const& endpoint) {
  MUTEX_LOCKER(locker, _lock);
  failedNonLocking(std::move(connection), endpoint);
}

void AgencyCommManager::failedNonLocking(std::unique_ptr<httpclient::GeneralClientConnection> connection,
                                         std::string const& endpoint) {
  if (_endpoints.front() == endpoint) {
    LOG_TOPIC(TRACE, Logger::AGENCYCOMM)
        << "failed agency connection '" << connection.get()
        << "', active endpoint " << endpoint << "'";

  } else {
    LOG_TOPIC(TRACE, Logger::AGENCYCOMM)
        << "failed agency connection '" << connection.get()
        << "', inactive endpoint " << endpoint << "'";
  }

  switchCurrentEndpoint();
  // Note that we intentionally let the connection go out of scope, because
  // it failed, so we would have to reopen it anyway. We keep the other
  // unused connections to the same endpoint which might be in the cache.
}

std::string AgencyCommManager::redirect(std::unique_ptr<httpclient::GeneralClientConnection> connection,
                                        std::string const& endpoint,
                                        std::string const& location, std::string& url) {
  MUTEX_LOCKER(locker, _lock);

  std::string specification;
  size_t delim = std::string::npos;

  if (location.substr(0, 7) == "http://") {
    specification = "http+tcp://" + location.substr(7);
    delim = specification.find_first_of('/', 12);
  } else if (location.substr(0, 8) == "https://") {
    specification = "http+ssl://" + location.substr(8);
    delim = specification.find_first_of('/', 13);
  }

  // invalid location header
  if (delim == std::string::npos) {
    failedNonLocking(std::move(connection), endpoint);
    return "";
  }

  std::string rest = specification.substr(delim);
  specification = Endpoint::unifiedForm(specification.substr(0, delim));

  LOG_TOPIC(TRACE, Logger::AGENCYCOMM)
      << "redirect: location = " << location
      << ", specification = " << specification << ", url = " << rest;

  if (endpoint == specification) {
    LOG_TOPIC(DEBUG, Logger::AGENCYCOMM)
        << "got an agency redirect back to the old agency '" << endpoint << "'";
    failedNonLocking(std::move(connection), endpoint);
    return "";
  }

  // The following is useful if another thread learned about a redirect first.
  // In this case we simply release the connection and let the client acquire
  // another one for the (new) active endpoint.
  if (endpoint != _endpoints.front()) {
    LOG_TOPIC(DEBUG, Logger::AGENCYCOMM)
        << "ignoring an agency redirect to '" << specification
        << "' from inactive endpoint '" << endpoint << "'";
    releaseNonLocking(std::move(connection), endpoint);
    return "";
  }

  url = rest;

  // Move redirected endpoint to the front to make it active:
  _endpoints.erase(std::remove(_endpoints.begin(), _endpoints.end(), specification),
                   _endpoints.end());

  LOG_TOPIC(DEBUG, Logger::AGENCYCOMM) << "Got an agency redirect from '" << endpoint
                                       << "' to '" << specification << "'";

  _endpoints.push_front(specification);

  return specification;
}

void AgencyCommManager::addEndpoint(std::string const& endpoint) {
  MUTEX_LOCKER(locker, _lock);

  std::string normalized = Endpoint::unifiedForm(endpoint);
  auto iter = _endpoints.begin();

  for (; iter != _endpoints.end(); ++iter) {
    if (*iter == normalized) {
      break;
    }
  }

  if (iter == _endpoints.end()) {
    LOG_TOPIC(DEBUG, Logger::AGENCYCOMM) << "using agency endpoint '" << normalized << "'";
    _endpoints.emplace_back(normalized);
  }
}

void AgencyCommManager::updateEndpoints(std::vector<std::string> const& newEndpoints) {
  std::set<std::string> updatedSet;
  for (std::string const& endp : newEndpoints) {
    updatedSet.emplace(Endpoint::unifiedForm(endp));
  }

  MUTEX_LOCKER(locker, _lock);

  std::set<std::string> currentSet;
  currentSet.insert(_endpoints.begin(), _endpoints.end());

  std::set<std::string> toRemove;
  std::set_difference(currentSet.begin(), currentSet.end(), updatedSet.begin(),
                      updatedSet.end(), std::inserter(toRemove, toRemove.begin()));

  std::set<std::string> toAdd;
  std::set_difference(updatedSet.begin(), updatedSet.end(), currentSet.begin(),
                      currentSet.end(), std::inserter(toAdd, toAdd.begin()));

  for (std::string const& rem : toRemove) {
    LOG_TOPIC(INFO, Logger::AGENCYCOMM) << "Removing endpoint " << rem << " from agent pool";
    _endpoints.erase(std::remove(_endpoints.begin(), _endpoints.end(), rem),
                     _endpoints.end());
  }

  for (std::string const& add : toAdd) {
    LOG_TOPIC(INFO, Logger::AGENCYCOMM) << "Adding endpoint " << add << " to agent pool";
    _endpoints.emplace_back(add);
  }
}

std::string AgencyCommManager::endpointsString() const {
  return basics::StringUtils::join(endpoints(), ", ");
}

std::vector<std::string> AgencyCommManager::endpoints() const {
  std::vector<std::string> result;

  MUTEX_LOCKER(locker, _lock);
  result.insert(result.begin(), _endpoints.begin(), _endpoints.end());

  return result;
}

std::unique_ptr<GeneralClientConnection> AgencyCommManager::createNewConnection() {
  if (_endpoints.empty()) {
    LOG_TOPIC(TRACE, Logger::AGENCYCOMM)
        << "no agency endpoint is know, cannot create connection";

    return nullptr;
  }

  std::string const& spec = _endpoints.front();
  std::unique_ptr<Endpoint> endpoint(Endpoint::clientFactory(spec));
  if (endpoint.get() == nullptr) {
    LOG_TOPIC(ERR, arangodb::Logger::AGENCYCOMM)
        << "invalid value for "
        << "--server.endpoint ('" << spec << "')";
    THROW_ARANGO_EXCEPTION(TRI_ERROR_BAD_PARAMETER);
  }

  return std::unique_ptr<GeneralClientConnection>(
      GeneralClientConnection::factory(endpoint, CONNECTION_OPTIONS._requestTimeout,
                                       CONNECTION_OPTIONS._connectTimeout,
                                       CONNECTION_OPTIONS._connectRetries, 0));
}

void AgencyCommManager::switchCurrentEndpoint() {
  _lock.assertLockedByCurrentThread();
  if (_endpoints.empty()) {
    return;
  }

  std::string current = _endpoints.front();
  _endpoints.pop_front();
  _endpoints.push_back(current);

  LOG_TOPIC(TRACE, Logger::AGENCYCOMM)
      << "switching active agency endpoint from '" << current << "' to '"
      << _endpoints.front() << "'";
}

// -----------------------------------------------------------------------------
// --SECTION--                                                        AgencyComm
// -----------------------------------------------------------------------------

std::string const AgencyComm::AGENCY_URL_PREFIX = "/_api/agency";

AgencyCommResult AgencyComm::sendServerState(double ttl) {
  // construct JSON value { "status": "...", "time": "..." }
  VPackBuilder builder;

  try {
    builder.openObject();
    std::string const status =
        ServerState::stateToString(ServerState::instance()->getState());
    builder.add("status", VPackValue(status));
    std::string const stamp = AgencyCommManager::generateStamp();
    builder.add("time", VPackValue(stamp));
    builder.close();
  } catch (...) {
    return AgencyCommResult();
  }

  AgencyCommResult result(
      setTransient("Sync/ServerStates/" + ServerState::instance()->getId(),
                   builder.slice(), ttl));

  return result;
}

std::string AgencyComm::version() {
  AgencyCommResult result =
      sendWithFailover(arangodb::rest::RequestType::GET,
                       AgencyCommManager::CONNECTION_OPTIONS._requestTimeout,
                       "/_api/version", VPackSlice::noneSlice());

  if (result.successful()) {
    return result._body;
  }

  return "";
}

AgencyCommResult AgencyComm::createDirectory(std::string const& key) {
  VPackBuilder builder;
  { VPackObjectBuilder dir(&builder); }

  AgencyOperation operation(key, AgencyValueOperationType::SET, builder.slice());
  AgencyWriteTransaction transaction(operation);

  return sendTransactionWithFailover(transaction);
}

AgencyCommResult AgencyComm::setValue(std::string const& key,
                                      std::string const& value, double ttl) {
  VPackBuilder builder;
  builder.add(VPackValue(value));

  AgencyOperation operation(key, AgencyValueOperationType::SET, builder.slice());
  operation._ttl = static_cast<uint64_t>(ttl);
  AgencyWriteTransaction transaction(operation);

  return sendTransactionWithFailover(transaction);
}

AgencyCommResult AgencyComm::setValue(std::string const& key,
                                      arangodb::velocypack::Slice const& slice, double ttl) {
  AgencyOperation operation(key, AgencyValueOperationType::SET, slice);
  operation._ttl = static_cast<uint64_t>(ttl);
  AgencyWriteTransaction transaction(operation);

  return sendTransactionWithFailover(transaction);
}

AgencyCommResult AgencyComm::setTransient(std::string const& key,
                                          arangodb::velocypack::Slice const& slice,
                                          double ttl) {
  AgencyOperation operation(key, AgencyValueOperationType::SET, slice);
  operation._ttl = static_cast<uint64_t>(ttl);
  AgencyTransientTransaction transaction(operation);

  return sendTransactionWithFailover(transaction);
}

bool AgencyComm::exists(std::string const& key) {
  AgencyCommResult result = getValues(key);

  if (!result.successful()) {
    return false;
  }

  auto parts = arangodb::basics::StringUtils::split(key, "/");
  std::vector<std::string> allParts;
  allParts.reserve(parts.size() + 1);
  allParts.push_back(AgencyCommManager::path());
  allParts.insert(allParts.end(), parts.begin(), parts.end());
  VPackSlice slice = result.slice()[0].get(allParts);
  return !slice.isNone();
}

AgencyCommResult AgencyComm::getValues(std::string const& key) {
  std::string url = AgencyComm::AGENCY_URL_PREFIX + "/read";

  VPackBuilder builder;
  {
    VPackArrayBuilder root(&builder);
    {
      VPackArrayBuilder keys(&builder);
      builder.add(VPackValue(AgencyCommManager::path(key)));
    }
  }

  AgencyCommResult result =
      sendWithFailover(arangodb::rest::RequestType::POST,
                       AgencyCommManager::CONNECTION_OPTIONS._requestTimeout,
                       url, builder.slice());

  if (!result.successful()) {
    return result;
  }

  try {
    result.setVPack(VPackParser::fromJson(result.bodyRef()));

    if (!result.slice().isArray()) {
      result.set(500, "got invalid result structure for getValues response");
      return result;
    }

    if (result.slice().length() != 1) {
      result.set(500,
                 "got invalid result structure length for getValues response");
      return result;
    }

    result._body.clear();
    result._statusCode = 200;

  } catch (std::exception const& e) {
    LOG_TOPIC(ERR, Logger::AGENCYCOMM) << "Error transforming result: " << e.what();
    result.clear();
  } catch (...) {
    LOG_TOPIC(ERR, Logger::AGENCYCOMM)
        << "Error transforming result: out of memory";
    result.clear();
  }

  return result;
}

AgencyCommResult AgencyComm::removeValues(std::string const& key, bool recursive) {
  AgencyWriteTransaction transaction(AgencyOperation(key, AgencySimpleOperationType::DELETE_OP));

  return sendTransactionWithFailover(transaction);
}

AgencyCommResult AgencyComm::increment(std::string const& key) {
  AgencyWriteTransaction transaction(
      AgencyOperation(key, AgencySimpleOperationType::INCREMENT_OP));

  return sendTransactionWithFailover(transaction);
}

AgencyCommResult AgencyComm::casValue(std::string const& key,
                                      arangodb::velocypack::Slice const& json,
                                      bool prevExist, double ttl, double timeout) {
  VPackBuilder newBuilder;
  newBuilder.add(json);

  AgencyOperation operation(key, AgencyValueOperationType::SET, newBuilder.slice());
  AgencyPrecondition precondition(key, AgencyPrecondition::Type::EMPTY, !prevExist);
  if (ttl >= 0.0) {
    operation._ttl = static_cast<uint64_t>(ttl);
  }

  VPackBuilder preBuilder;
  precondition.toVelocyPack(preBuilder);

  AgencyWriteTransaction transaction(operation, precondition);
  return sendTransactionWithFailover(transaction, timeout);
}

AgencyCommResult AgencyComm::casValue(std::string const& key, VPackSlice const& oldJson,
                                      VPackSlice const& newJson, double ttl, double timeout) {
  VPackBuilder newBuilder;
  newBuilder.add(newJson);

  VPackBuilder oldBuilder;
  oldBuilder.add(oldJson);

  AgencyOperation operation(key, AgencyValueOperationType::SET, newBuilder.slice());
  AgencyPrecondition precondition(key, AgencyPrecondition::Type::VALUE,
                                  oldBuilder.slice());
  if (ttl >= 0.0) {
    operation._ttl = static_cast<uint64_t>(ttl);
  }

  AgencyWriteTransaction transaction(operation, precondition);
  return sendTransactionWithFailover(transaction, timeout);
}

uint64_t AgencyComm::uniqid(uint64_t count, double timeout) {
#ifdef DEBUG_SYNC_REPLICATION
  if (AgencyComm::syncReplDebug == true) {
    return debugUniqId++;
  }
#endif
  static int const maxTries = 1000000;
  // this is pretty much forever, but we simply cannot continue at all
  // if we do not get a unique id from the agency.
  int tries = 0;

  AgencyCommResult result;

  uint64_t oldValue = 0;

  while (tries++ < maxTries) {
    result = getValues("Sync/LatestID");
    if (!result.successful()) {
      std::this_thread::sleep_for(std::chrono::microseconds(500000));
      continue;
    }

    VPackSlice oldSlice = result.slice()[0].get(std::vector<std::string>(
        {AgencyCommManager::path(), "Sync", "LatestID"}));

    if (!(oldSlice.isSmallInt() || oldSlice.isUInt())) {
      LOG_TOPIC(WARN, Logger::AGENCYCOMM)
          << "Sync/LatestID in agency is not an unsigned integer, fixing...";
      try {
        VPackBuilder builder;
        builder.add(VPackValue(0));

        // create the key on the fly
        setValue("Sync/LatestID", builder.slice(), 0.0);

      } catch (...) {
        // Could not build local key. Try again
      }
      continue;
    }

    // If we get here, slice is pointing to an unsigned integer, which
    // is the value in the agency.
    oldValue = 0;
    try {
      oldValue = oldSlice.getUInt();
    } catch (...) {
    }
    uint64_t const newValue = oldValue + count;

    VPackBuilder newBuilder;
    try {
      newBuilder.add(VPackValue(newValue));
    } catch (...) {
      std::this_thread::sleep_for(std::chrono::microseconds(500000));
      continue;
    }

    result = casValue("Sync/LatestID", oldSlice, newBuilder.slice(), 0.0, timeout);

    if (result.successful()) {
      break;
    }
    // The cas did not work, simply try again!
  }

  return oldValue;
}

AgencyCommResult AgencyComm::registerCallback(std::string const& key,
                                              std::string const& endpoint) {
  VPackBuilder builder;
  builder.add(VPackValue(endpoint));

  AgencyOperation operation(key, AgencyValueOperationType::OBSERVE, builder.slice());
  AgencyWriteTransaction transaction(operation);
  AgencyCommResult res;
  for (size_t i = 0; i < 3; ++i) {
    res = sendTransactionWithFailover(transaction);
    if (res.successful()) {
      return res;
    }
  }
  return res;
}

AgencyCommResult AgencyComm::unregisterCallback(std::string const& key,
                                                std::string const& endpoint) {
  VPackBuilder builder;
  builder.add(VPackValue(endpoint));

  AgencyOperation operation(key, AgencyValueOperationType::UNOBSERVE, builder.slice());
  AgencyWriteTransaction transaction(operation);

  return sendTransactionWithFailover(transaction);
}

bool AgencyComm::lockRead(std::string const& key, double ttl, double timeout) {
  VPackBuilder builder;
  try {
    builder.add(VPackValue("READ"));
  } catch (...) {
    return false;
  }
  return lock(key, ttl, timeout, builder.slice());
}

bool AgencyComm::lockWrite(std::string const& key, double ttl, double timeout) {
  VPackBuilder builder;
  try {
    builder.add(VPackValue("WRITE"));
  } catch (...) {
    return false;
  }
  return lock(key, ttl, timeout, builder.slice());
}

bool AgencyComm::unlockRead(std::string const& key, double timeout) {
  VPackBuilder builder;
  try {
    builder.add(VPackValue("READ"));
  } catch (...) {
    return false;
  }
  return unlock(key, builder.slice(), timeout);
}

bool AgencyComm::unlockWrite(std::string const& key, double timeout) {
  VPackBuilder builder;
  try {
    builder.add(VPackValue("WRITE"));
  } catch (...) {
    return false;
  }
  return unlock(key, builder.slice(), timeout);
}

AgencyCommResult AgencyComm::sendTransactionWithFailover(AgencyTransaction const& transaction,
                                                         double timeout) {
  std::string url = AgencyComm::AGENCY_URL_PREFIX;

  url += transaction.path();

  VPackBuilder builder;
  {
    VPackArrayBuilder guard(&builder);
    transaction.toVelocyPack(builder);
  }

  LOG_TOPIC(TRACE, Logger::AGENCYCOMM)
      << "sending " << builder.toJson() << "'" << url << "'";

  AgencyCommResult result =
      sendWithFailover(arangodb::rest::RequestType::POST,
                       (timeout == 0.0) ? AgencyCommManager::CONNECTION_OPTIONS._requestTimeout
                                        : timeout,
                       url, builder.slice());

  if (!result.successful() &&
      result.httpCode() != (int)arangodb::rest::ResponseCode::PRECONDITION_FAILED) {
    return result;
  }

  try {
    result.setVPack(VPackParser::fromJson(result.bodyRef()));

    if (!transaction.validate(result)) {
      result.set(500, std::string("validation failed for response to URL " + url));
      LOG_TOPIC(DEBUG, Logger::AGENCYCOMM)
          << "validation failed for url: " << url
          << ", type: " << transaction.typeName()
          << ", sent: " << builder.toJson() << ", received: " << result.bodyRef();
      return result;
    }

    result._body.clear();

  } catch (std::exception const& e) {
    LOG_TOPIC(ERR, Logger::AGENCYCOMM)
        << "Error transforming result: " << e.what()
        << ", status code: " << result._statusCode
        << ", incriminating body: " << result.bodyRef() << ", url: " << url
        << ", timeout: " << timeout << ", data sent: " << builder.toJson();
    result.clear();
  } catch (...) {
    LOG_TOPIC(ERR, Logger::AGENCYCOMM)
        << "Error transforming result: out of memory";
    result.clear();
  }

  return result;
}

bool AgencyComm::ensureStructureInitialized() {
  LOG_TOPIC(TRACE, Logger::AGENCYCOMM) << "checking if agency is initialized";

  while (true) {
    while (shouldInitializeStructure()) {
      LOG_TOPIC(TRACE, Logger::AGENCYCOMM)
          << "Agency is fresh. Needs initial structure.";
      // mop: we are the chosen one .. great success

      if (tryInitializeStructure()) {
        LOG_TOPIC(TRACE, Logger::AGENCYCOMM)
            << "Successfully initialized agency";
        break;
      }

      LOG_TOPIC(WARN, Logger::AGENCYCOMM)
          << "Initializing agency failed. We'll try again soon";
      // We should really have exclusive access, here, this is strange!
      std::this_thread::sleep_for(std::chrono::seconds(1));
    }

    AgencyCommResult result = getValues("InitDone");

    if (result.successful()) {
      VPackSlice value = result.slice()[0].get(
          std::vector<std::string>({AgencyCommManager::path(), "InitDone"}));
      if (value.isBoolean() && value.getBoolean()) {
        // expecting a value of "true"
        LOG_TOPIC(TRACE, Logger::AGENCYCOMM) << "Found an initialized agency";
        break;
      }
    } else {
      if (result.httpCode() == 401) {
        // unauthorized
        LOG_TOPIC(FATAL, Logger::STARTUP) << "Unauthorized. Wrong credentials.";
        FATAL_ERROR_EXIT();
      }
    }

    LOG_TOPIC(TRACE, Logger::AGENCYCOMM)
        << "Waiting for agency to get initialized";

    std::this_thread::sleep_for(std::chrono::seconds(1));
  }

  return true;
}

bool AgencyComm::lock(std::string const& key, double ttl, double timeout,
                      VPackSlice const& slice) {
  if (ttl == 0.0) {
    ttl = AgencyCommManager::CONNECTION_OPTIONS._lockTimeout;
  }

  if (timeout == 0.0) {
    timeout = AgencyCommManager::CONNECTION_OPTIONS._lockTimeout;
  }
  unsigned long sleepTime = INITIAL_SLEEP_TIME;
  double const end = TRI_microtime() + timeout;

  VPackBuilder builder;
  try {
    builder.add(VPackValue("UNLOCKED"));
  } catch (...) {
    return false;
  }
  VPackSlice oldSlice = builder.slice();

  while (true) {
    AgencyCommResult result = casValue(key + "/Lock", oldSlice, slice, ttl, timeout);

    if (!result.successful() &&
        result.httpCode() == (int)arangodb::rest::ResponseCode::PRECONDITION_FAILED) {
      // key does not yet exist. create it now
      result = casValue(key + "/Lock", slice, false, ttl, timeout);
    }

    if (result.successful()) {
      return true;
    }

    std::this_thread::sleep_for(std::chrono::microseconds(sleepTime));

    if (sleepTime < MAX_SLEEP_TIME) {
      sleepTime += INITIAL_SLEEP_TIME;
    }

    if (TRI_microtime() >= end) {
      return false;
    }
  }

  TRI_ASSERT(false);
  return false;
}

bool AgencyComm::unlock(std::string const& key, VPackSlice const& slice, double timeout) {
  if (timeout == 0.0) {
    timeout = AgencyCommManager::CONNECTION_OPTIONS._lockTimeout;
  }

  unsigned long sleepTime = INITIAL_SLEEP_TIME;
  double const end = TRI_microtime() + timeout;

  VPackBuilder builder;
  try {
    builder.add(VPackValue("UNLOCKED"));
  } catch (...) {
    // Out of Memory
    return false;
  }
  VPackSlice newSlice = builder.slice();

  while (true) {
    AgencyCommResult result = casValue(key + "/Lock", slice, newSlice, 0.0, timeout);

    if (result.successful()) {
      return true;
    }

    std::this_thread::sleep_for(std::chrono::microseconds(sleepTime));

    if (sleepTime < MAX_SLEEP_TIME) {
      sleepTime += INITIAL_SLEEP_TIME;
    }

    if (TRI_microtime() >= end) {
      return false;
    }
  }

  TRI_ASSERT(false);
  return false;
}

AgencyCommResult AgencyComm::sendWithFailover(arangodb::rest::RequestType method,
                                              double const timeout,
                                              std::string const& initialUrl,
                                              VPackSlice inBody) {
  std::string endpoint;
  std::unique_ptr<GeneralClientConnection> connection =
      AgencyCommManager::MANAGER->acquire(endpoint);

  std::vector<std::string> clientIds;
  VPackSlice body = inBody.resolveExternals();

  if (body.isArray()) {
    // In the writing case we want to find all transactions with client IDs
    // and remember these IDs:
    for (auto const& query : VPackArrayIterator(body)) {
      if (query.isArray() && query.length() == 3 && query[0].isObject() &&
          query[2].isString()) {
        clientIds.push_back(query[2].copyString());
      }
    }
  }
  AgencyCommResult result;
  std::string url;

  std::chrono::duration<double> waitInterval(.0);  // seconds
  auto started = std::chrono::steady_clock::now();
  auto timeOut = started + std::chrono::duration<double>(timeout);
  double conTimeout = 1.0;

  int tries = 0;

  auto waitSomeTime = [&waitInterval, &result]() -> bool {
    // Returning true means timeout because of shutdown:
    auto serverFeature = application_features::ApplicationServer::getFeature<ServerFeature>(
        "Server");
    if (serverFeature->isStopping() ||
        !application_features::ApplicationServer::isRetryOK()) {
      LOG_TOPIC(INFO, Logger::AGENCYCOMM)
          << "Unsuccessful AgencyComm: Timeout because of shutdown "
          << "errorCode: " << result.errorCode()
          << " errorMessage: " << result.errorMessage()
          << " errorDetails: " << result.errorDetails();
      return true;
    }

    // Raise waits to a maximum 10 seconds
    auto waitUntil = std::chrono::steady_clock::now() + waitInterval;

    std::this_thread::sleep_until(waitUntil);
    if (waitInterval.count() == 0.0) {
      waitInterval = std::chrono::duration<double>(0.25);
    } else if (waitInterval.count() < 5.0) {
      waitInterval *= 1.0749292929292;
    }
    return false;
  };

  bool isInquiry = false;  // Set to true whilst we investigate a potentially
                           // failed transaction.
  static std::string const writeURL{"/_api/agency/write"};
  bool isWriteTrans = (initialUrl == writeURL);

  while (true) {  // will be left by timeout eventually
    // If for some reason we did not find an agency endpoint, we bail out:
    if (connection == nullptr) {
      LOG_TOPIC(ERR, Logger::AGENCYCOMM) << "No agency endpoints.";
      result.set(400, "No endpoints for agency found.");
      break;
    }

    // From second loop on delay a bit:
    if (tries++ > 0) {
      if (waitSomeTime()) {
        break;  // timeout because of shutdown
      }
    }

    // timeout exit strategy:
    if (std::chrono::steady_clock::now() > timeOut) {
      result.set(0, "timeout in AgencyComm operation");
      break;
    }

    double elapsed =
        1.e-2 * (std::round(1.e+2 * std::chrono::duration<double>(
                                        std::chrono::steady_clock::now() - started)
                                        .count()));

    // Some reporting:
    if (tries > 20) {
      auto serverState = application_features::ApplicationServer::server->state();
      application_features::ApplicationServer::getFeature<ServerFeature>(
          "Server");
      std::string serverStateStr;
      switch (serverState) {
        case arangodb::application_features::ServerState::UNINITIALIZED:
        case arangodb::application_features::ServerState::IN_COLLECT_OPTIONS:
        case arangodb::application_features::ServerState::IN_VALIDATE_OPTIONS:
        case arangodb::application_features::ServerState::IN_PREPARE:
        case arangodb::application_features::ServerState::IN_START:
          serverStateStr = "in startup";
          break;
        case arangodb::application_features::ServerState::IN_WAIT:
          serverStateStr = "running";
          break;
        case arangodb::application_features::ServerState::IN_STOP:
        case arangodb::application_features::ServerState::IN_UNPREPARE:
        case arangodb::application_features::ServerState::STOPPED:
        case arangodb::application_features::ServerState::ABORT:
          serverStateStr = "in shutdown";
      }
      LOG_TOPIC(INFO, Logger::AGENCYCOMM)
          << "Flaky agency communication to " << endpoint
          << ". Unsuccessful consecutive tries: " << tries << " (" << elapsed
          << "s). Network checks advised."
          << " Server " << serverStateStr << ".";
    }

    if (1 < tries) {
      LOG_TOPIC(DEBUG, Logger::AGENCYCOMM)
          << "Retrying agency communication at '" << endpoint
          << ". Unsuccessful consecutive tries: " << tries << " (" << elapsed
          << "s). Network checks advised.";
    }

    if (!isInquiry) {
      // try to send transaction; if we fail completely, retry
      try {
        std::string bodyString;
        if (!body.isNone()) {
          bodyString = body.toJson();
        }
        url = initialUrl;  // Attention: overwritten by redirect below!
        result = send(connection.get(), method, conTimeout, url, bodyString);
#ifdef ARANGODB_ENABLE_MAINTAINER_MODE
        if (!clientIds.empty()) {
          if (clientIds[0] == "INTEGRATION_TEST_INQUIRY_ERROR_0") {
            result._statusCode = 0;
          } else if (clientIds[0] == "INTEGRATION_TEST_INQUIRY_ERROR_503") {
            result._statusCode = 503;
          }
        }
#endif
      } catch (...) {
        // Rotate to new agent endpoint:
        AgencyCommManager::MANAGER->failed(std::move(connection), endpoint);
        endpoint.clear();
        connection = AgencyCommManager::MANAGER->acquire(endpoint);
        continue;
      }

      // got a result or shutdown, we are done
      if (result.successful() || application_features::ApplicationServer::isStopping()) {
        AgencyCommManager::MANAGER->release(std::move(connection), endpoint);
        break;
      }

      // do not retry on client errors
      if (result._statusCode >= 400 && result._statusCode <= 499) {
        AgencyCommManager::MANAGER->release(std::move(connection), endpoint);
        break;
      }

      // Note that in case of a timeout or 503 we do not know whether the
      // operation has been executed. In this case we should inquire about
      // the operation. If it actually was done, we are good. If not, we
      // can retry. If in doubt, we have to retry inquire until the global
      // timeout is reached.
      // Also note that we only inquire about WriteTransactions.
      if (isWriteTrans && !clientIds.empty() && result._sent &&
          (result._statusCode == 0 || result._statusCode == 503)) {
        isInquiry = true;
      }

      // This leaves the redirect, timeout and 503 cases, which are handled
      // below.
    } else {
      // isInquiry case, we are getting here because a previous try to send
      // the transaction lead to isInquiry == true because we got a timeout
      // or a 503.
      VPackBuilder b;
      {
        VPackArrayBuilder ab(&b);
        for (auto const& i : clientIds) {
          b.add(VPackValue(i));
        }
      }

      LOG_TOPIC(DEBUG, Logger::AGENCYCOMM)
          << "Failed agency comm (" << result._statusCode << ")! "
          << "Inquiring about clientIds " << clientIds << ".";

      url = "/_api/agency/inquire";  // attention: overwritten by redirect!
      result = send(connection.get(), method, conTimeout, url, b.toJson());

      // Inquire returns a body like write, if the transactions are not known,
      // the list of results is empty.
      // _statusCode can be 200 or 404 (if nothing was found)
      if (result.successful()) {
        std::shared_ptr<VPackBuilder> resultBody = VPackParser::fromJson(result._body);
        VPackSlice outer = resultBody->slice();

        // If we get an answer, and it contains a "results" key,
        // we release the connection and break out of the loop letting the
        // inquiry result go to the client. Otherwise try again.
        if (outer.isObject() && outer.hasKey("results")) {
          VPackSlice results = outer.get("results");
          if (results.length() > 0) {
            LOG_TOPIC(DEBUG, Logger::AGENCYCOMM) << "Inquired " << resultBody->toJson();
            AgencyCommManager::MANAGER->release(std::move(connection), endpoint);
            break;
          } else {
            // Nothing known, so do a retry of the original operation:
            LOG_TOPIC(DEBUG, Logger::AGENCYCOMM)
                << "Nothing known, do a retry.";
            isInquiry = false;
            continue;
          }
        } else {
          // How odd, we are supposed to get at least {results=[...]}, let's
          // retry...
          isInquiry = false;
          continue;
        }
      } else if (result._statusCode == 404) {
        // clientId was not found, agency has never heard of this trx, retry:
        isInquiry = false;
        continue;
      }
      // This can still be a timeout or 503 case, both are handled below
    }  // end of inquiry case

    // Now do some stuff which is needed in both cases:

    // sometimes the agency will return a 307 (temporary redirect)
    // in this case we have to pick it up and use the new location returned
    if (result._statusCode == (int)arangodb::rest::ResponseCode::TEMPORARY_REDIRECT) {
      // Note that this may overwrite url, but we do not care.
      endpoint = AgencyCommManager::MANAGER->redirect(std::move(connection), endpoint,
                                                      result._location, url);
      connection = AgencyCommManager::MANAGER->acquire(endpoint);
      waitInterval = std::chrono::duration<double>(0.0);
      continue;
    }

    // In case of a timeout, we increase the patience:
    if (result._statusCode == 0) {
      if (conTimeout < 15.0) {  // double until we have 16s
        conTimeout *= 2;
      }
    }

    if (result._statusCode == 0 || result._statusCode == 503) {
      // Rotate to new agent endpoint:
      AgencyCommManager::MANAGER->failed(std::move(connection), endpoint);
      endpoint.clear();
      connection = AgencyCommManager::MANAGER->acquire(endpoint);
    }
  }

  // Log error
  if (!result.successful() && result.httpCode() != 412) {
    LOG_TOPIC(DEBUG, Logger::AGENCYCOMM)
        << "Unsuccessful AgencyComm: "
        << "errorCode: " << result.errorCode()
        << " errorMessage: " << result.errorMessage()
        << " errorDetails: " << result.errorDetails();
  }

  return result;
}

AgencyCommResult AgencyComm::send(arangodb::httpclient::GeneralClientConnection* connection,
                                  arangodb::rest::RequestType method, double timeout,
                                  std::string const& url, std::string const& body) {
  TRI_ASSERT(connection != nullptr);

  if (method == arangodb::rest::RequestType::GET ||
      method == arangodb::rest::RequestType::HEAD ||
      method == arangodb::rest::RequestType::DELETE_REQ) {
    TRI_ASSERT(body.empty());
  }

  TRI_ASSERT(!url.empty());

  AgencyCommResult result;

  LOG_TOPIC(TRACE, Logger::AGENCYCOMM)
      << "sending " << arangodb::HttpRequest::translateMethod(method)
      << " request to agency at endpoint '"
      << connection->getEndpoint()->specification() << "', url '" << url
      << "': " << body;

  arangodb::httpclient::SimpleHttpClientParams params(timeout, false);
  AuthenticationFeature* af = AuthenticationFeature::instance();
  TRI_ASSERT(af != nullptr);
  params.setJwt(af->tokenCache().jwtToken());
  params.keepConnectionOnDestruction(true);
  arangodb::httpclient::SimpleHttpClient client(connection, params);

  // set up headers
  std::unordered_map<std::string, std::string> headers;

  if (method == arangodb::rest::RequestType::POST) {
    // the agency needs this content-type for the body
    headers["content-type"] = "application/json";
  }

  // send the actual request
  std::unique_ptr<arangodb::httpclient::SimpleHttpResult> response(
      client.request(method, url, body.c_str(), body.size(), headers));

  if (response == nullptr) {
    result._message = "could not send request to agency";
    LOG_TOPIC(TRACE, Logger::AGENCYCOMM) << "could not send request to agency";

    return result;
  }

  result._sent = response->haveSentRequestFully();

  if (!response->isComplete()) {
    result._message = "sending request to agency failed";
    LOG_TOPIC(TRACE, Logger::AGENCYCOMM) << "sending request to agency failed";

    return result;
  }

  result._connected = true;

  if (response->getHttpReturnCode() == (int)arangodb::rest::ResponseCode::TEMPORARY_REDIRECT) {
    // temporary redirect. now save location header

    bool found = false;
    result._location = response->getHeaderField(StaticStrings::Location, found);

    LOG_TOPIC(TRACE, Logger::AGENCYCOMM)
        << "redirecting to location: '" << result._location << "'";

    if (!found) {
      // a 307 without a location header does not make any sense
      result._message = "invalid agency response (header missing)";

      return result;
    }
  }

  result._message = response->getHttpReturnMessage();
  result._statusCode = response->getHttpReturnCode();

  basics::StringBuffer& sb = response->getBody();
  result._body = std::string(sb.c_str(), sb.length());

  LOG_TOPIC(TRACE, Logger::AGENCYCOMM)
      << "request to agency returned status code " << result._statusCode
      << ", message: '" << result._message << "', body: '" << result._body << "'";

  return result;
}

bool AgencyComm::tryInitializeStructure() {
  VPackBuilder builder;

  try {
    VPackObjectBuilder b(&builder);

    builder.add(  // Cluster Id --------------------------
        "Cluster", VPackValue(to_string(boost::uuids::random_generator()())));

    builder.add(VPackValue("Agency"));  // Agency ------------------------------
    {
      VPackObjectBuilder a(&builder);
      builder.add("Definition", VPackValue(1));
    }

    builder.add(VPackValue("Current"));  // Current ----------------------------
    {
      VPackObjectBuilder c(&builder);
      addEmptyVPackObject("AsyncReplication", builder);
      builder.add(VPackValue("Collections"));
      {
        VPackObjectBuilder d(&builder);
        addEmptyVPackObject("_system", builder);
      }
      builder.add("Version", VPackValue(1));
      addEmptyVPackObject("ShardsCopied", builder);
      addEmptyVPackObject("NewServers", builder);
      addEmptyVPackObject("Coordinators", builder);
      builder.add("Lock", VPackValue("UNLOCKED"));
      addEmptyVPackObject("DBServers", builder);
      addEmptyVPackObject("Singles", builder);
      builder.add(VPackValue("ServersRegistered"));
      {
        VPackObjectBuilder c(&builder);
        builder.add("Version", VPackValue(1));
      }
      addEmptyVPackObject("Databases", builder);
    }

    builder.add("InitDone", VPackValue(true));  // InitDone

    builder.add(VPackValue("Plan"));  // Plan ----------------------------------
    {
      VPackObjectBuilder c(&builder);
      addEmptyVPackObject("AsyncReplication", builder);
      addEmptyVPackObject("Coordinators", builder);
      builder.add(VPackValue("Databases"));
      {
        VPackObjectBuilder d(&builder);
        builder.add(VPackValue("_system"));
        {
          VPackObjectBuilder d2(&builder);
          builder.add("name", VPackValue("_system"));
          builder.add("id", VPackValue("1"));
        }
      }
      builder.add("Lock", VPackValue("UNLOCKED"));
      addEmptyVPackObject("DBServers", builder);
      addEmptyVPackObject("Singles", builder);
      builder.add("Version", VPackValue(1));
      builder.add(VPackValue("Collections"));
      {
        VPackObjectBuilder d(&builder);
        addEmptyVPackObject("_system", builder);
      }
      builder.add(VPackValue("Views"));
      {
        VPackObjectBuilder d(&builder);
        addEmptyVPackObject("_system", builder);
      }
    }

    builder.add(VPackValue("Sync"));  // Sync ----------------------------------
    {
      VPackObjectBuilder c(&builder);
      builder.add("LatestID", VPackValue(1));
      builder.add("UserVersion", VPackValue(1));
      builder.add("HeartbeatIntervalMs", VPackValue(1000));
    }

    builder.add(VPackValue("Supervision"));  // Supervision --------------------
    {
      VPackObjectBuilder c(&builder);
      addEmptyVPackObject("Health", builder);
      addEmptyVPackObject("Shards", builder);
      addEmptyVPackObject("DBServers", builder);
    }

    builder.add(VPackValue("Target"));  // Target ------------------------------
    {
      VPackObjectBuilder c(&builder);
      builder.add("NumberOfCoordinators", VPackSlice::nullSlice());
      builder.add("NumberOfDBServers", VPackSlice::nullSlice());
      builder.add(VPackValue("CleanedServers"));
      { VPackArrayBuilder dd(&builder); }
      builder.add(VPackValue("FailedServers"));
      { VPackObjectBuilder dd(&builder); }
      builder.add("Lock", VPackValue("UNLOCKED"));
      // MapLocalToID is not used for anything since 3.4. It was used in
      // previous versions to store server ids from --cluster.my-local-info that
      // were mapped to server UUIDs
      addEmptyVPackObject("MapLocalToID", builder);
      addEmptyVPackObject("Failed", builder);
      addEmptyVPackObject("Finished", builder);
      addEmptyVPackObject("Pending", builder);
      addEmptyVPackObject("ToDo", builder);
      builder.add("Version", VPackValue(1));
    }

  } catch (std::exception const& e) {
    LOG_TOPIC(ERR, Logger::AGENCYCOMM)
        << "Couldn't create initializing structure " << e.what();
    return false;
  } catch (...) {
    LOG_TOPIC(ERR, Logger::AGENCYCOMM)
        << "Couldn't create initializing structure";
    return false;
  }

  try {
    LOG_TOPIC(TRACE, Logger::AGENCYCOMM)
        << "Initializing agency with " << builder.toJson();

    AgencyOperation initOperation("", AgencyValueOperationType::SET, builder.slice());

    AgencyWriteTransaction initTransaction;
    initTransaction.operations.push_back(initOperation);

    AgencyCommResult result = sendTransactionWithFailover(initTransaction);
    if (result.httpCode() == TRI_ERROR_HTTP_UNAUTHORIZED) {
      LOG_TOPIC(ERR, Logger::AUTHENTICATION)
          << "Cannot authenticate with agency,"
          << " check value of --server.jwt-secret";
    }

    return result.successful();
  } catch (std::exception const& e) {
    LOG_TOPIC(FATAL, Logger::AGENCYCOMM)
        << "Fatal error initializing agency " << e.what();
    FATAL_ERROR_EXIT();
  } catch (...) {
    LOG_TOPIC(FATAL, Logger::AGENCYCOMM) << "Fatal error initializing agency";
    FATAL_ERROR_EXIT();
  }
}

bool AgencyComm::shouldInitializeStructure() {
  VPackBuilder builder;
  builder.add(VPackValue(false));

  // "InitDone" key should not previously exist
  auto result = casValue("InitDone", builder.slice(), false, 60.0,
                         AgencyCommManager::CONNECTION_OPTIONS._requestTimeout);

  if (!result.successful()) {
    // somebody else has or is initializing the agency
    LOG_TOPIC(TRACE, Logger::AGENCYCOMM)
        << "someone else is initializing the agency";
    return false;
  }

  return true;
}
