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

// =================== RocksDBReadOnlyMethods ====================

RocksDBReadOnlyMethods::RocksDBReadOnlyMethods(RocksDBTransactionState* state)
    : _state(state) {
  _db = rocksutils::globalRocksDB();
}

arangodb::Result RocksDBReadOnlyMethods::Get(RocksDBKey const& key,
                                             std::string* val) {
  rocksdb::Status s = _db->Get(_state->_rocksReadOptions, key.string(), val);
  return s.ok() ? arangodb::Result() : rocksutils::convertStatus(s);
}

arangodb::Result RocksDBReadOnlyMethods::Put(RocksDBKey const& key,
                                             rocksdb::Slice const& val) {
  THROW_ARANGO_EXCEPTION(TRI_ERROR_ARANGO_READ_ONLY);
}

arangodb::Result RocksDBReadOnlyMethods::Delete(RocksDBKey const& key) {
  THROW_ARANGO_EXCEPTION(TRI_ERROR_ARANGO_READ_ONLY);
}

std::unique_ptr<rocksdb::Iterator> RocksDBReadOnlyMethods::NewIterator() {
  return std::unique_ptr<rocksdb::Iterator>(
      _db->NewIterator(_state->_rocksReadOptions));
}

// =================== RocksDBTrxMethods ====================

RocksDBTrxMethods::RocksDBTrxMethods(RocksDBTransactionState* state)
    : _state(state) {}

arangodb::Result RocksDBTrxMethods::Get(RocksDBKey const& key,
                                        std::string* val) {
  rocksdb::Status s = _state->_rocksTransaction->Get(_state->_rocksReadOptions,
                                                     key.string(), val);
  return s.ok() ? arangodb::Result() : rocksutils::convertStatus(s);
}

arangodb::Result RocksDBTrxMethods::Put(RocksDBKey const& key,
                                        rocksdb::Slice const& val) {
  rocksdb::Status s = _state->_rocksTransaction->Put(key.string(), val);
  return s.ok() ? arangodb::Result() : rocksutils::convertStatus(s);
}

arangodb::Result RocksDBTrxMethods::Delete(RocksDBKey const& key) {
  rocksdb::Status s = _state->_rocksTransaction->Delete(key.string());
  return s.ok() ? arangodb::Result() : rocksutils::convertStatus(s);
}

std::unique_ptr<rocksdb::Iterator> RocksDBTrxMethods::NewIterator() {
  return std::unique_ptr<rocksdb::Iterator>(
      _state->_rocksTransaction->GetIterator(_state->_rocksReadOptions));
}

void RocksDBTrxMethods::SetSavePoint() {
  _state->_rocksTransaction->SetSavePoint();
}

arangodb::Result RocksDBTrxMethods::RollbackToSavePoint() {
  return rocksutils::convertStatus(
      _state->_rocksTransaction->RollbackToSavePoint());
}
