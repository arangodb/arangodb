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

#ifndef ARANGOD_AQL_AQL_TRANSACTION_H
#define ARANGOD_AQL_AQL_TRANSACTION_H 1

#include "Aql/Collection.h"
#include "Basics/Common.h"
#include "Basics/Result.h"
#include "Transaction/Methods.h"
#include "Transaction/Options.h"
#include "Transaction/StandaloneContext.h"
#include "VocBase/vocbase.h"

namespace arangodb {
namespace aql {

class AqlTransaction : public transaction::Methods {
 public:
  /// @brief create the transaction and add all collections
  /// from the query context
  static AqlTransaction* create(
      std::shared_ptr<transaction::Context> const& transactionContext,
      std::map<std::string, aql::Collection*> const* collections,
      transaction::Options const& options, bool isMainTransaction,
      std::unordered_set<std::string> inaccessibleCollections =
          std::unordered_set<std::string>());

  /// @brief end the transaction
  ~AqlTransaction() {}

  /// @brief add a list of collections to the transaction
  Result addCollections(
      std::map<std::string, aql::Collection*> const& collections) {
    Result res;
    for (auto const& it : collections) {
      res = processCollection(it.second);

      if (!res.ok()) {
        return res;
      }
    }
    return res;
  }

  /// @brief add a collection to the transaction
  Result processCollection(aql::Collection*);

  /// @brief add a coordinator collection to the transaction
  Result processCollectionCoordinator(aql::Collection*);

  /// @brief add a regular collection to the transaction
  Result processCollectionNormal(aql::Collection* collection);

  /// @brief documentCollection
  LogicalCollection* documentCollection(TRI_voc_cid_t cid);

  /// @brief clone, used to make daughter transactions for parts of a
  /// distributed AQL query running on the coordinator
  transaction::Methods* clone(transaction::Options const&) const override;

  /// @brief lockCollections, this is needed in a corner case in AQL: we need
  /// to lock all shards in a controlled way when we set up a distributed
  /// execution engine. To this end, we prevent the standard mechanism to
  /// lock collections on the DBservers when we instantiate the query. Then,
  /// in a second round, we need to lock the shards in exactly the right
  /// order via an HTTP call. This method is used to implement that HTTP action.
  int lockCollections() override;

 protected:
  AqlTransaction(
      std::shared_ptr<transaction::Context> const& transactionContext,
      transaction::Options const& options)
      : transaction::Methods(transactionContext, options) {}

  /// protected so we can create different subclasses
  AqlTransaction(
      std::shared_ptr<transaction::Context> const& transactionContext,
      std::map<std::string, aql::Collection*> const* collections,
      transaction::Options const& options, bool isMainTransaction)
      : transaction::Methods(transactionContext, options),
        _collections(*collections) {
    if (!isMainTransaction) {
      addHint(transaction::Hints::Hint::LOCK_NEVER);
    } else {
      addHint(transaction::Hints::Hint::LOCK_ENTIRELY);
    }

    for (auto it : *collections) {
      if (!processCollection(it.second).ok()) {
        break;
      }
    }
  }

 protected:
  /// @brief keep a copy of the collections, this is needed for the clone
  /// operation
  std::map<std::string, aql::Collection*> _collections;
};
}
}

#endif
