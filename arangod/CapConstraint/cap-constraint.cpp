////////////////////////////////////////////////////////////////////////////////
/// @brief cap constraint
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
/// @author Dr. Frank Celler
/// @author Copyright 2014, ArangoDB GmbH, Cologne, Germany
/// @author Copyright 2013, triAGENS GmbH, Cologne, Germany
////////////////////////////////////////////////////////////////////////////////

#include "cap-constraint.h"

#include "Basics/logging.h"
#include "Basics/tri-strings.h"
#include "Utils/Exception.h"
#include "Utils/transactions.h"
#include "VocBase/document-collection.h"
#include "VocBase/headers.h"
#include "VocBase/server.h"

// -----------------------------------------------------------------------------
// --SECTION--                                                    CAP CONSTRAINT
// -----------------------------------------------------------------------------

// -----------------------------------------------------------------------------
// --SECTION--                                                 private functions
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @brief apply the cap constraint for the collection
////////////////////////////////////////////////////////////////////////////////

static int ApplyCap (TRI_cap_constraint_t* cap,
                     TRI_document_collection_t* document,
                     TRI_transaction_collection_t* trxCollection) {

  TRI_headers_t* headers = document->_headersPtr;  // PROTECTED by trx in trxCollection
  size_t currentCount    = headers->count();
  int64_t currentSize    = headers->size();

  int res = TRI_ERROR_NO_ERROR;

  // delete while at least one of the constraints is still violated
  while ((cap->_count > 0 && currentCount > cap->_count) ||
         (cap->_size > 0 && currentSize > cap->_size)) {
    TRI_doc_mptr_t* oldest = headers->front();

    if (oldest != nullptr) {
      TRI_ASSERT(oldest->getDataPtr() != nullptr);  // ONLY IN INDEX, PROTECTED by RUNTIME
      size_t oldSize = ((TRI_df_marker_t*) (oldest->getDataPtr()))->_size;  // ONLY IN INDEX, PROTECTED by RUNTIME

      TRI_ASSERT(oldSize > 0);

      if (trxCollection != nullptr) {
        res = TRI_DeleteDocumentDocumentCollection(trxCollection, nullptr, oldest);

        if (res != TRI_ERROR_NO_ERROR) {
          LOG_WARNING("cannot cap collection: %s", TRI_errno_string(res));
          break;
        }
      }
      else {
        headers->unlink(oldest);
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
                          TRI_document_collection_t* document) {
  TRI_ASSERT(cap->_count > 0 || cap->_size > 0);

  TRI_headers_t* headers = document->_headersPtr;  // ONLY IN INDEX (CAP)
  size_t currentCount    = headers->count();
  int64_t currentSize    = headers->size();

  if ((cap->_count > 0 && currentCount <= cap->_count) &&
      (cap->_size > 0 && currentSize <= cap->_size)) {
    // nothing to do
    return TRI_ERROR_NO_ERROR;
  }
  else {
    TRI_vocbase_t* vocbase = document->_vocbase;
    TRI_voc_cid_t cid = document->_info._cid;

    triagens::arango::SingleCollectionWriteTransaction<UINT64_MAX> trx(new triagens::arango::StandaloneTransactionContext(), vocbase, cid);
    trx.addHint(TRI_TRANSACTION_HINT_LOCK_NEVER, false);
    trx.addHint(TRI_TRANSACTION_HINT_NO_BEGIN_MARKER, false);
    trx.addHint(TRI_TRANSACTION_HINT_NO_ABORT_MARKER, false);
    trx.addHint(TRI_TRANSACTION_HINT_SINGLE_OPERATION, false); // this is actually not true, but necessary to create trx id 0

    int res = trx.begin();

    if (res != TRI_ERROR_NO_ERROR) {
      return res;
    }

    TRI_transaction_collection_t* trxCollection = trx.trxCollection();
    res = ApplyCap(cap, document, trxCollection);

    res = trx.finish(res);

    return res;
  }
}

////////////////////////////////////////////////////////////////////////////////
/// @brief return the memory used by the index
////////////////////////////////////////////////////////////////////////////////

static size_t MemoryCapConstraint (TRI_index_t const* idx) {
  return 0;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief describes a cap constraint as a json object
////////////////////////////////////////////////////////////////////////////////

static TRI_json_t* JsonCapConstraint (TRI_index_t const* idx) {
  TRI_json_t* json;

  // recast as a cap constraint
  TRI_cap_constraint_t const* cap = (TRI_cap_constraint_t const*) idx;

  // create json object and fill it
  json = TRI_JsonIndex(TRI_CORE_MEM_ZONE, idx);

  if (json == nullptr) {
    return nullptr;
  }

  TRI_Insert3ArrayJson(TRI_CORE_MEM_ZONE, json, "size",  TRI_CreateNumberJson(TRI_CORE_MEM_ZONE, (double) cap->_count));
  TRI_Insert3ArrayJson(TRI_CORE_MEM_ZONE, json, "byteSize",  TRI_CreateNumberJson(TRI_CORE_MEM_ZONE, (double) cap->_size));

  return json;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief removes a cap constraint from collection
////////////////////////////////////////////////////////////////////////////////

static void RemoveIndexCapConstraint (TRI_index_t* idx,
                                      TRI_document_collection_t* document) {
  document->_capConstraint = nullptr;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief inserts a document
////////////////////////////////////////////////////////////////////////////////

static int InsertCapConstraint (TRI_index_t* idx,
                                TRI_doc_mptr_t const* doc,
                                bool isRollback) {
  TRI_cap_constraint_t* cap = (TRI_cap_constraint_t*) idx;

  if (cap->_size > 0) {
    // there is a size restriction
    TRI_df_marker_t* marker;

    marker = (TRI_df_marker_t*) doc->getDataPtr();  // ONLY IN INDEX, PROTECTED by RUNTIME

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
  TRI_cap_constraint_t* cap = (TRI_cap_constraint_t*) idx;

  TRI_ASSERT(cap->_count > 0 || cap->_size > 0);

  return ApplyCap(cap, trxCollection->_collection->_collection, trxCollection);
}

////////////////////////////////////////////////////////////////////////////////
/// @brief removes a document
////////////////////////////////////////////////////////////////////////////////

static int RemoveCapConstraint (TRI_index_t* idx,
                                TRI_doc_mptr_t const* doc,
                                bool isRollback) {
  return TRI_ERROR_NO_ERROR;
}

// -----------------------------------------------------------------------------
// --SECTION--                                      constructors and destructors
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @brief creates a cap constraint
////////////////////////////////////////////////////////////////////////////////

TRI_index_t* TRI_CreateCapConstraint (TRI_document_collection_t* document,
                                      TRI_idx_iid_t iid,
                                      size_t count,
                                      int64_t size) {
  TRI_cap_constraint_t* cap = static_cast<TRI_cap_constraint_t*>(TRI_Allocate(TRI_CORE_MEM_ZONE, sizeof(TRI_cap_constraint_t), false));

  if (cap == nullptr) {
    return nullptr;
  }

  TRI_index_t* idx = &cap->base;

  TRI_InitIndex(idx, iid, TRI_IDX_TYPE_CAP_CONSTRAINT, document, false, false);
  TRI_InitVectorString(&idx->_fields, TRI_CORE_MEM_ZONE);

  idx->memory      = MemoryCapConstraint;
  idx->json        = JsonCapConstraint;
  idx->removeIndex = RemoveIndexCapConstraint;
  idx->insert      = InsertCapConstraint;
  idx->postInsert  = PostInsertCapConstraint;
  idx->remove      = RemoveCapConstraint;

  cap->_count      = count;
  cap->_size       = size;

  InitialiseCap(cap, document);

  return idx;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief frees the memory allocated, but does not free the pointer
////////////////////////////////////////////////////////////////////////////////

void TRI_DestroyCapConstraint (TRI_index_t* idx) {
  TRI_DestroyVectorString(&idx->_fields);
}

////////////////////////////////////////////////////////////////////////////////
/// @brief frees the memory allocated and frees the pointer
////////////////////////////////////////////////////////////////////////////////

void TRI_FreeCapConstraint (TRI_index_t* idx) {
  TRI_DestroyCapConstraint(idx);
  TRI_Free(TRI_CORE_MEM_ZONE, idx);
}

// -----------------------------------------------------------------------------
// --SECTION--                                                       END-OF-FILE
// -----------------------------------------------------------------------------

// Local Variables:
// mode: outline-minor
// outline-regexp: "/// @brief\\|/// {@inheritDoc}\\|/// @page\\|// --SECTION--\\|/// @\\}"
// End:
