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
////////////////////////////////////////////////////////////////////////////////

#include "ReplicationApplierConfiguration.h"

#include "ApplicationFeatures/ApplicationServer.h"
#include "Basics/Exceptions.h"
#include "Cluster/ClusterFeature.h"
#include "GeneralServer/AuthenticationFeature.h"
#include "Logger/LogMacros.h"
#include "Replication/ReplicationFeature.h"

#include <velocypack/Builder.h>
#include <velocypack/Iterator.h>
#include <velocypack/Slice.h>
#include <velocypack/velocypack-aliases.h>

using namespace arangodb;

/// @brief construct the configuration with default values
ReplicationApplierConfiguration::ReplicationApplierConfiguration(application_features::ApplicationServer& server)
    : _server(server),
      _endpoint(),
      _database(),
      _username(),
      _password(),
      _jwt(),
      _requestTimeout(600.0),
      _connectTimeout(10.0),
      _ignoreErrors(0),
      _maxConnectRetries(100),
      _lockTimeoutRetries(0),
      _chunkSize(0),
      _connectionRetryWaitTime(15 * 1000 * 1000),
      _idleMinWaitTime(1000 * 1000),
      _idleMaxWaitTime(5 * 500 * 1000),
      _initialSyncMaxWaitTime(300 * 1000 * 1000),
      _autoResyncRetries(2),
      _maxPacketSize(512 * 1024 * 1024),
      _sslProtocol(0),
      _skipCreateDrop(false),
      _autoStart(false),
      _adaptivePolling(true),
      _autoResync(false),
      _includeSystem(true),
      _includeFoxxQueues(false),
      _requireFromPresent(true),
      _incremental(false),
      _verbose(false),
      _restrictType(RestrictType::None) {
  if (_server.hasFeature<ReplicationFeature>()) {
    auto& feature = _server.getFeature<ReplicationFeature>();
    _requestTimeout = feature.requestTimeout();
    _connectTimeout = feature.connectTimeout();
  }
}

/// @brief construct the configuration with default values
ReplicationApplierConfiguration& ReplicationApplierConfiguration::operator=(
    ReplicationApplierConfiguration const& other) {
  _endpoint = other._endpoint;
  _database = other._database;
  _username = other._username;
  _password = other._password;
  _jwt = other._jwt;
  _requestTimeout = other._requestTimeout;
  _connectTimeout = other._connectTimeout;
  _ignoreErrors = other._ignoreErrors;
  _maxConnectRetries = other._maxConnectRetries;
  _lockTimeoutRetries = other._lockTimeoutRetries;
  _chunkSize = other._chunkSize;
  _connectionRetryWaitTime = other._connectionRetryWaitTime;
  _idleMinWaitTime = other._idleMinWaitTime;
  _idleMaxWaitTime = other._idleMaxWaitTime;
  _initialSyncMaxWaitTime = other._initialSyncMaxWaitTime;
  _autoResyncRetries = other._autoResyncRetries;
  _maxPacketSize = other._maxPacketSize;
  _sslProtocol = other._sslProtocol;
  _skipCreateDrop = other._skipCreateDrop;
  _autoStart = other._autoStart;
  _adaptivePolling = other._adaptivePolling;
  _autoResync = other._autoResync;
  _includeSystem = other._includeSystem;
  _includeFoxxQueues = other._includeFoxxQueues;
  _requireFromPresent = other._requireFromPresent;
  _incremental = other._incremental;
  _verbose = other._verbose;
  _restrictType = other._restrictType;
  _restrictCollections.clear();
  _restrictCollections.insert(other._restrictCollections.begin(),
                              other._restrictCollections.end());

  return *this;
}

/// @brief reset the configuration to defaults
void ReplicationApplierConfiguration::reset() {
  _endpoint.clear();
  _database.clear();
  _username.clear();
  _password.clear();
  _jwt.clear();
  _requestTimeout = 600.0;
  _connectTimeout = 10.0;
  _ignoreErrors = 0;
  _maxConnectRetries = 100;
  _lockTimeoutRetries = 0;
  _chunkSize = 0;
  _connectionRetryWaitTime = 15 * 1000 * 1000;
  _idleMinWaitTime = 1000 * 1000;
  _idleMaxWaitTime = 5 * 500 * 1000;
  _initialSyncMaxWaitTime = 300 * 1000 * 1000;
  _autoResyncRetries = 2;
  _maxPacketSize = 512 * 1024 * 1024;
  _sslProtocol = 0;
  _skipCreateDrop = false;
  _autoStart = false;
  _adaptivePolling = true;
  _autoResync = false;
  _includeSystem = true;
  _includeFoxxQueues = false;
  _requireFromPresent = true;
  _incremental = false;
  _verbose = false;
  _restrictType = RestrictType::None;
  _restrictCollections.clear();
#ifdef ARANGODB_ENABLE_MAINTAINER_MODE
  _force32mode = false;
#endif
    
  if (_server.hasFeature<ReplicationFeature>()) {
    auto& feature = _server.getFeature<ReplicationFeature>();
    _requestTimeout = feature.requestTimeout();
    _connectTimeout = feature.connectTimeout();
  }
}

/// @brief get a VelocyPack representation
/// expects builder to be in an open Object state
void ReplicationApplierConfiguration::toVelocyPack(VPackBuilder& builder, bool includePassword,
                                                   bool includeJwt) const {
  if (!_endpoint.empty()) {
    builder.add("endpoint", VPackValue(_endpoint));
  }
  if (!_database.empty()) {
    builder.add("database", VPackValue(_database));
  }

  bool hasUsernamePassword = false;
  if (!_username.empty()) {
    hasUsernamePassword = true;
    builder.add("username", VPackValue(_username));
  }
  if (includePassword && !_password.empty()) {
    hasUsernamePassword = true;
    builder.add("password", VPackValue(_password));
  }
  if (includeJwt && !hasUsernamePassword && !_jwt.empty()) {
    builder.add("jwt", VPackValue(_jwt));
  }

  builder.add("requestTimeout", VPackValue(_requestTimeout));
  builder.add("connectTimeout", VPackValue(_connectTimeout));
  builder.add("ignoreErrors", VPackValue(_ignoreErrors));
  builder.add("maxConnectRetries", VPackValue(_maxConnectRetries));
  builder.add("lockTimeoutRetries", VPackValue(_lockTimeoutRetries));
  builder.add("sslProtocol", VPackValue(_sslProtocol));
  builder.add("chunkSize", VPackValue(_chunkSize));
  builder.add("skipCreateDrop", VPackValue(_skipCreateDrop));
  builder.add("autoStart", VPackValue(_autoStart));
  builder.add("adaptivePolling", VPackValue(_adaptivePolling));
  builder.add("autoResync", VPackValue(_autoResync));
  builder.add("autoResyncRetries", VPackValue(_autoResyncRetries));
  builder.add("maxPacketSize", VPackValue(_maxPacketSize));
  builder.add("includeSystem", VPackValue(_includeSystem));
  builder.add("includeFoxxQueues", VPackValue(_includeFoxxQueues));
  builder.add("requireFromPresent", VPackValue(_requireFromPresent));
  builder.add("verbose", VPackValue(_verbose));
  builder.add("incremental", VPackValue(_incremental));
  builder.add("restrictType", VPackValue(restrictTypeToString(_restrictType)));

  builder.add("restrictCollections", VPackValue(VPackValueType::Array));
  for (std::string const& it : _restrictCollections) {
    builder.add(VPackValue(it));
  }
  builder.close();  // restrictCollections

  builder.add("connectionRetryWaitTime",
              VPackValue(static_cast<double>(_connectionRetryWaitTime) / (1000.0 * 1000.0)));
  builder.add("initialSyncMaxWaitTime",
              VPackValue(static_cast<double>(_initialSyncMaxWaitTime) / (1000.0 * 1000.0)));
  builder.add("idleMinWaitTime",
              VPackValue(static_cast<double>(_idleMinWaitTime) / (1000.0 * 1000.0)));
  builder.add("idleMaxWaitTime",
              VPackValue(static_cast<double>(_idleMaxWaitTime) / (1000.0 * 1000.0)));
#ifdef ARANGODB_ENABLE_MAINTAINER_MODE
  builder.add("force32mode", VPackValue(_force32mode));
#endif
}

/// @brief create a configuration object from velocypack
ReplicationApplierConfiguration ReplicationApplierConfiguration::fromVelocyPack(
    application_features::ApplicationServer& server, VPackSlice slice,
    std::string const& databaseName) {
  return fromVelocyPack(ReplicationApplierConfiguration(server), slice, databaseName);
}

/// @brief create a configuration object from velocypack, merging it with an
/// existing one
ReplicationApplierConfiguration ReplicationApplierConfiguration::fromVelocyPack(
    ReplicationApplierConfiguration const& existing, VPackSlice slice,
    std::string const& databaseName) {
  // copy existing configuration
  ReplicationApplierConfiguration configuration = existing;

  // read the database name
  VPackSlice value = slice.get("database");
  if (!value.isString()) {
    configuration._database = databaseName;
  } else {
    configuration._database = value.copyString();
  }

  // read username / password
  value = slice.get("username");
  bool hasUsernamePassword = false;
  if (value.isString() && value.getStringLength() > 0) {
    hasUsernamePassword = true;
    configuration._username = value.copyString();

    value = slice.get("password");
    if (value.isString()) {
      configuration._password = value.copyString();
    }
  }

  if (!hasUsernamePassword) {
    value = slice.get("jwt");
    if (value.isString()) {
      configuration._jwt = value.copyString();
    } else {
      // use internal JWT token in any cluster setup
      auto& cluster = existing._server.getFeature<ClusterFeature>();
      if (cluster.isEnabled()) {
        if (existing._server.hasFeature<AuthenticationFeature>()) {
          configuration._jwt =
              existing._server.getFeature<AuthenticationFeature>().tokenCache().jwtToken();
        }
      }
    }
  }

  value = slice.get("requestTimeout");
  if (value.isNumber()) {
    if (existing._server.hasFeature<ReplicationFeature>()) {
      auto& feature = existing._server.getFeature<ReplicationFeature>();
      configuration._requestTimeout = feature.checkRequestTimeout(value.getNumber<double>());
    }
  }

  value = slice.get("connectTimeout");
  if (value.isNumber()) {
    if (existing._server.hasFeature<ReplicationFeature>()) {
      auto& feature = existing._server.getFeature<ReplicationFeature>();
      configuration._connectTimeout = feature.checkConnectTimeout(value.getNumber<double>());
    }
  }

  value = slice.get("maxConnectRetries");
  if (value.isNumber()) {
    configuration._maxConnectRetries = value.getNumber<uint64_t>();
  }

  value = slice.get("lockTimeoutRetries");
  if (value.isNumber()) {
    configuration._lockTimeoutRetries = value.getNumber<uint64_t>();
  }

  value = slice.get("sslProtocol");
  if (value.isNumber()) {
    configuration._sslProtocol = value.getNumber<uint32_t>();
  }

  value = slice.get("chunkSize");
  if (value.isNumber()) {
    configuration._chunkSize = value.getNumber<uint64_t>();
  }

  value = slice.get("skipCreateDrop");
  if (value.isBoolean()) {
    configuration._skipCreateDrop = value.getBoolean();
  }

  value = slice.get("autoStart");
  if (value.isBoolean()) {
    configuration._autoStart = value.getBoolean();
  }

  value = slice.get("adaptivePolling");
  if (value.isBoolean()) {
    configuration._adaptivePolling = value.getBoolean();
  }

  value = slice.get("autoResync");
  if (value.isBoolean()) {
    configuration._autoResync = value.getBoolean();
  }

  value = slice.get("includeSystem");
  if (value.isBoolean()) {
    configuration._includeSystem = value.getBoolean();
  }
  
  value = slice.get("includeFoxxQueues");
  if (value.isBoolean()) {
    configuration._includeFoxxQueues = value.getBoolean();
  }

  value = slice.get("requireFromPresent");
  if (value.isBoolean()) {
    configuration._requireFromPresent = value.getBoolean();
  }

  value = slice.get("verbose");
  if (value.isBoolean()) {
    configuration._verbose = value.getBoolean();
  }

  value = slice.get("incremental");
  if (value.isBoolean()) {
    configuration._incremental = value.getBoolean();
  }

  value = slice.get("ignoreErrors");
  if (value.isNumber()) {
    configuration._ignoreErrors = value.getNumber<uint64_t>();
  } else if (value.isBoolean()) {
    if (value.getBoolean()) {
      configuration._ignoreErrors = UINT64_MAX;
    } else {
      configuration._ignoreErrors = 0;
    }
  }

  value = slice.get("restrictType");
  if (value.isString()) {
    configuration._restrictType = restrictTypeFromString(value.copyString());
  }

  value = slice.get("restrictCollections");
  if (value.isArray()) {
    configuration._restrictCollections.clear();

    for (VPackSlice it : VPackArrayIterator(value)) {
      if (it.isString()) {
        configuration._restrictCollections.emplace(it.copyString());
      }
    }
  }

  value = slice.get("connectionRetryWaitTime");
  if (value.isNumber()) {
    double v = value.getNumber<double>();
    if (v > 0.0) {
      configuration._connectionRetryWaitTime = static_cast<uint64_t>(v * 1000.0 * 1000.0);
    }
  }

  value = slice.get("initialSyncMaxWaitTime");
  if (value.isNumber()) {
    double v = value.getNumber<double>();
    if (v > 0.0) {
      configuration._initialSyncMaxWaitTime = static_cast<uint64_t>(v * 1000.0 * 1000.0);
    }
  }

  value = slice.get("idleMinWaitTime");
  if (value.isNumber()) {
    double v = value.getNumber<double>();
    if (v > 0.0) {
      configuration._idleMinWaitTime = static_cast<uint64_t>(v * 1000.0 * 1000.0);
    }
  }

  value = slice.get("idleMaxWaitTime");
  if (value.isNumber()) {
    double v = value.getNumber<double>();
    if (v > 0.0) {
      configuration._idleMaxWaitTime = static_cast<uint64_t>(v * 1000.0 * 1000.0);
    }
  }

  value = slice.get("autoResyncRetries");
  if (value.isNumber()) {
    configuration._autoResyncRetries = value.getNumber<uint64_t>();
  }

  value = slice.get("maxPacketSize");
  if (value.isNumber()) {
    configuration._maxPacketSize = value.getNumber<uint64_t>();
  }

  // read the endpoint
  value = slice.get("endpoint");
  if (!value.isNone()) {
    if (!value.isString()) {
      // we haven't found an endpoint. now don't let the start fail but continue
      configuration._autoStart = false;
    } else {
      configuration._endpoint = value.copyString();
    }
  }

#ifdef ARANGODB_ENABLE_MAINTAINER_MODE
  value = slice.get("force32mode");
  if (value.isBool()) {
    configuration._force32mode = value.getBool();
  }
#endif

  return configuration;
}

/// @brief validate the configuration. will throw if the config is invalid
void ReplicationApplierConfiguration::validate() const {
  if (_endpoint.empty()) {
    THROW_ARANGO_EXCEPTION_MESSAGE(TRI_ERROR_REPLICATION_INVALID_APPLIER_CONFIGURATION,
                                   "invalid value for <endpoint>");
  }

  if ((_restrictType == RestrictType::None && !_restrictCollections.empty()) ||
      (_restrictType != RestrictType::None && _restrictCollections.empty())) {
    THROW_ARANGO_EXCEPTION_MESSAGE(
        TRI_ERROR_REPLICATION_INVALID_APPLIER_CONFIGURATION,
        "invalid value for <restrictCollections> or <restrictType>");
  }
}

ReplicationApplierConfiguration::RestrictType ReplicationApplierConfiguration::restrictTypeFromString(
    std::string const& value) {
  if (value.empty() || value == "none") {
    return RestrictType::None;
  }
  if (value == "include") {
    return RestrictType::Include;
  }
  if (value == "exclude") {
    return RestrictType::Exclude;
  }

  THROW_ARANGO_EXCEPTION_MESSAGE(TRI_ERROR_REPLICATION_INVALID_APPLIER_CONFIGURATION,
                                 "invalid value for <restrictType>");
}

std::string ReplicationApplierConfiguration::restrictTypeToString(
    ReplicationApplierConfiguration::RestrictType type) {
  switch (type) {
    case RestrictType::Include:
      return "include";
    case RestrictType::Exclude:
      return "exclude";
    case RestrictType::None:
    default: { return ""; }
  }
}
