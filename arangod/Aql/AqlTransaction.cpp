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

std::shared_ptr<AqlTransaction> AqlTransaction::create(
    std::shared_ptr<transaction::Context> const& transactionContext,
    std::map<std::string, aql::Collection*> const* collections,
    transaction::Options const& options, bool isMainTransaction,
    std::unordered_set<std::string> inaccessibleCollections) {
#ifdef USE_ENTERPRISE
  if (options.skipInaccessibleCollections) {
    return std::make_shared<transaction::IgnoreNoAccessAqlTransaction>(transactionContext, collections,
                                                                       options, isMainTransaction,
                                                                       inaccessibleCollections);
  }
#endif
  return std::make_shared<AqlTransaction>(transactionContext, collections, options, isMainTransaction);
}

/// @brief add a list of collections to the transaction
Result AqlTransaction::addCollections(std::map<std::string, aql::Collection*> const& collections) {
  Result res;
  for (auto const& it : collections) {
    res = processCollection(it.second);

    if (!res.ok()) {
      break;
    }
  }
  return res;
}

/// @brief add a collection to the transaction
Result AqlTransaction::processCollection(aql::Collection* collection) {
  Result res;

  if (_state->isCoordinator()) {
    auto cid = resolver()->getCollectionId(collection->name());

    return res.reset(addCollection(cid, collection->name(), collection->accessType()));
  }

  TRI_voc_cid_t cid = 0;
  auto col = resolver()->getCollection(collection->name());

  if (col != nullptr) {
    cid = col->id();
  } else {
    cid = resolver()->getCollectionId(collection->name());
  }

  res = addCollection(cid, collection->name(), collection->accessType());

  if (res.ok() && col != nullptr) {
    collection->setCollection(col.get());
  }

  return res;
}

LogicalCollection* AqlTransaction::documentCollection(TRI_voc_cid_t cid) {
  TransactionCollection* trxColl = this->trxCollection(cid);
  TRI_ASSERT(trxColl != nullptr);
  return trxColl->collection().get();
}

/// @brief lockCollections, this is needed in a corner case in AQL: we need
/// to lock all shards in a controlled way when we set up a distributed
/// execution engine. To this end, we prevent the standard mechanism to
/// lock collections on the DBservers when we instantiate the query. Then,
/// in a second round, we need to lock the shards in exactly the right
/// order via an HTTP call. This method is used to implement that HTTP action.

int AqlTransaction::lockCollections() { return state()->lockCollections(); }


AqlTransaction::AqlTransaction(
    std::shared_ptr<transaction::Context> const& transactionContext,
    transaction::Options const& options)
    : transaction::Methods(transactionContext, options) {
  addHint(transaction::Hints::Hint::INTERMEDIATE_COMMITS);
}

  /// protected so we can create different subclasses
AqlTransaction::AqlTransaction(
    std::shared_ptr<transaction::Context> const& transactionContext,
    std::map<std::string, aql::Collection*> const* collections,
    transaction::Options const& options, bool isMainTransaction)
    : transaction::Methods(transactionContext, options), _collections(*collections) {
  if (!isMainTransaction) {
    addHint(transaction::Hints::Hint::LOCK_NEVER);
  } else {
    addHint(transaction::Hints::Hint::LOCK_ENTIRELY);
  }
  addHint(transaction::Hints::Hint::INTERMEDIATE_COMMITS);

  Result res = addCollections(*collections);
  if (res.fail()) {
    THROW_ARANGO_EXCEPTION(res);
  }
}
