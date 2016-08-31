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

#ifndef ARANGOD_VOC_BASE_TRANSACTION_H
#define ARANGOD_VOC_BASE_TRANSACTION_H 1

#include "Basics/Common.h"
#include "Basics/SmallVector.h"
#include "VocBase/voc-types.h"

#ifdef ARANGODB_ENABLE_ROCKSDB
namespace rocksdb {
class Transaction;
}
#endif

namespace arangodb {
class DocumentDitch;
class LogicalCollection;

namespace wal {
struct DocumentOperation;
}
}

struct TRI_transaction_collection_t;
struct TRI_vocbase_t;

////////////////////////////////////////////////////////////////////////////////
/// @brief time (in µs) that is spent waiting for a lock
////////////////////////////////////////////////////////////////////////////////

#define TRI_TRANSACTION_DEFAULT_LOCK_TIMEOUT 30000000ULL

////////////////////////////////////////////////////////////////////////////////
/// @brief sleep time (in µs) while waiting for lock acquisition
////////////////////////////////////////////////////////////////////////////////

#define TRI_TRANSACTION_DEFAULT_SLEEP_DURATION 10000ULL

////////////////////////////////////////////////////////////////////////////////
/// @brief transaction type
////////////////////////////////////////////////////////////////////////////////

enum TRI_transaction_type_e {
  TRI_TRANSACTION_NONE = 0,
  TRI_TRANSACTION_READ = 1,
  TRI_TRANSACTION_WRITE = 2
};

////////////////////////////////////////////////////////////////////////////////
/// @brief transaction statuses
////////////////////////////////////////////////////////////////////////////////

enum TRI_transaction_status_e {
  TRI_TRANSACTION_UNDEFINED = 0,
  TRI_TRANSACTION_CREATED = 1,
  TRI_TRANSACTION_RUNNING = 2,
  TRI_TRANSACTION_COMMITTED = 3,
  TRI_TRANSACTION_ABORTED = 4
};

////////////////////////////////////////////////////////////////////////////////
/// @brief typedef for transaction hints
////////////////////////////////////////////////////////////////////////////////

typedef uint32_t TRI_transaction_hint_t;

////////////////////////////////////////////////////////////////////////////////
/// @brief hints that can be used for transactions
////////////////////////////////////////////////////////////////////////////////

enum TRI_transaction_hint_e {
  TRI_TRANSACTION_HINT_NONE = 0,
  TRI_TRANSACTION_HINT_SINGLE_OPERATION = 1,
  TRI_TRANSACTION_HINT_LOCK_ENTIRELY = 2,
  TRI_TRANSACTION_HINT_LOCK_NEVER = 4,
  TRI_TRANSACTION_HINT_NO_BEGIN_MARKER = 8,
  TRI_TRANSACTION_HINT_NO_ABORT_MARKER = 16,
  TRI_TRANSACTION_HINT_NO_THROTTLING = 32,
  TRI_TRANSACTION_HINT_TRY_LOCK = 64,
  TRI_TRANSACTION_HINT_NO_COMPACTION_LOCK = 128,
  TRI_TRANSACTION_HINT_NO_USAGE_LOCK = 256,
  TRI_TRANSACTION_HINT_RECOVERY = 512
};

////////////////////////////////////////////////////////////////////////////////
/// @brief transaction type
////////////////////////////////////////////////////////////////////////////////

struct TRI_transaction_t {
  TRI_transaction_t(TRI_vocbase_t* vocbase, double timeout, bool waitForSync);
  ~TRI_transaction_t();

  bool hasFailedOperations() const {
    return (_hasOperations && _status == TRI_TRANSACTION_ABORTED);
  }
  
  TRI_vocbase_t* _vocbase;            // vocbase
  TRI_voc_tid_t _id;                  // local trx id
  TRI_transaction_type_e _type;       // access type (read|write)
  TRI_transaction_status_e _status;   // current status
  arangodb::SmallVector<TRI_transaction_collection_t*>::allocator_type::arena_type _arena; // memory for collections
  arangodb::SmallVector<TRI_transaction_collection_t*> _collections; // list of participating collections
#ifdef ARANGODB_ENABLE_ROCKSDB
  rocksdb::Transaction* _rocksTransaction;
#endif
  TRI_transaction_hint_t _hints;      // hints;
  int _nestingLevel;
  bool _allowImplicit;
  bool _hasOperations;
  bool _waitForSync;   // whether or not the collection had a synchronous op
  bool _beginWritten;  // whether or not the begin marker was already written
  uint64_t _timeout;   // timeout for lock acquisition
};

////////////////////////////////////////////////////////////////////////////////
/// @brief collection used in a transaction
////////////////////////////////////////////////////////////////////////////////

struct TRI_transaction_collection_t {
  TRI_transaction_collection_t(TRI_transaction_t* trx, TRI_voc_cid_t cid, TRI_transaction_type_e accessType, int nestingLevel)
      : _transaction(trx), _cid(cid), _accessType(accessType), _nestingLevel(nestingLevel), _collection(nullptr), _operations(nullptr),
        _originalRevision(0), _lockType(TRI_TRANSACTION_NONE), _compactionLocked(false), _waitForSync(false) {}

  TRI_transaction_t* _transaction;     // the transaction
  TRI_voc_cid_t const _cid;                  // collection id
  TRI_transaction_type_e _accessType;  // access type (read|write)
  int _nestingLevel;  // the transaction level that added this collection
  arangodb::LogicalCollection* _collection;  // vocbase collection pointer
  std::vector<arangodb::wal::DocumentOperation*>* _operations;
  TRI_voc_rid_t _originalRevision;   // collection revision at trx start
  TRI_transaction_type_e _lockType;  // collection lock type
  bool
      _compactionLocked;  // was the compaction lock grabbed for the collection?
  bool _waitForSync;      // whether or not the collection has waitForSync
};

////////////////////////////////////////////////////////////////////////////////
/// @brief return the type of the transaction as a string
////////////////////////////////////////////////////////////////////////////////

inline char const* TRI_TransactionTypeGetStr(TRI_transaction_type_e t) {
  switch (t) {
    case TRI_TRANSACTION_READ:
      return "read";
    case TRI_TRANSACTION_WRITE:
      return "write";
    case TRI_TRANSACTION_NONE: {
    }
  }
  return "";
}

////////////////////////////////////////////////////////////////////////////////
/// @brief get the transaction type from a string
////////////////////////////////////////////////////////////////////////////////

TRI_transaction_type_e TRI_GetTransactionTypeFromStr(char const*);

////////////////////////////////////////////////////////////////////////////////
/// @brief get the transaction id for usage in a marker
////////////////////////////////////////////////////////////////////////////////

static inline TRI_voc_tid_t TRI_MarkerIdTransaction(
    TRI_transaction_t const* trx) {
  if ((trx->_hints &
       (TRI_transaction_hint_t)TRI_TRANSACTION_HINT_SINGLE_OPERATION) != 0) {
    return 0;
  }

  return trx->_id;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief return the collection from a transaction
////////////////////////////////////////////////////////////////////////////////

TRI_transaction_collection_t* TRI_GetCollectionTransaction(
    TRI_transaction_t const*, TRI_voc_cid_t, TRI_transaction_type_e);

////////////////////////////////////////////////////////////////////////////////
/// @brief add a collection to a transaction
////////////////////////////////////////////////////////////////////////////////

int TRI_AddCollectionTransaction(TRI_transaction_t*, TRI_voc_cid_t,
                                 TRI_transaction_type_e, int, bool, bool);

////////////////////////////////////////////////////////////////////////////////
/// @brief make sure all declared collections are used & locked
////////////////////////////////////////////////////////////////////////////////

int TRI_EnsureCollectionsTransaction(TRI_transaction_t*, int = 0);

////////////////////////////////////////////////////////////////////////////////
/// @brief request a lock for a collection
////////////////////////////////////////////////////////////////////////////////

int TRI_LockCollectionTransaction(TRI_transaction_collection_t*,
                                  TRI_transaction_type_e, int);

////////////////////////////////////////////////////////////////////////////////
/// @brief request an unlock for a collection
////////////////////////////////////////////////////////////////////////////////

int TRI_UnlockCollectionTransaction(TRI_transaction_collection_t*,
                                    TRI_transaction_type_e, int);

////////////////////////////////////////////////////////////////////////////////
/// @brief check whether a collection is locked in a transaction
////////////////////////////////////////////////////////////////////////////////

bool TRI_IsLockedCollectionTransaction(TRI_transaction_collection_t const*,
                                       TRI_transaction_type_e, int);

////////////////////////////////////////////////////////////////////////////////
/// @brief check whether a collection is locked in a transaction
////////////////////////////////////////////////////////////////////////////////

bool TRI_IsLockedCollectionTransaction(TRI_transaction_collection_t const*);

////////////////////////////////////////////////////////////////////////////////
/// @brief check whether a collection is contained in a transaction
////////////////////////////////////////////////////////////////////////////////

bool TRI_IsContainedCollectionTransaction(TRI_transaction_t*, TRI_voc_cid_t);

////////////////////////////////////////////////////////////////////////////////
/// @brief begin a transaction
////////////////////////////////////////////////////////////////////////////////

int TRI_BeginTransaction(TRI_transaction_t*, TRI_transaction_hint_t, int);

////////////////////////////////////////////////////////////////////////////////
/// @brief commit a transaction
////////////////////////////////////////////////////////////////////////////////

int TRI_CommitTransaction(TRI_transaction_t*, int);

////////////////////////////////////////////////////////////////////////////////
/// @brief abort a transaction
////////////////////////////////////////////////////////////////////////////////

int TRI_AbortTransaction(TRI_transaction_t*, int);

////////////////////////////////////////////////////////////////////////////////
/// @brief whether or not a transaction consists of a single operation
////////////////////////////////////////////////////////////////////////////////

bool TRI_IsSingleOperationTransaction(TRI_transaction_t const*);

#endif
