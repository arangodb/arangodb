////////////////////////////////////////////////////////////////////////////////
/// @brief wrapper for single collection transactions
///
/// @file
///
/// DISCLAIMER
///
/// Copyright 2004-2013 triAGENS GmbH, Cologne, Germany
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
/// @author Copyright 2011-2013, triAGENS GmbH, Cologne, Germany
////////////////////////////////////////////////////////////////////////////////

#ifndef TRIAGENS_UTILS_SINGLE_COLLECTION_TRANSACTION_H
#define TRIAGENS_UTILS_SINGLE_COLLECTION_TRANSACTION_H 1

#include "Basics/Common.h"

#include "BasicsC/logging.h"
#include "BasicsC/voc-errors.h"
#include "Basics/StringUtils.h"
#include "Utils/Transaction.h"

#include "VocBase/barrier.h"
#include "VocBase/document-collection.h"
#include "VocBase/server.h"
#include "VocBase/transaction.h"
#include "VocBase/vocbase.h"
#include "VocBase/voc-types.h"

namespace triagens {
  namespace arango {

    template<typename T>
    class SingleCollectionTransaction : public Transaction<T> {

// -----------------------------------------------------------------------------
// --SECTION--                                 class SingleCollectionTransaction
// -----------------------------------------------------------------------------

// -----------------------------------------------------------------------------
// --SECTION--                                      constructors and destructors
// -----------------------------------------------------------------------------

      public:

////////////////////////////////////////////////////////////////////////////////
/// @brief create the transaction, using a collection id
////////////////////////////////////////////////////////////////////////////////

        SingleCollectionTransaction (TRI_vocbase_t* vocbase,
                                     TRI_voc_cid_t cid,
                                     TRI_transaction_type_e accessType) :
          Transaction<T>(vocbase, TRI_GetIdServer(), true),
          _cid(cid),
          _trxCollection(nullptr),
          _accessType(accessType) {

          // add the (sole) collection
          this->addCollection(cid, _accessType);
        }

////////////////////////////////////////////////////////////////////////////////
/// @brief create the transaction, using a collection name
////////////////////////////////////////////////////////////////////////////////

        SingleCollectionTransaction (TRI_vocbase_t* vocbase,
                                     std::string const& name,
                                     TRI_transaction_type_e accessType) :
          Transaction<T>(vocbase, TRI_GetIdServer(), true),
          _cid(this->resolver()->getCollectionId(name)),
          _accessType(accessType) {

          // add the (sole) collection
          this->addCollection(_cid, _accessType);
        }

////////////////////////////////////////////////////////////////////////////////
/// @brief end the transaction
////////////////////////////////////////////////////////////////////////////////

        virtual ~SingleCollectionTransaction () {
        }

// -----------------------------------------------------------------------------
// --SECTION--                                                  public functions
// -----------------------------------------------------------------------------

      public:

////////////////////////////////////////////////////////////////////////////////
/// @brief get the underlying transaction collection
////////////////////////////////////////////////////////////////////////////////

        inline TRI_transaction_collection_t* trxCollection () {
          TRI_ASSERT(this->_cid > 0);

          if (this->_trxCollection == nullptr) {
            this->_trxCollection = TRI_GetCollectionTransaction(this->_trx, this->_cid, _accessType);
          }

          TRI_ASSERT(this->_trxCollection != nullptr);
          return this->_trxCollection;
        }

////////////////////////////////////////////////////////////////////////////////
/// @brief get the underlying primary collection
////////////////////////////////////////////////////////////////////////////////

        inline TRI_document_collection_t* documentCollection () {
          TRI_transaction_collection_t* trxCollection = this->trxCollection();

          TRI_ASSERT(trxCollection != nullptr);
          TRI_ASSERT(trxCollection->_collection != nullptr);
          TRI_ASSERT(trxCollection->_collection->_collection != nullptr);
          
          return trxCollection->_collection->_collection;
        }

////////////////////////////////////////////////////////////////////////////////
/// @brief return the barrier for the collection
/// note that the barrier must already exist
////////////////////////////////////////////////////////////////////////////////
         
         inline TRI_barrier_t* barrier () {
           TRI_transaction_collection_t* trxCollection = this->trxCollection();
           TRI_ASSERT(trxCollection->_barrier != nullptr);

           return trxCollection->_barrier;
         }

////////////////////////////////////////////////////////////////////////////////
/// @brief whether or not a barrier is available for a collection
////////////////////////////////////////////////////////////////////////////////

         inline bool hasBarrier () {
           TRI_transaction_collection_t* trxCollection = this->trxCollection();

           return (trxCollection->_barrier != nullptr);
         }

////////////////////////////////////////////////////////////////////////////////
/// @brief get the underlying collection's id
////////////////////////////////////////////////////////////////////////////////

        inline TRI_voc_cid_t cid () const {
          return _cid;
        }

////////////////////////////////////////////////////////////////////////////////
/// @brief explicitly lock the underlying collection for read access
////////////////////////////////////////////////////////////////////////////////

        int lockRead () {
          return this->lock(this->trxCollection(), TRI_TRANSACTION_READ);
        }

////////////////////////////////////////////////////////////////////////////////
/// @brief explicitly unlock the underlying collection after read access
////////////////////////////////////////////////////////////////////////////////

        int unlockRead () {
          return this->unlock(this->trxCollection(), TRI_TRANSACTION_READ);
        }

////////////////////////////////////////////////////////////////////////////////
/// @brief explicitly lock the underlying collection for write access
////////////////////////////////////////////////////////////////////////////////

        int lockWrite () {
          return this->lock(this->trxCollection(), TRI_TRANSACTION_WRITE);
        }

////////////////////////////////////////////////////////////////////////////////
/// @brief read any (random) document within a transaction
////////////////////////////////////////////////////////////////////////////////

        inline int readRandom (TRI_doc_mptr_copy_t* mptr) {
          TRI_ASSERT(mptr != nullptr);
          return this->readAny(this->trxCollection(), mptr);
        }

////////////////////////////////////////////////////////////////////////////////
/// @brief read a document within a transaction
////////////////////////////////////////////////////////////////////////////////

        inline int read (TRI_doc_mptr_copy_t* mptr, const string& key) {
          TRI_ASSERT(mptr != nullptr);
          return this->readSingle(this->trxCollection(), mptr, key);
        }

////////////////////////////////////////////////////////////////////////////////
/// @brief read all document ids within a transaction
////////////////////////////////////////////////////////////////////////////////

        int read (std::vector<std::string>& ids) {
          return this->readAll(this->trxCollection(), ids, true);
        }

////////////////////////////////////////////////////////////////////////////////
/// @brief read a document within a transaction
////////////////////////////////////////////////////////////////////////////////

        int readPositional (std::vector<TRI_doc_mptr_copy_t>& documents,
                            int64_t offset,
                            int64_t count) {
          return this->readOrdered(this->trxCollection(), documents, offset, count);
        }

////////////////////////////////////////////////////////////////////////////////
/// @brief read documents within a transaction, using skip and limit
////////////////////////////////////////////////////////////////////////////////

        int read (vector<TRI_doc_mptr_copy_t>& docs,
                  TRI_voc_ssize_t skip,
                  TRI_voc_size_t limit,
                  uint32_t* total) {

          return this->readSlice(this->trxCollection(), docs, skip, limit, total);
        }

////////////////////////////////////////////////////////////////////////////////
/// @brief read documents within a transaction, using skip and limit and an
/// internal offset into the primary index. this can be used for incremental
/// access to the documents
////////////////////////////////////////////////////////////////////////////////

        int readOffset (std::vector<TRI_doc_mptr_copy_t>& docs,
                  TRI_voc_size_t& internalSkip,
                  TRI_voc_size_t batchSize,
                  TRI_voc_ssize_t skip,
                  uint32_t* total) {

          return this->readIncremental(this->trxCollection(), docs, internalSkip, batchSize, skip, total);
        }

// -----------------------------------------------------------------------------
// --SECTION--                                                 private variables
// -----------------------------------------------------------------------------

      private:

////////////////////////////////////////////////////////////////////////////////
/// @brief collection id
////////////////////////////////////////////////////////////////////////////////

        TRI_voc_cid_t _cid;

////////////////////////////////////////////////////////////////////////////////
/// @brief trxCollection cache
////////////////////////////////////////////////////////////////////////////////

        TRI_transaction_collection_t* _trxCollection;

////////////////////////////////////////////////////////////////////////////////
/// @brief collection access type
////////////////////////////////////////////////////////////////////////////////

        TRI_transaction_type_e _accessType;

    };

  }
}

#endif

// Local Variables:
// mode: outline-minor
// outline-regexp: "/// @brief\\|/// {@inheritDoc}\\|/// @addtogroup\\|/// @page\\|// --SECTION--\\|/// @\\}"
// End:
