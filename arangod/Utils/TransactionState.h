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

struct TRI_transaction_collection_t;

/// @brief transaction statuses
enum TRI_transaction_status_e {
  TRI_TRANSACTION_UNDEFINED = 0,
  TRI_TRANSACTION_CREATED = 1,
  TRI_TRANSACTION_RUNNING = 2,
  TRI_TRANSACTION_COMMITTED = 3,
  TRI_TRANSACTION_ABORTED = 4
};

/// @brief transaction type
struct TransactionState {
  TransactionState(TRI_vocbase_t* vocbase, double timeout, bool waitForSync);
  ~TransactionState();

  bool hasFailedOperations() const {
    return (_hasOperations && _status == TRI_TRANSACTION_ABORTED);
  }
  
  TRI_vocbase_t* _vocbase;            // vocbase
  TRI_voc_tid_t _id;                  // local trx id
  AccessMode::Type _type;       // access type (read|write)
  TRI_transaction_status_e _status;   // current status
  SmallVector<TRI_transaction_collection_t*>::allocator_type::arena_type _arena; // memory for collections
  SmallVector<TRI_transaction_collection_t*> _collections; // list of participating collections
  rocksdb::Transaction* _rocksTransaction;
  TransactionHints _hints;      // hints;
  int _nestingLevel;
  bool _allowImplicit;
  bool _hasOperations;
  bool _waitForSync;   // whether or not the collection had a synchronous op
  bool _beginWritten;  // whether or not the begin marker was already written
  double _timeout;     // timeout for lock acquisition
};

/// @brief collection used in a transaction
struct TRI_transaction_collection_t {
  TRI_transaction_collection_t(TransactionState* trx, TRI_voc_cid_t cid, AccessMode::Type accessType, int nestingLevel)
      : _transaction(trx), _cid(cid), _accessType(accessType), _nestingLevel(nestingLevel), _collection(nullptr), _operations(nullptr),
        _originalRevision(0), _lockType(AccessMode::Type::NONE), _compactionLocked(false), _waitForSync(false) {}

  TransactionState* _transaction;     // the transaction
  TRI_voc_cid_t const _cid;                  // collection id
  AccessMode::Type _accessType;  // access type (read|write)
  int _nestingLevel;  // the transaction level that added this collection
  LogicalCollection* _collection;  // vocbase collection pointer
  std::vector<MMFilesDocumentOperation*>* _operations;
  TRI_voc_rid_t _originalRevision;   // collection revision at trx start
  AccessMode::Type _lockType;  // collection lock type
  bool
      _compactionLocked;  // was the compaction lock grabbed for the collection?
  bool _waitForSync;      // whether or not the collection has waitForSync
};


/// @brief get the transaction id for usage in a marker
static inline TRI_voc_tid_t TRI_MarkerIdTransaction(
    TransactionState const* trx) {
  if (trx->_hints.has(TransactionHints::Hint::SINGLE_OPERATION)) {
    return 0;
  }

  return trx->_id;
}

/// @brief return the collection from a transaction
TRI_transaction_collection_t* TRI_GetCollectionTransaction(
    TransactionState const*, TRI_voc_cid_t, AccessMode::Type);

/// @brief add a collection to a transaction
int TRI_AddCollectionTransaction(TransactionState*, TRI_voc_cid_t,
                                 AccessMode::Type, int, bool, bool);

/// @brief make sure all declared collections are used & locked
int TRI_EnsureCollectionsTransaction(TransactionState*, int = 0);

/// @brief request a lock for a collection
int TRI_LockCollectionTransaction(TRI_transaction_collection_t*,
                                  AccessMode::Type, int);

/// @brief request an unlock for a collection
int TRI_UnlockCollectionTransaction(TRI_transaction_collection_t*,
                                    AccessMode::Type, int);

/// @brief check whether a collection is locked in a transaction
bool TRI_IsLockedCollectionTransaction(TRI_transaction_collection_t const*,
                                       AccessMode::Type, int);

/// @brief check whether a collection is locked in a transaction
bool TRI_IsLockedCollectionTransaction(TRI_transaction_collection_t const*);

/// @brief check whether a collection is contained in a transaction
bool TRI_IsContainedCollectionTransaction(TransactionState*, TRI_voc_cid_t);

/// @brief add a WAL operation for a transaction collection
int TRI_AddOperationTransaction(TransactionState*, TRI_voc_rid_t,
                                MMFilesDocumentOperation&,
                                MMFilesWalMarker const* marker, bool&);

/// @brief begin a transaction
int TRI_BeginTransaction(TransactionState*, TransactionHints, int);

/// @brief commit a transaction
int TRI_CommitTransaction(Transaction*, TransactionState*, int);

/// @brief abort a transaction
int TRI_AbortTransaction(Transaction*, TransactionState*, int);

/// @brief whether or not a transaction consists of a single operation
bool TRI_IsSingleOperationTransaction(TransactionState const*);

}

#endif
