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

#ifndef ARANGOD_UTILS_TRANSACTION_STATE_H
#define ARANGOD_UTILS_TRANSACTION_STATE_H 1

#include "Basics/Common.h"
#include "Basics/SmallVector.h"
#include "Utils/Transaction.h"
#include "Utils/TransactionHints.h"
#include "VocBase/AccessMode.h"
#include "VocBase/voc-types.h"
                                
struct TRI_vocbase_t;

namespace rocksdb {
class Transaction;
}

namespace arangodb {
class LogicalCollection;
struct MMFilesDocumentOperation;
class MMFilesWalMarker;
class Transaction;
struct TransactionCollection;

/// @brief transaction type
struct TransactionState {
  TransactionState() = delete;
  TransactionState(TransactionState const&) = delete;
  TransactionState& operator=(TransactionState const&) = delete;

  TransactionState(TRI_vocbase_t* vocbase, double timeout, bool waitForSync);
  ~TransactionState();

 public:

  /// @brief return the collection from a transaction
  TransactionCollection* collection(TRI_voc_cid_t cid, AccessMode::Type accessType);

  /// @brief add a collection to a transaction
  int addCollection(TRI_voc_cid_t cid, AccessMode::Type accessType, int nestingLevel, bool force, bool allowImplicitCollections);

  /// @brief make sure all declared collections are used & locked
  int ensureCollections(int nestingLevel = 0);

  /// @brief use all participating collections of a transaction
  int useCollections(int nestingLevel);
  
  /// @brief release collection locks for a transaction
  int unuseCollections(int nestingLevel);
  
  /// @brief whether or not a transaction consists of a single operation
  bool isSingleOperation() const;

  /// @brief begin a transaction
  int beginTransaction(TransactionHints hints, int nestingLevel);

  /// @brief commit a transaction
  int commitTransaction(Transaction* trx, int nestingLevel);

  /// @brief abort a transaction
  int abortTransaction(Transaction* trx, int nestingLevel);

  /// @brief add a WAL operation for a transaction collection
  int addOperation(TRI_voc_rid_t, MMFilesDocumentOperation&, MMFilesWalMarker const* marker, bool&);

  /// @brief update the status of a transaction
  void updateStatus(Transaction::Status status);

  bool hasFailedOperations() const {
    return (_hasOperations && _status == Transaction::Status::ABORTED);
  }

  /// @brief whether or not a specific hint is set for the transaction
  bool hasHint(TransactionHints::Hint hint) const {
    return _hints.has(hint);
  }

 private:
  /// @brief whether or not a transaction is read-only
  bool isReadOnlyTransaction() const {
    return (_type == AccessMode::Type::READ);
  }

  /// @brief whether or not a marker needs to be written
  bool needWriteMarker(bool isBeginMarker) const {
    if (isBeginMarker) {
      return (!isReadOnlyTransaction() && !isSingleOperation());
    }

    return (_nestingLevel == 0 && _beginWritten &&
            !isReadOnlyTransaction() && !isSingleOperation());
  }

  /// @brief write WAL begin marker
  int writeBeginMarker();

  /// @brief write WAL abort marker
  int writeAbortMarker();

  /// @brief write WAL commit marker
  int writeCommitMarker();

  /// @brief free all operations for a transaction
  void freeOperations(arangodb::Transaction* activeTrx);

  /// @brief release collection locks for a transaction
  int releaseCollections();

  /// @brief clear the query cache for all collections that were modified by
  /// the transaction
  void clearQueryCache();

 public:
  
  TRI_vocbase_t* _vocbase;            // vocbase
  TRI_voc_tid_t _id;                  // local trx id
  AccessMode::Type _type;             // access type (read|write)
  Transaction::Status _status;        // current status
  SmallVector<TransactionCollection*>::allocator_type::arena_type _arena; // memory for collections
  SmallVector<TransactionCollection*> _collections; // list of participating collections
  rocksdb::Transaction* _rocksTransaction;
  TransactionHints _hints;            // hints;
  int _nestingLevel;
  bool _allowImplicit;
  bool _hasOperations;
  bool _waitForSync;   // whether or not the collection had a synchronous op
  bool _beginWritten;  // whether or not the begin marker was already written
  double _timeout;     // timeout for lock acquisition
};

/// @brief get the transaction id for usage in a marker
static inline TRI_voc_tid_t TRI_MarkerIdTransaction(
    TransactionState const* trx) {
  if (trx->_hints.has(TransactionHints::Hint::SINGLE_OPERATION)) {
    return 0;
  }

  return trx->_id;
}

}

#endif
