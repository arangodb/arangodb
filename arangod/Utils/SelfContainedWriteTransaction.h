////////////////////////////////////////////////////////////////////////////////
/// @brief wrapper for self-contained, single collection write transactions
///
/// @file
///
/// DISCLAIMER
///
/// Copyright 2004-2012 triAGENS GmbH, Cologne, Germany
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
/// Copyright holder is triAGENS GmbH, Cologne, Germany
///
/// @author Jan Steemann
/// @author Copyright 2011-2012, triAGENS GmbH, Cologne, Germany
////////////////////////////////////////////////////////////////////////////////

#ifndef TRIAGENS_UTILS_SELF_CONTAINED_WRITE_TRANSACTION_H
#define TRIAGENS_UTILS_SELF_CONTAINED_WRITE_TRANSACTION_H 1

#include "Utils/CollectionWriteLock.h"
#include "Utils/SelfContainedTransaction.h"

namespace triagens {
  namespace arango {

// -----------------------------------------------------------------------------
// --SECTION--                               class SelfContainedWriteTransaction
// -----------------------------------------------------------------------------

    class SelfContainedWriteTransaction : public SelfContainedTransaction {

////////////////////////////////////////////////////////////////////////////////
/// @addtogroup ArangoDB
/// @{
////////////////////////////////////////////////////////////////////////////////

// -----------------------------------------------------------------------------
// --SECTION--                                      constructors and destructors
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @addtogroup ArangoDB
/// @{
////////////////////////////////////////////////////////////////////////////////

      public:

////////////////////////////////////////////////////////////////////////////////
/// @brief create the transaction, using a collection object
////////////////////////////////////////////////////////////////////////////////

        SelfContainedWriteTransaction (Collection* collection) :
          SelfContainedTransaction(collection), 
          _synchronous(false) {
        }

////////////////////////////////////////////////////////////////////////////////
/// @brief end the transaction
////////////////////////////////////////////////////////////////////////////////

        ~SelfContainedWriteTransaction () {
        }

////////////////////////////////////////////////////////////////////////////////
/// @}
////////////////////////////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////////////////////////
/// @addtogroup ArangoDB
/// @{
////////////////////////////////////////////////////////////////////////////////

// -----------------------------------------------------------------------------
// --SECTION--                                                  public functions
// -----------------------------------------------------------------------------

      public:

////////////////////////////////////////////////////////////////////////////////
/// @brief return whether the last write in a transaction was synchronous
////////////////////////////////////////////////////////////////////////////////

        inline bool synchronous () const {
          return _synchronous;
        }

////////////////////////////////////////////////////////////////////////////////
/// @brief create a single document within a transaction
////////////////////////////////////////////////////////////////////////////////

        TRI_doc_mptr_t createDocument (TRI_json_t const* json, 
                                       bool forceSync) {
          TRI_primary_collection_t* primary = _collection->primary();
          TRI_doc_operation_context_t context;

          TRI_InitContextPrimaryCollection(&context, primary, TRI_DOC_UPDATE_ERROR, forceSync);
          _synchronous = context._sync;

          CollectionWriteLock lock(_collection);

          return primary->createJson(&context, TRI_DOC_MARKER_KEY_DOCUMENT, json, 0);
        }

////////////////////////////////////////////////////////////////////////////////
/// @brief create a single edge within a transaction
////////////////////////////////////////////////////////////////////////////////

        TRI_doc_mptr_t createEdge (TRI_json_t const* json, 
                                   bool forceSync, 
                                   void const* data) {
          TRI_primary_collection_t* primary = _collection->primary();
          TRI_doc_operation_context_t context;

          TRI_InitContextPrimaryCollection(&context, primary, TRI_DOC_UPDATE_ERROR, forceSync);
          _synchronous = context._sync;

          CollectionWriteLock lock(_collection);

          return primary->createJson(&context, TRI_DOC_MARKER_KEY_EDGE, json, data);
        }

////////////////////////////////////////////////////////////////////////////////
/// @brief update a single document within a transaction
////////////////////////////////////////////////////////////////////////////////

        TRI_doc_mptr_t updateJson (const string& key, 
                                   TRI_json_t* const json, 
                                   const TRI_doc_update_policy_e policy, 
                                   bool forceSync, 
                                   const TRI_voc_rid_t expectedRevision, 
                                   TRI_voc_rid_t* actualRevision) {
          TRI_primary_collection_t* primary = _collection->primary();
          TRI_doc_operation_context_t context;

          TRI_InitContextPrimaryCollection(&context, primary, policy, forceSync);
          context._expectedRid = expectedRevision;
          context._previousRid = actualRevision;
          _synchronous = context._sync;

          CollectionWriteLock lock(_collection);

          return primary->updateJson(&context, json, (TRI_voc_key_t) key.c_str());
        }

////////////////////////////////////////////////////////////////////////////////
/// @brief delete a single document within a transaction
////////////////////////////////////////////////////////////////////////////////

        int destroy (const string& key, 
                     const TRI_doc_update_policy_e policy, 
                     bool forceSync, 
                     const TRI_voc_rid_t expectedRevision, 
                     TRI_voc_rid_t* actualRevision) {
          TRI_primary_collection_t* primary = _collection->primary();
          TRI_doc_operation_context_t context;

          TRI_InitContextPrimaryCollection(&context, primary, policy, forceSync);
          context._expectedRid = expectedRevision;
          context._previousRid = actualRevision;
          _synchronous = context._sync;

          CollectionWriteLock lock(_collection);

          return primary->destroy(&context, (TRI_voc_key_t) key.c_str());
        }

////////////////////////////////////////////////////////////////////////////////
/// @}
////////////////////////////////////////////////////////////////////////////////

// -----------------------------------------------------------------------------
// --SECTION--                                       virtual protected functions
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @addtogroup ArangoDB
/// @{
////////////////////////////////////////////////////////////////////////////////

      protected:

////////////////////////////////////////////////////////////////////////////////
/// @brief get the transaction type
////////////////////////////////////////////////////////////////////////////////

        TRI_transaction_type_e type () const {
          return TRI_TRANSACTION_WRITE;
        }

////////////////////////////////////////////////////////////////////////////////
/// @}
////////////////////////////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////////////////////////
/// @addtogroup ArangoDB
/// @{
////////////////////////////////////////////////////////////////////////////////

// -----------------------------------------------------------------------------
// --SECTION--                                                 private variables
// -----------------------------------------------------------------------------

      private:

////////////////////////////////////////////////////////////////////////////////
/// @brief whether of not the last write action was executed synchronously
////////////////////////////////////////////////////////////////////////////////

        bool _synchronous;

////////////////////////////////////////////////////////////////////////////////
/// @}
////////////////////////////////////////////////////////////////////////////////

    };

  }
}

#endif

// Local Variables:
// mode: outline-minor
// outline-regexp: "^\\(/// @brief\\|/// {@inheritDoc}\\|/// @addtogroup\\|/// @page\\|// --SECTION--\\|/// @\\}\\)"
// End:
