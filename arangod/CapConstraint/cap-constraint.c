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
#include "BasicsC/strings.h"
#include "VocBase/document-collection.h"

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
/// @brief describes a cap constraint as a json object
////////////////////////////////////////////////////////////////////////////////

static TRI_json_t* JsonCapConstraint (TRI_index_t* idx,
                                      TRI_primary_collection_t const* collection) {
  TRI_json_t* json;
  TRI_cap_constraint_t* cap;

  // recast as a cap constraint
  cap = (TRI_cap_constraint_t*) idx;

  // create json object and fill it
  json = TRI_CreateArrayJson(TRI_CORE_MEM_ZONE);

  TRI_Insert3ArrayJson(TRI_CORE_MEM_ZONE, json, "id",   TRI_CreateNumberJson(TRI_CORE_MEM_ZONE, idx->_iid));
  TRI_Insert3ArrayJson(TRI_CORE_MEM_ZONE, json, "type", TRI_CreateStringCopyJson(TRI_CORE_MEM_ZONE, "cap"));
  TRI_Insert3ArrayJson(TRI_CORE_MEM_ZONE, json, "size",  TRI_CreateNumberJson(TRI_CORE_MEM_ZONE, cap->_size));

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
                                TRI_doc_mptr_t const* doc) {
  TRI_cap_constraint_t* cap;

  cap = (TRI_cap_constraint_t*) idx;

  return TRI_AddLinkedArray(&cap->_array, doc);
}

////////////////////////////////////////////////////////////////////////////////
/// @brief post processing of insert
////////////////////////////////////////////////////////////////////////////////

static int PostInsertCapConstraint (TRI_index_t* idx,
                                    TRI_doc_mptr_t const* doc) {
  TRI_cap_constraint_t* cap;
  TRI_primary_collection_t* primary;

  cap = (TRI_cap_constraint_t*) idx;
  primary = idx->_collection;

  while (cap->_size < cap->_array._array._nrUsed) {
    TRI_doc_operation_context_t rollbackContext;
    TRI_doc_mptr_t* oldest;
    int res;

    oldest = CONST_CAST(TRI_PopFrontLinkedArray(&cap->_array));

    if (oldest == NULL) {
      LOG_WARNING("cap collection %llu is empty, but collection contains elements",
                  (unsigned long long) primary->base._info._cid);
      break;
    }

    LOG_DEBUG("removing document '%s' because of cap constraint", (char*) oldest->_key);

    TRI_InitContextPrimaryCollection(&rollbackContext, primary, TRI_DOC_UPDATE_LAST_WRITE, false);
    res = TRI_DeleteDocumentDocumentCollection(&rollbackContext, oldest);

    if (res != TRI_ERROR_NO_ERROR) {
      LOG_WARNING("cannot cap collection: %s", TRI_last_error());
      return res;
    }
  }

  return TRI_ERROR_NO_ERROR;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief removes a document
////////////////////////////////////////////////////////////////////////////////

static int RemoveCapConstraint (TRI_index_t* idx, TRI_doc_mptr_t const* doc) {
  TRI_cap_constraint_t* cap;

  cap = (TRI_cap_constraint_t*) idx;
  TRI_RemoveLinkedArray(&cap->_array, doc);

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

TRI_index_t* TRI_CreateCapConstraint (struct TRI_primary_collection_s* collection,
                                      size_t size) {
  TRI_cap_constraint_t* cap;

  cap = TRI_Allocate(TRI_CORE_MEM_ZONE, sizeof(TRI_cap_constraint_t), false);

  TRI_InitIndex(&cap->base, TRI_IDX_TYPE_CAP_CONSTRAINT, collection, false);
  cap->base.json = JsonCapConstraint;
  cap->base.removeIndex = RemoveIndexCapConstraint;

  cap->base.insert     = InsertCapConstraint;
  cap->base.postInsert = PostInsertCapConstraint;
  cap->base.remove     = RemoveCapConstraint;

  TRI_InitLinkedArray(&cap->_array, TRI_CORE_MEM_ZONE);

  cap->_size = size;

  return &cap->base;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief frees the memory allocated, but does not free the pointer
////////////////////////////////////////////////////////////////////////////////

void TRI_DestroyCapConstraint (TRI_index_t* idx) {
  TRI_cap_constraint_t* cap = (TRI_cap_constraint_t*) idx;

  TRI_DestroyLinkedArray(&cap->_array);
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
