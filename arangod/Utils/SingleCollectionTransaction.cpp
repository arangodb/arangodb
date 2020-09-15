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

#include "SingleCollectionTransaction.h"
#include "Basics/StringUtils.h"
#include "StorageEngine/TransactionCollection.h"
#include "StorageEngine/TransactionState.h"
#include "Transaction/Context.h"
#include "Transaction/Methods.h"
#include "Utils/CollectionNameResolver.h"
#include "VocBase/LogicalDataSource.h"

#include "Logger/Logger.h"

namespace arangodb {

/// @brief create the transaction, using a data-source
SingleCollectionTransaction::SingleCollectionTransaction(
    std::shared_ptr<transaction::Context> const& transactionContext,
    LogicalDataSource const& dataSource, AccessMode::Type accessType)
    : transaction::Methods(transactionContext),
      _cid(dataSource.id()),
      _trxCollection(nullptr),
      _documentCollection(nullptr),
      _accessType(accessType) {
  // add the (sole) data-source
  Result res = addCollection(dataSource.id(), dataSource.name(), _accessType);
  if (res.fail()) {
    THROW_ARANGO_EXCEPTION(res);
  }
  addHint(transaction::Hints::Hint::NO_DLD);
}

/// @brief create the transaction, using a collection name
SingleCollectionTransaction::SingleCollectionTransaction(
    std::shared_ptr<transaction::Context> const& transactionContext,
    std::string const& name, AccessMode::Type accessType)
    : transaction::Methods(transactionContext),
      _cid(0),
      _trxCollection(nullptr),
      _documentCollection(nullptr),
      _accessType(accessType) {
  // add the (sole) collection
  _cid = resolver()->getCollectionId(name);
  Result res = addCollection(_cid, name, _accessType);
  if (res.fail()) {
    THROW_ARANGO_EXCEPTION(res);
  }
  addHint(transaction::Hints::Hint::NO_DLD);
}

/// @brief get the underlying transaction collection
TransactionCollection* SingleCollectionTransaction::resolveTrxCollection() {
  TRI_ASSERT(_cid.isSet());

  if (_trxCollection == nullptr) {
    _trxCollection = _state->collection(_cid, _accessType);

    if (_trxCollection != nullptr) {
      _documentCollection = _trxCollection->collection().get();
    }
  }

  TRI_ASSERT(_trxCollection != nullptr);
  return _trxCollection;
}

/// @brief get the underlying document collection
/// note that we have two identical versions because this is called
/// in two different situations
LogicalCollection* SingleCollectionTransaction::documentCollection() {
  if (_documentCollection == nullptr) {
    resolveTrxCollection();
  }
  TRI_ASSERT(_documentCollection != nullptr);
  return _documentCollection;
}

DataSourceId SingleCollectionTransaction::addCollectionAtRuntime(std::string const& name,
                                                                 AccessMode::Type type) {
  TRI_ASSERT(!name.empty());
  if ((name[0] < '0' || name[0] > '9') && 
      name != resolveTrxCollection()->collectionName()) {
    THROW_ARANGO_EXCEPTION_MESSAGE(TRI_ERROR_TRANSACTION_UNREGISTERED_COLLECTION, 
                                   std::string(TRI_errno_string(TRI_ERROR_TRANSACTION_UNREGISTERED_COLLECTION)) + ": " + name);
  }
  
  if (AccessMode::isWriteOrExclusive(type) &&
      !AccessMode::isWriteOrExclusive(_accessType)) {
    // trying to write access a collection that is marked read-access
    THROW_ARANGO_EXCEPTION_MESSAGE(TRI_ERROR_TRANSACTION_UNREGISTERED_COLLECTION,
                                   std::string(TRI_errno_string(TRI_ERROR_TRANSACTION_UNREGISTERED_COLLECTION)) + ": " + name +
                                   " [" + AccessMode::typeString(type) + "]");
  }

  return _cid;
}

/// @brief get the underlying collection's name
std::string SingleCollectionTransaction::name() {
  // will ensure we have the _trxCollection object set
  return resolveTrxCollection()->collectionName();
}

}  // namespace arangodb
