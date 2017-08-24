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

#ifndef ARANGOD_STORAGE_ENGINE_TRANSACTION_COLLECTION_H
#define ARANGOD_STORAGE_ENGINE_TRANSACTION_COLLECTION_H 1

#include "Basics/Common.h"
#include "VocBase/AccessMode.h"
#include "VocBase/voc-types.h"
                                
namespace arangodb {
class LogicalCollection;
namespace transaction {
class Methods;
}

class TransactionState;

/// @brief collection used in a transaction
class TransactionCollection {
 public:
  TransactionCollection(TransactionCollection const&) = delete;
  TransactionCollection& operator=(TransactionCollection const&) = delete;

  TransactionCollection(TransactionState* trx, TRI_voc_cid_t cid, AccessMode::Type accessType)
      : _transaction(trx), _cid(cid), _collection(nullptr), _accessType(accessType) {}
  
  virtual ~TransactionCollection() {}
  
  inline TRI_voc_cid_t id() const { return _cid; }
  
  LogicalCollection* collection() const {
    return _collection;  // vocbase collection pointer
  }
  
  std::string collectionName() const;
  
  AccessMode::Type accessType() const { return _accessType; }

  /// @brief request a main-level lock for a collection
  virtual int lock() = 0;
 
  /// @brief request a lock for a collection
  virtual int lock(AccessMode::Type, int nestingLevel) = 0;

  /// @brief request an unlock for a collection
  virtual int unlock(AccessMode::Type, int nestingLevel) = 0;

  /// @brief check whether a collection is locked in a specific mode in a transaction
  virtual bool isLocked(AccessMode::Type, int nestingLevel) const = 0;
  
  /// @brief check whether a collection is locked at all
  virtual bool isLocked() const = 0;

  /// @brief whether or not any write operations for the collection happened
  virtual bool hasOperations() const = 0;
  
  virtual void freeOperations(transaction::Methods* activeTrx, bool mustRollback) = 0;
  
  virtual bool canAccess(AccessMode::Type accessType) const = 0;
  
  virtual int updateUsage(AccessMode::Type accessType, int nestingLevel) = 0;
  virtual int use(int nestingLevel) = 0;
  virtual void unuse(int nestingLevel) = 0;
  virtual void release() = 0;

 protected:
  TransactionState* _transaction;  // the transaction state
  TRI_voc_cid_t const _cid;        // collection id
  LogicalCollection* _collection;  // vocbase collection pointer
  AccessMode::Type _accessType;  // access type (read|write)
};

}

#endif
