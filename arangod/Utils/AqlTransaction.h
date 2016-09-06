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

#ifndef ARANGOD_UTILS_AQL_TRANSACTION_H
#define ARANGOD_UTILS_AQL_TRANSACTION_H 1

#include "Basics/Common.h"
#include "Aql/Collection.h"
#include "Cluster/ServerState.h"
#include "Utils/StandaloneTransactionContext.h"
#include "Utils/Transaction.h"
#include "VocBase/transaction.h"
#include "VocBase/vocbase.h"

namespace arangodb {

class AqlTransaction : public Transaction {
 public:
  //////////////////////////////////////////////////////////////////////////////
  /// @brief create the transaction and add all collections from the query
  /// context
  //////////////////////////////////////////////////////////////////////////////

  AqlTransaction(
      std::shared_ptr<TransactionContext> transactionContext, 
      std::map<std::string, arangodb::aql::Collection*> const* collections,
      bool isMainTransaction)
      : Transaction(transactionContext),
        _collections(*collections) {
    if (!isMainTransaction) {
      this->addHint(TRI_TRANSACTION_HINT_LOCK_NEVER, true);
    } else {
      this->addHint(TRI_TRANSACTION_HINT_LOCK_ENTIRELY, false);
    }

    for (auto it : *collections) {
      if (processCollection(it.second) != TRI_ERROR_NO_ERROR) {
        break;
      }
    }
  }

  //////////////////////////////////////////////////////////////////////////////
  /// @brief end the transaction
  //////////////////////////////////////////////////////////////////////////////

  ~AqlTransaction() {}
  
  //////////////////////////////////////////////////////////////////////////////
  /// @brief add a list of collections to the transaction
  //////////////////////////////////////////////////////////////////////////////

  int addCollectionList(
      std::map<std::string, arangodb::aql::Collection*>* collections) {
    int ret = TRI_ERROR_NO_ERROR;
    for (auto it : *collections) {
      ret = processCollection(it.second);
      if (ret != TRI_ERROR_NO_ERROR) {
        break;
      }
    }
    return ret;
  }

  //////////////////////////////////////////////////////////////////////////////
  /// @brief add a collection to the transaction
  //////////////////////////////////////////////////////////////////////////////

  int processCollection(arangodb::aql::Collection*); 

  //////////////////////////////////////////////////////////////////////////////
  /// @brief add a coordinator collection to the transaction
  //////////////////////////////////////////////////////////////////////////////

  int processCollectionCoordinator(arangodb::aql::Collection*);

  //////////////////////////////////////////////////////////////////////////////
  /// @brief add a regular collection to the transaction
  //////////////////////////////////////////////////////////////////////////////

  int processCollectionNormal(arangodb::aql::Collection* collection);

  //////////////////////////////////////////////////////////////////////////////
  /// @brief ditch
  //////////////////////////////////////////////////////////////////////////////

  arangodb::DocumentDitch* ditch(TRI_voc_cid_t cid) {
    return this->_transactionContext->ditch(cid);
  }

  //////////////////////////////////////////////////////////////////////////////
  /// @brief documentCollection
  //////////////////////////////////////////////////////////////////////////////

  LogicalCollection* documentCollection(TRI_voc_cid_t cid);

  //////////////////////////////////////////////////////////////////////////////
  /// @brief clone, used to make daughter transactions for parts of a
  /// distributed
  /// AQL query running on the coordinator
  //////////////////////////////////////////////////////////////////////////////

  arangodb::Transaction* clone() const override {
    return new arangodb::AqlTransaction(
        arangodb::StandaloneTransactionContext::Create(this->_vocbase),
        &_collections, false);
  }

  //////////////////////////////////////////////////////////////////////////////
  /// @brief lockCollections, this is needed in a corner case in AQL: we need
  /// to lock all shards in a controlled way when we set up a distributed
  /// execution engine. To this end, we prevent the standard mechanism to
  /// lock collections on the DBservers when we instantiate the query. Then,
  /// in a second round, we need to lock the shards in exactly the right
  /// order via an HTTP call. This method is used to implement that HTTP action.
  //////////////////////////////////////////////////////////////////////////////

  int lockCollections() override;

  //////////////////////////////////////////////////////////////////////////////
  /// @brief keep a copy of the collections, this is needed for the clone
  /// operation
  //////////////////////////////////////////////////////////////////////////////

 private:
  std::map<std::string, arangodb::aql::Collection*> _collections;
};
}

#endif
