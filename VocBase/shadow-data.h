////////////////////////////////////////////////////////////////////////////////
/// @brief shadow data storage
///
/// @file
///
/// DISCLAIMER
///
/// Copyright 2011-2012 triagens GmbH, Cologne, Germany
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
/// @author Copyright 2012, triagens GmbH, Cologne, Germany
////////////////////////////////////////////////////////////////////////////////

#ifndef TRIAGENS_DURHAM_VOC_BASE_SHADOW_DATA_H
#define TRIAGENS_DURHAM_VOC_BASE_SHADOW_DATA_H 1

#include "BasicsC/common.h"

#include "BasicsC/locks.h"
#include "BasicsC/associative.h"
#include "VocBase/document-collection.h"

#ifdef __cplusplus
extern "C" {
#endif

// -----------------------------------------------------------------------------
// --SECTION--                                                      public types
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @addtogroup VocBase
/// @{
////////////////////////////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////////////////////////
/// @brief shadow data
////////////////////////////////////////////////////////////////////////////////

typedef struct TRI_shadow_s {
  int64_t _rc;

    TRI_voc_cid_t _cid;
    TRI_voc_did_t _did;
    TRI_voc_rid_t _rid;

  void* _data;
}
TRI_shadow_t;

////////////////////////////////////////////////////////////////////////////////
/// @brief shadow data storage
////////////////////////////////////////////////////////////////////////////////

typedef struct TRI_shadow_store_s {
  TRI_mutex_t _lock;

  TRI_associative_pointer_t _index;

  void* (*createShadow) (struct TRI_shadow_store_s*, struct TRI_doc_collection_s*, struct TRI_doc_mptr_s const*);
  bool (*verifyShadow) (struct TRI_shadow_store_s*, struct TRI_doc_collection_s*, struct TRI_doc_mptr_s const*, void*);
  void (*destroyShadow) (struct TRI_shadow_store_s*, TRI_shadow_t*);
}
TRI_shadow_store_t;

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
/// @brief initialisases a shadow data storage
////////////////////////////////////////////////////////////////////////////////

void TRI_InitShadowDataStore (TRI_shadow_store_t* store,
                              void* (*create) (TRI_shadow_store_t*, TRI_doc_collection_t*, TRI_doc_mptr_t const*),
                              bool (*verify) (TRI_shadow_store_t*, TRI_doc_collection_t*, TRI_doc_mptr_t const*, void*),
                              void (*destory) (TRI_shadow_store_t*, TRI_shadow_t*));

////////////////////////////////////////////////////////////////////////////////
/// @brief destroys a shadow data storage, but does not free the pointer
////////////////////////////////////////////////////////////////////////////////

void TRI_DestroyShadowDataStore (TRI_shadow_store_t* store);

////////////////////////////////////////////////////////////////////////////////
/// @}
////////////////////////////////////////////////////////////////////////////////

// -----------------------------------------------------------------------------
// --SECTION--                                                    public methods
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @addtogroup VocBase
/// @{
////////////////////////////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////////////////////////
/// @brief looks up or create shadow data
////////////////////////////////////////////////////////////////////////////////

TRI_shadow_t* TRI_FindShadowData (TRI_shadow_store_t* store, TRI_vocbase_t* vocbase, TRI_voc_cid_t cid, TRI_voc_did_t did);

////////////////////////////////////////////////////////////////////////////////
/// @brief releases shadow data
////////////////////////////////////////////////////////////////////////////////

void TRI_ReleaseShadowData (TRI_shadow_store_t* store, TRI_shadow_t* shadow);

////////////////////////////////////////////////////////////////////////////////
/// @}
////////////////////////////////////////////////////////////////////////////////

#ifdef __cplusplus
}
#endif

#endif

// Local Variables:
// mode: outline-minor
// outline-regexp: "^\\(/// @brief\\|/// {@inheritDoc}\\|/// @addtogroup\\|// --SECTION--\\|/// @\\}\\)"
// End:
