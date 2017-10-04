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

#ifndef ARANGOD_REPLICATION_REPLICATION_APPLIER_CONFIGURATION_H
#define ARANGOD_REPLICATION_REPLICATION_APPLIER_CONFIGURATION_H 1

#include "Basics/Common.h"

namespace arangodb {
namespace velocypack {
class Builder;
class Slice;
}

/// @brief struct containing a replication apply configuration
class ReplicationApplierConfiguration {
  // leftover from struct
 public:
  std::string _endpoint;
  std::string _database;
  std::string _username;
  std::string _password;
  std::string _jwt;
  double _requestTimeout;
  double _connectTimeout;
  uint64_t _ignoreErrors;
  uint64_t _maxConnectRetries;
  uint64_t _lockTimeoutRetries;
  uint64_t _chunkSize;
  uint64_t _connectionRetryWaitTime;
  uint64_t _idleMinWaitTime;  // 500 * 1000
  uint64_t _idleMaxWaitTime;  // 5 * 500 * 1000
  uint64_t _initialSyncMaxWaitTime;
  uint64_t _autoResyncRetries;
  uint32_t _sslProtocol;
  bool _autoStart;
  bool _adaptivePolling;
  bool _autoResync;
  bool _includeSystem;
  bool _requireFromPresent;
  bool _incremental;
  bool _verbose;
  bool _useCollectionId;
  std::string _restrictType;
  std::unordered_map<std::string, bool> _restrictCollections;

 public:
  ReplicationApplierConfiguration();
  ~ReplicationApplierConfiguration() {}

  ReplicationApplierConfiguration(ReplicationApplierConfiguration const&) = default;
  ReplicationApplierConfiguration& operator=(ReplicationApplierConfiguration const&) = default;

  /// @brief reset the configuration to defaults
  void reset();

  /// @brief get a VelocyPack representation
  /// expects builder to be in an open Object state
  void toVelocyPack(arangodb::velocypack::Builder&, bool includePassword) const;
  
  /// @brief create a configuration object from velocypack
  static ReplicationApplierConfiguration fromVelocyPack(arangodb::velocypack::Slice slice, 
                                                        std::string const& databaseName);
};

}

#endif
