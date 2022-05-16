////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2014-2022 ArangoDB GmbH, Cologne, Germany
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
/// @author Manuel Pöter
////////////////////////////////////////////////////////////////////////////////

#pragma once

#include "RocksDBEngine/RocksDBCommon.h"
#include "RocksDBEngine/RocksDBMethods.h"

namespace rocksdb {
class Slice;
}  // namespace rocksdb

namespace arangodb {
class RocksDBKey;
class RocksDBTransactionState;

struct ReadOptions : public rocksdb::ReadOptions {
  bool readOwnWrites = false;
#ifdef ARANGODB_ENABLE_MAINTAINER_MODE
  // only used to control whether we verify in NewIterator that we do not
  // create a read-own-write iterator with intermediate commits enabled.
  bool checkIntermediateCommits = true;
#endif
};

class RocksDBTransactionMethods : public RocksDBMethods {
 public:
  explicit RocksDBTransactionMethods(RocksDBTransactionState* state)
      : _state(state) {}
  virtual ~RocksDBTransactionMethods() = default;

  virtual Result beginTransaction() = 0;

  virtual Result commitTransaction() = 0;

  virtual Result abortTransaction() = 0;

  // Only relevant for RocksDBTrxMethods
  virtual Result checkIntermediateCommit() { return {}; }

  /// @returns tick of last operation in a transaction
  /// @note the value is guaranteed to be valid only after
  ///       transaction is committed
  virtual TRI_voc_tick_t lastOperationTick() const noexcept = 0;

  virtual uint64_t numCommits() const noexcept = 0;

  virtual rocksdb::ReadOptions iteratorReadOptions()
      const = 0;  // TODO - remove later

  /// @brief acquire a database snapshot if we do not yet have one.
  /// Returns true if a snapshot was acquire
  virtual bool ensureSnapshot() = 0;

  virtual rocksdb::SequenceNumber GetSequenceNumber() const noexcept = 0;

  virtual bool hasOperations() const noexcept = 0;

  virtual uint64_t numOperations() const noexcept = 0;

  virtual void prepareOperation(DataSourceId cid, RevisionId rid,
                                TRI_voc_document_operation_e operationType) = 0;

  /// @brief undo the effects of the previous prepareOperation call
  virtual void rollbackOperation(
      TRI_voc_document_operation_e operationType) = 0;

  /// @brief add an operation for a transaction
  virtual Result addOperation(TRI_voc_document_operation_e opType) = 0;

  using ReadOptionsCallback = std::function<void(ReadOptions&)>;

  virtual std::unique_ptr<rocksdb::Iterator> NewIterator(
      rocksdb::ColumnFamilyHandle*, ReadOptionsCallback) = 0;

  virtual bool iteratorMustCheckBounds(ReadOwnWrites) const = 0;

  virtual void SetSavePoint() = 0;
  virtual rocksdb::Status RollbackToSavePoint() = 0;
  virtual rocksdb::Status RollbackToWriteBatchSavePoint() = 0;
  virtual void PopSavePoint() = 0;

#ifdef ARANGODB_ENABLE_MAINTAINER_MODE
  std::size_t countInBounds(RocksDBKeyBounds const& bounds,
                            bool isElementInRange = false);
#endif

 protected:
  RocksDBTransactionState* _state;
};

}  // namespace arangodb
