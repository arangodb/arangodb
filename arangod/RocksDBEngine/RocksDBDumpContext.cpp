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

#include "Basics/Exceptions.h"
#include "Basics/system-functions.h"
#include "RestServer/DatabaseFeature.h"
#include "RocksDBEngine/RocksDBEngine.h"
#include "Utils/CollectionGuard.h"
#include "Utils/DatabaseGuard.h"

#include <rocksdb/db.h>
#include <rocksdb/snapshot.h>
#include <rocksdb/utilities/transaction_db.h>

using namespace arangodb;

RocksDBDumpContext::RocksDBDumpContext(
    RocksDBEngine& engine, DatabaseFeature& databaseFeature, std::string id,
    uint64_t batchSize, uint64_t prefetchCount, uint64_t parallelism,
    std::vector<std::string> const& shards, double ttl, std::string const& user,
    std::string const& database)
    : _engine(engine),
      _id(std::move(id)),
      _batchSize(batchSize),
      _prefetchCount(prefetchCount),
      _parallelism(parallelism),
      _ttl(ttl),
      _user(user),
      _database(database),
      _expires(TRI_microtime() + _ttl) {
  // this DatabaseGuard will protect the database object from being deleted
  // while the context is in use. that way we only have to ensure once that the
  // database is there. creating this guard will throw if the database cannot be
  // found.
  _databaseGuard = std::make_unique<DatabaseGuard>(databaseFeature, database);

  // these CollectionGuards will protect the collection/shard objects from being
  // deleted while the context is in use. that we we only have to ensure once
  // that the collections are there. creating the guards will throw if any of
  // the collections/shards cannot be found.
  for (auto const& it : shards) {
    _collectionGuards.emplace(
        it, std::make_unique<CollectionGuard>(&_databaseGuard->database(), it));
  }

  // acquire RocksDB snapshot
  _snapshot =
      std::make_shared<rocksdb::ManagedSnapshot>(_engine.db()->GetRootDB());
}

// will automatically delete the RocksDB snapshot and all guards
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

LogicalCollection& RocksDBDumpContext::collection(
    std::string const& name) const {
  auto it = _collectionGuards.find(name);
  if (it == _collectionGuards.end()) {
    THROW_ARANGO_EXCEPTION(TRI_ERROR_ARANGO_DATA_SOURCE_NOT_FOUND);
  }

  return *((*it).second->collection());
}
