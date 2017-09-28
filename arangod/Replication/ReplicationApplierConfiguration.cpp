////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2014-2017 ArangoDB GmbH, Cologne, Germany
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

#include <velocypack/Builder.h>
#include <velocypack/velocypack-aliases.h>

using namespace arangodb;

/// @brief construct the configuration with default values
ReplicationApplierConfiguration::ReplicationApplierConfiguration() 
    : _endpoint(),
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
      _sslProtocol(0),
      _autoStart(false),
      _adaptivePolling(true),
      _autoResync(false),
      _includeSystem(true),
      _requireFromPresent(false),
      _incremental(false),
      _verbose(false),
      _useCollectionId(true),
      _restrictType(),
      _restrictCollections() {}

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
  _sslProtocol = 0;
  _autoStart = false; 
  _adaptivePolling = true;
  _autoResync = false;
  _includeSystem = true;
  _requireFromPresent = false;
  _incremental = false;
  _verbose = false;
  _useCollectionId = true;
  _restrictType.clear();
  _restrictCollections.clear();
}

/// @brief get a VelocyPack representation
///        Expects builder to be in an open Object state
void ReplicationApplierConfiguration::toVelocyPack(bool includePassword, 
                                                   VPackBuilder& builder) const {
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
  if (includePassword) {
    hasUsernamePassword = true;
    builder.add("password", VPackValue(_password));
  }
  if (!hasUsernamePassword && !_jwt.empty()) {
    builder.add("jwt", VPackValue(_jwt));
  }

  builder.add("requestTimeout", VPackValue(_requestTimeout));
  builder.add("connectTimeout", VPackValue(_connectTimeout));
  builder.add("ignoreErrors", VPackValue(_ignoreErrors));
  builder.add("maxConnectRetries", VPackValue(_maxConnectRetries));
  builder.add("lockTimeoutRetries", VPackValue(_lockTimeoutRetries));
  builder.add("sslProtocol", VPackValue(_sslProtocol));
  builder.add("chunkSize", VPackValue(_chunkSize));
  builder.add("autoStart", VPackValue(_autoStart));
  builder.add("adaptivePolling", VPackValue(_adaptivePolling));
  builder.add("autoResync", VPackValue(_autoResync));
  builder.add("autoResyncRetries", VPackValue(_autoResyncRetries));
  builder.add("includeSystem", VPackValue(_includeSystem));
  builder.add("requireFromPresent", VPackValue(_requireFromPresent));
  builder.add("verbose", VPackValue(_verbose));
  builder.add("incremental", VPackValue(_incremental));
  builder.add("useCollectionId", VPackValue(_useCollectionId));
  builder.add("restrictType", VPackValue(_restrictType));

  builder.add("restrictCollections", VPackValue(VPackValueType::Array));
  for (auto& it : _restrictCollections) {
    builder.add(VPackValue(it.first));
  }
  builder.close();  // restrictCollections

  builder.add("connectionRetryWaitTime",
              VPackValue(static_cast<double>(_connectionRetryWaitTime) /
                         (1000.0 * 1000.0)));
  builder.add("initialSyncMaxWaitTime",
              VPackValue(static_cast<double>(_initialSyncMaxWaitTime) /
                         (1000.0 * 1000.0)));
  builder.add(
      "idleMinWaitTime",
      VPackValue(static_cast<double>(_idleMinWaitTime) / (1000.0 * 1000.0)));
  builder.add(
      "idleMaxWaitTime",
      VPackValue(static_cast<double>(_idleMaxWaitTime) / (1000.0 * 1000.0)));
}

/// @brief get a VelocyPack representation
std::shared_ptr<VPackBuilder>
ReplicationApplierConfiguration::toVelocyPack(
    bool includePassword) const {
  auto builder = std::make_shared<VPackBuilder>();
  {
    VPackObjectBuilder b(builder.get());
    toVelocyPack(includePassword, *builder);
  }

  return builder;
}
