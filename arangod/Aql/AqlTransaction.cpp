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
  
  auto ctx = transaction::StandaloneContext::Create(vocbase());
  return new AqlTransaction(ctx, &_collections, options, false);
}

/// @brief add a collection to the transaction
Result AqlTransaction::processCollection(aql::Collection* collection) {
  if (ServerState::instance()->isCoordinator()) {
    auto cid = resolver()->getCollectionId(collection->name());

    return addCollection(cid, collection->name(), collection->accessType());
  }

  TRI_voc_cid_t cid = 0;
  auto col = resolver()->getCollection(collection->name());

  if (col != nullptr) {
    cid = col->id();
  } else {
    cid = resolver()->getCollectionId(collection->name());
  }

  Result res =
      addCollection(cid, collection->name(), collection->accessType());

  if (res.ok() && col != nullptr) {
    collection->setCollection(col.get());
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
