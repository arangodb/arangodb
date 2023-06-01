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

#include <atomic>
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

  RocksDBDumpContextGuard createContext(uint64_t batchSize,
                                        uint64_t prefetchCount,
                                        uint64_t parallelism,
                                        std::vector<std::string> shards,
                                        double ttl, std::string const& user,
                                        std::string const& database);

  RocksDBDumpContextGuard find(std::string const& id,
                               std::string const& database,
                               std::string const& user);

  void remove(std::string const& id, std::string const& database,
              std::string const& user);

  void dropDatabase(TRI_vocbase_t& vocbase);

  void garbageCollect(bool force);

  void beginShutdown();

 private:
  std::string generateId();

  RocksDBEngine& _engine;

  std::mutex _lock;

  std::unordered_map<std::string, std::shared_ptr<RocksDBDumpContext>>
      _contexts;

  std::atomic<uint64_t> _nextId;
};

}  // namespace arangodb
