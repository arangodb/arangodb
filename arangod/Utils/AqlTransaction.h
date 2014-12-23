////////////////////////////////////////////////////////////////////////////////
/// @brief wrapper for Aql transactions
///
/// @file
///
/// DISCLAIMER
///
/// Copyright 2014 ArangoDB GmbH, Cologne, Germany
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
/// @author Copyright 2014, ArangoDB GmbH, Cologne, Germany
/// @author Copyright 2011-2013, triAGENS GmbH, Cologne, Germany
////////////////////////////////////////////////////////////////////////////////

#ifndef ARANGODB_UTILS_AQL_TRANSACTION_H
#define ARANGODB_UTILS_AQL_TRANSACTION_H 1

#include "Basics/Common.h"

#include "Aql/Collection.h"
#include "Cluster/ServerState.h"
#include "Utils/CollectionNameResolver.h"
#include "Utils/Transaction.h"
#include "Utils/StandaloneTransactionContext.h"
#include "VocBase/transaction.h"
#include "VocBase/vocbase.h"
#include <v8.h>

namespace triagens {
  namespace arango {

    class AqlTransaction : public Transaction {

// -----------------------------------------------------------------------------
// --SECTION--                                              class AqlTransaction
// -----------------------------------------------------------------------------

// -----------------------------------------------------------------------------
// --SECTION--                                      constructors and destructors
// -----------------------------------------------------------------------------

      public:

////////////////////////////////////////////////////////////////////////////////
/// @brief create the transaction and add all collections from the query
/// context
////////////////////////////////////////////////////////////////////////////////

        AqlTransaction (TransactionContext* transactionContext,
                        TRI_vocbase_t* vocbase,
                        std::map<std::string, triagens::aql::Collection*> const* collections,
                        bool isMainTransaction)
          : Transaction(transactionContext, vocbase, 0),
            _collections(*collections) {

          if (! isMainTransaction) {
            this->addHint(TRI_TRANSACTION_HINT_LOCK_NEVER, true);
          }
          else {
            this->addHint(TRI_TRANSACTION_HINT_LOCK_ENTIRELY, false);
          }

          for (auto it : *collections) {
            if (processCollection(it.second) != TRI_ERROR_NO_ERROR) {
              break;
            }
          }
        }

////////////////////////////////////////////////////////////////////////////////
/// @brief end the transaction
////////////////////////////////////////////////////////////////////////////////

        ~AqlTransaction () {
        }

////////////////////////////////////////////////////////////////////////////////
/// @brief add a list of collections to the transaction
////////////////////////////////////////////////////////////////////////////////

        int addCollectionList (std::map<std::string, triagens::aql::Collection*>* collections) {
          int ret = TRI_ERROR_NO_ERROR;
          for (auto it : *collections) {
            ret = processCollection(it.second);
            if (ret != TRI_ERROR_NO_ERROR) {
              break;
            }
          }
          return ret;
        }

////////////////////////////////////////////////////////////////////////////////
/// @brief add a collection to the transaction
////////////////////////////////////////////////////////////////////////////////

        int processCollection (triagens::aql::Collection* collection) {
          if (ServerState::instance()->isCoordinator()) {
            return processCollectionCoordinator(collection);
          }
          else {
            return processCollectionNormal(collection);
          }
        }

////////////////////////////////////////////////////////////////////////////////
/// @brief add a coordinator collection to the transaction
////////////////////////////////////////////////////////////////////////////////

        int processCollectionCoordinator (triagens::aql::Collection* collection) {
          TRI_voc_cid_t cid = this->resolver()->getCollectionIdCluster(collection->getName());

          return this->addCollection(cid, collection->getName().c_str(), collection->accessType);
        }

////////////////////////////////////////////////////////////////////////////////
/// @brief add a regular collection to the transaction
////////////////////////////////////////////////////////////////////////////////

        int processCollectionNormal (triagens::aql::Collection* collection) {
          TRI_vocbase_col_t const* col = this->resolver()->getCollectionStruct(collection->getName());
          TRI_voc_cid_t cid = 0;

          if (col != nullptr) {
            cid = col->_cid;
          }

          int res = this->addCollection(cid, collection->getName().c_str(), collection->accessType);

          if (res == TRI_ERROR_NO_ERROR &&
              col != nullptr) {
            collection->collection = const_cast<TRI_vocbase_col_t*>(col);
          }

          return res;
        }

////////////////////////////////////////////////////////////////////////////////
/// @brief barrier
////////////////////////////////////////////////////////////////////////////////

        TRI_barrier_t* barrier (TRI_voc_cid_t cid) {
          TRI_transaction_collection_t* trxColl = this->trxCollection(cid);
          return trxColl->_barrier;
        }

////////////////////////////////////////////////////////////////////////////////
/// @brief documentCollection
////////////////////////////////////////////////////////////////////////////////

        TRI_document_collection_t* documentCollection (TRI_voc_cid_t cid) {
          TRI_transaction_collection_t* trxColl = this->trxCollection(cid);
          return trxColl->_collection->_collection;
        }

////////////////////////////////////////////////////////////////////////////////
/// @brief clone, used to make daughter transactions for parts of a distributed
/// AQL query running on the coordinator
////////////////////////////////////////////////////////////////////////////////

        triagens::arango::AqlTransaction* clone () const {
          return new triagens::arango::AqlTransaction(
                           new triagens::arango::StandaloneTransactionContext(),
                           this->_vocbase, 
                           &_collections, false);
        }

////////////////////////////////////////////////////////////////////////////////
/// @brief lockCollections, this is needed in a corner case in AQL: we need
/// to lock all shards in a controlled way when we set up a distributed
/// execution engine. To this end, we prevent the standard mechanism to
/// lock collections on the DBservers when we instanciate the query. Then,
/// in a second round, we need to lock the shards in exactly the right
/// order via an HTTP call. This method is used to implement that HTTP action.
////////////////////////////////////////////////////////////////////////////////

        int lockCollections () {
          auto trx = getInternals();
          size_t i = trx->_collections._length;

          while (i-- > 0) {
            TRI_transaction_collection_t* trxCollection 
              = static_cast<TRI_transaction_collection_t*>
                           (TRI_AtVectorPointer(&trx->_collections, i));
            int res = TRI_UnlockCollectionTransaction(trxCollection, 
                                         trxCollection->_accessType, 0);
            if (res != TRI_ERROR_NO_ERROR) {
              return res;
            }
          }
          return TRI_ERROR_NO_ERROR;
        }

////////////////////////////////////////////////////////////////////////////////
/// @brief keep a copy of the collections, this is needed for the clone 
/// operation
////////////////////////////////////////////////////////////////////////////////

      private:

        std::map<std::string, triagens::aql::Collection*> _collections;
    };

  }
}

#endif

// -----------------------------------------------------------------------------
// --SECTION--                                                       END-OF-FILE
// -----------------------------------------------------------------------------

// Local Variables:
// mode: outline-minor
// outline-regexp: "/// @brief\\|/// {@inheritDoc}\\|/// @page\\|// --SECTION--\\|/// @\\}"
// End:
