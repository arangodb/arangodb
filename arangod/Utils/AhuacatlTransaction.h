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

#ifndef ARANGODB_UTILS_AHUACATL_TRANSACTION_H
#define ARANGODB_UTILS_AHUACATL_TRANSACTION_H 1

#include "Basics/Common.h"

#include "Cluster/ServerState.h"

#include "Utils/CollectionNameResolver.h"
#include "Utils/Transaction.h"
#include "Utils/V8TransactionContext.h"
#include "VocBase/server.h"
#include "VocBase/transaction.h"

struct TRI_vocbase_s;

namespace triagens {
  namespace arango {

    class AhuacatlTransaction : public Transaction {

// -----------------------------------------------------------------------------
// --SECTION--                                         class AhuacatlTransaction
// -----------------------------------------------------------------------------

// -----------------------------------------------------------------------------
// --SECTION--                                      constructors and destructors
// -----------------------------------------------------------------------------

      public:

////////////////////////////////////////////////////////////////////////////////
/// @brief create the transaction and add all collections from the query
/// context
////////////////////////////////////////////////////////////////////////////////

        AhuacatlTransaction (struct TRI_vocbase_s* vocbase,
                             TRI_aql_context_t* context)
          : Transaction(new V8TransactionContext(true), vocbase, 0),
            _context(context) {

          this->addHint(TRI_TRANSACTION_HINT_LOCK_ENTIRELY, false);

          TRI_vector_pointer_t* collections = &context->_collections;

          size_t const n = collections->_length;

          for (size_t i = 0; i < n; ++i) {
            TRI_aql_collection_t* collection = static_cast<TRI_aql_collection_t*>(TRI_AtVectorPointer(collections, i));

            processCollection(collection);
          }
        }

////////////////////////////////////////////////////////////////////////////////
/// @brief end the transaction
////////////////////////////////////////////////////////////////////////////////

        ~AhuacatlTransaction () {
        }

////////////////////////////////////////////////////////////////////////////////
/// @brief add a collection to the transaction
////////////////////////////////////////////////////////////////////////////////

        void processCollection (TRI_aql_collection_t* collection) {
          if (ServerState::instance()->isCoordinator()) {
            processCollectionCoordinator(collection);
          }
          else {
            processCollectionNormal(collection);
          }
        }

////////////////////////////////////////////////////////////////////////////////
/// @brief add a coordinator collection to the transaction
////////////////////////////////////////////////////////////////////////////////

        void processCollectionCoordinator (TRI_aql_collection_t* collection) {
          TRI_voc_cid_t cid = this->resolver()->getCollectionIdCluster(collection->_name);

          this->addCollection(cid, collection->_name, getCollectionAccessType(collection));
        }

////////////////////////////////////////////////////////////////////////////////
/// @brief add a regular collection to the transaction
////////////////////////////////////////////////////////////////////////////////

        void processCollectionNormal (TRI_aql_collection_t* collection) {
          TRI_vocbase_col_t const* col = this->resolver()->getCollectionStruct(collection->_name);
          TRI_voc_cid_t cid = 0;

          if (col != nullptr) {
            cid = col->_cid;
          }

          int res = this->addCollection(cid, collection->_name, getCollectionAccessType(collection));

          if (res == TRI_ERROR_NO_ERROR && col != nullptr) {
            collection->_collection = const_cast<TRI_vocbase_col_t*>(col);
          }
        }

////////////////////////////////////////////////////////////////////////////////
/// @brief determine the access type (read | write) for a collection
////////////////////////////////////////////////////////////////////////////////

        TRI_transaction_type_e getCollectionAccessType (TRI_aql_collection_t const* collection) const {
          if (_context->_type == TRI_AQL_QUERY_READ ||
              (_context->_writeCollection != nullptr &&
               strcmp(collection->_name, _context->_writeCollection) != 0)) {
            // read-only query or write-query with a different write-to collection
            return TRI_TRANSACTION_READ;
          }

          // data-modifying query
          return TRI_TRANSACTION_WRITE;
        }

// -----------------------------------------------------------------------------
// --SECTION--                                                 private variables
// -----------------------------------------------------------------------------

      private:

        TRI_aql_context_t* _context;

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
