////////////////////////////////////////////////////////////////////////////////
/// @brief wrapper for self-contained, single collection write transactions
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

#ifndef ARANGODB_UTILS_SINGLE_COLLECTION_WRITE_TRANSACTION_H
#define ARANGODB_UTILS_SINGLE_COLLECTION_WRITE_TRANSACTION_H 1

#include "Basics/Common.h"

#include "Utils/CollectionNameResolver.h"
#include "Utils/SingleCollectionTransaction.h"

#include "ShapedJson/shaped-json.h"
#include "VocBase/transaction.h"
#include "VocBase/vocbase.h"
#include "VocBase/voc-types.h"

namespace triagens {
  namespace arango {

    template<uint64_t N>
    class SingleCollectionWriteTransaction : public SingleCollectionTransaction {

// -----------------------------------------------------------------------------
// --SECTION--                            class SingleCollectionWriteTransaction
// -----------------------------------------------------------------------------

// -----------------------------------------------------------------------------
// --SECTION--                                      constructors and destructors
// -----------------------------------------------------------------------------

      public:

////////////////////////////////////////////////////////////////////////////////
/// @brief create the transaction, using a collection object
///
/// A single collection write transaction executes write operations on the
/// underlying collection. It can be limited to at most one write operation.
/// If so, whenever it tries to execute more than one write operation, an error
/// can be raised automatically. Executing only one write operation is
/// sufficient for the basic CRUD operations and allows using the transaction
/// API even efficiently for single CRUD operations. This optimisation will
/// allow avoiding a lot of the general transaction overhead.
////////////////////////////////////////////////////////////////////////////////

        SingleCollectionWriteTransaction (TransactionContext* transactionContext,
                                          TRI_vocbase_t* vocbase,
                                          TRI_voc_cid_t cid) 
          : SingleCollectionTransaction(transactionContext, vocbase, cid, TRI_TRANSACTION_WRITE),
            _numWrites(0) {

          if (N == 1) {
            this->addHint(TRI_TRANSACTION_HINT_SINGLE_OPERATION, false);
          }
        }

////////////////////////////////////////////////////////////////////////////////
/// @brief same as above, but create using collection name
////////////////////////////////////////////////////////////////////////////////

        SingleCollectionWriteTransaction (TransactionContext* transactionContext, 
                                          TRI_vocbase_t* vocbase,
                                          std::string const& name) 
          : SingleCollectionTransaction(transactionContext, vocbase, name, TRI_TRANSACTION_WRITE),
            _numWrites(0) {

          if (N == 1) {
            this->addHint(TRI_TRANSACTION_HINT_SINGLE_OPERATION, false);
          }
        }

////////////////////////////////////////////////////////////////////////////////
/// @brief end the transaction
////////////////////////////////////////////////////////////////////////////////

        virtual ~SingleCollectionWriteTransaction () {
        }

// -----------------------------------------------------------------------------
// --SECTION--                                                  public functions
// -----------------------------------------------------------------------------

      public:

////////////////////////////////////////////////////////////////////////////////
/// @brief return whether a write in the transaction was synchronous
////////////////////////////////////////////////////////////////////////////////

        inline bool synchronous () const {
          return TRI_WasSynchronousCollectionTransaction(this->_trx, this->cid());
        }

////////////////////////////////////////////////////////////////////////////////
/// @brief explicitly lock the underlying collection for write access
////////////////////////////////////////////////////////////////////////////////

        int lockWrite () {
          return this->lock(this->trxCollection(), TRI_TRANSACTION_WRITE);
        }

////////////////////////////////////////////////////////////////////////////////
/// @brief create a single document within a transaction, using json
////////////////////////////////////////////////////////////////////////////////

        int createDocument (TRI_doc_mptr_copy_t* mptr,
                            TRI_json_t const* json,
                            bool forceSync) {
#ifdef TRI_ENABLE_MAINTAINER_MODE
          if (_numWrites++ > N) {
            return TRI_ERROR_TRANSACTION_INTERNAL;
          }
#endif

          TRI_ASSERT(mptr != nullptr);

          return this->create(this->trxCollection(),
                              mptr,
                              json,
                              nullptr,
                              forceSync);
        }

////////////////////////////////////////////////////////////////////////////////
/// @brief create a single edge within a transaction, using json
////////////////////////////////////////////////////////////////////////////////

        int createEdge (TRI_doc_mptr_copy_t* mptr,
                        TRI_json_t const* json,
                        bool forceSync,
                        void const* data) {
#ifdef TRI_ENABLE_MAINTAINER_MODE
          if (_numWrites++ > N) {
            return TRI_ERROR_TRANSACTION_INTERNAL;
          }
#endif

          TRI_ASSERT(mptr != nullptr);

          return this->create(this->trxCollection(),
                              mptr,
                              json,
                              data,
                              forceSync);
        }

////////////////////////////////////////////////////////////////////////////////
/// @brief create a single document within a transaction, using shaped json
////////////////////////////////////////////////////////////////////////////////

        int createDocument (TRI_voc_key_t key,
                            TRI_doc_mptr_copy_t* mptr,
                            TRI_shaped_json_t const* shaped,
                            bool forceSync) {
#ifdef TRI_ENABLE_MAINTAINER_MODE
          if (_numWrites++ > N) {
            return TRI_ERROR_TRANSACTION_INTERNAL;
          }
#endif

          TRI_ASSERT(mptr != nullptr);

          return this->create(this->trxCollection(),
                              key,
                              0,
                              mptr,
                              shaped,
                              nullptr,
                              forceSync);
        }

////////////////////////////////////////////////////////////////////////////////
/// @brief create a single edge within a transaction, using shaped json
////////////////////////////////////////////////////////////////////////////////

        int createEdge (TRI_voc_key_t key,
                        TRI_doc_mptr_copy_t* mptr,
                        TRI_shaped_json_t const* shaped,
                        bool forceSync,
                        void const* data) {
#ifdef TRI_ENABLE_MAINTAINER_MODE
          if (_numWrites++ > N) {
            return TRI_ERROR_TRANSACTION_INTERNAL;
          }
#endif

          TRI_ASSERT(mptr != nullptr);

          return this->create(this->trxCollection(),
                              key,
                              0,
                              mptr,
                              shaped,
                              data,
                              forceSync);
        }

////////////////////////////////////////////////////////////////////////////////
/// @brief update (replace!) a single document within a transaction,
/// using json
////////////////////////////////////////////////////////////////////////////////

        int updateDocument (std::string const& key,
                            TRI_doc_mptr_copy_t* mptr,
                            TRI_json_t* const json,
                            TRI_doc_update_policy_e policy,
                            bool forceSync,
                            TRI_voc_rid_t expectedRevision,
                            TRI_voc_rid_t* actualRevision) {
#ifdef TRI_ENABLE_MAINTAINER_MODE
          if (_numWrites++ > N) {
            return TRI_ERROR_TRANSACTION_INTERNAL;
          }
#endif

          TRI_ASSERT(mptr != nullptr);

          return this->update(this->trxCollection(),
                              key,
                              0,
                              mptr,
                              json,
                              policy,
                              expectedRevision,
                              actualRevision,
                              forceSync);
        }

////////////////////////////////////////////////////////////////////////////////
/// @brief update (replace!) a single document within a transaction,
/// using shaped json
////////////////////////////////////////////////////////////////////////////////

        int updateDocument (std::string const& key,
                            TRI_doc_mptr_copy_t* mptr,
                            TRI_shaped_json_t* const shaped,
                            TRI_doc_update_policy_e policy,
                            bool forceSync,
                            TRI_voc_rid_t expectedRevision,
                            TRI_voc_rid_t* actualRevision) {
#ifdef TRI_ENABLE_MAINTAINER_MODE
          if (_numWrites++ > N) {
            return TRI_ERROR_TRANSACTION_INTERNAL;
          }
#endif

          TRI_ASSERT(mptr != nullptr);

          return this->update(this->trxCollection(),
                              key,
                              0,
                              mptr,
                              shaped,
                              policy,
                              expectedRevision,
                              actualRevision,
                              forceSync);
        }

////////////////////////////////////////////////////////////////////////////////
/// @brief delete a single document within a transaction
////////////////////////////////////////////////////////////////////////////////

        int deleteDocument (std::string const& key,
                            TRI_doc_update_policy_e policy,
                            bool forceSync,
                            TRI_voc_rid_t expectedRevision,
                            TRI_voc_rid_t* actualRevision) {
#ifdef TRI_ENABLE_MAINTAINER_MODE
          if (_numWrites++ > N) {
            return TRI_ERROR_TRANSACTION_INTERNAL;
          }
#endif

          return this->remove(this->trxCollection(),
                              key,
                              0,
                              policy,
                              expectedRevision,
                              actualRevision,
                              forceSync);
        }

////////////////////////////////////////////////////////////////////////////////
/// @brief truncate all documents within a transaction
////////////////////////////////////////////////////////////////////////////////

        int truncate (bool forceSync) {
#ifdef TRI_ENABLE_MAINTAINER_MODE
          if (_numWrites++ > N) {
            return TRI_ERROR_TRANSACTION_INTERNAL;
          }
#endif
          return this->removeAll(this->trxCollection(), forceSync);
        }

// -----------------------------------------------------------------------------
// --SECTION--                                                 private variables
// -----------------------------------------------------------------------------

      private:

////////////////////////////////////////////////////////////////////////////////
/// @brief number of writes the transaction has executed
/// the value should be 0 or 1, but not get any higher because values higher
/// than one indicate internal errors
////////////////////////////////////////////////////////////////////////////////

        uint64_t _numWrites;

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
