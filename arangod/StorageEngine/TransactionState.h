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
class Transaction;
class TransactionCollection;

/// @brief transaction type
class TransactionState {
 public:
  TransactionState() = delete;
  TransactionState(TransactionState const&) = delete;
  TransactionState& operator=(TransactionState const&) = delete;

  explicit TransactionState(TRI_vocbase_t* vocbase);
  virtual ~TransactionState();

  double timeout() const { return _timeout; }
  void timeout(double value) { 
    if (value > 0.0) {
      _timeout = value;
    } 
  }
  bool waitForSync() const { return _waitForSync; }
  void waitForSync(bool value) { _waitForSync = value; }

  std::vector<std::string> collectionNames() const;

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
  
  int lockCollections();
  
  /// @brief whether or not a transaction consists of a single operation
  bool isSingleOperation() const {
    return hasHint(TransactionHints::Hint::SINGLE_OPERATION);
  }

  /// @brief update the status of a transaction
  void updateStatus(Transaction::Status status);
  
  /// @brief whether or not a specific hint is set for the transaction
  bool hasHint(TransactionHints::Hint hint) const {
    return _hints.has(hint);
  }

  /// @brief begin a transaction
  virtual int beginTransaction(TransactionHints hints, int nestingLevel) = 0;

  /// @brief commit a transaction
  virtual int commitTransaction(Transaction* trx, int nestingLevel) = 0;

  /// @brief abort a transaction
  virtual int abortTransaction(Transaction* trx, int nestingLevel) = 0;

  /// TODO: implement this in base class
  virtual bool hasFailedOperations() const = 0;

  /// @brief get the transaction id for usage in a marker
  TRI_voc_tid_t idForMarker() {
    if (isSingleOperation()) {
      return 0;
    }
    return _id;
  }

 protected:
  /// @brief find a collection in the transaction's list of collections
  TransactionCollection* findCollection(TRI_voc_cid_t cid, size_t& position) const;

  /// @brief whether or not a transaction is read-only
  bool isReadOnlyTransaction() const {
    return (_type == AccessMode::Type::READ);
  }

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

 protected:
  SmallVector<TransactionCollection*>::allocator_type::arena_type _arena; // memory for collections
  SmallVector<TransactionCollection*> _collections; // list of participating collections
 public:
  rocksdb::Transaction* _rocksTransaction;
  TransactionHints _hints;            // hints;
  int _nestingLevel;
  bool _allowImplicit;
  bool _hasOperations;
  bool _waitForSync;   // whether or not the transaction had a synchronous op
  bool _beginWritten;  // whether or not the begin marker was already written
  double _timeout;     // timeout for lock acquisition
};

}

#endif
