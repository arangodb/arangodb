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

#include "BasicsC/common.h"

#include "BasicsC/voc-errors.h"
#include "Basics/StringUtils.h"
#include "Logger/Logger.h"
#include "Utils/Transaction.h"

#include "VocBase/barrier.h"
#include "VocBase/primary-collection.h"
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

////////////////////////////////////////////////////////////////////////////////
/// @addtogroup ArangoDB
/// @{
////////////////////////////////////////////////////////////////////////////////

      public:

////////////////////////////////////////////////////////////////////////////////
/// @brief create the transaction, using a collection object
///
/// A single collection transaction operates on a single collection (you guessed
/// it)
////////////////////////////////////////////////////////////////////////////////

        SingleCollectionTransaction (TRI_vocbase_t* const vocbase,
                                     const triagens::arango::CollectionNameResolver& resolver,
                                     const TRI_voc_cid_t cid,
                                     const TRI_transaction_type_e accessType) :
          Transaction<T>(vocbase, resolver),
          _cid(cid),
          _accessType(accessType) {

          // add the (sole) collection
          this->addCollection(cid, _accessType);
        }

////////////////////////////////////////////////////////////////////////////////
/// @brief end the transaction
////////////////////////////////////////////////////////////////////////////////

        virtual ~SingleCollectionTransaction () {
        }

////////////////////////////////////////////////////////////////////////////////
/// @}
////////////////////////////////////////////////////////////////////////////////

// -----------------------------------------------------------------------------
// --SECTION--                                                  public functions
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @addtogroup ArangoDB
/// @{
////////////////////////////////////////////////////////////////////////////////

      public:

////////////////////////////////////////////////////////////////////////////////
/// @brief get the underlying transaction collection
////////////////////////////////////////////////////////////////////////////////

        inline TRI_transaction_collection_t* trxCollection () {
          TRI_ASSERT_MAINTAINER(_cid > 0);

          TRI_transaction_collection_t* trxCollection = TRI_GetCollectionTransaction(this->_trx, this->_cid, _accessType);

          TRI_ASSERT_MAINTAINER(trxCollection != 0);
          return trxCollection;
        }

////////////////////////////////////////////////////////////////////////////////
/// @brief get the underlying primary collection
////////////////////////////////////////////////////////////////////////////////

        inline TRI_primary_collection_t* primaryCollection () {
          TRI_transaction_collection_t* trxCollection = this->trxCollection();

          TRI_ASSERT_MAINTAINER(trxCollection != 0);
          TRI_ASSERT_MAINTAINER(trxCollection->_collection != 0);
          TRI_ASSERT_MAINTAINER(trxCollection->_collection->_collection != 0);
          
          return trxCollection->_collection->_collection;
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
/// @brief explicitly lock the underlying collection for write access
////////////////////////////////////////////////////////////////////////////////

        int lockWrite () {
          return this->lock(this->trxCollection(), TRI_TRANSACTION_WRITE);
        }

////////////////////////////////////////////////////////////////////////////////
/// @brief read any (random) document within a transaction
////////////////////////////////////////////////////////////////////////////////

        inline int readRandom (TRI_doc_mptr_t* mptr, TRI_barrier_t** barrier) {
          return this->readAny(this->trxCollection(), mptr, barrier);
        }

////////////////////////////////////////////////////////////////////////////////
/// @brief read a document within a transaction
////////////////////////////////////////////////////////////////////////////////

        inline int read (TRI_doc_mptr_t* mptr, const string& key) {
          return this->readSingle(this->trxCollection(), mptr, key);
        }

////////////////////////////////////////////////////////////////////////////////
/// @brief read all document ids within a transaction
////////////////////////////////////////////////////////////////////////////////

        int read (vector<string>& ids) {
          return this->readAll(this->trxCollection(), ids, true);
        }

////////////////////////////////////////////////////////////////////////////////
/// @brief read a document within a transaction
////////////////////////////////////////////////////////////////////////////////

        int readPositional (vector<TRI_doc_mptr_t const*>& documents,
                            int64_t offset,
                            int64_t count) {
          return this->readOrdered(this->trxCollection(), documents, offset, count);
        }

////////////////////////////////////////////////////////////////////////////////
/// @brief read documents within a transaction, using skip and limit
////////////////////////////////////////////////////////////////////////////////

        int read (vector<TRI_doc_mptr_t>& docs,
                  TRI_barrier_t** barrier,
                  TRI_voc_ssize_t skip,
                  TRI_voc_size_t limit,
                  uint32_t* total) {

          return this->readSlice(this->trxCollection(), docs, barrier, skip, limit, total);
        }

////////////////////////////////////////////////////////////////////////////////
/// @brief read documents within a transaction, using skip and limit and an
/// internal offset into the primary index. this can be used for incremental
/// access to the documents
////////////////////////////////////////////////////////////////////////////////

        int readOffset (vector<TRI_doc_mptr_t>& docs,
                  TRI_barrier_t** barrier,
                  TRI_voc_size_t& internalSkip,
                  TRI_voc_size_t batchSize,
                  TRI_voc_ssize_t skip,
                  uint32_t* total) {

          return this->readIncremental(this->trxCollection(), docs, barrier, internalSkip, batchSize, skip, total);
        }

////////////////////////////////////////////////////////////////////////////////
/// @}
////////////////////////////////////////////////////////////////////////////////

// -----------------------------------------------------------------------------
// --SECTION--                                                 private variables
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @addtogroup ArangoDB
/// @{
////////////////////////////////////////////////////////////////////////////////

      private:

////////////////////////////////////////////////////////////////////////////////
/// @brief collection id
////////////////////////////////////////////////////////////////////////////////

        TRI_voc_cid_t _cid;

////////////////////////////////////////////////////////////////////////////////
/// @brief collection access type
////////////////////////////////////////////////////////////////////////////////

        TRI_transaction_type_e _accessType;

////////////////////////////////////////////////////////////////////////////////
/// @}
////////////////////////////////////////////////////////////////////////////////

    };

  }
}

#endif

// Local Variables:
// mode: outline-minor
// outline-regexp: "/// @brief\\|/// {@inheritDoc}\\|/// @addtogroup\\|/// @page\\|// --SECTION--\\|/// @\\}"
// End:
