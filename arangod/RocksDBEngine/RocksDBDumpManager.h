////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2014-2023 ArangoDB GmbH, Cologne, Germany
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

#pragma once

#include "RocksDBEngine/RocksDBDumpContext.h"
#include "RocksDBEngine/RocksDBDumpContextGuard.h"

#include <cstdint>
#include <memory>
#include <mutex>
#include <string>
#include <unordered_map>
#include <vector>

struct TRI_vocbase_t;

namespace arangodb {
class RocksDBEngine;

class RocksDBDumpManager {
 public:
  explicit RocksDBDumpManager(RocksDBEngine& engine);

  ~RocksDBDumpManager();

  // create a new context. a unique id is assigned automatically.
  // the context can later be accessed by passing the context's id
  // into find(), together with the same database name and user name
  // that were used when creating the context.
  RocksDBDumpContextGuard createContext(uint64_t batchSize,
                                        uint64_t prefetchCount,
                                        uint64_t parallelism,
                                        std::vector<std::string> const& shards,
                                        double ttl, std::string const& user,
                                        std::string const& database);

  // look up context by id. must provide the same database name and
  // user name as when creating the context. otherwise a "forbidden"
  // exception is thrown.
  RocksDBDumpContextGuard find(std::string const& id,
                               std::string const& database,
                               std::string const& user);

  // remove a context by id. must provide the same database name and
  // user name as when creating the context. otherwise a "forbidden"
  // exception is thrown.
  // if no other thread uses the context, it will be destroyed. otherwise
  // the last shared_ptr to the context that goes out of scope will
  // destroy the context.
  void remove(std::string const& id, std::string const& database,
              std::string const& user);

  // delete all contexts for the given database.
  void dropDatabase(TRI_vocbase_t& vocbase);

  void garbageCollect(bool force);

  void beginShutdown();

 private:
  std::string generateId();

  RocksDBEngine& _engine;

  // lock for _contexts
  std::mutex _lock;

  // this maps stores contexts by their id. contexts are handed out from the
  // manager as shared_ptrs. if remove is called on a context, it will be
  // destroyed once the last shared_ptr to it goes out of scope.
  std::unordered_map<std::string, std::shared_ptr<RocksDBDumpContext>>
      _contexts;
};

}  // namespace arangodb
