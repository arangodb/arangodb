////////////////////////////////////////////////////////////////////////////////
/// @brief base transaction wrapper
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

#ifndef TRIAGENS_UTILS_TRANSACTION_H
#define TRIAGENS_UTILS_TRANSACTION_H 1

#include "VocBase/barrier.h"
#include "VocBase/document-collection.h"
#include "VocBase/transaction.h"
#include "VocBase/update-policy.h"
#include "VocBase/vocbase.h"
#include "VocBase/voc-shaper.h"

#include "BasicsC/random.h"
#include "BasicsC/tri-strings.h"

#include "Logger/Logger.h"
#include "Utils/CollectionNameResolver.h"
#include "Utils/DocumentHelper.h"

namespace triagens {
  namespace arango {

// -----------------------------------------------------------------------------
// --SECTION--                                                 class Transaction
// -----------------------------------------------------------------------------


    template<typename T>
    class Transaction : public T {

////////////////////////////////////////////////////////////////////////////////
/// @addtogroup ArangoDB
/// @{
////////////////////////////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////////////////////////
/// @brief Transaction
////////////////////////////////////////////////////////////////////////////////

      private:
        Transaction (const Transaction&);
        Transaction& operator= (const Transaction&);

////////////////////////////////////////////////////////////////////////////////
/// @}
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
/// @brief create the transaction
////////////////////////////////////////////////////////////////////////////////

        Transaction (TRI_vocbase_t* const vocbase,
                     const triagens::arango::CollectionNameResolver& resolver) :
          T(),
          _setupState(TRI_ERROR_NO_ERROR),
          _nestingLevel(0),
          _hints(0),
          _timeout(0.0),
          _waitForSync(false),
          _trx(0),
          _vocbase(vocbase),
          _resolver(resolver) {

          TRI_ASSERT_MAINTAINER(_vocbase != 0);

          this->setupTransaction();
        }

////////////////////////////////////////////////////////////////////////////////
/// @brief destroy the transaction
////////////////////////////////////////////////////////////////////////////////

        virtual ~Transaction () {
          if (_trx == 0) {
            return;
          }
          
          if (isEmbeddedTransaction()) {
            _trx->_nestingLevel--;
          }
          else {
            if (getStatus() == TRI_TRANSACTION_RUNNING) {
              // auto abort a running transaction
              this->abort();
            }

            // free the data associated with the transaction
            freeTransaction();
          }
        }

////////////////////////////////////////////////////////////////////////////////
/// @}
////////////////////////////////////////////////////////////////////////////////

// -----------------------------------------------------------------------------
// --SECTION--                                                    public methods
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @addtogroup ArangoDB
/// @{
////////////////////////////////////////////////////////////////////////////////

      public:

////////////////////////////////////////////////////////////////////////////////
/// @brief return the collection name resolver
////////////////////////////////////////////////////////////////////////////////
        
        const CollectionNameResolver& resolver () const {
          return _resolver;
        }

////////////////////////////////////////////////////////////////////////////////
/// @brief whether or not the transaction is embedded
////////////////////////////////////////////////////////////////////////////////

        inline bool isEmbeddedTransaction () const {
          return (_nestingLevel > 0);
        }

////////////////////////////////////////////////////////////////////////////////
/// @brief whether or not shaped json in this trx should be copied
////////////////////////////////////////////////////////////////////////////////

        inline bool mustCopyShapedJson () const {
          if (_trx != 0 && _trx->_hasOperations) {
            return true;
          }

          return false;
        }

////////////////////////////////////////////////////////////////////////////////
/// @brief get the status of the transaction
////////////////////////////////////////////////////////////////////////////////

        inline TRI_transaction_status_e getStatus () const {
          if (_trx != 0) {
            return _trx->_status;
          }

          return TRI_TRANSACTION_UNDEFINED;
        }

////////////////////////////////////////////////////////////////////////////////
/// @brief begin the transaction
////////////////////////////////////////////////////////////////////////////////

        int begin () {
          if (_trx == 0) {
            return TRI_ERROR_TRANSACTION_INTERNAL;
          }
          
          if (_setupState != TRI_ERROR_NO_ERROR) {
            return _setupState;
          }

          int res = TRI_BeginTransaction(_trx, _hints, _nestingLevel);

          return res;
        }

////////////////////////////////////////////////////////////////////////////////
/// @brief commit / finish the transaction
////////////////////////////////////////////////////////////////////////////////

        int commit () {
          if (_trx == 0 || getStatus() != TRI_TRANSACTION_RUNNING) {
            // transaction not created or not running
            return TRI_ERROR_TRANSACTION_INTERNAL;
          }

          int res = TRI_CommitTransaction(_trx, _nestingLevel);

          return res;
        }

////////////////////////////////////////////////////////////////////////////////
/// @brief abort the transaction
////////////////////////////////////////////////////////////////////////////////

        int abort () {
          if (_trx == 0 || getStatus() != TRI_TRANSACTION_RUNNING) {
            // transaction not created or not running
            return TRI_ERROR_TRANSACTION_INTERNAL;
          }

          int res = TRI_AbortTransaction(_trx, _nestingLevel);

          return res;
        }

////////////////////////////////////////////////////////////////////////////////
/// @brief finish a transaction (commit or abort), based on the previous state
////////////////////////////////////////////////////////////////////////////////

        int finish (const int errorNum) {
          int res;

          if (errorNum == TRI_ERROR_NO_ERROR) {
            // there was no previous error, so we'll commit
            res = this->commit();
          }
          else {
            // there was a previous error, so we'll abort
            this->abort();

            // return original error number
            res = errorNum;
          }
          
          return res;
        }

////////////////////////////////////////////////////////////////////////////////
/// @}
////////////////////////////////////////////////////////////////////////////////

// -----------------------------------------------------------------------------
// --SECTION--                                                 protected methods
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @addtogroup ArangoDB
/// @{
////////////////////////////////////////////////////////////////////////////////

      protected:

////////////////////////////////////////////////////////////////////////////////
/// @brief return a collection's primary collection
////////////////////////////////////////////////////////////////////////////////
         
         TRI_primary_collection_t* primaryCollection (TRI_transaction_collection_t const* trxCollection) const {
           TRI_ASSERT_MAINTAINER(_trx != 0);
           TRI_ASSERT_MAINTAINER(getStatus() == TRI_TRANSACTION_RUNNING);
           TRI_ASSERT_MAINTAINER(trxCollection->_collection != 0);
           TRI_ASSERT_MAINTAINER(trxCollection->_collection->_collection != 0);

           return trxCollection->_collection->_collection;
         }

////////////////////////////////////////////////////////////////////////////////
/// @brief return a collection's shaper
////////////////////////////////////////////////////////////////////////////////
         
         TRI_shaper_t* shaper (TRI_transaction_collection_t const* trxCollection) const {
           TRI_ASSERT_MAINTAINER(_trx != 0);
           TRI_ASSERT_MAINTAINER(getStatus() == TRI_TRANSACTION_RUNNING);
           TRI_ASSERT_MAINTAINER(trxCollection->_collection != 0);
           TRI_ASSERT_MAINTAINER(trxCollection->_collection->_collection != 0);

           return trxCollection->_collection->_collection->_shaper;
         }

////////////////////////////////////////////////////////////////////////////////
/// @brief add a collection by id
////////////////////////////////////////////////////////////////////////////////

        int addCollection (TRI_voc_cid_t cid,
                           TRI_transaction_type_e type) {
          if (_trx == 0) {
            return registerError(TRI_ERROR_INTERNAL);
          }

          if (cid == 0) {
            // invalid cid
            return registerError(TRI_ERROR_ARANGO_COLLECTION_NOT_FOUND);
          }

          const TRI_transaction_status_e status = getStatus();
              
          if (status == TRI_TRANSACTION_COMMITTED ||
              status == TRI_TRANSACTION_ABORTED) {
            // transaction already finished?
            return registerError(TRI_ERROR_TRANSACTION_INTERNAL);
          }

          int res;

          if (this->isEmbeddedTransaction()) {
            res = this->addCollectionEmbedded(cid, type);
          }
          else {
            res = this->addCollectionToplevel(cid, type);
          }
          
          return res;
        }

////////////////////////////////////////////////////////////////////////////////
/// @brief add a collection by name
////////////////////////////////////////////////////////////////////////////////

        int addCollection (const string& name,
                           TRI_transaction_type_e type) {
          if (name == TRI_TRANSACTION_COORDINATOR_COLLECTION) {
            return registerError(TRI_ERROR_TRANSACTION_DISALLOWED_OPERATION);
          }

          return addCollection(_resolver.getCollectionId(name), type);
        }

////////////////////////////////////////////////////////////////////////////////
/// @brief set the lock acquisition timeout
////////////////////////////////////////////////////////////////////////////////

        void setTimeout (double timeout) {
          _timeout = timeout; 
        }

////////////////////////////////////////////////////////////////////////////////
/// @brief set the waitForSync property
////////////////////////////////////////////////////////////////////////////////

        void setWaitForSync () {
          _waitForSync = true;
        }

////////////////////////////////////////////////////////////////////////////////
/// @brief add a transaction hint
////////////////////////////////////////////////////////////////////////////////

        void addHint (const TRI_transaction_hint_e hint) {
          _hints |= (TRI_transaction_hint_t) hint;
        }

////////////////////////////////////////////////////////////////////////////////
/// @brief read- or write-lock a collection
////////////////////////////////////////////////////////////////////////////////

        int lock (TRI_transaction_collection_t* trxCollection,
                  const TRI_transaction_type_e type) {

          if (_trx == 0 || getStatus() != TRI_TRANSACTION_RUNNING) {
            return TRI_ERROR_TRANSACTION_INTERNAL;
          }

          int res = TRI_LockCollectionTransaction(trxCollection, type, _nestingLevel);

          return res;
        }

////////////////////////////////////////////////////////////////////////////////
/// @brief read- or write-unlock a collection
////////////////////////////////////////////////////////////////////////////////

        int unlock (TRI_transaction_collection_t* trxCollection,
                    const TRI_transaction_type_e type) {

          if (_trx == 0 || getStatus() != TRI_TRANSACTION_RUNNING) {
            return TRI_ERROR_TRANSACTION_INTERNAL;
          }

          int res = TRI_UnlockCollectionTransaction(trxCollection, type, _nestingLevel);

          return res;
        }

////////////////////////////////////////////////////////////////////////////////
/// @brief read- or write-unlock a collection
////////////////////////////////////////////////////////////////////////////////

        bool isLocked (TRI_transaction_collection_t* const trxCollection,
                       const TRI_transaction_type_e type) {
          if (_trx == 0 || getStatus() != TRI_TRANSACTION_RUNNING) {
            return false;
          }

          const bool locked = TRI_IsLockedCollectionTransaction(trxCollection, type, _nestingLevel);

          return locked;
        }

////////////////////////////////////////////////////////////////////////////////
/// @brief read any (random) document
////////////////////////////////////////////////////////////////////////////////

        int readAny (TRI_transaction_collection_t* trxCollection,
                     TRI_doc_mptr_t* mptr,
                     TRI_barrier_t** barrier) {

          TRI_primary_collection_t* primary = primaryCollection(trxCollection);

          *barrier = TRI_CreateBarrierElement(&primary->_barrierList);

          if (*barrier == 0) {
            return TRI_ERROR_OUT_OF_MEMORY;
          }

          // READ-LOCK START
          int res = this->lock(trxCollection, TRI_TRANSACTION_READ);

          if (res != TRI_ERROR_NO_ERROR) {
            TRI_FreeBarrier(*barrier);
            *barrier = 0;
            return res;
          }

          if (primary->_primaryIndex._nrUsed == 0) {
            TRI_FreeBarrier(*barrier);
            *barrier = 0;

            // no document found
            mptr->_key = 0;
            mptr->_data = 0;
          }
          else {
            size_t total = primary->_primaryIndex._nrAlloc;
            size_t pos = TRI_UInt32Random() % total;
            void** beg = primary->_primaryIndex._table;

            while (beg[pos] == 0 || (((TRI_doc_mptr_t*) beg[pos])->_validTo != 0)) {
              pos = (pos + 1) % total;
            }

            *mptr = *((TRI_doc_mptr_t*) beg[pos]);
          }

          this->unlock(trxCollection, TRI_TRANSACTION_READ);
          // READ-LOCK END

          return TRI_ERROR_NO_ERROR;
        }

////////////////////////////////////////////////////////////////////////////////
/// @brief read a single document, identified by key
////////////////////////////////////////////////////////////////////////////////

        int readSingle (TRI_transaction_collection_t* trxCollection,
                        TRI_doc_mptr_t* mptr,
                        const string& key) {
          
          TRI_primary_collection_t* primary = primaryCollection(trxCollection);

          int res = primary->read(trxCollection,
                                  (TRI_voc_key_t) key.c_str(), 
                                  mptr,
                                  ! isLocked(trxCollection, TRI_TRANSACTION_READ));

          return res;
        }

////////////////////////////////////////////////////////////////////////////////
/// @brief read all documents
////////////////////////////////////////////////////////////////////////////////

        int readAll (TRI_transaction_collection_t* trxCollection,
                     vector<string>& ids, 
                     bool lock) {

          TRI_primary_collection_t* primary = primaryCollection(trxCollection);

          if (lock) {
            // READ-LOCK START
            int res = this->lock(trxCollection, TRI_TRANSACTION_READ);
          
            if (res != TRI_ERROR_NO_ERROR) {
              return res;
            }
          }
          
          if (primary->_primaryIndex._nrUsed > 0) {
            void** ptr = primary->_primaryIndex._table;
            void** end = ptr + primary->_primaryIndex._nrAlloc;

            for (;  ptr < end;  ++ptr) {
              if (*ptr) {
                TRI_doc_mptr_t const* d = (TRI_doc_mptr_t const*) *ptr;

                if (d->_validTo == 0) {
                  ids.push_back(d->_key);
                }
              }
            }
          }

          if (lock) {
            this->unlock(trxCollection, TRI_TRANSACTION_READ);
            // READ-LOCK END
          }

          return TRI_ERROR_NO_ERROR;
        }

////////////////////////////////////////////////////////////////////////////////
/// @brief read all master pointers, using skip and limit
////////////////////////////////////////////////////////////////////////////////

        int readSlice (TRI_transaction_collection_t* trxCollection,
                       vector<TRI_doc_mptr_t>& docs,
                       TRI_barrier_t** barrier,
                       TRI_voc_ssize_t skip,
                       TRI_voc_size_t limit,
                       uint32_t* total) {
          
          TRI_primary_collection_t* primary = primaryCollection(trxCollection);

          if (limit == 0) {
            // nothing to do
            return TRI_ERROR_NO_ERROR;
          }

          // READ-LOCK START
          int res = this->lock(trxCollection, TRI_TRANSACTION_READ);

          if (res != TRI_ERROR_NO_ERROR) {
            return res;
          }

          if (primary->_primaryIndex._nrUsed == 0) {
            // nothing to do

            this->unlock(trxCollection, TRI_TRANSACTION_READ);
            // READ-LOCK END
            return TRI_ERROR_NO_ERROR;
          }

          *barrier = TRI_CreateBarrierElement(&primary->_barrierList);

          if (*barrier == 0) {
            this->unlock(trxCollection, TRI_TRANSACTION_READ);
            return TRI_ERROR_OUT_OF_MEMORY;
          }

          void** beg = primary->_primaryIndex._table;
          void** ptr = beg;
          void** end = ptr + primary->_primaryIndex._nrAlloc;
          uint32_t count = 0;
          
          *total = (uint32_t) primary->_primaryIndex._nrUsed;

          // apply skip
          if (skip > 0) {
            // skip from the beginning
            for (;  ptr < end && 0 < skip;  ++ptr) {
              if (*ptr) {
                TRI_doc_mptr_t const* d = (TRI_doc_mptr_t const*) *ptr;

                if (d->_validTo == 0) {
                  --skip;
                }
              }
            }
          }
          else if (skip < 0) {
            // skip from the end
            ptr = end - 1;

            for (; beg <= ptr; --ptr) {
              if (*ptr) {
                TRI_doc_mptr_t const* d = (TRI_doc_mptr_t const*) *ptr;

                if (d->_validTo == 0) {
                  ++skip;

                  if (skip == 0) {
                    break;
                  }
                }
              }
            }

            if (ptr < beg) {
              ptr = beg;
            }
          }

          // fetch documents, taking limit into account
          for (; ptr < end && count < limit; ++ptr) {
            if (*ptr) {
              TRI_doc_mptr_t* d = (TRI_doc_mptr_t*) *ptr;

              if (d->_validTo == 0) {
                docs.push_back(*d);
                ++count;
              }
            }
          }

          this->unlock(trxCollection, TRI_TRANSACTION_READ);
          // READ-LOCK END

          if (count == 0) {
            // barrier not needed, kill it
            TRI_FreeBarrier(*barrier);
          }

          return TRI_ERROR_NO_ERROR;
        }

////////////////////////////////////////////////////////////////////////////////
/// @brief create a single document, using JSON
////////////////////////////////////////////////////////////////////////////////

        int create (TRI_transaction_collection_t* trxCollection,
                    const TRI_df_marker_type_e markerType,
                    TRI_doc_mptr_t* mptr,
                    TRI_json_t const* json,
                    void const* data,
                    const bool forceSync) {

          TRI_voc_key_t key = 0;
          int res = DocumentHelper::getKey(json, &key);

          if (res != TRI_ERROR_NO_ERROR) {
            return res;
          }

          TRI_shaped_json_t* shaped = TRI_ShapedJsonJson(shaper(trxCollection), json);

          if (shaped == 0) {
            return TRI_ERROR_ARANGO_SHAPER_FAILED;
          }

          res = create(trxCollection, 
                       markerType, 
                       key, 
                       mptr, 
                       shaped, 
                       data, 
                       forceSync); 

          TRI_FreeShapedJson(shaper(trxCollection), shaped);

          return res;
        }

////////////////////////////////////////////////////////////////////////////////
/// @brief create a single document, using shaped json
////////////////////////////////////////////////////////////////////////////////

        inline int create (TRI_transaction_collection_t* trxCollection,
                           const TRI_df_marker_type_e markerType,
                           const TRI_voc_key_t key,
                           TRI_doc_mptr_t* mptr,
                           TRI_shaped_json_t const* shaped,
                           void const* data,
                           const bool forceSync) {
         
          TRI_primary_collection_t* primary = primaryCollection(trxCollection);

          int res = primary->insert(trxCollection,
                                    key, 
                                    mptr, 
                                    markerType, 
                                    shaped, 
                                    data, 
                                    ! isLocked(trxCollection, TRI_TRANSACTION_WRITE), 
                                    forceSync);

          return res;
        }

////////////////////////////////////////////////////////////////////////////////
/// @brief update a single document, using JSON
////////////////////////////////////////////////////////////////////////////////

        int update (TRI_transaction_collection_t* trxCollection,
                    const string& key,
                    TRI_doc_mptr_t* mptr,
                    TRI_json_t* const json,
                    const TRI_doc_update_policy_e policy,
                    const TRI_voc_rid_t expectedRevision,
                    TRI_voc_rid_t* actualRevision,
                    const bool forceSync) {

          TRI_shaped_json_t* shaped = TRI_ShapedJsonJson(shaper(trxCollection), json);

          if (shaped == 0) {
            return TRI_ERROR_ARANGO_SHAPER_FAILED;
          }

          int res = update(trxCollection, 
                           key, 
                           mptr, 
                           shaped, 
                           policy, 
                           expectedRevision, 
                           actualRevision, 
                           forceSync); 

          TRI_FreeShapedJson(shaper(trxCollection), shaped);

          return res;
        }

////////////////////////////////////////////////////////////////////////////////
/// @brief update a single document, using shaped json
////////////////////////////////////////////////////////////////////////////////

        inline int update (TRI_transaction_collection_t* const trxCollection,
                           const string& key,
                           TRI_doc_mptr_t* mptr,
                           TRI_shaped_json_t* const shaped,
                           const TRI_doc_update_policy_e policy,
                           const TRI_voc_rid_t expectedRevision,
                           TRI_voc_rid_t* actualRevision,
                           const bool forceSync) {

          TRI_doc_update_policy_t updatePolicy;
          TRI_InitUpdatePolicy(&updatePolicy, policy, expectedRevision, actualRevision);
          
          TRI_primary_collection_t* primary = primaryCollection(trxCollection);
          
          int res = primary->update(trxCollection, 
                                    (const TRI_voc_key_t) key.c_str(), 
                                    mptr, 
                                    shaped, 
                                    &updatePolicy, 
                                    ! isLocked(trxCollection, TRI_TRANSACTION_WRITE), 
                                    forceSync);
          
          return res;
        }

////////////////////////////////////////////////////////////////////////////////
/// @brief delete a single document
////////////////////////////////////////////////////////////////////////////////

        int remove (TRI_transaction_collection_t* trxCollection,
                    const string& key,
                    const TRI_doc_update_policy_e policy,
                    const TRI_voc_rid_t expectedRevision,
                    TRI_voc_rid_t* actualRevision,
                    const bool forceSync) {
          
          TRI_doc_update_policy_t updatePolicy;
          TRI_InitUpdatePolicy(&updatePolicy, policy, expectedRevision, actualRevision);
          
          TRI_primary_collection_t* primary = primaryCollection(trxCollection);

          int res = primary->remove(trxCollection,
                                    (TRI_voc_key_t) key.c_str(), 
                                    &updatePolicy, 
                                    ! isLocked(trxCollection, TRI_TRANSACTION_WRITE), 
                                    forceSync);

          return res;
        }

////////////////////////////////////////////////////////////////////////////////
/// @brief truncate a collection
/// the caller must make sure a barrier is held
////////////////////////////////////////////////////////////////////////////////

        int removeAll (TRI_transaction_collection_t* const trxCollection,
                       const bool forceSync) {

          vector<string> ids;
          
          TRI_primary_collection_t* primary = primaryCollection(trxCollection);
          
          // WRITE-LOCK START
          int res = this->lock(trxCollection, TRI_TRANSACTION_WRITE);
          
          if (res != TRI_ERROR_NO_ERROR) {
            return res;
          }
          
          res = readAll(trxCollection, ids, false);

          if (res != TRI_ERROR_NO_ERROR) {
            this->unlock(trxCollection, TRI_TRANSACTION_WRITE);
            return res;
          }

          TRI_doc_update_policy_t updatePolicy;
          TRI_InitUpdatePolicy(&updatePolicy, TRI_DOC_UPDATE_LAST_WRITE, 0, NULL);
          const size_t n = ids.size();

          for (size_t i = 0; i < n; ++i) {
            const string& id = ids[i];
          
            res = primary->remove(trxCollection,
                                  (const TRI_voc_key_t) id.c_str(), 
                                  &updatePolicy, 
                                  false,
                                  forceSync);

            if (res != TRI_ERROR_NO_ERROR) {
              // halt on first error
              break;
            }
          }

          this->unlock(trxCollection, TRI_TRANSACTION_WRITE);
          // WRITE-LOCK END

          return res;
        }

////////////////////////////////////////////////////////////////////////////////
/// @}
////////////////////////////////////////////////////////////////////////////////

// -----------------------------------------------------------------------------
// --SECTION--                                                   private methods
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @addtogroup ArangoDB
/// @{
////////////////////////////////////////////////////////////////////////////////

      private:

////////////////////////////////////////////////////////////////////////////////
/// @brief register an error for the transaction
////////////////////////////////////////////////////////////////////////////////

        int registerError (int errorNum) {
          TRI_ASSERT_MAINTAINER(errorNum != TRI_ERROR_NO_ERROR);

          if (_setupState == TRI_ERROR_NO_ERROR) {
            _setupState = errorNum;
          }

          TRI_ASSERT_MAINTAINER(_setupState != TRI_ERROR_NO_ERROR);

          return errorNum;
        }

////////////////////////////////////////////////////////////////////////////////
/// @brief add a collection to an embedded transaction
////////////////////////////////////////////////////////////////////////////////

        int addCollectionEmbedded (TRI_voc_cid_t cid, TRI_transaction_type_e type) {
          TRI_ASSERT_MAINTAINER(_trx != 0);

          int res = TRI_AddCollectionTransaction(_trx, cid, type, _nestingLevel);

          if (res != TRI_ERROR_NO_ERROR) {
            return registerError(res);
          }

          return TRI_ERROR_NO_ERROR;
        }

////////////////////////////////////////////////////////////////////////////////
/// @brief add a collection to a top-level transaction
////////////////////////////////////////////////////////////////////////////////

        int addCollectionToplevel (TRI_voc_cid_t cid, TRI_transaction_type_e type) {
          TRI_ASSERT_MAINTAINER(_trx != 0);
          
          int res;

          if (getStatus() != TRI_TRANSACTION_CREATED) {
            // transaction already started?
            res = TRI_ERROR_TRANSACTION_INTERNAL;
          }
          else {
            res = TRI_AddCollectionTransaction(_trx, cid, type, _nestingLevel);
          }

          if (res != TRI_ERROR_NO_ERROR) {
            registerError(res);
          }

          return res;
        }

////////////////////////////////////////////////////////////////////////////////
/// @brief initialise the transaction
/// this will first check if the transaction is embedded in a parent 
/// transaction. if not, it will create a transaction of its own
////////////////////////////////////////////////////////////////////////////////

        int setupTransaction () {
          // check in the context if we are running embedded
          _trx = this->getParentTransaction();

          if (_trx != 0) {
            // yes, we are embedded
            _setupState = setupEmbedded();
          }
          else {
            // non-embedded
            _setupState = setupToplevel();
          }

          // this may well be TRI_ERROR_NO_ERROR...
          return _setupState;
        }

////////////////////////////////////////////////////////////////////////////////
/// @brief set up an embedded transaction
////////////////////////////////////////////////////////////////////////////////

        int setupEmbedded () {
          TRI_ASSERT_MAINTAINER(_nestingLevel == 0);

          _nestingLevel = ++_trx->_nestingLevel;
            
          if (! this->isEmbeddable()) {
            // we are embedded but this is disallowed...
            LOGGER_WARNING("logic error. invalid nesting of transactions");

            return TRI_ERROR_TRANSACTION_NESTED;
          }
            
          return TRI_ERROR_NO_ERROR;
        }

////////////////////////////////////////////////////////////////////////////////
/// @brief set up a top-level transaction
////////////////////////////////////////////////////////////////////////////////

        int setupToplevel () {
          TRI_ASSERT_MAINTAINER(_nestingLevel == 0);

          // we are not embedded. now start our own transaction 
          _trx = TRI_CreateTransaction(_vocbase->_transactionContext, _timeout, _waitForSync);

          if (_trx == 0) {
            return TRI_ERROR_OUT_OF_MEMORY;
          }

          // register the transaction in the context
          return this->registerTransaction(_trx);
        }

////////////////////////////////////////////////////////////////////////////////
/// @brief free transaction
////////////////////////////////////////////////////////////////////////////////

        int freeTransaction () {
          TRI_ASSERT_MAINTAINER(! isEmbeddedTransaction());

          if (_trx != 0) {
            this->unregisterTransaction();

            TRI_FreeTransaction(_trx);
            _trx = 0;
          }

          return TRI_ERROR_NO_ERROR;
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
/// @brief error that occurred on transaction initialisation (before begin())
////////////////////////////////////////////////////////////////////////////////

        int _setupState;

////////////////////////////////////////////////////////////////////////////////
/// @brief how deep the transaction is down in a nested transaction structure
////////////////////////////////////////////////////////////////////////////////

        int _nestingLevel;

////////////////////////////////////////////////////////////////////////////////
/// @brief transaction hints
////////////////////////////////////////////////////////////////////////////////

        TRI_transaction_hint_t _hints;

////////////////////////////////////////////////////////////////////////////////
/// @brief timeout for lock acquisition
////////////////////////////////////////////////////////////////////////////////

        double _timeout;

////////////////////////////////////////////////////////////////////////////////
/// @brief wait for sync property for transaction
////////////////////////////////////////////////////////////////////////////////

        bool _waitForSync;

////////////////////////////////////////////////////////////////////////////////
/// @}
////////////////////////////////////////////////////////////////////////////////

// -----------------------------------------------------------------------------
// --SECTION--                                               protected variables
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @addtogroup ArangoDB
/// @{
////////////////////////////////////////////////////////////////////////////////

      protected:

////////////////////////////////////////////////////////////////////////////////
/// @brief the C transaction struct
////////////////////////////////////////////////////////////////////////////////

        TRI_transaction_t* _trx;

////////////////////////////////////////////////////////////////////////////////
/// @brief the vocbase
////////////////////////////////////////////////////////////////////////////////

        TRI_vocbase_t* const _vocbase;

////////////////////////////////////////////////////////////////////////////////
/// @brief collection name resolver
////////////////////////////////////////////////////////////////////////////////

        const CollectionNameResolver _resolver;

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
