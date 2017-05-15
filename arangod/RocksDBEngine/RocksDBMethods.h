////////////////////////////////////////////////////////////////////////////////
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
/// @author Simon Grätzer
////////////////////////////////////////////////////////////////////////////////

#ifndef ARANGOD_ROCKSDB_ROCKSDB_METHODS_H
#define ARANGOD_ROCKSDB_ROCKSDB_METHODS_H 1

#include "Basics/Result.h"

namespace rocksdb {
class Transaction;
class Slice;
class Iterator;
class TransactionDB;
}  // namespace rocksdb

namespace arangodb {

class RocksDBKey;
class RocksDBMethods;
class RocksDBTransactionState;

class RocksDBSavePoint {
 public:
  RocksDBSavePoint(RocksDBMethods* trx, bool handled,
                   std::function<void()> const& rollbackCallback);
  ~RocksDBSavePoint();

  void commit();

 private:
  void rollback();

 private:
  RocksDBMethods* _trx;
  std::function<void()> const _rollbackCallback;
  bool _handled;
};

class RocksDBMethods {
 public:
  // RocksDBOperations(rocksdb::ReadOptions ro, rocksdb::WriteOptions wo) :
  // _readOptions(ro), _writeOptions(wo) {}
  virtual ~RocksDBMethods() {}
  virtual arangodb::Result Get(RocksDBKey const&, std::string*) = 0;
  virtual arangodb::Result Put(RocksDBKey const&, rocksdb::Slice const&) = 0;
  // virtual arangodb::Result Merge(RocksDBKey const&, rocksdb::Slice const&) =
  // 0;
  virtual arangodb::Result Delete(RocksDBKey const&) = 0;
  virtual std::unique_ptr<rocksdb::Iterator> NewIterator() = 0;

  virtual void SetSavePoint() = 0;
  virtual arangodb::Result RollbackToSavePoint() = 0;
};

class RocksDBReadOnlyMethods : public RocksDBMethods {
 public:
  RocksDBReadOnlyMethods(RocksDBTransactionState* state);

  arangodb::Result Get(RocksDBKey const& key, std::string* val) override;

  arangodb::Result Put(RocksDBKey const& key,
                       rocksdb::Slice const& val) override;
  arangodb::Result Delete(RocksDBKey const& key) override;
  std::unique_ptr<rocksdb::Iterator> NewIterator() override;

  void SetSavePoint() override {}
  arangodb::Result RollbackToSavePoint() override { return arangodb::Result(); }

 private:
  RocksDBTransactionState* _state;
  rocksdb::TransactionDB* _db;
};

/// transactional operations
class RocksDBTrxMethods : public RocksDBMethods {
 public:
  RocksDBTrxMethods(RocksDBTransactionState* state);

  arangodb::Result Get(RocksDBKey const& key, std::string* val) override;

  arangodb::Result Put(RocksDBKey const& key,
                       rocksdb::Slice const& val) override;
  arangodb::Result Delete(RocksDBKey const& key) override;
  std::unique_ptr<rocksdb::Iterator> NewIterator() override;

  void SetSavePoint() override;
  arangodb::Result RollbackToSavePoint() override;

 private:
  RocksDBTransactionState* _state;
};

}  // namespace arangodb

#endif
