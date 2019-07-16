////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2014-2016 ArangoDB GmbH, Cologne, Germany
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

#ifndef ARANGOD_MMFILES_TRANSACTION_STATE_H
#define ARANGOD_MMFILES_TRANSACTION_STATE_H 1

#include "Basics/Common.h"
#include "Basics/Result.h"
#include "StorageEngine/TransactionState.h"

namespace rocksdb {
class Transaction;
}

namespace arangodb {

class LocalDocumentId;
class LogicalCollection;
struct MMFilesDocumentOperation;
class MMFilesWalMarker;

/// @brief transaction type
class MMFilesTransactionState final : public TransactionState {
 public:
  MMFilesTransactionState(TRI_vocbase_t& vocbase, TRI_voc_tid_t tid,
                          transaction::Options const& options);
  ~MMFilesTransactionState();

  /// @brief begin a transaction
  Result beginTransaction(transaction::Hints hints) override;

  /// @brief commit a transaction
  Result commitTransaction(transaction::Methods* trx) override;

  /// @brief abort a transaction
  Result abortTransaction(transaction::Methods* trx) override;

  bool hasFailedOperations() const override {
    return (_hasOperations && _status == transaction::Status::ABORTED);
  }

  /// @brief add a WAL operation for a transaction collection
  int addOperation(LocalDocumentId const& documentId, TRI_voc_rid_t revisionId,
                   MMFilesDocumentOperation&, MMFilesWalMarker const* marker, bool&);

  /// @brief get the transaction id for usage in a marker
  TRI_voc_tid_t idForMarker() {
    if (isSingleOperation()) {
      return 0;
    }
    return _id;
  }

  /// @brief get (or create) a rocksdb WriteTransaction
  rocksdb::Transaction* rocksTransaction();

  /// @returns tick of last operation in a transaction
  /// @note the value is valid only after transaction is committed
  TRI_voc_tick_t lastOperationTick() const noexcept final { return _lastWrittenOperationTick; }

 private:
  /// @brief whether or not a marker needs to be written
  bool needWriteMarker(bool isBeginMarker) const {
    if (isBeginMarker) {
      return (!isReadOnlyTransaction() && !isSingleOperation());
    }

    return (isTopLevelTransaction() && _beginWritten && !isReadOnlyTransaction() &&
            !isSingleOperation());
  }

  /// @brief write WAL begin marker
  int writeBeginMarker();

  /// @brief write WAL abort marker
  int writeAbortMarker();

  /// @brief write WAL commit marker
  int writeCommitMarker();

  /// @brief free all operations for a transaction
  void freeOperations(transaction::Methods* activeTrx);
  
 private:

  rocksdb::Transaction* _rocksTransaction;
  bool _beginWritten;
  bool _hasOperations;
  
  /// @brief tick of last added & written operation
  TRI_voc_tick_t _lastWrittenOperationTick;
};

}  // namespace arangodb

#endif
