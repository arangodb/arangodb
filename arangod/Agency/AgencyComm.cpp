////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2014-2020 ArangoDB GmbH, Cologne, Germany
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

#include "Agency/AgencyPaths.h"
#include "ApplicationFeatures/ApplicationServer.h"
#include "Basics/application-exit.h"
#include "RestServer/ServerFeature.h"

#include <memory>
#include <thread>

#include <velocypack/Iterator.h>
#include <velocypack/velocypack-aliases.h>
#include <set>

#include "Agency/AsyncAgencyComm.h"
#include "Basics/MutexLocker.h"
#include "Basics/ReadLocker.h"
#include "Basics/StaticStrings.h"
#include "Basics/StringBuffer.h"
#include "Basics/StringUtils.h"
#include "Basics/VelocyPackHelper.h"
#include "Basics/WriteLocker.h"
#include "Basics/ScopeGuard.h"
#include "Basics/system-functions.h"
#include "Cluster/ServerState.h"
#include "Endpoint/Endpoint.h"
#include "GeneralServer/AuthenticationFeature.h"
#include "Logger/Logger.h"
#include "Random/RandomGenerator.h"
#include "Rest/GeneralRequest.h"
#include "RestServer/MetricsFeature.h"

#include <boost/uuid/uuid.hpp>
#include <boost/uuid/uuid_generators.hpp>
#include <boost/uuid/uuid_io.hpp>

using namespace arangodb;
using namespace arangodb::application_features;
using namespace arangodb::rest;

static void addEmptyVPackObject(std::string const& name, VPackBuilder& builder) {
  builder.add(name, VPackSlice::emptyObjectSlice());
}

static std::string const writeURL{"/_api/agency/write"};

const std::vector<std::string> AgencyTransaction::TypeUrl({"/read", "/write",
                                                           "/transact",
                                                           "/transient"});
  
// -----------------------------------------------------------------------------
// --SECTION--                                                AgencyPrecondition
// -----------------------------------------------------------------------------

AgencyPrecondition::AgencyPrecondition()
    : type(AgencyPrecondition::Type::NONE), empty(true) {}

AgencyPrecondition::AgencyPrecondition(std::string const& key, Type t, bool e)
    : key(AgencyCommHelper::path(key)), type(t), empty(e) {}

AgencyPrecondition::AgencyPrecondition(std::string const& key, Type t, VPackSlice const& s)
    : key(AgencyCommHelper::path(key)), type(t), empty(false), value(s) {}

AgencyPrecondition::AgencyPrecondition(std::shared_ptr<cluster::paths::Path const> const& path,
                                       Type t, const velocypack::Slice& s)
    : key(path->str()), type(t), empty(false), value(s) {}

AgencyPrecondition::AgencyPrecondition(std::shared_ptr<cluster::paths::Path const> const& path,
                                       Type t, bool e)
    : key(path->str()), type(t), empty(e) {}

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
        case AgencyPrecondition::Type::TIN:
          builder.add("in", value);
          break;
        case AgencyPrecondition::Type::NOTIN:
          builder.add("notin", value);
          break;
        case AgencyPrecondition::Type::INTERSECTION_EMPTY:
          builder.add("intersectionEmpty", value);
          break;
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
      VPackObjectBuilder preconditionDefinitionGuard(&builder);
      switch (type) {
        case AgencyPrecondition::Type::EMPTY:
          builder.add("oldEmpty", VPackValue(empty));
          break;
        case AgencyPrecondition::Type::VALUE:
          builder.add("old", value);
          break;
        case AgencyPrecondition::Type::TIN:
          builder.add("in", value);
          break;
        case AgencyPrecondition::Type::NOTIN:
          builder.add("notin", value);
          break;
        case AgencyPrecondition::Type::INTERSECTION_EMPTY:
          builder.add("intersectionEmpty", value);
          break;
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
    : _key(AgencyCommHelper::path(key)), _opType() {
  _opType.type = AgencyOperationType::Type::READ;
}

AgencyOperation::AgencyOperation(std::string const& key, AgencySimpleOperationType opType)
    : _key(AgencyCommHelper::path(key)), _opType() {
  _opType.type = AgencyOperationType::Type::SIMPLE;
  _opType.simple = opType;
}

AgencyOperation::AgencyOperation(std::string const& key,
                                 AgencyValueOperationType opType, VPackSlice value)
    : _key(AgencyCommHelper::path(key)), _opType(), _value(value) {
  _opType.type = AgencyOperationType::Type::VALUE;
  _opType.value = opType;
}

AgencyOperation::AgencyOperation(std::string const& key, AgencyValueOperationType opType,
                                 VPackSlice newValue, VPackSlice oldValue)
    : _key(AgencyCommHelper::path(key)), _opType(), _value(newValue), _value2(oldValue) {
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

std::string AgencyWriteTransaction::randomClientId() {
  std::string uuid = to_string(boost::uuids::random_generator()());

  auto ss = ServerState::instance();
  if (ss != nullptr && !ss->getId().empty()) {
    return ss->getId() + ":" + uuid;
  }
  return uuid;
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

AgencyCommResult::AgencyCommResult(int code, std::string message)
    : _message(std::move(message)), _statusCode(code) {}

AgencyCommResult::AgencyCommResult(AgencyCommResult&& other) noexcept
    : _location(std::move(other._location)),
      _message(std::move(other._message)),
      _values(std::move(other._values)),
      _statusCode(other._statusCode),
      _connected(other._connected),
      _sent(other._sent),
      _vpack(std::move(other._vpack)) {
  other._statusCode = 0;
  other._connected = false;
  other._sent = false;
}

AgencyCommResult& AgencyCommResult::operator=(AgencyCommResult&& other) noexcept {
  if (this != &other) {
    _location = std::move(other._location);
    _message = std::move(other._message);
    _values = std::move(other._values);
    _statusCode = other._statusCode;
    _connected = other._connected;
    _sent = other._sent;
    _vpack = std::move(other._vpack);

    other._statusCode = 0;
    other._connected = false;
    other._sent = false;
  }
  return *this;
}

void AgencyCommResult::set(int code, std::string message) {
  _message = std::move(message);
  _statusCode = code;
  _location.clear();
  _values.clear();
  _vpack.reset();
}

bool AgencyCommResult::connected() const { return _connected; }

int AgencyCommResult::httpCode() const { return _statusCode; }

bool AgencyCommResult::sent() const { return _sent; }

int AgencyCommResult::errorCode() const {
  return asResult().errorNumber();
}

std::string AgencyCommResult::errorMessage() const {
  return asResult().errorMessage();
}

std::pair<std::optional<int>, std::optional<std::string_view>> AgencyCommResult::parseBodyError() const {
  auto result = std::pair<std::optional<int>, std::optional<std::string_view>>{};

  if (_vpack != nullptr) {
    auto const body = _vpack->slice();
    if (body.isObject()) {
      // Try to extract the "errorCode" attribute.
      try {
        auto const errorCode = body.get(StaticStrings::ErrorCode).getNumber<int>();
        // Save error code if possible, set default error message first
        result.first = errorCode;
      } catch (VPackException const&) {
      }

      // Now try to extract the message.
      if (auto const errMsg = body.get(StaticStrings::ErrorMessage); errMsg.isString()) {
        result.second = errMsg.stringView();
      } else if (auto const errMsg = body.get("message"); errMsg.isString()) {
        result.second = errMsg.stringView();
      }
    }
  }

  return result;
}

std::string AgencyCommResult::errorDetails() const {
  auto const errorMessage = this->errorMessage();

  if (errorMessage.empty()) {
    return _message;
  }

  return _message + " (" + errorMessage + ")";
}

std::string AgencyCommResult::body() const {
  if (_vpack != nullptr) {
    return slice().toJson();
  } else {
    return "";
  }
}

Result AgencyCommResult::asResult() const {
  if (successful()) {
    return Result{};
  } else {
    auto const err = parseBodyError();
    auto const errorCode = std::invoke([&]() -> int {
      if (err.first) {
        return *err.first;
      } else if (_statusCode > 0) {
        return _statusCode;
      } else {
        return TRI_ERROR_INTERNAL;
      }
    });
    auto const errorMessage = std::invoke([&]() -> std::string_view {
      if (err.second) {
        return *err.second;
      } else if (!_message.empty()) {
        return _message;
      } else if (!_connected) {
        return "unable to connect to agency";
      } else {
        return TRI_errno_string(errorCode);
      }
    });

    return Result(errorCode, errorMessage);
  }
}

void AgencyCommResult::clear() {
  // clear existing values. They free themselves
  _values.clear();

  _location = "";
  _message = "";
  _vpack.reset();
  _statusCode = 0;
  _sent = false;
  _connected = false;
}

VPackSlice AgencyCommResult::slice() const {
  // the "slice()" method must only be called in case the result
  // was successful. Otherwise we will not have a valid result
  // to look into
  TRI_ASSERT(_vpack != nullptr);
  if (_vpack == nullptr) {
    // don't segfault in production when we don't have assertions
    // turned on
    THROW_ARANGO_EXCEPTION_MESSAGE(
        TRI_ERROR_INTERNAL, "call to AgencyCommResult::slice() without valid precondition check");
  }
  return _vpack->slice(); 
}

void AgencyCommResult::toVelocyPack(VPackBuilder& builder) const {
  {
    VPackObjectBuilder dump(&builder);
    builder.add("location", VPackValue(_location));
    builder.add("message", VPackValue(_message));
    builder.add("sent", VPackValue(_sent));
    // body is for backwards compatibility only, can be removed in 3.8
    builder.add("body", VPackValue(body()));
    if (_vpack != nullptr) {
      if (_vpack->isClosed()) {
        builder.add("vpack", _vpack->slice());
      }
    }
    builder.add("statusCode", VPackValue(_statusCode));
    builder.add(VPackValue("values"));
    {
      VPackObjectBuilder v(&builder);
      for (auto const& value : _values) {
        builder.add(VPackValue(value.first));
        auto const& entry = value.second;
        {
          VPackObjectBuilder vv(&builder);
          builder.add("index", VPackValue(entry._index));
          builder.add("isDir", VPackValue(entry._isDir));
          if (entry._vpack != nullptr && entry._vpack->isClosed()) {
            builder.add("vpack", entry._vpack->slice());
          }
        }
      }
    }
  }
}

VPackBuilder AgencyCommResult::toVelocyPack() const {
  VPackBuilder builder;
  toVelocyPack(builder);
  return builder;
}

namespace std {
ostream& operator<<(ostream& out, AgencyCommResult const& a) {
  out << a.toVelocyPack().toJson();
  return out;
}
}  // namespace std

// -----------------------------------------------------------------------------
// --SECTION--                                                 AgencyCommHelper
// -----------------------------------------------------------------------------

AgencyConnectionOptions AgencyCommHelper::CONNECTION_OPTIONS(15.0, 120.0, 120.0, 100);
std::string AgencyCommHelper::PREFIX;

void AgencyCommHelper::initialize(std::string const& prefix) {
  PREFIX = prefix;
}

void AgencyCommHelper::shutdown() {}

std::string AgencyCommHelper::path() { return PREFIX; }

std::string AgencyCommHelper::path(std::string const& p1) {
  return PREFIX + "/" + basics::StringUtils::trim(p1, "/");
}

std::string AgencyCommHelper::path(std::string const& p1, std::string const& p2) {
  return PREFIX + "/" + basics::StringUtils::trim(p1, "/") + "/" +
         basics::StringUtils::trim(p2, "/");
}

std::vector<std::string> AgencyCommHelper::slicePath(std::string const& p1) {
  std::string const p2 = basics::StringUtils::trim(p1, "/");
  std::vector<std::string> split = basics::StringUtils::split(p2, '/');
  if (split.size() > 0 && split[0] != AgencyCommHelper::path()) {
    split.insert(split.begin(), AgencyCommHelper::path());
  }
  return split;
}

std::string AgencyCommHelper::generateStamp() {
  time_t tt = time(nullptr);
  struct tm tb;
  char buffer[21];

  TRI_gmtime(tt, &tb);

  size_t len = ::strftime(buffer, sizeof(buffer), "%Y-%m-%dT%H:%M:%SZ", &tb);

  return std::string(buffer, len);
}

// -----------------------------------------------------------------------------
// --SECTION--                                                        AgencyComm
// -----------------------------------------------------------------------------

std::string const AgencyComm::AGENCY_URL_PREFIX = "/_api/agency";

AgencyComm::AgencyComm(application_features::ApplicationServer& server)
    : _server(server),
      _agency_comm_request_time_ms(server.getFeature<arangodb::MetricsFeature>().histogram<log_scale_t<uint64_t>>(
          StaticStrings::AgencyCommRequestTimeMs)) {}

AgencyCommResult AgencyComm::sendServerState(double timeout) {
  // construct JSON value { "status": "...", "time": "..." }
  VPackBuilder builder;

  try {
    builder.openObject();
    std::string const status =
        ServerState::stateToString(ServerState::instance()->getState());
    builder.add("status", VPackValue(status));
    std::string const stamp = AgencyCommHelper::generateStamp();
    builder.add("time", VPackValue(stamp));
    builder.close();
  } catch (...) {
    return AgencyCommResult();
  }

  return AgencyCommResult(setTransient("Sync/ServerStates/" + ServerState::instance()->getId(),
                          builder.slice(), 0, timeout));
}

std::string AgencyComm::version() {
  AgencyCommResult result =
      sendWithFailover(arangodb::rest::RequestType::GET,
                       AgencyCommHelper::CONNECTION_OPTIONS._requestTimeout,
                       "/_api/version", VPackSlice::noneSlice());

  if (result.successful() && result.slice().isString()) {
    return result.slice().copyString();
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
                                          uint64_t ttl,
                                          double timeout) {
  AgencyOperation operation(key, AgencyValueOperationType::SET, slice);
  operation._ttl = ttl;
  AgencyTransientTransaction transaction(operation);

  return sendTransactionWithFailover(transaction, timeout);
}

bool AgencyComm::exists(std::string const& key) {
  AgencyCommResult result = getValues(key);

  if (!result.successful()) {
    return false;
  }

  auto parts = arangodb::basics::StringUtils::split(key, '/');
  std::vector<std::string> allParts;
  allParts.reserve(parts.size() + 1);
  allParts.push_back(AgencyCommHelper::path());
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
      builder.add(VPackValue(AgencyCommHelper::path(key)));
    }
  }

  AgencyCommResult result =
      sendWithFailover(arangodb::rest::RequestType::POST,
                       AgencyCommHelper::CONNECTION_OPTIONS._requestTimeout,
                       url, builder.slice());

  if (!result.successful()) {
    return result;
  }

  try {
    if (!result.slice().isArray()) {
      result.set(500, "got invalid result structure for getValues response");
      return result;
    }

    if (result.slice().length() != 1) {
      result.set(500,
                 "got invalid result structure length for getValues response");
      return result;
    }

    result._statusCode = 200;

  } catch (std::exception const& e) {
    LOG_TOPIC("a6906", ERR, Logger::AGENCYCOMM)
        << "Error transforming result: " << e.what();
    result.clear();
  } catch (...) {
    LOG_TOPIC("5391b", ERR, Logger::AGENCYCOMM)
        << "Error transforming result: out of memory";
    result.clear();
  }

  return result;
}

AgencyCommResult AgencyComm::dump() {
  // We only get the dump from the leader, else its snapshot might be wrong
  // or at least outdated. If there is no leader, one has to contact the
  // agency directly with `/_api/agency/state`.
  std::string url = AgencyComm::AGENCY_URL_PREFIX + "/state?redirectToLeader=true";

  AgencyCommResult result =
      sendWithFailover(arangodb::rest::RequestType::GET,
                       AgencyCommHelper::CONNECTION_OPTIONS._requestTimeout,
                       url, VPackSlice::noneSlice());

  if (!result.successful()) {
    return result;
  }

  result._statusCode = 200;

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
  AgencyCommResult readResult;
  AgencyCommResult writeResult;

  TRI_ASSERT(!writeResult.successful());

  uint64_t oldValue = 0;

  while (!writeResult.successful()) {
    if (server().isStopping()) {
      THROW_ARANGO_EXCEPTION(TRI_ERROR_SHUTTING_DOWN);
    }

    readResult = getValues("Sync/LatestID");
    if (!readResult.successful()) {
      std::this_thread::sleep_for(std::chrono::milliseconds(20));
      continue;
    }

    VPackSlice oldSlice = readResult.slice()[0].get(
        cluster::paths::root()->arango()->sync()->latestId()->vec());

    try {
      oldValue = oldSlice.getNumber<decltype(oldValue)>();
    } catch (velocypack::Exception& e) {
      LOG_TOPIC("74f97", ERR, Logger::AGENCYCOMM)
          << "Sync/LatestID in agency could not be parsed: " << e.what()
          << "; If this error persists, contact the ArangoDB support.";
      auto message = std::string("Failed to parse Sync/LatestID: ") + e.what();
      THROW_ARANGO_EXCEPTION_MESSAGE(TRI_ERROR_INTERNAL, message);
    }

    // If we get here, oldSlice is pointing to an unsigned integer, which
    // is the value in the agency, and oldValue is set to its value.

    uint64_t const newValue = oldValue + count;

    VPackBuilder newBuilder;
    try {
      newBuilder.add(VPackValue(newValue));

      writeResult = casValue("Sync/LatestID", oldSlice, newBuilder.slice(), 0.0, timeout);
    } catch (...) {
    }

    std::this_thread::sleep_for(std::chrono::milliseconds(20));
    // The cas did not work, simply try again!
  }

  TRI_ASSERT(oldValue != 0);
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
  std::string url = AgencyComm::AGENCY_URL_PREFIX + transaction.path();

  VPackBuilder builder;
  {
    VPackArrayBuilder guard(&builder);
    transaction.toVelocyPack(builder);
  }

  LOG_TOPIC("4e477", TRACE, Logger::AGENCYCOMM)
      << "sending " << builder.toJson() << "'" << url << "'";

  AgencyCommResult result =
      sendWithFailover(arangodb::rest::RequestType::POST,
                       (timeout == 0.0) ? AgencyCommHelper::CONNECTION_OPTIONS._requestTimeout
                                        : timeout,
                       url, builder.slice());

  if (!result.successful() &&
      result.httpCode() != (int)arangodb::rest::ResponseCode::PRECONDITION_FAILED) {
    return result;
  }

  try {
    if (!transaction.validate(result)) {
      result.set(500, std::string("validation failed for response to URL " + url));
      LOG_TOPIC("f2083", DEBUG, Logger::AGENCYCOMM)
          << "validation failed for url: " << url
          << ", type: " << transaction.typeName()
          << ", sent: " << builder.toJson() << ", received: " << result.body();
      return result;
    }
  } catch (std::exception const& e) {
    LOG_TOPIC("e13a5", ERR, Logger::AGENCYCOMM)
        << "Error transforming result: " << e.what()
        << ", status code: " << result._statusCode
        << ", incriminating body: " << result.body() << ", url: " << url
        << ", timeout: " << timeout << ", data sent: " << builder.toJson();
    result.clear();
  } catch (...) {
    LOG_TOPIC("18245", ERR, Logger::AGENCYCOMM)
        << "Error transforming result: out of memory";
    result.clear();
  }

  return result;
}

application_features::ApplicationServer& AgencyComm::server() {
  return _server;
}

bool AgencyComm::ensureStructureInitialized() {
  LOG_TOPIC("748e2", TRACE, Logger::AGENCYCOMM)
      << "checking if agency is initialized";

  while (!_server.isStopping() && shouldInitializeStructure()) {
    LOG_TOPIC("17e16", TRACE, Logger::AGENCYCOMM)
        << "Agency is fresh. Needs initial structure.";

    if (tryInitializeStructure()) {
      LOG_TOPIC("4c5aa", TRACE, Logger::AGENCYCOMM)
          << "Successfully initialized agency";
      break;
    }

    LOG_TOPIC("63f7b", INFO, Logger::AGENCYCOMM)
        << "Initializing agency failed. We'll try again soon";
    // We should really have exclusive access, here, this is strange!
    std::this_thread::sleep_for(std::chrono::seconds(1));

    LOG_TOPIC("9d265", TRACE, Logger::AGENCYCOMM)
        << "Waiting for agency to get initialized";

    std::this_thread::sleep_for(std::chrono::seconds(1));
  }

  return true;
}

bool AgencyComm::lock(std::string const& key, double ttl, double timeout,
                      VPackSlice const& slice) {
  if (ttl == 0.0) {
    ttl = AgencyCommHelper::CONNECTION_OPTIONS._lockTimeout;
  }

  if (timeout == 0.0) {
    timeout = AgencyCommHelper::CONNECTION_OPTIONS._lockTimeout;
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
    timeout = AgencyCommHelper::CONNECTION_OPTIONS._lockTimeout;
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

namespace {

AgencyCommResult toAgencyCommResult(AsyncAgencyCommResult const& result) {
  AgencyCommResult oldResult;
  if (result.ok()) {
    oldResult._connected = true;
    oldResult._sent = true;

    if (result.statusCode() == fuerte::StatusTemporaryRedirect) {
      bool found = false;
      oldResult._location =
          result.response->header.metaByKey(StaticStrings::Location, found);

      if (!found) {
        oldResult._message = "invalid agency response (header missing)";
        return oldResult;
      }
    }

    oldResult._message = "";
    oldResult._statusCode = result.statusCode();
    if (result.response->isContentTypeJSON()) {
      auto vpack = VPackParser::fromJson(result.response->payloadAsString());
      oldResult.setVPack(std::move(vpack));
    } else if (result.response->isContentTypeVPack()) {
      auto vpack = std::make_shared<velocypack::Builder>(result.slice());
      oldResult.setVPack(std::move(vpack));
    }
  } else {
    oldResult._connected = false;
    oldResult._sent = false;
    oldResult._message = "sending request to agency failed";
  }
  return oldResult;
}

}  // namespace

AgencyCommResult AgencyComm::sendWithFailover(arangodb::rest::RequestType method,
                                              double const timeout,
                                              std::string const& initialUrl,
                                              VPackSlice inBody) {
  VPackBuffer<uint8_t> buffer;
  {
    VPackBuilder builder(buffer);
    builder.add(inBody);
  }

  AsyncAgencyComm comm;
  AsyncAgencyCommResult result;

  auto started = std::chrono::steady_clock::now();

  TRI_DEFER({
    auto end = std::chrono::steady_clock::now();

    _agency_comm_request_time_ms.count(std::chrono::duration_cast<std::chrono::milliseconds>(end - started).count());
  });

  if (method == arangodb::rest::RequestType::POST) {
    bool isWriteTrans = (initialUrl == ::writeURL);
    if (isWriteTrans) {
      LOG_TOPIC("4e44e", TRACE, Logger::AGENCYCOMM) << "sendWithFailover: "
          << "sending write transaction with POST " << inBody.toJson() << " '"
          << initialUrl << "'";
      result = comm.withSkipScheduler(true)
                   .sendWriteTransaction(std::chrono::duration<double>(timeout),
                                         std::move(buffer))
                   .get();
    } else {
      LOG_TOPIC("4e44f", TRACE, Logger::AGENCYCOMM) << "sendWithFailover: "
          << "sending non-write transaction with POST " << inBody.toJson()
          << " '" << initialUrl << "'";
      result = comm.withSkipScheduler(true)
                   .sendWithFailover(fuerte::RestVerb::Post, initialUrl,
                                     std::chrono::duration<double>(timeout),
                                     AsyncAgencyComm::RequestType::READ, std::move(buffer))
                   .get();
    }
  } else if (method == arangodb::rest::RequestType::GET) {
    LOG_TOPIC("4e448", TRACE, Logger::AGENCYCOMM) << "sendWithFailover: "
        << "sending transaction with GET " << inBody.toJson() << " '"
        << initialUrl << "'";
    result = comm.withSkipScheduler(true)
                 .sendWithFailover(fuerte::RestVerb::Get, initialUrl,
                                   std::chrono::duration<double>(timeout),
                                   AsyncAgencyComm::RequestType::CUSTOM, std::move(buffer))
                 .get();
  } else {
    return AgencyCommResult{static_cast<int>(rest::ResponseCode::METHOD_NOT_ALLOWED),
                            "method not supported"};
  }
  LOG_TOPIC("4e440", TRACE, Logger::AGENCYCOMM)
      << "sendWithFailover done for " << inBody.toJson() << " '" << initialUrl << "'";
  return toAgencyCommResult(result);
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
        VPackObjectBuilder c2(&builder);
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
      builder.add(VPackValue("Analyzers"));
      {
        VPackObjectBuilder d(&builder);
        builder.add(VPackValue("_system"));
        buildInitialAnalyzersSlice(builder);
      }
    }

    builder.add(VPackValue("Sync"));  // Sync ----------------------------------
    {
      VPackObjectBuilder c(&builder);
      builder.add("LatestID", VPackValue(1));
      addEmptyVPackObject("Problems", builder);
      builder.add("UserVersion", VPackValue(1));
      addEmptyVPackObject("ServerStates", builder);
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
      builder.add(VPackValue("ToBeCleanedServers"));
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
    LOG_TOPIC("bef45", ERR, Logger::AGENCYCOMM)
        << "Couldn't create initializing structure " << e.what();
    return false;
  } catch (...) {
    LOG_TOPIC("4b5c2", ERR, Logger::AGENCYCOMM)
        << "Couldn't create initializing structure";
    return false;
  }

  try {
    LOG_TOPIC("58ffe", TRACE, Logger::AGENCYCOMM)
        << "Initializing agency with " << builder.toJson();

    AgencyWriteTransaction initTransaction(
        AgencyOperation("", AgencyValueOperationType::SET, builder.slice()),
        AgencyPrecondition("Plan", AgencyPrecondition::Type::EMPTY, true));

    AgencyCommResult result = sendTransactionWithFailover(initTransaction);
    if (result.httpCode() == TRI_ERROR_HTTP_UNAUTHORIZED) {
      LOG_TOPIC("a695d", ERR, Logger::AUTHENTICATION)
          << "Cannot authenticate with agency,"
          << " check value of --server.jwt-secret";
    }

    return result.successful();
  } catch (std::exception const& e) {
    LOG_TOPIC("0174e", FATAL, Logger::AGENCYCOMM)
        << "Fatal error initializing agency " << e.what();
    FATAL_ERROR_EXIT();
  } catch (...) {
    LOG_TOPIC("6cc28", FATAL, Logger::AGENCYCOMM)
        << "Fatal error initializing agency";
    FATAL_ERROR_EXIT();
  }
}

bool AgencyComm::shouldInitializeStructure() {
  size_t nFail = 0;

  while (!_server.isStopping()) {
    auto result = getValues("Plan");

    if (!result.successful()) {  // Not 200 - 299

      if (result.httpCode() == 401) {
        // unauthorized
        LOG_TOPIC("32781", FATAL, Logger::STARTUP)
            << "Unauthorized. Wrong credentials.";
        FATAL_ERROR_EXIT();
      }

      // Agency not ready yet
      LOG_TOPIC("36253", TRACE, Logger::AGENCYCOMM)
          << "waiting for agency to become ready";
      continue;

    } else {
      if (result.slice().isArray() && result.slice().length() == 1) {
        // No plan entry? Should initialize
        if (result.slice()[0].isObject() && result.slice()[0].length() == 0) {
          LOG_TOPIC("98732", DEBUG, Logger::AGENCYCOMM)
              << "agency initialization should be performed";
          return true;
        } else {
          LOG_TOPIC("abedb", DEBUG, Logger::AGENCYCOMM)
              << "agency initialization under way or done";
          return false;
        }
      } else {
        // Should never get here
        TRI_ASSERT(false);
        if (nFail++ < 3) {
          LOG_TOPIC("fed52", DEBUG, Logger::AGENCYCOMM)
              << "What the hell just happened?";
        } else {
          LOG_TOPIC("54fea", FATAL, Logger::AGENCYCOMM)
              << "Illegal response from agency during bootstrap: "
              << result.slice().toJson();
          FATAL_ERROR_EXIT();
        }
        continue;
      }
    }

    std::this_thread::sleep_for(std::chrono::milliseconds(250));
  }

  return false;
}

void AgencyComm::buildInitialAnalyzersSlice(VPackBuilder& builder) {
  AnalyzersRevision::getEmptyRevision()->toVelocyPack(builder);
}
