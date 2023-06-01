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

#include "RocksDBDumpContext.h"

#include "Basics/system-functions.h"
#include "RocksDBEngine/RocksDBEngine.h"

#include <rocksdb/db.h>
#include <rocksdb/snapshot.h>
#include <rocksdb/utilities/transaction_db.h>

using namespace arangodb;

RocksDBDumpContext::RocksDBDumpContext(RocksDBEngine& engine, std::string id,
                                       uint64_t batchSize,
                                       uint64_t prefetchCount,
                                       uint64_t parallelism,
                                       std::vector<std::string> shards,
                                       double ttl, std::string const& user,
                                       std::string const& database)
    : _engine(engine),
      _id(std::move(id)),
      _batchSize(batchSize),
      _prefetchCount(prefetchCount),
      _parallelism(parallelism),
      _shards(std::move(shards)),
      _ttl(ttl),
      _user(user),
      _database(database),
      _expires(TRI_microtime() + _ttl) {
  // acquire RocksDB snapshot
  _snapshot =
      std::make_shared<rocksdb::ManagedSnapshot>(_engine.db()->GetRootDB());
}

// will automatically delete the RocksDB snapshot
RocksDBDumpContext::~RocksDBDumpContext() = default;

std::string const& RocksDBDumpContext::id() const noexcept { return _id; }

std::string const& RocksDBDumpContext::database() const noexcept {
  return _database;
}

std::string const& RocksDBDumpContext::user() const noexcept { return _user; }

double RocksDBDumpContext::ttl() const noexcept { return _ttl; }

double RocksDBDumpContext::expires() const noexcept {
  return _expires.load(std::memory_order_relaxed);
}

bool RocksDBDumpContext::canAccess(std::string const& database,
                                   std::string const& user) const noexcept {
  return database == _database && user == _user;
}

void RocksDBDumpContext::extendLifetime() noexcept {
  _expires.fetch_add(_ttl, std::memory_order_relaxed);
}
