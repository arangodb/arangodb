////////////////////////////////////////////////////////////////////////////////
/// @brief helper for cache suite
///
/// @file
///
/// DISCLAIMER
///
/// Copyright 2017 ArangoDB GmbH, Cologne, Germany
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
/// @author Daniel H. Larkin
/// @author Copyright 2017, ArangoDB GmbH, Cologne, Germany
////////////////////////////////////////////////////////////////////////////////

#include "TransactionalStore.h"
#include "Basics/Common.h"
#include "Basics/StringBuffer.h"
#include "Basics/files.h"
#include "Cache/Common.h"
#include "Cache/Manager.h"
#include "Cache/TransactionalCache.h"

#include <rocksdb/db.h>
#include <rocksdb/options.h>
#include <rocksdb/utilities/transaction.h>
#include <rocksdb/utilities/transaction_db.h>

#include <chrono>

using namespace arangodb::cache;

std::atomic<uint32_t> TransactionalStore::_sequence(0);

TransactionalStore::Document::Document() : Document(0) {}

TransactionalStore::Document::Document(uint64_t k)
    : key(k),
      timestamp(static_cast<uint64_t>(
          std::chrono::steady_clock::now().time_since_epoch().count())),
      sequence(0) {}

void TransactionalStore::Document::advance() {
  timestamp = static_cast<uint64_t>(
      std::chrono::steady_clock::now().time_since_epoch().count());
  sequence++;
}

void TransactionalStore::Document::clear() {
  memset(this, 0, sizeof(Document));
}

bool TransactionalStore::Document::empty() const { return (key == 0); }

TransactionalStore::Transaction::Transaction(arangodb::cache::Transaction* c,
                                             rocksdb::Transaction* r)
    : cache(c), rocks(r) {}

TransactionalStore::TransactionalStore(Manager* manager)
    : _manager(manager),
      _directory(TRI_UNKNOWN_MEM_ZONE),
      _id(++_sequence),
      _readOptions(rocksdb::ReadOptions()),
      _writeOptions(rocksdb::WriteOptions()),
      _txOptions(rocksdb::TransactionOptions()) {
  TRI_ASSERT(manager != nullptr);
  _cache = manager->createCache(CacheType::Transactional, true);
  TRI_ASSERT(_cache.get() != nullptr);

  _directory.appendText(TRI_GetTempPath());
  _directory.appendChar(TRI_DIR_SEPARATOR_CHAR);
  _directory.appendText("cache-test-transactional-store-");
  _directory.appendText(std::to_string(_id));

  rocksdb::Options options;
  options.create_if_missing = true;
  options.max_open_files = 128;

  auto status = rocksdb::TransactionDB::Open(
      options, rocksdb::TransactionDBOptions(), _directory.c_str(), &_db);
  if (!status.ok()) {
    throw;
  }
  TRI_ASSERT(status.ok());
}

TransactionalStore::~TransactionalStore() {
  delete _db;
  TRI_ASSERT(_directory.length() > 20);
  TRI_RemoveDirectory(_directory.c_str());
  _manager->destroyCache(_cache);
}

Cache* TransactionalStore::cache() { return _cache.get(); }

TransactionalStore::Transaction* TransactionalStore::beginTransaction(
    bool readOnly) {
  auto cache = _manager->beginTransaction(readOnly);
  auto rocks = _db->BeginTransaction(_writeOptions, _txOptions);
  rocks->SetSnapshot();
  return new Transaction(cache, rocks);
}

bool TransactionalStore::commit(TransactionalStore::Transaction* tx) {
  if (tx != nullptr) {
    auto status = tx->rocks->Commit();
    if (status.ok()) {
      _manager->endTransaction(tx->cache);
      delete tx->rocks;
      delete tx;
      return true;
    }
  }
  return false;
}

bool TransactionalStore::rollback(TransactionalStore::Transaction* tx) {
  if (tx != nullptr) {
    tx->rocks->Rollback();
    _manager->endTransaction(tx->cache);
    delete tx->rocks;
    delete tx;
    return true;
  }
  return false;
}

bool TransactionalStore::insert(TransactionalStore::Transaction* tx,
                                TransactionalStore::Document const& document) {
  bool useInternalTransaction = (tx == nullptr);
  if (useInternalTransaction) {
    tx = beginTransaction(false);
  }

  bool inserted = false;
  Document d = lookup(tx, document.key);
  if (d.empty()) {  // ensure document with this key does not exist
    // blacklist in cache first
    _cache->blacklist(&(document.key), sizeof(uint64_t));

    // now write to rocksdb
    rocksdb::Slice kSlice(reinterpret_cast<char const*>(&(document.key)),
                          sizeof(uint64_t));
    rocksdb::Slice vSlice(reinterpret_cast<char const*>(&document),
                          sizeof(Document));
    auto status = tx->rocks->Put(kSlice, vSlice);
    inserted = status.ok();
  }

  if (useInternalTransaction) {
    bool ok = commit(tx);
    if (!ok) {
      rollback(tx);
      inserted = false;
    }
  }

  return inserted;
}

bool TransactionalStore::update(TransactionalStore::Transaction* tx,
                                TransactionalStore::Document const& document) {
  bool useInternalTransaction = (tx == nullptr);
  if (useInternalTransaction) {
    tx = beginTransaction(false);
  }

  bool updated = false;
  Document d = lookup(tx, document.key);
  if (!d.empty()) {  // ensure document with this key exists
    // blacklist in cache first
    _cache->blacklist(&(document.key), sizeof(uint64_t));

    // now write to rocksdb
    rocksdb::Slice kSlice(reinterpret_cast<char const*>(&(document.key)),
                          sizeof(uint64_t));
    rocksdb::Slice vSlice(reinterpret_cast<char const*>(&document),
                          sizeof(Document));
    auto status = tx->rocks->Put(kSlice, vSlice);
    updated = status.ok();
  }

  if (useInternalTransaction) {
    bool ok = commit(tx);
    if (!ok) {
      rollback(tx);
      updated = false;
    }
  }

  return updated;
}

bool TransactionalStore::remove(TransactionalStore::Transaction* tx,
                                uint64_t key) {
  bool useInternalTransaction = (tx == nullptr);
  if (useInternalTransaction) {
    tx = beginTransaction(false);
  }

  bool removed = false;
  Document d = lookup(tx, key);
  if (!d.empty()) {  // ensure document with this key exists
    // blacklist in cache first
    _cache->blacklist(&key, sizeof(uint64_t));

    // now write to rocksdb
    rocksdb::Slice kSlice(reinterpret_cast<char*>(&key), sizeof(uint64_t));
    auto status = tx->rocks->Delete(kSlice);
    removed = status.ok();
  }

  if (useInternalTransaction) {
    bool ok = commit(tx);
    if (!ok) {
      rollback(tx);
      removed = false;
    }
  }

  return removed;
}

TransactionalStore::Document TransactionalStore::lookup(
    TransactionalStore::Transaction* tx, uint64_t key) {
  bool useInternalTransaction = (tx == nullptr);
  if (useInternalTransaction) {
    tx = beginTransaction(true);
  }

  Document result;
  {
    Finding f = _cache->find(&key, sizeof(uint64_t));
    if (f.found()) {
      CachedValue const* cv = f.value();
      memcpy(&result, cv->value(), sizeof(Document));
    }
  }
  if (result.empty()) {
    auto readOptions = rocksdb::ReadOptions();
    readOptions.snapshot = tx->rocks->GetSnapshot();
    rocksdb::Slice kSlice(reinterpret_cast<char*>(&key), sizeof(uint64_t));
    std::string buffer;
    auto status = tx->rocks->Get(readOptions, kSlice, &buffer);
    if (status.ok()) {
      memcpy(&result, buffer.data(), sizeof(Document));
      CachedValue* value = CachedValue::construct(&key, sizeof(uint64_t),
                                                  &result, sizeof(Document));
      if (value) {
        auto status = _cache->insert(value);
        if (status.fail()) {
          delete value;
        }
      }
    }
  }

  if (useInternalTransaction) {
    bool ok = commit(tx);
    if (!ok) {
      rollback(tx);
    }
  }

  return result;
}
