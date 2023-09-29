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

#include "Metrics/Fwd.h"
#include "RocksDBEngine/RocksDBDumpContext.h"

#include <cstdint>
#include <memory>
#include <mutex>
#include <string>
#include <unordered_map>

struct TRI_vocbase_t;

namespace arangodb {
struct DumpLimits;
class RocksDBEngine;

namespace metrics {
class MetricsFeature;
}

namespace velocypack {
struct Options;
}

class RocksDBDumpManager {
 public:
  explicit RocksDBDumpManager(RocksDBEngine& engine,
                              metrics::MetricsFeature& metricsFeature,
                              DumpLimits const& limits);

  ~RocksDBDumpManager();

  // create a new context. a unique id is assigned automatically.
  // the context can later be accessed by passing the context's id
  // into find(), together with the same database name and user name
  // that were used when creating the context.
  std::shared_ptr<RocksDBDumpContext> createContext(
      RocksDBDumpContextOptions opts, std::string const& user,
      std::string const& database, bool useVPack);

  // look up context by id. must provide the same database name and
  // user name as when creating the context. otherwise a "forbidden"
  // exception is thrown.
  std::shared_ptr<RocksDBDumpContext> find(std::string const& id,
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

  std::unique_ptr<RocksDBDumpContext::Batch> requestBatch(
      RocksDBDumpContext& context, std::string const& collectionName,
      std::uint64_t& batchSize, bool useVPack,
      velocypack::Options const* vpackOptions);

  bool reserveCapacity(std::uint64_t value) noexcept;
  void trackMemoryUsage(std::uint64_t size) noexcept;
  void untrackMemoryUsage(std::uint64_t size) noexcept;

 private:
  using MapType =
      std::unordered_map<std::string, std::shared_ptr<RocksDBDumpContext>>;

  // generate a new context id
  std::string generateId();

  // look up a context by id. will throw in case that the context cannot be
  // found or the user is different.
  // assumes that _lock is already acquired by the caller.
  MapType::iterator lookupContext(std::string const& id,
                                  std::string const& database,
                                  std::string const& user);

  RocksDBEngine& _engine;

  // lock for _contexts
  std::mutex _lock;

  // this maps stores contexts by their id. contexts are handed out from the
  // manager as shared_ptrs. if remove is called on a context, it will be
  // destroyed once the last shared_ptr to it goes out of scope.
  MapType _contexts;

  DumpLimits const& _limits;

  metrics::Gauge<std::uint64_t>& _dumpsOngoing;
  metrics::Gauge<std::uint64_t>& _dumpsMemoryUsage;
  metrics::Counter& _dumpsThreadsBlocked;
};

}  // namespace arangodb
