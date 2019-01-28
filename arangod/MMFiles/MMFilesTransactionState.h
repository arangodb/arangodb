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
#include "Basics/SmallVector.h"
#include "StorageEngine/TransactionState.h"
#include "Transaction/Hints.h"
#include "Transaction/Methods.h"
#include "VocBase/AccessMode.h"
#include "VocBase/LocalDocumentId.h"
#include "VocBase/voc-types.h"

struct TRI_vocbase_t;

namespace rocksdb {

class Transaction;
}

namespace arangodb {

class LogicalCollection;
struct MMFilesDocumentOperation;
class MMFilesWalMarker;

namespace transaction {

class Methods;
struct Options;

}  // namespace transaction

class TransactionCollection;

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

 private:
  /// @brief whether or not a marker needs to be written
  bool needWriteMarker(bool isBeginMarker) const {
    if (isBeginMarker) {
      return (!isReadOnlyTransaction() && !isSingleOperation());
    }

    return (_nestingLevel == 0 && _beginWritten && !isReadOnlyTransaction() &&
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

  rocksdb::Transaction* _rocksTransaction;
  bool _beginWritten;
  bool _hasOperations;
};

}  // namespace arangodb

#endif
