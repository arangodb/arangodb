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

#include "AqlTransaction.h"

#include "Aql/Collection.h"
#include "Aql/Collections.h"
#include "StorageEngine/TransactionState.h"
#include "Utils/CollectionNameResolver.h"
#include "VocBase/LogicalCollection.h"

#ifdef USE_ENTERPRISE
#include "Enterprise/Transaction/IgnoreNoAccessAqlTransaction.h"
#endif

using namespace arangodb;
using namespace arangodb::aql;

std::unique_ptr<AqlTransaction> AqlTransaction::create(
    std::shared_ptr<transaction::Context> const& transactionContext,
    aql::Collections const& collections, transaction::Options const& options,
    std::unordered_set<std::string> inaccessibleCollections) {
#ifdef USE_ENTERPRISE
  if (!inaccessibleCollections.empty()) {
    return std::make_unique<transaction::IgnoreNoAccessAqlTransaction>(
        transactionContext, collections, options,
        std::move(inaccessibleCollections));
  }
#endif
  return std::make_unique<AqlTransaction>(transactionContext, collections, options);
}

AqlTransaction::AqlTransaction(
    std::shared_ptr<transaction::Context> const& transactionContext,
    transaction::Options const& options)
    : transaction::Methods(transactionContext, options) {
  addHint(transaction::Hints::Hint::INTERMEDIATE_COMMITS);
}

  /// protected so we can create different subclasses
AqlTransaction::AqlTransaction(
    std::shared_ptr<transaction::Context> const& transactionContext,
    aql::Collections const& collections,
    transaction::Options const& options)
    : transaction::Methods(transactionContext, options) { 
  addHint(transaction::Hints::Hint::INTERMEDIATE_COMMITS);

  collections.visit([this](std::string const&, aql::Collection& collection) {
    Result res = processCollection(collection);

    if (res.fail()) {
      THROW_ARANGO_EXCEPTION(res);
    }

    return true;
  });
}

/// @brief add a collection to the transaction
Result AqlTransaction::processCollection(aql::Collection& collection) {
  if (collection.hasCollectionObject()) {
    // we should get here for all existing collections/shards, but
    // not for views.
    auto c = collection.getCollection();
    // note that c->name() and collection->name() may differ if the collection
    // is accessed in a query by its id instead of its name.
    return addCollection(c->id(), c->name(), collection.accessType());
  }

  // views
  DataSourceId cid = resolver()->getCollectionId(collection.name());
  return addCollection(cid, collection.name(), collection.accessType());
}
