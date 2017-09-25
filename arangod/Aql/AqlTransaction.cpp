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
#include "Logger/Logger.h"
#include "StorageEngine/TransactionCollection.h"
#include "StorageEngine/TransactionState.h"
#include "Utils/CollectionNameResolver.h"
#include "VocBase/LogicalCollection.h"

#ifdef USE_ENTERPRISE
#include "Enterprise/Transaction/IgnoreNoAccessAqlTransaction.h"
#endif

using namespace arangodb;
using namespace arangodb::aql;

AqlTransaction* AqlTransaction::create(
    std::shared_ptr<transaction::Context> const& transactionContext,
    std::map<std::string, aql::Collection*> const* collections,
    transaction::Options const& options, bool isMainTransaction,
    std::unordered_set<std::string> inaccessibleCollections) {
#ifdef USE_ENTERPRISE
  if (options.skipInaccessibleCollections) {
    return new transaction::IgnoreNoAccessAqlTransaction(
        transactionContext, collections, options, isMainTransaction,
        inaccessibleCollections);
  }
#endif
  return new AqlTransaction(transactionContext, collections, options,
                            isMainTransaction);
}

/// @brief clone, used to make daughter transactions for parts of a
/// distributed AQL query running on the coordinator
transaction::Methods* AqlTransaction::clone(
    transaction::Options const& options) const {
  return new AqlTransaction(transaction::StandaloneContext::Create(vocbase()),
                            &_collections, options, false);
}

/// @brief add a collection to the transaction
Result AqlTransaction::processCollection(aql::Collection* collection) {
  if (ServerState::instance()->isCoordinator()) {
    return processCollectionCoordinator(collection);
  }
  return processCollectionNormal(collection);
}

/// @brief add a coordinator collection to the transaction

Result AqlTransaction::processCollectionCoordinator(
    aql::Collection* collection) {
  TRI_voc_cid_t cid = resolver()->getCollectionId(collection->getName());

  return addCollection(cid, collection->getName().c_str(),
                       collection->accessType);
}

/// @brief add a regular collection to the transaction

Result AqlTransaction::processCollectionNormal(aql::Collection* collection) {
  TRI_voc_cid_t cid = 0;

  arangodb::LogicalCollection const* col =
      resolver()->getCollectionStruct(collection->getName());
  /*if (col == nullptr) {
    auto startTime = TRI_microtime();
    auto endTime = startTime + 60.0;
    do {
      usleep(10000);
      if (TRI_microtime() > endTime) {
        break;
      }
      col = this->resolver()->getCollectionStruct(collection->getName());
    } while (col == nullptr);
  }
  */
  if (col != nullptr) {
    cid = col->cid();
  }

  Result res =
      addCollection(cid, collection->getName(), collection->accessType);

  if (res.ok() && col != nullptr) {
    collection->setCollection(const_cast<arangodb::LogicalCollection*>(col));
  }

  return res;
}

LogicalCollection* AqlTransaction::documentCollection(TRI_voc_cid_t cid) {
  TransactionCollection* trxColl = this->trxCollection(cid);
  TRI_ASSERT(trxColl != nullptr);
  return trxColl->collection();
}

/// @brief lockCollections, this is needed in a corner case in AQL: we need
/// to lock all shards in a controlled way when we set up a distributed
/// execution engine. To this end, we prevent the standard mechanism to
/// lock collections on the DBservers when we instantiate the query. Then,
/// in a second round, we need to lock the shards in exactly the right
/// order via an HTTP call. This method is used to implement that HTTP action.

int AqlTransaction::lockCollections() { return state()->lockCollections(); }
