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

#include "RocksDBMethods.h"
#include "Logger/Logger.h"
#include "RocksDBEngine/RocksDBCommon.h"
#include "RocksDBEngine/RocksDBTransactionState.h"

#include <rocksdb/db.h>
#include <rocksdb/options.h>
#include <rocksdb/status.h>
#include <rocksdb/utilities/optimistic_transaction_db.h>
#include <rocksdb/utilities/transaction.h>
#include <rocksdb/utilities/write_batch_with_index.h>

using namespace arangodb;

// ================= RocksDBSavePoint ==================

RocksDBSavePoint::RocksDBSavePoint(
    RocksDBMethods* trx, bool handled,
    std::function<void()> const& rollbackCallback)
    : _trx(trx), _rollbackCallback(rollbackCallback), _handled(handled) {
  TRI_ASSERT(trx != nullptr);
  if (!_handled) {
    _trx->SetSavePoint();
  }
}

RocksDBSavePoint::~RocksDBSavePoint() {
  if (!_handled) {
    rollback();
  }
}

void RocksDBSavePoint::commit() {
  // note: _handled may already be true here
  _handled = true;  // this will prevent the rollback
}

void RocksDBSavePoint::rollback() {
  TRI_ASSERT(!_handled);
  _trx->RollbackToSavePoint();
  _handled = true;  // in order to not roll back again by accident
  _rollbackCallback();
}

// =================== RocksDBMethods ===================

rocksdb::ReadOptions const& RocksDBMethods::readOptions() {
  return _state->_rocksReadOptions;
}

// =================== RocksDBReadOnlyMethods ====================

RocksDBReadOnlyMethods::RocksDBReadOnlyMethods(RocksDBTransactionState* state)
    : RocksDBMethods(state) {
  _db = rocksutils::globalRocksDB();
}

bool RocksDBReadOnlyMethods::Exists(rocksdb::ColumnFamilyHandle* cf,
                                    RocksDBKey const& key) {
  TRI_ASSERT(cf != nullptr);
  std::string val;  // do not care about value
  bool mayExists = _db->KeyMayExist(_state->_rocksReadOptions, cf, key.string(),
                                    &val, nullptr);
  if (mayExists) {
    rocksdb::Status s =
        _db->Get(_state->_rocksReadOptions, cf, key.string(), &val);
    return !s.IsNotFound();
  }
  return false;
}

arangodb::Result RocksDBReadOnlyMethods::Get(rocksdb::ColumnFamilyHandle* cf,
                                             RocksDBKey const& key,
                                             std::string* val) {
  TRI_ASSERT(cf != nullptr);
  rocksdb::ReadOptions const& ro = _state->_rocksReadOptions;
  TRI_ASSERT(ro.snapshot != nullptr);
  rocksdb::Status s = _db->Get(ro, cf, key.string(), val);
  return s.ok() ? arangodb::Result() : rocksutils::convertStatus(s);
}

arangodb::Result RocksDBReadOnlyMethods::Put(rocksdb::ColumnFamilyHandle* cf,
                                             RocksDBKey const&,
                                             rocksdb::Slice const&,
                                             rocksutils::StatusHint) {
  THROW_ARANGO_EXCEPTION(TRI_ERROR_ARANGO_READ_ONLY);
}

arangodb::Result RocksDBReadOnlyMethods::Delete(rocksdb::ColumnFamilyHandle* cf,
                                                RocksDBKey const& key) {
  THROW_ARANGO_EXCEPTION(TRI_ERROR_ARANGO_READ_ONLY);
}

std::unique_ptr<rocksdb::Iterator> RocksDBReadOnlyMethods::NewIterator(
    rocksdb::ReadOptions const& opts, rocksdb::ColumnFamilyHandle* cf) {
  TRI_ASSERT(cf != nullptr);
  return std::unique_ptr<rocksdb::Iterator>(_db->NewIterator(opts, cf));
}

// =================== RocksDBTrxMethods ====================

RocksDBTrxMethods::RocksDBTrxMethods(RocksDBTransactionState* state)
    : RocksDBMethods(state) {}

bool RocksDBTrxMethods::Exists(rocksdb::ColumnFamilyHandle* cf,
                               RocksDBKey const& key) {
  TRI_ASSERT(cf != nullptr);
  std::string val;
  rocksdb::Status s = _state->_rocksTransaction->Get(_state->_rocksReadOptions,
                                                     cf, key.string(), &val);
  return !s.IsNotFound();
}

arangodb::Result RocksDBTrxMethods::Get(rocksdb::ColumnFamilyHandle* cf,
                                        RocksDBKey const& key,
                                        std::string* val) {
  TRI_ASSERT(cf != nullptr);
  rocksdb::ReadOptions const& ro = _state->_rocksReadOptions;
  TRI_ASSERT(ro.snapshot != nullptr);
  rocksdb::Status s = _state->_rocksTransaction->Get(ro, cf, key.string(), val);
  return s.ok() ? arangodb::Result() : rocksutils::convertStatus(s);
}

arangodb::Result RocksDBTrxMethods::Put(rocksdb::ColumnFamilyHandle* cf,
                                        RocksDBKey const& key,
                                        rocksdb::Slice const& val,
                                        rocksutils::StatusHint hint) {
  TRI_ASSERT(cf != nullptr);
  rocksdb::Status s = _state->_rocksTransaction->Put(cf, key.string(), val);
  return s.ok() ? arangodb::Result() : rocksutils::convertStatus(s, hint);
}

arangodb::Result RocksDBTrxMethods::Delete(rocksdb::ColumnFamilyHandle* cf,
                                           RocksDBKey const& key) {
  TRI_ASSERT(cf != nullptr);
  rocksdb::Status s = _state->_rocksTransaction->Delete(cf, key.string());
  return s.ok() ? arangodb::Result() : rocksutils::convertStatus(s);
}

std::unique_ptr<rocksdb::Iterator> RocksDBTrxMethods::NewIterator(
    rocksdb::ReadOptions const& opts, rocksdb::ColumnFamilyHandle* cf) {
  TRI_ASSERT(cf != nullptr);
  return std::unique_ptr<rocksdb::Iterator>(
      _state->_rocksTransaction->GetIterator(opts, cf));
}

void RocksDBTrxMethods::SetSavePoint() {
  _state->_rocksTransaction->SetSavePoint();
}

arangodb::Result RocksDBTrxMethods::RollbackToSavePoint() {
  return rocksutils::convertStatus(
      _state->_rocksTransaction->RollbackToSavePoint());
}

// =================== RocksDBBatchedMethods ====================

RocksDBBatchedMethods::RocksDBBatchedMethods(RocksDBTransactionState* state,
                                             rocksdb::WriteBatchWithIndex* wb)
    : RocksDBMethods(state), _wb(wb) {
  _db = rocksutils::globalRocksDB();
}

bool RocksDBBatchedMethods::Exists(rocksdb::ColumnFamilyHandle* cf,
                                   RocksDBKey const& key) {
  TRI_ASSERT(cf != nullptr);
  rocksdb::ReadOptions ro;
  std::string val;  // do not care about value
  rocksdb::Status s = _wb->GetFromBatchAndDB(_db, ro, cf, key.string(), &val);
  return !s.IsNotFound();
}

arangodb::Result RocksDBBatchedMethods::Get(rocksdb::ColumnFamilyHandle* cf,
                                            RocksDBKey const& key,
                                            std::string* val) {
  TRI_ASSERT(cf != nullptr);
  rocksdb::ReadOptions ro;
  rocksdb::Status s = _wb->GetFromBatchAndDB(_db, ro, cf, key.string(), val);
  return s.ok() ? arangodb::Result() : rocksutils::convertStatus(s);
}

arangodb::Result RocksDBBatchedMethods::Put(rocksdb::ColumnFamilyHandle* cf,
                                            RocksDBKey const& key,
                                            rocksdb::Slice const& val,
                                            rocksutils::StatusHint) {
  TRI_ASSERT(cf != nullptr);
  _wb->Put(cf, key.string(), val);
  return arangodb::Result();
}

arangodb::Result RocksDBBatchedMethods::Delete(rocksdb::ColumnFamilyHandle* cf,
                                               RocksDBKey const& key) {
  TRI_ASSERT(cf != nullptr);
  _wb->Delete(cf, key.string());
  return arangodb::Result();
}

std::unique_ptr<rocksdb::Iterator> RocksDBBatchedMethods::NewIterator(
    rocksdb::ReadOptions const& ro, rocksdb::ColumnFamilyHandle* cf) {
  TRI_ASSERT(cf != nullptr);
  return std::unique_ptr<rocksdb::Iterator>(
      _wb->NewIteratorWithBase(_db->NewIterator(ro, cf)));
}
