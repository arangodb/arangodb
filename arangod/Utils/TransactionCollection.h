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

#ifndef ARANGOD_UTILS_TRANSACTION_COLLECTION_H
#define ARANGOD_UTILS_TRANSACTION_COLLECTION_H 1

#include "Basics/Common.h"
#include "VocBase/AccessMode.h"
#include "VocBase/voc-types.h"
                                
namespace arangodb {
class LogicalCollection;
struct MMFilesDocumentOperation;
struct TransactionState;

/// @brief collection used in a transaction
struct TransactionCollection {
  friend struct TransactionState;

  TransactionCollection(TransactionState* trx, TRI_voc_cid_t cid, AccessMode::Type accessType, int nestingLevel)
      : _transaction(trx), _cid(cid), _accessType(accessType), _nestingLevel(nestingLevel), _collection(nullptr), _operations(nullptr),
        _originalRevision(0), _lockType(AccessMode::Type::NONE), _compactionLocked(false), _waitForSync(false) {}

 public:
  /// @brief request a lock for a collection
  int lock(AccessMode::Type, int nestingLevel);

  /// @brief request an unlock for a collection
  int unlock(AccessMode::Type, int nestingLevel);

  /// @brief check whether a collection is locked in a specific mode in a transaction
  bool isLocked(AccessMode::Type, int nestingLevel) const;
  
  /// @brief check whether a collection is locked at all
  bool isLocked() const;

 private:
  /// @brief request a lock for a collection
  int doLock(AccessMode::Type, int nestingLevel);

  /// @brief request an unlock for a collection
  int doUnlock(AccessMode::Type, int nestingLevel);

 public:
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

}

#endif
