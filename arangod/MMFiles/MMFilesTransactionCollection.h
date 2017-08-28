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

#ifndef ARANGOD_MMFILES_TRANSACTION_COLLECTION_H
#define ARANGOD_MMFILES_TRANSACTION_COLLECTION_H 1

#include "Basics/Common.h"
#include "Basics/SmallVector.h"
#include "StorageEngine/TransactionCollection.h"
#include "VocBase/AccessMode.h"
#include "VocBase/voc-types.h"
                                
namespace arangodb {
struct MMFilesDocumentOperation;
namespace transaction {
class Methods;
}
class TransactionState;

/// @brief collection used in a transaction
class MMFilesTransactionCollection final : public TransactionCollection {
 public:

  MMFilesTransactionCollection(TransactionState* trx, TRI_voc_cid_t cid, AccessMode::Type accessType, int nestingLevel);
  ~MMFilesTransactionCollection();
  
  /// @brief request a main-level lock for a collection
  int lock() override;
 
  /// @brief request a lock for a collection
  int lock(AccessMode::Type, int nestingLevel) override;

  /// @brief request an unlock for a collection
  int unlock(AccessMode::Type, int nestingLevel) override;

  /// @brief check whether a collection is locked in a specific mode in a transaction
  bool isLocked(AccessMode::Type, int nestingLevel) const override;
  
  /// @brief check whether a collection is locked at all
  bool isLocked() const override;
  
  /// @brief whether or not any write operations for the collection happened
  bool hasOperations() const override;

  void freeOperations(transaction::Methods* activeTrx, bool mustRollback) override;

  bool canAccess(AccessMode::Type accessType) const override;
  int updateUsage(AccessMode::Type accessType, int nestingLevel) override;
  int use(int nestingLevel) override;
  void unuse(int nestingLevel) override;
  void release() override;
  
  void addOperation(MMFilesDocumentOperation* operation);
  
 private:
  /// @brief request a lock for a collection
  int doLock(AccessMode::Type, int nestingLevel);

  /// @brief request an unlock for a collection
  int doUnlock(AccessMode::Type, int nestingLevel);

 private:
  SmallVector<MMFilesDocumentOperation*, 64>::allocator_type::arena_type _arena;
  SmallVector<MMFilesDocumentOperation*, 64> _operations;
  TRI_voc_rid_t _originalRevision;   // collection revision at trx start
  int _nestingLevel;  // the transaction level that added this collection
  bool _compactionLocked;  // was the compaction lock grabbed for the collection?
  bool _waitForSync;      // whether or not the collection has waitForSync
  
  AccessMode::Type _lockType;  // collection lock type
};

}

#endif
