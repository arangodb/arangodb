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

#include "AqlTransaction.h"
#include "CollectionNameResolver.h"
#include "VocBase/LogicalCollection.h"

using namespace arangodb;

/// @brief add a collection to the transaction

int AqlTransaction::processCollection(aql::Collection* collection) {
  int state = setupState();

  if (state != TRI_ERROR_NO_ERROR) {
    return state;
  }

  if (ServerState::instance()->isCoordinator()) {
    return processCollectionCoordinator(collection);
  }
  return processCollectionNormal(collection);
}

/// @brief add a coordinator collection to the transaction

int AqlTransaction::processCollectionCoordinator(aql::Collection*  collection) {
  TRI_voc_cid_t cid =
      this->resolver()->getCollectionId(collection->getName());

  return this->addCollection(cid, collection->getName().c_str(),
                             collection->accessType);
}

/// @brief add a regular collection to the transaction

int AqlTransaction::processCollectionNormal(aql::Collection* collection) {
  arangodb::LogicalCollection const* col =
      this->resolver()->getCollectionStruct(collection->getName());
  TRI_voc_cid_t cid = 0;

  if (col != nullptr) {
    cid = col->cid();
  }

  int res =
      this->addCollection(cid, collection->getName(), collection->accessType);

  if (res == TRI_ERROR_NO_ERROR && col != nullptr) {
    collection->setCollection(const_cast<arangodb::LogicalCollection*>(col));
  }

  return res;
}

LogicalCollection* AqlTransaction::documentCollection(TRI_voc_cid_t cid) {
  TRI_transaction_collection_t* trxColl = this->trxCollection(cid);
  TRI_ASSERT(trxColl != nullptr);
  return trxColl->_collection;
  }


/// @brief lockCollections, this is needed in a corner case in AQL: we need
/// to lock all shards in a controlled way when we set up a distributed
/// execution engine. To this end, we prevent the standard mechanism to
/// lock collections on the DBservers when we instantiate the query. Then,
/// in a second round, we need to lock the shards in exactly the right
/// order via an HTTP call. This method is used to implement that HTTP action.

int AqlTransaction::lockCollections() {
  auto trx = getInternals();

  for (auto& trxCollection : trx->_collections) {
    int res = TRI_LockCollectionTransaction(trxCollection,
                                            trxCollection->_accessType, 0);
    if (res != TRI_ERROR_NO_ERROR) {
      return res;
    }
  }
  return TRI_ERROR_NO_ERROR;
}

