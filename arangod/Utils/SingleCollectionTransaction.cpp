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

#include "SingleCollectionTransaction.h"
#include "StorageEngine/TransactionCollection.h"
#include "StorageEngine/TransactionState.h"
#include "Utils/CollectionNameResolver.h"
#include "Utils/OperationResult.h"
#include "Transaction/Methods.h"
#include "Transaction/Context.h"
#include "VocBase/LogicalCollection.h"

using namespace arangodb;

/// @brief create the transaction, using a collection id
SingleCollectionTransaction::SingleCollectionTransaction(
  std::shared_ptr<transaction::Context> const& transactionContext, TRI_voc_cid_t cid, 
  AccessMode::Type accessType)
      : transaction::Methods(transactionContext),
        _cid(cid),
        _trxCollection(nullptr),
        _documentCollection(nullptr),
        _accessType(accessType) {

  // add the (sole) collection
  addCollection(cid, _accessType);
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
  addCollection(_cid, name.c_str(), _accessType);
  addHint(transaction::Hints::Hint::NO_DLD);
}

/// @brief get the underlying transaction collection
TransactionCollection* SingleCollectionTransaction::trxCollection() {
  TRI_ASSERT(_cid > 0);

  if (_trxCollection == nullptr) {
    _trxCollection = _state->collection(_cid, _accessType);

    if (_trxCollection != nullptr) {
      _documentCollection = _trxCollection->collection();
    }
  }

  TRI_ASSERT(_trxCollection != nullptr);
  return _trxCollection;
}

/// @brief get the underlying document collection
/// note that we have two identical versions because this is called
/// in two different situations
LogicalCollection* SingleCollectionTransaction::documentCollection() {
  if (_documentCollection != nullptr) {
    return _documentCollection;
  }
 
  trxCollection(); 
  TRI_ASSERT(_documentCollection != nullptr);

  return _documentCollection;
}
  
/// @brief get the underlying collection's name
std::string SingleCollectionTransaction::name() { 
  trxCollection(); // will ensure we have the _trxCollection object set
  TRI_ASSERT(_trxCollection != nullptr);
  return _trxCollection->collectionName();
}
