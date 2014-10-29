////////////////////////////////////////////////////////////////////////////////
/// @brief wrapper for single collection transactions
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

#ifndef ARANGODB_UTILS_SINGLE_COLLECTION_TRANSACTION_H
#define ARANGODB_UTILS_SINGLE_COLLECTION_TRANSACTION_H 1

#include "Basics/Common.h"

#include "Basics/logging.h"
#include "Basics/voc-errors.h"
#include "Basics/StringUtils.h"
#include "Utils/Transaction.h"
#include "Utils/TransactionContext.h"

#include "VocBase/barrier.h"
#include "VocBase/document-collection.h"
#include "VocBase/server.h"
#include "VocBase/transaction.h"
#include "VocBase/vocbase.h"
#include "VocBase/voc-types.h"

namespace triagens {
  namespace arango {

    class SingleCollectionTransaction : public Transaction {

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

        SingleCollectionTransaction (TransactionContext* transactionContext,
                                     TRI_vocbase_t* vocbase,
                                     TRI_voc_cid_t cid,
                                     TRI_transaction_type_e accessType) 
          : Transaction(transactionContext, vocbase, 0),
            _cid(cid),
            _trxCollection(nullptr),
            _documentCollection(nullptr),
            _accessType(accessType) {

          // add the (sole) collection
          this->addCollection(cid, _accessType);
        }

////////////////////////////////////////////////////////////////////////////////
/// @brief create the transaction, using a collection name
////////////////////////////////////////////////////////////////////////////////

        SingleCollectionTransaction (TransactionContext* transactionContext,
                                     TRI_vocbase_t* vocbase,
                                     std::string const& name,
                                     TRI_transaction_type_e accessType) 
          : Transaction(transactionContext, vocbase, 0),
            _cid(this->resolver()->getCollectionId(name)),
            _trxCollection(nullptr),
            _documentCollection(nullptr),
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

            if (this->_trxCollection != nullptr && this->_trxCollection->_collection != nullptr) {
              this->_documentCollection = this->_trxCollection->_collection->_collection;
            }
          }

          TRI_ASSERT(this->_trxCollection != nullptr);
          return this->_trxCollection;
        }

////////////////////////////////////////////////////////////////////////////////
/// @brief get the underlying document collection
/// note that we have two identical versions because this is called 
/// in two different situations
////////////////////////////////////////////////////////////////////////////////

        inline TRI_document_collection_t* documentCollection () {
          if (this->_documentCollection != nullptr) {
            return this->_documentCollection;
          }

          this->trxCollection();
          TRI_ASSERT(this->_documentCollection != nullptr);

          return this->_documentCollection;
        }

        inline TRI_document_collection_t* documentCollection (TRI_voc_cid_t) {
          return documentCollection();
        }

////////////////////////////////////////////////////////////////////////////////
/// @brief return the barrier for the collection
/// note that the barrier must already exist
/// furthermore note that we have two calling conventions because this
/// is called in two different ways
////////////////////////////////////////////////////////////////////////////////

         inline TRI_barrier_t* barrier () {
           TRI_transaction_collection_t* trxCollection = this->trxCollection();
           TRI_ASSERT(trxCollection->_barrier != nullptr);

           return trxCollection->_barrier;
         }

         inline TRI_barrier_t* barrier (TRI_voc_cid_t) {
           return barrier();
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

        inline int read (TRI_doc_mptr_copy_t* mptr, const std::string& key) {
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

        int read (std::vector<TRI_doc_mptr_copy_t>& docs,
                  TRI_voc_ssize_t skip,
                  TRI_voc_size_t limit,
                  uint32_t* total) {

          return this->readSlice(this->trxCollection(), docs, skip, limit, total);
        }

////////////////////////////////////////////////////////////////////////////////
/// @brief read all documents within a transaction
////////////////////////////////////////////////////////////////////////////////

        int read (std::vector<TRI_doc_mptr_t*>& docs) {
          return this->readSlice(this->trxCollection(), docs);
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
                  TRI_voc_size_t limit,
                  uint32_t* total) {

          return this->readIncremental(this->trxCollection(), docs, internalSkip, batchSize, skip, limit, total);
        }

////////////////////////////////////////////////////////////////////////////////
/// @brief selects documents from a collection, hashing the document key and
/// only returning these documents which fall into a specific partition
////////////////////////////////////////////////////////////////////////////////

        int readPartition (std::vector<TRI_doc_mptr_copy_t>& docs,
                  uint64_t partitionId,
                  uint64_t numberOfPartitions,
                  uint32_t* total) {

          return this->readNth(this->trxCollection(), docs, partitionId, numberOfPartitions, total);
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
/// @brief TRI_document_collection_t* cache
////////////////////////////////////////////////////////////////////////////////

        TRI_document_collection_t* _documentCollection;

////////////////////////////////////////////////////////////////////////////////
/// @brief collection access type
////////////////////////////////////////////////////////////////////////////////

        TRI_transaction_type_e _accessType;

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
