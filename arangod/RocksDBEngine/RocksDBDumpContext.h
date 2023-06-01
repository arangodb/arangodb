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

#include <atomic>
#include <cstdint>
#include <memory>
#include <string>
#include <vector>

namespace rocksdb {
class ManagedSnapshot;
}  // namespace rocksdb

namespace arangodb {
class RocksDBEngine;

class RocksDBDumpContext {
 public:
  RocksDBDumpContext(RocksDBDumpContext const&) = delete;
  RocksDBDumpContext& operator=(RocksDBDumpContext const&) = delete;

  RocksDBDumpContext(RocksDBEngine& engine, std::string id, uint64_t batchSize,
                     uint64_t prefetchCount, uint64_t parallelism,
                     std::vector<std::string> shards, double ttl,
                     std::string const& user, std::string const& database);

  ~RocksDBDumpContext();

  std::string const& id() const noexcept;

  std::string const& database() const noexcept;
  std::string const& user() const noexcept;

  double ttl() const noexcept;

  double expires() const noexcept;

  bool canAccess(std::string const& database,
                 std::string const& user) const noexcept;

  void extendLifetime() noexcept;

 private:
  RocksDBEngine& _engine;

  // these parameters will not change during the lifetime of the object
  std::string const _id;
  [[maybe_unused]] uint64_t const _batchSize;
  [[maybe_unused]] uint64_t const _prefetchCount;
  [[maybe_unused]] uint64_t const _parallelism;
  [[maybe_unused]] std::vector<std::string> const _shards;
  [[maybe_unused]] double const _ttl;
  std::string const _user;
  std::string const _database;

  std::atomic<double> _expires;

  std::shared_ptr<rocksdb::ManagedSnapshot> _snapshot;
};

}  // namespace arangodb
