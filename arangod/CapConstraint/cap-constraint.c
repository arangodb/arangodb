////////////////////////////////////////////////////////////////////////////////
/// @brief cap constraint
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
/// @author Dr. Frank Celler
/// @author Copyright 2013, triAGENS GmbH, Cologne, Germany
////////////////////////////////////////////////////////////////////////////////

#include "cap-constraint.h"

#include "BasicsC/logging.h"
#include "BasicsC/tri-strings.h"
#include "VocBase/document-collection.h"
#include "VocBase/headers.h"

// -----------------------------------------------------------------------------
// --SECTION--                                                    CAP CONSTRAINT
// -----------------------------------------------------------------------------

// -----------------------------------------------------------------------------
// --SECTION--                                                 private functions
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @addtogroup VocBase
/// @{
////////////////////////////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////////////////////////
/// @brief apply the cap constraint for the collection
////////////////////////////////////////////////////////////////////////////////

static int ApplyCap (TRI_cap_constraint_t* cap,
                     TRI_primary_collection_t* primary,
                     TRI_transaction_collection_t* trxCollection) {
  TRI_document_collection_t* document;
  TRI_headers_t* headers;
  int64_t currentSize;
  size_t currentCount;
  int res;

  document     = (TRI_document_collection_t*) primary;
  headers      = document->_headers;
  currentCount = headers->count(headers);
  currentSize  = headers->size(headers);

  res = TRI_ERROR_NO_ERROR;

  // delete while at least one of the constraints is violated
  while ((cap->_count > 0 && currentCount > cap->_count) || 
         (cap->_size > 0 && currentSize > cap->_size)) {
    TRI_doc_mptr_t* oldest = headers->front(headers);

    if (oldest != NULL) {
      size_t oldSize;
     
      assert(oldest->_data != NULL);
      oldSize = ((TRI_df_marker_t*) (oldest->_data))->_size;

      assert(oldSize > 0);

      if (trxCollection != NULL) {
        res = TRI_DeleteDocumentDocumentCollection(trxCollection, NULL, oldest);
        
        if (res != TRI_ERROR_NO_ERROR) {
          LOG_WARNING("cannot cap collection: %s", TRI_errno_string(res));
          break;
        }
      }
      else {
        headers->unlink(headers, oldest);
      }
      
      currentCount--;
      currentSize -= (int64_t) oldSize;
    }
    else {
      // we should not get here
      LOG_WARNING("logic error in %s", __FUNCTION__);
      break;
    }
  }

  return res;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief initialise the cap constraint
////////////////////////////////////////////////////////////////////////////////

static int InitialiseCap (TRI_cap_constraint_t* cap, 
                          TRI_primary_collection_t* primary) { 
  TRI_document_collection_t* document;
  TRI_headers_t* headers;
  size_t currentCount;
  int64_t currentSize;
  
  TRI_ASSERT_MAINTAINER(cap->_count > 0 || cap->_size > 0);
  
  document = (TRI_document_collection_t*) primary;
  headers = document->_headers;
  currentCount = headers->count(headers);
  currentSize = headers->size(headers);
  
  if ((cap->_count >0 && currentCount <= cap->_count) &&
      (cap->_size > 0 && currentSize <= cap->_size)) {
    // nothing to do
    return TRI_ERROR_NO_ERROR;
  }
  else {
    TRI_vocbase_t* vocbase;
    TRI_transaction_t* trx;
    TRI_voc_cid_t cid;
    int res;

    vocbase = primary->base._vocbase;
    cid = primary->base._info._cid;

    trx = TRI_CreateTransaction(vocbase->_transactionContext, true, 0.0, false);

    if (trx == NULL) {
      return TRI_ERROR_OUT_OF_MEMORY;
    }

    res = TRI_AddCollectionTransaction(trx, cid, TRI_TRANSACTION_WRITE, TRI_TRANSACTION_TOP_LEVEL);

    if (res == TRI_ERROR_NO_ERROR) {
      TRI_transaction_collection_t* trxCollection;

      trxCollection = TRI_GetCollectionTransaction(trx, cid, TRI_TRANSACTION_WRITE);

      if (trxCollection != NULL) {
        res = TRI_BeginTransaction(trx, (TRI_transaction_hint_t) TRI_TRANSACTION_HINT_LOCK_NEVER, TRI_TRANSACTION_TOP_LEVEL);

        if (res == TRI_ERROR_NO_ERROR) {
          res = ApplyCap(cap, primary, trxCollection);

          if (res == TRI_ERROR_NO_ERROR) {
            res = TRI_CommitTransaction(trx, TRI_TRANSACTION_TOP_LEVEL);
          }
          else {
            TRI_AbortTransaction(trx, TRI_TRANSACTION_TOP_LEVEL);
          }
        }
      }
      else {
        res = TRI_ERROR_INTERNAL;
      }
    }

    TRI_FreeTransaction(trx);

    return res;
  }
}

////////////////////////////////////////////////////////////////////////////////
/// @brief return the index type name
////////////////////////////////////////////////////////////////////////////////

static const char* TypeNameCapConstraint (TRI_index_t const* idx) {
  return "cap";
}

////////////////////////////////////////////////////////////////////////////////
/// @brief describes a cap constraint as a json object
////////////////////////////////////////////////////////////////////////////////

static TRI_json_t* JsonCapConstraint (TRI_index_t* idx,
                                      TRI_primary_collection_t const* primary) {
  TRI_json_t* json;
  TRI_cap_constraint_t* cap;

  // recast as a cap constraint
  cap = (TRI_cap_constraint_t*) idx;

  // create json object and fill it
  json = TRI_JsonIndex(TRI_CORE_MEM_ZONE, idx);

  TRI_Insert3ArrayJson(TRI_CORE_MEM_ZONE, json, "size",  TRI_CreateNumberJson(TRI_CORE_MEM_ZONE, cap->_count));
  TRI_Insert3ArrayJson(TRI_CORE_MEM_ZONE, json, "byteSize",  TRI_CreateNumberJson(TRI_CORE_MEM_ZONE, cap->_size));

  return json;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief removes a cap constraint from collection
////////////////////////////////////////////////////////////////////////////////

static void RemoveIndexCapConstraint (TRI_index_t* idx,
                                      TRI_primary_collection_t* collection) {
  collection->_capConstraint = NULL;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief inserts a document
////////////////////////////////////////////////////////////////////////////////

static int InsertCapConstraint (TRI_index_t* idx,
                                TRI_doc_mptr_t const* doc,
                                const bool isRollback) {
  TRI_cap_constraint_t* cap;

  cap = (TRI_cap_constraint_t*) idx;

  if (cap->_size > 0) {
    // there is a size restriction
    TRI_df_marker_t* marker;
    
    marker = (TRI_df_marker_t*) doc->_data;

    // check if the document would be too big
    if ((int64_t) marker->_size > (int64_t) cap->_size) {
      return TRI_ERROR_ARANGO_DOCUMENT_TOO_LARGE;
    }
  }

  return TRI_ERROR_NO_ERROR;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief post processing of insert
////////////////////////////////////////////////////////////////////////////////

static int PostInsertCapConstraint (TRI_transaction_collection_t* trxCollection,
                                    TRI_index_t* idx,
                                    TRI_doc_mptr_t const* doc) {
  TRI_cap_constraint_t* cap;

  cap = (TRI_cap_constraint_t*) idx;

  TRI_ASSERT_MAINTAINER(cap->_count > 0 || cap->_size > 0);

  return ApplyCap(cap, trxCollection->_collection->_collection, trxCollection);
}

////////////////////////////////////////////////////////////////////////////////
/// @brief removes a document
////////////////////////////////////////////////////////////////////////////////

static int RemoveCapConstraint (TRI_index_t* idx, 
                                TRI_doc_mptr_t const* doc,
                                const bool isRollback) {
  return TRI_ERROR_NO_ERROR;
}

////////////////////////////////////////////////////////////////////////////////
/// @}
////////////////////////////////////////////////////////////////////////////////

// -----------------------------------------------------------------------------
// --SECTION--                                      constructors and destructors
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @addtogroup VocBase
/// @{
////////////////////////////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////////////////////////
/// @brief creates a cap constraint
////////////////////////////////////////////////////////////////////////////////

TRI_index_t* TRI_CreateCapConstraint (struct TRI_primary_collection_s* primary,
                                      size_t count,
                                      int64_t size) {
  TRI_cap_constraint_t* cap;
  TRI_index_t* idx;

  cap = TRI_Allocate(TRI_CORE_MEM_ZONE, sizeof(TRI_cap_constraint_t), false);
  idx = &cap->base;

  idx->typeName = TypeNameCapConstraint;
  TRI_InitIndex(idx, TRI_IDX_TYPE_CAP_CONSTRAINT, primary, false, true);

  idx->json        = JsonCapConstraint;
  idx->removeIndex = RemoveIndexCapConstraint;
  idx->insert      = InsertCapConstraint;
  idx->postInsert  = PostInsertCapConstraint;
  idx->remove      = RemoveCapConstraint;

  cap->_count      = count;
  cap->_size       = size;

  InitialiseCap(cap, primary);

  return idx;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief frees the memory allocated, but does not free the pointer
////////////////////////////////////////////////////////////////////////////////

void TRI_DestroyCapConstraint (TRI_index_t* idx) {
}

////////////////////////////////////////////////////////////////////////////////
/// @brief frees the memory allocated and frees the pointer
////////////////////////////////////////////////////////////////////////////////

void TRI_FreeCapConstraint (TRI_index_t* idx) {
  TRI_DestroyCapConstraint(idx);
  TRI_Free(TRI_CORE_MEM_ZONE, idx);
}

////////////////////////////////////////////////////////////////////////////////
/// @}
////////////////////////////////////////////////////////////////////////////////

// -----------------------------------------------------------------------------
// --SECTION--                                                       END-OF-FILE
// -----------------------------------------------------------------------------

// Local Variables:
// mode: outline-minor
// outline-regexp: "/// @brief\\|/// {@inheritDoc}\\|/// @addtogroup\\|/// @page\\|// --SECTION--\\|/// @\\}"
// End:
