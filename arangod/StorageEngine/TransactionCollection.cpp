////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2016 ArangoDB GmbH, Cologne, Germany
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

#include "TransactionCollection.h"

#include "Logger/Logger.h"
#include "StorageEngine/TransactionState.h"
#include "VocBase/LogicalCollection.h"

using namespace arangodb;

std::string TransactionCollection::collectionName() const {
  TRI_ASSERT(_collection != nullptr);
  return _collection->name();
}

static_assert(AccessMode::Type::NONE < AccessMode::Type::READ &&
              AccessMode::Type::READ < AccessMode::Type::WRITE &&
              AccessMode::Type::WRITE < AccessMode::Type::EXCLUSIVE,
  "AccessMode::Type total order fail");

/// @brief check if a collection is locked in a specific mode in a transaction
bool TransactionCollection::isLocked(AccessMode::Type accessType,
                                     int nestingLevel) const {
  if (accessType > _accessType) {
    // wrong lock type
    LOG_TOPIC(WARN, arangodb::Logger::ENGINES)
        << "logic error. checking wrong lock type";
    return false;
  }

  return isLocked();
}

bool TransactionCollection::isLocked() const {
  if (_collection == nullptr) {
    return false;
  }
  std::string const& collName{_collection->name()};
  if (_transaction->isLockedShard(collName)) {
    return true;
  }
  return _lockType > AccessMode::Type::NONE;
}

/// @brief request a main-level lock for a collection
/// returns TRI_ERROR_LOCKED in case the lock was successfully acquired
/// returns TRI_ERROR_NO_ERROR in case the lock does not need to be acquired and no other error occurred
/// returns any other error code otherwise
int TransactionCollection::lockRecursive() {
  return lockRecursive(_accessType, 0);
}

/// @brief request a lock for a collection
/// returns TRI_ERROR_LOCKED in case the lock was successfully acquired
/// returns TRI_ERROR_NO_ERROR in case the lock does not need to be acquired and no other error occurred
/// returns any other error code otherwise
int TransactionCollection::lockRecursive(AccessMode::Type accessType,
                                         int nestingLevel) {
  if (AccessMode::isWriteOrExclusive(accessType) &&
      !AccessMode::isWriteOrExclusive(_accessType)) {
    // wrong lock type
    return TRI_ERROR_INTERNAL;
  }

  if (isLocked()) {
    // already locked
    return TRI_ERROR_NO_ERROR;
  }

  return doLock(accessType, nestingLevel);
}

/// @brief request an unlock for a collection
int TransactionCollection::unlockRecursive(AccessMode::Type accessType,
                                           int nestingLevel) {
  if (AccessMode::isWriteOrExclusive(accessType) &&
      !AccessMode::isWriteOrExclusive(_accessType)) {
    // wrong lock type: write-unlock requested but collection is read-only
    return TRI_ERROR_INTERNAL;
  }

  if (!isLocked()) {
    // already unlocked
    return TRI_ERROR_NO_ERROR;
  }

  return doUnlock(accessType, nestingLevel);
}
