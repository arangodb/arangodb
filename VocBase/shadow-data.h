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

#include <BasicsC/common.h>
#include <BasicsC/locks.h>
#include <BasicsC/associative.h>

#include "VocBase/vocbase.h"
#include "VocBase/document-collection.h"

#ifdef __cplusplus
extern "C" {
#endif

// -----------------------------------------------------------------------------
// --SECTION--                                                           DEFINES
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @addtogroup VocBase
/// @{
////////////////////////////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////////////////////////
/// @brief delete at most this number of shadows during a gc cycle
////////////////////////////////////////////////////////////////////////////////

#define SHADOW_MAX_DELETE 256

////////////////////////////////////////////////////////////////////////////////
/// @}
////////////////////////////////////////////////////////////////////////////////

// -----------------------------------------------------------------------------
// --SECTION--                                                       SHADOW DATA
// -----------------------------------------------------------------------------

// -----------------------------------------------------------------------------
// --SECTION--                                                      public types
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @addtogroup VocBase
/// @{
////////////////////////////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////////////////////////
/// @brief typedef for shadow types
///
/// Shadows are first created with the SHADOW_TRANSIENT type. This means that
/// the shadow will exist only temporarily and will be destroyed when the 
/// refcount gets back to 0. Shadows of type SHADOW_PERSISTENT will remain in
/// the shadow store even with a refcount of 0 until their ttl is over.
////////////////////////////////////////////////////////////////////////////////

typedef enum {
  SHADOW_TRANSIENT  = 1,
  SHADOW_PERSISTENT = 2
}
TRI_shadow_type_e;

////////////////////////////////////////////////////////////////////////////////
/// @brief typedef for shadow ids
////////////////////////////////////////////////////////////////////////////////

typedef TRI_voc_tick_t TRI_shadow_id; 

////////////////////////////////////////////////////////////////////////////////
/// @brief shadow data (base struct)
////////////////////////////////////////////////////////////////////////////////

typedef struct TRI_shadow_s {
  TRI_shadow_id     _id;
  int64_t           _rc;        // refcount
  double            _timestamp; // creation timestamp
  void*             _data;      // pointer to data
  bool              _deleted;   // deleted flag
  TRI_shadow_type_e _type;      // transient or persistent
}
TRI_shadow_t;

////////////////////////////////////////////////////////////////////////////////
/// @brief shadow data storage
////////////////////////////////////////////////////////////////////////////////

typedef struct TRI_shadow_store_s {
  TRI_mutex_t               _lock;
  TRI_associative_pointer_t _ids; // ids
  TRI_associative_pointer_t _pointers; // data pointers

  void (*destroyShadow) (void*);
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
/// @brief creates a shadow data storage
////////////////////////////////////////////////////////////////////////////////

TRI_shadow_store_t* TRI_CreateShadowStore (void (*destroy) (void*));

////////////////////////////////////////////////////////////////////////////////
/// @brief destroys a shadow data storage
///
/// Note: all remaining shadows will be destroyed
////////////////////////////////////////////////////////////////////////////////

void TRI_FreeShadowStore (TRI_shadow_store_t* const store);

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
/// @brief look up a shadow in the index using its data pointer and return 
/// its id
////////////////////////////////////////////////////////////////////////////////

TRI_shadow_id TRI_GetIdDataShadowData (TRI_shadow_store_t* const, 
                                       const void* const);

////////////////////////////////////////////////////////////////////////////////
/// @brief look up a shadow in the index using its data pointer
///
/// If the shadow is found, this will return the data pointer, NULL otherwise.
/// When the shadow is found, its refcount will also be increased by one
////////////////////////////////////////////////////////////////////////////////

void* TRI_BeginUsageDataShadowData (TRI_shadow_store_t* const, const void* const);

////////////////////////////////////////////////////////////////////////////////
/// @brief look up a shadow in the index using its id
///
/// If the shadow is found, this will return the data pointer, NULL otherwise.
/// When the shadow is found, its refcount will also be increased by one
////////////////////////////////////////////////////////////////////////////////

void* TRI_BeginUsageIdShadowData (TRI_shadow_store_t* const, const TRI_shadow_id);

////////////////////////////////////////////////////////////////////////////////
/// @brief look up a shadow in the index using its data pointer
///
/// If the shadow is found, its refcount will be decreased by one.
/// If the refcount is 0 and the shadow is of type SHADOW_TRANSIENT, the shadow
/// object will be destroyed.
////////////////////////////////////////////////////////////////////////////////

void TRI_EndUsageDataShadowData (TRI_shadow_store_t* const, const void* const);

////////////////////////////////////////////////////////////////////////////////
/// @brief look up a shadow in the index using its id
///
/// If the shadow is found, its refcount will be decreased by one.
/// If the refcount is 0 and the shadow is of type SHADOW_TRANSIENT, the shadow
/// object will be destroyed.
////////////////////////////////////////////////////////////////////////////////

void TRI_EndUsageIdShadowData (TRI_shadow_store_t* const, const TRI_shadow_id);

////////////////////////////////////////////////////////////////////////////////
/// @brief set the persistence flag for a shadow using its data pointer
////////////////////////////////////////////////////////////////////////////////

bool TRI_PersistDataShadowData (TRI_shadow_store_t* const, const void* const);

////////////////////////////////////////////////////////////////////////////////
/// @brief set the persistence flag for a shadow using its id
////////////////////////////////////////////////////////////////////////////////

bool TRI_PersistIdShadowData (TRI_shadow_store_t* const, const TRI_shadow_id);

////////////////////////////////////////////////////////////////////////////////
/// @brief set the deleted flag for a shadow using its data pointer
////////////////////////////////////////////////////////////////////////////////

bool TRI_DeleteDataShadowData (TRI_shadow_store_t* const, const void* const);

////////////////////////////////////////////////////////////////////////////////
/// @brief set the deleted flag for a shadow using its id
////////////////////////////////////////////////////////////////////////////////

bool TRI_DeleteIdShadowData (TRI_shadow_store_t* const, const TRI_shadow_id);

////////////////////////////////////////////////////////////////////////////////
/// @brief enumerate all shadows and remove them if 
/// - their refcount is 0 and they are transient
/// - their refcount is 0 and they are expired
/// - the force flag is set
/// 
/// The max age must be specified in seconds. The max age is ignored if the
/// force flag is set. In this case all remaining shadows will be deleted
////////////////////////////////////////////////////////////////////////////////

void TRI_CleanupShadowData (TRI_shadow_store_t* const, const double, const bool);

////////////////////////////////////////////////////////////////////////////////
/// @brief store a new shadow in the store
////////////////////////////////////////////////////////////////////////////////

TRI_shadow_t* TRI_StoreShadowData (TRI_shadow_store_t* const, 
                                   const void* const);

////////////////////////////////////////////////////////////////////////////////
/// @}
////////////////////////////////////////////////////////////////////////////////

/*
// -----------------------------------------------------------------------------
// --SECTION--                                  UNUSED AND UNTESTED CODE FOLLOWS
// -----------------------------------------------------------------------------

// -----------------------------------------------------------------------------
// --SECTION--                                                  SHADOW DOCUMENTS
// -----------------------------------------------------------------------------

// -----------------------------------------------------------------------------
// --SECTION--                                      constructors and destructors
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @addtogroup VocBase
/// @{
////////////////////////////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////////////////////////
/// @brief shadow document
////////////////////////////////////////////////////////////////////////////////
  
typedef struct TRI_shadow_document_s {
  TRI_shadow_t* _base;

  TRI_voc_cid_t _cid;
  TRI_voc_did_t _did;
  TRI_voc_rid_t _rid;
}
TRI_shadow_document_t;

////////////////////////////////////////////////////////////////////////////////
/// @brief shadow document storage
////////////////////////////////////////////////////////////////////////////////

typedef struct TRI_shadow_document_store_s {
  TRI_shadow_store_t* _base;

  void* (*createShadow) (struct TRI_shadow_document_store_s*, struct TRI_doc_collection_s*, struct TRI_doc_mptr_s const*);
  bool (*verifyShadow) (struct TRI_shadow_document_store_s*, struct TRI_doc_collection_s*, struct TRI_doc_mptr_s const*, void*);
  void (*destroyShadow) (struct TRI_shadow_document_store_s*, TRI_shadow_document_t*);
}
TRI_shadow_document_store_t;

////////////////////////////////////////////////////////////////////////////////
/// @brief initialises a shadow document storage
////////////////////////////////////////////////////////////////////////////////

TRI_shadow_document_store_t* TRI_CreateShadowDocumentStore (
  void* (*create) (TRI_shadow_document_store_t*, TRI_doc_collection_t*, TRI_doc_mptr_t const*),
  bool (*verify) (TRI_shadow_document_store_t*, TRI_doc_collection_t*, TRI_doc_mptr_t const*, void*),
  void (*destroy) (TRI_shadow_document_store_t*, TRI_shadow_document_t*));

////////////////////////////////////////////////////////////////////////////////
/// @brief destroys a shadow document storage
////////////////////////////////////////////////////////////////////////////////

void TRI_FreeShadowDocumentStore (TRI_shadow_document_store_t* const);

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
/// @brief looks up or creates a shadow document
////////////////////////////////////////////////////////////////////////////////

TRI_shadow_document_t* TRI_FindShadowDocument (TRI_shadow_document_store_t* const, 
                                               TRI_vocbase_t*, 
                                               TRI_voc_cid_t,
                                               TRI_voc_did_t);

////////////////////////////////////////////////////////////////////////////////
/// @brief releases a shadow document
////////////////////////////////////////////////////////////////////////////////

bool TRI_ReleaseShadowDocument (TRI_shadow_document_store_t* const, 
                                TRI_shadow_document_t*);

////////////////////////////////////////////////////////////////////////////////
/// @brief enumerate all shadows and remove them if expired
/// 
/// The max age must be specified in seconds
////////////////////////////////////////////////////////////////////////////////

void TRI_CleanupShadowDocuments (TRI_shadow_document_store_t* const, const double);

////////////////////////////////////////////////////////////////////////////////
/// @}
////////////////////////////////////////////////////////////////////////////////
*/
#ifdef __cplusplus
}
#endif

#endif

// Local Variables:
// mode: outline-minor
// outline-regexp: "^\\(/// @brief\\|/// {@inheritDoc}\\|/// @addtogroup\\|// --SECTION--\\|/// @\\}\\)"
// End:
