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
#include "Utils/V8TransactionContext.h"
#include "VocBase/transaction.h"
#include "VocBase/vocbase.h"

#define AQL_TRANSACTION_V8 triagens::arango::AqlTransaction<triagens::arango::V8TransactionContext<true>>

namespace triagens {
  namespace arango {

    template<typename T>
    class AqlTransaction : public Transaction<T> {

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

        AqlTransaction (TRI_vocbase_t* vocbase,
                        std::map<std::string, triagens::aql::Collection*>* collections)
          : Transaction<T>(vocbase, 0) {

          this->addHint(TRI_TRANSACTION_HINT_LOCK_ENTIRELY, false);

          for (auto it = collections->begin(); it != collections->end(); ++it) {
            if (processCollection((*it).second) != TRI_ERROR_NO_ERROR) {
              break;
            }
          }
        }

////////////////////////////////////////////////////////////////////////////////
/// @brief end the transaction
////////////////////////////////////////////////////////////////////////////////

        ~AqlTransaction () {
        }

        int addCollectionList (std::map<std::string, triagens::aql::Collection*>* collections) {
          int ret = TRI_ERROR_NO_ERROR;
          for (auto it = collections->begin(); it != collections->end(); ++it) {
            ret = processCollection((*it).second);
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
          TRI_voc_cid_t cid = this->resolver()->getCollectionIdCluster(collection->name);

          return this->addCollection(cid, collection->name.c_str(), collection->accessType);
        }

////////////////////////////////////////////////////////////////////////////////
/// @brief add a regular collection to the transaction
////////////////////////////////////////////////////////////////////////////////

        int processCollectionNormal (triagens::aql::Collection* collection) {
          TRI_vocbase_col_t const* col = this->resolver()->getCollectionStruct(collection->name);
          TRI_voc_cid_t cid = 0;

          if (col != nullptr) {
            cid = col->_cid;
          }

          int res = this->addCollection(cid, collection->name.c_str(), collection->accessType);

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
