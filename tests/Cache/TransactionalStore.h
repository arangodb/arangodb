////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2014-2020 ArangoDB GmbH, Cologne, Germany
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
/// @author Dan Larkin-York
/// @author Copyright 2017, ArangoDB GmbH, Cologne, Germany
////////////////////////////////////////////////////////////////////////////////

#ifndef UNITTESTS_CACHE_TRANSACTIONAL_STORE_H
#define UNITTESTS_CACHE_TRANSACTIONAL_STORE_H

#include <chrono>

#include <rocksdb/db.h>
#include <rocksdb/options.h>
#include <rocksdb/utilities/transaction.h>
#include <rocksdb/utilities/transaction_db.h>

#include "Basics/StringBuffer.h"
#include "Cache/Manager.h"
#include "Cache/TransactionalCache.h"

namespace arangodb {
namespace cache {

class TransactionalStore {
 public:
  struct Document {
    std::uint64_t key;
    std::uint64_t timestamp;
    std::uint64_t sequence;

    Document();
    Document(std::uint64_t k);
    void advance();
    void clear();
    bool empty() const;
  };

  struct Transaction {
    arangodb::cache::Transaction* cache;
    rocksdb::Transaction* rocks;

    Transaction(arangodb::cache::Transaction* c, rocksdb::Transaction* r);
  };

 public:
  TransactionalStore(Manager* manager);
  ~TransactionalStore();

  Cache* cache();

  TransactionalStore::Transaction* beginTransaction(bool readOnly);
  bool commit(TransactionalStore::Transaction* tx);
  bool rollback(TransactionalStore::Transaction* tx);

  bool insert(TransactionalStore::Transaction* tx, Document const& document);
  bool update(TransactionalStore::Transaction* tx, Document const& document);
  bool remove(TransactionalStore::Transaction* tx, std::uint64_t key);
  Document lookup(TransactionalStore::Transaction* tx, std::uint64_t key);

 private:
  static std::atomic<std::uint32_t> _sequence;
  Manager* _manager;
  std::shared_ptr<Cache> _cache;

  arangodb::basics::StringBuffer _directory;
  std::uint32_t _id;
  rocksdb::TransactionDB* _db;
  rocksdb::ReadOptions _readOptions;
  rocksdb::WriteOptions _writeOptions;
  rocksdb::TransactionOptions _txOptions;
};

};  // end namespace cache
};  // end namespace arangodb

#endif
