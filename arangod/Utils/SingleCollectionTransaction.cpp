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
#include "Utils/CollectionNameResolver.h"
#include "Utils/Transaction.h"
#include "Utils/TransactionContext.h"
#include "VocBase/Ditch.h"
#include "VocBase/LogicalCollection.h"
#include "VocBase/transaction.h"

using namespace arangodb;

////////////////////////////////////////////////////////////////////////////////
/// @brief create the transaction, using a collection id
////////////////////////////////////////////////////////////////////////////////

SingleCollectionTransaction::SingleCollectionTransaction(
  std::shared_ptr<TransactionContext> transactionContext, TRI_voc_cid_t cid, 
  TRI_transaction_type_e accessType)
      : Transaction(transactionContext),
        _cid(cid),
        _trxCollection(nullptr),
        _documentCollection(nullptr),
        _accessType(accessType) {

  // add the (sole) collection
  if (setupState() == TRI_ERROR_NO_ERROR) {
    addCollection(cid, _accessType);
  }
}

////////////////////////////////////////////////////////////////////////////////
/// @brief create the transaction, using a collection name
////////////////////////////////////////////////////////////////////////////////

SingleCollectionTransaction::SingleCollectionTransaction(
  std::shared_ptr<TransactionContext> transactionContext,
  std::string const& name, TRI_transaction_type_e accessType)
      : Transaction(transactionContext),
        _cid(0),
        _trxCollection(nullptr),
        _documentCollection(nullptr),
        _accessType(accessType) {
  // add the (sole) collection
  if (setupState() == TRI_ERROR_NO_ERROR) {
    _cid = resolver()->getCollectionId(name);
    addCollection(_cid, name.c_str(), _accessType);
  }
}

////////////////////////////////////////////////////////////////////////////////
/// @brief get the underlying transaction collection
////////////////////////////////////////////////////////////////////////////////

TRI_transaction_collection_t* SingleCollectionTransaction::trxCollection() {
  TRI_ASSERT(_cid > 0);

  if (_trxCollection == nullptr) {
    _trxCollection =
        TRI_GetCollectionTransaction(_trx, _cid, _accessType);

    if (_trxCollection != nullptr) {
      _documentCollection =
          _trxCollection->_collection;
    }
  }

  TRI_ASSERT(_trxCollection != nullptr);
  return _trxCollection;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief get the underlying document collection
/// note that we have two identical versions because this is called
/// in two different situations
////////////////////////////////////////////////////////////////////////////////

LogicalCollection* SingleCollectionTransaction::documentCollection() {
  if (_documentCollection != nullptr) {
    return _documentCollection;
  }
 
  trxCollection(); 
  TRI_ASSERT(_documentCollection != nullptr);

  return _documentCollection;
}

//////////////////////////////////////////////////////////////////////////////
/// @brief return the ditch for the collection
/// note that the ditch must already exist
/// furthermore note that we have two calling conventions because this
/// is called in two different ways
//////////////////////////////////////////////////////////////////////////////

DocumentDitch* SingleCollectionTransaction::ditch() const {
  return _transactionContext->ditch(_cid);
}

DocumentDitch* SingleCollectionTransaction::ditch(TRI_voc_cid_t) const { 
  return ditch(); 
}

////////////////////////////////////////////////////////////////////////////////
/// @brief whether or not a ditch is available for a collection
////////////////////////////////////////////////////////////////////////////////

bool SingleCollectionTransaction::hasDitch() const {
  return (ditch() != nullptr);
}
  
////////////////////////////////////////////////////////////////////////////////
/// @brief get the underlying collection's name
////////////////////////////////////////////////////////////////////////////////

std::string SingleCollectionTransaction::name() { 
  trxCollection(); // will ensure we have the _trxCollection object set
  TRI_ASSERT(_trxCollection != nullptr);
  TRI_ASSERT(_trxCollection->_collection != nullptr);
  return _trxCollection->_collection->name(); 
}

////////////////////////////////////////////////////////////////////////////////
/// @brief explicitly lock the underlying collection for read access
////////////////////////////////////////////////////////////////////////////////

int SingleCollectionTransaction::lockRead() {
  return lock(trxCollection(), TRI_TRANSACTION_READ);
}

//////////////////////////////////////////////////////////////////////////////
/// @brief explicitly unlock the underlying collection after read access
//////////////////////////////////////////////////////////////////////////////

int SingleCollectionTransaction::unlockRead() {
  return unlock(trxCollection(), TRI_TRANSACTION_READ);
}

////////////////////////////////////////////////////////////////////////////////
/// @brief explicitly lock the underlying collection for write access
////////////////////////////////////////////////////////////////////////////////

int SingleCollectionTransaction::lockWrite() {
  return lock(trxCollection(), TRI_TRANSACTION_WRITE);
}

