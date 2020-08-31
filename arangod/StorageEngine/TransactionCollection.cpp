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

#include "TransactionCollection.h"

#include "Logger/LogMacros.h"
#include "Logger/Logger.h"
#include "Logger/LoggerStream.h"
#include "StorageEngine/TransactionState.h"
#include "VocBase/LogicalCollection.h"

using namespace arangodb;

TransactionCollection::~TransactionCollection() {
  TRI_ASSERT(_collection == nullptr);
}

std::string const& TransactionCollection::collectionName() const {
  TRI_ASSERT(_collection != nullptr);
  return _collection->name();
}

static_assert(AccessMode::Type::NONE < AccessMode::Type::READ &&
                  AccessMode::Type::READ < AccessMode::Type::WRITE &&
                  AccessMode::Type::WRITE < AccessMode::Type::EXCLUSIVE,
              "AccessMode::Type total order fail");

/// @brief check if a collection is locked in a specific mode in a transaction
bool TransactionCollection::isLocked(AccessMode::Type accessType) const {
  if (accessType > _accessType) {
    // wrong lock type
    LOG_TOPIC("39ef2", WARN, arangodb::Logger::ENGINES)
        << "logic error. checking wrong lock type";
    return false;
  }

  return isLocked();
}

bool TransactionCollection::isLocked() const {
  if (_collection == nullptr) {
    return false;
  }
  return _lockType > AccessMode::Type::NONE;
}

Result TransactionCollection::updateUsage(AccessMode::Type accessType) {
  if (AccessMode::isWriteOrExclusive(accessType) &&
      !AccessMode::isWriteOrExclusive(_accessType)) {
    if (_transaction->status() != transaction::Status::CREATED) {
      // trying to write access a collection that is marked read-access
      return Result(TRI_ERROR_TRANSACTION_UNREGISTERED_COLLECTION,
                    std::string(TRI_errno_string(TRI_ERROR_TRANSACTION_UNREGISTERED_COLLECTION)) + ": " + collectionName() + 
                    " [" + AccessMode::typeString(accessType) + "]");
    }

    // upgrade collection type to write-access
    _accessType = accessType;
  }
  
  // all correct
  return {};
}
