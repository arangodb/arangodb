////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2014-2022 ArangoDB GmbH, Cologne, Germany
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
/// @author Simon Grätzer
////////////////////////////////////////////////////////////////////////////////

#pragma once

#include "Basics/Common.h"
#include "Basics/Mutex.h"
#include "Basics/ResultT.h"
#include "Replication/utilities.h"
#include "RocksDBEngine/RocksDBReplicationContext.h"
#include "VocBase/Identifiers/ServerId.h"

#include <cstdint>
#include <memory>
#include <string>
#include <unordered_map>

struct TRI_vocbase_t;

namespace arangodb {
class LogicalCollection;
class RocksDBEngine;
class RocksDBReplicationContextGuard;

using RocksDBReplicationId = std::uint64_t;

class RocksDBReplicationManager {
 public:
  /// @brief create a contexts repository
  explicit RocksDBReplicationManager(RocksDBEngine&);

  /// @brief destroy a contexts repository
  ~RocksDBReplicationManager();

  /// @brief creates a new context which must be later returned using release()
  /// or destroy(); guarantees that RocksDB file deletion is disabled while
  /// there are active contexts
  RocksDBReplicationContextGuard createContext(RocksDBEngine&, double ttl,
                                               SyncerId syncerId,
                                               ServerId clientId,
                                               std::string const& patchCount);

  /// @brief remove a context by id
  ResultT<std::tuple<SyncerId, ServerId, std::string>> remove(
      RocksDBReplicationId id);

  /// @brief find an existing context by id
  /// if found, the context will be returned with the isUsed() flag set to true.
  /// it must be returned later using release() or destroy()
  /// the second parameter shows if the context you are looking for is busy or
  /// not
  RocksDBReplicationContextGuard find(
      RocksDBReplicationId, double ttl = replutils::BatchInfo::DefaultTimeout);

  /// @brief find an existing context by id and extend lifetime
  /// may be used concurrently on used contexts
  /// populates clientId
  ResultT<std::tuple<SyncerId, ServerId, std::string>> extendLifetime(
      RocksDBReplicationId, double ttl = replutils::BatchInfo::DefaultTimeout);

  /// @brief return a context for later use (if deleted = false - otherwise
  /// remove the context)
  void release(std::shared_ptr<RocksDBReplicationContext>&& context,
               bool deleted);

  /// @brief drop contexts by database
  void drop(TRI_vocbase_t& vocbase);

  /// @brief drop contexts by collection
  void drop(LogicalCollection& collection);

  /// @brief drop all contexts
  void dropAll();

  /// @brief run a garbage collection on the contexts
  bool garbageCollect(bool force);

  /// @brief tell the replication manager that a shutdown is in progress
  /// effectively this will block the creation of new contexts
  void beginShutdown();

 private:
  /// @brief return a context for garbage collection
  void destroy(RocksDBReplicationContext*);

  /// @brief mutex for the contexts repository
  Mutex _lock;

  /// @brief list of current contexts
  std::unordered_map<RocksDBReplicationId,
                     std::shared_ptr<RocksDBReplicationContext>>
      _contexts;
};

}  // namespace arangodb
