////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2014-2020 ArangoDB GmbH, Cologne, Germany
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

#include <memory>

#include "Basics/Common.h"
#include "VocBase/AccessMode.h"
#include "VocBase/Identifiers/DataSourceId.h"
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

  TransactionCollection(TransactionState* trx, DataSourceId cid, AccessMode::Type accessType)
      : _transaction(trx),
        _cid(cid),
        _accessType(accessType),
        _lockType(AccessMode::Type::NONE) {}

  virtual ~TransactionCollection();

  inline DataSourceId id() const { return _cid; }

  std::shared_ptr<LogicalCollection> const& collection() const {
    return _collection;  // vocbase collection pointer
  }

  std::string const& collectionName() const;

  AccessMode::Type accessType() const { return _accessType; }
  
  Result updateUsage(AccessMode::Type accessType);

  /// @brief check whether a collection is locked in a specific mode in a
  /// transaction
  bool isLocked(AccessMode::Type) const;

  /// @brief check whether a collection is locked at all
  bool isLocked() const;

  /// @brief whether or not any write operations for the collection happened
  virtual bool hasOperations() const = 0;

  virtual bool canAccess(AccessMode::Type accessType) const = 0;

  virtual Result lockUsage() = 0;
  virtual void releaseUsage() = 0;

 protected:
  TransactionState* _transaction;                  // the transaction state
  DataSourceId const _cid;                         // collection id
  std::shared_ptr<LogicalCollection> _collection;  // vocbase collection pointer
  AccessMode::Type _accessType;                    // access type (read|write)
  AccessMode::Type _lockType;                      // actual held lock type

 private:
  // perform lock, sets _lockType
  virtual Result doLock(AccessMode::Type) = 0;
  virtual Result doUnlock(AccessMode::Type) = 0;
};

}  // namespace arangodb

#endif
