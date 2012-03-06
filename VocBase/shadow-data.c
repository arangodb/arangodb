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

#include "VocBase/shadow-data.h"

// -----------------------------------------------------------------------------
// --SECTION--                                                       SHADOW DATA
// -----------------------------------------------------------------------------

// -----------------------------------------------------------------------------
// --SECTION--                                                 private functions
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @brief init a shadow data structure
////////////////////////////////////////////////////////////////////////////////
  
static TRI_shadow_t* CreateShadow (const void* const element) {
  TRI_shadow_t* shadow = (TRI_shadow_t*) TRI_Allocate(sizeof(TRI_shadow_t)); 

  if (!shadow) {
    return NULL;
  }

  shadow->_rc        = 1;
  shadow->_data      = (void*) element;
  shadow->_id        = TRI_NewTickVocBase();
  shadow->_timestamp = TRI_microtime();

  return shadow;
}

////////////////////////////////////////////////////////////////////////////////
/// @addtogroup VocBase
/// @{
////////////////////////////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////////////////////////
/// @brief hashes an element
////////////////////////////////////////////////////////////////////////////////

static uint64_t HashShadowElement (TRI_associative_pointer_t* array, void const* e) {
  TRI_shadow_t const* element = e;

  return element->_id;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief tests if two elements are equal
////////////////////////////////////////////////////////////////////////////////

static bool EqualShadowElement (TRI_associative_pointer_t* array, void const* l, void const* r) {
  TRI_shadow_t const* left = l;
  TRI_shadow_t const* right = r;

  return left->_id == right->_id;
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
/// @brief creates a shadow data storage
////////////////////////////////////////////////////////////////////////////////

TRI_shadow_store_t* TRI_CreateShadowStore (void (*destroy) (TRI_shadow_store_t*, TRI_shadow_t*)) {
  TRI_shadow_store_t* store = 
    (TRI_shadow_store_t*) TRI_Allocate(sizeof(TRI_shadow_store_t));

  if (store) {
    TRI_InitAssociativePointer(&store->_index,
                               NULL,
                               HashShadowElement,
                               NULL,
                               EqualShadowElement);

    store->destroyShadow = destroy;
  }

  return store;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief destroys a shadow data storage, but does not free the pointer
////////////////////////////////////////////////////////////////////////////////

void TRI_DestroyShadowStore (TRI_shadow_store_t* const store) {
  assert(store);

  TRI_DestroyAssociativePointer(&store->_index);
}

////////////////////////////////////////////////////////////////////////////////
/// @brief destroys a shadow data storage
////////////////////////////////////////////////////////////////////////////////

void TRI_FreeShadowStore (TRI_shadow_store_t* const store) {
  assert(store);

  TRI_DestroyAssociativePointer(&store->_index);
  TRI_Free(store);
}

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
/// @brief looks up a shadow by id
////////////////////////////////////////////////////////////////////////////////

TRI_shadow_t* TRI_FindShadowData (TRI_shadow_store_t* store, const TRI_shadow_id id) {
  TRI_shadow_t* shadow;
  TRI_shadow_t search;
  union { TRI_shadow_t* s; TRI_shadow_t const* c; } cnv;
  
  assert(store);

  TRI_LockMutex(&store->_lock);

  search._id = id;
  cnv.c = (TRI_shadow_t*) TRI_LookupByElementAssociativePointer(&store->_index, 
                                                                &search);
  shadow = cnv.s;

  if (shadow) {
    ++shadow->_rc;
  }

  TRI_UnlockMutex(&store->_lock);

  // might be NULL
  return shadow;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief store a new shadow in the store
////////////////////////////////////////////////////////////////////////////////

TRI_shadow_t* TRI_StoreShadowData (TRI_shadow_store_t* store, const void* const element) {
  TRI_shadow_t* shadow;
  
  assert(store);

  shadow = CreateShadow(element);
  if (shadow) {
    TRI_LockMutex(&store->_lock);
    TRI_InsertElementAssociativePointer(&store->_index, shadow, true);
    TRI_UnlockMutex(&store->_lock);
  }

  // might be NULL
  return shadow; 
}

////////////////////////////////////////////////////////////////////////////////
/// @brief releases shadow data
////////////////////////////////////////////////////////////////////////////////

void TRI_ReleaseShadowData (TRI_shadow_store_t* store, TRI_shadow_t* shadow) {
  assert(shadow);

  TRI_LockMutex(&store->_lock);

  // release the element
  --shadow->_rc;

  // need to destroy the element
  if (shadow->_rc < 1) {
    TRI_RemoveElementAssociativePointer(&store->_index, shadow);
    store->destroyShadow(store, shadow);
    TRI_Free(shadow);
  }

  TRI_UnlockMutex(&store->_lock);
}

// -----------------------------------------------------------------------------
// --SECTION--                                                  SHADOW DOCUMENTS
// -----------------------------------------------------------------------------

// -----------------------------------------------------------------------------
// --SECTION--                                                 private functions
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @addtogroup VocBase
/// @{
////////////////////////////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////////////////////////
/// @brief hashes an element
////////////////////////////////////////////////////////////////////////////////

static uint64_t HashShadowDocumentElement (TRI_associative_pointer_t* array, 
                                           void const* e) {
  TRI_shadow_document_t const* element = e;

  return element->_did;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief tests if two elements are equal
////////////////////////////////////////////////////////////////////////////////

static bool EqualShadowDocumentElement (TRI_associative_pointer_t* array, 
                                        void const* l, 
                                        void const* r) {
  TRI_shadow_document_t const* left = l;
  TRI_shadow_document_t const* right = r;

  return ((left->_base->_id == right->_base->_id) || 
          (left->_cid == right->_cid && left->_did == right->_did));
}

////////////////////////////////////////////////////////////////////////////////
/// @brief creates a shadow document data structure
////////////////////////////////////////////////////////////////////////////////

static TRI_shadow_document_t* CreateShadowDocument(void* const element, 
                                                   TRI_voc_cid_t cid, 
                                                   TRI_voc_did_t did,
                                                   TRI_voc_rid_t rid) {
  TRI_shadow_document_t* shadow;
  TRI_shadow_t* base = CreateShadow(element);

  if (!base) {
    return NULL;
  }

  shadow = TRI_Allocate(sizeof(TRI_shadow_document_t));
  if (!shadow) {
    TRI_Free(base);
    return NULL;
  }

  shadow->_base = base;
  shadow->_cid  = cid;
  shadow->_did  = did;
  shadow->_rid  = rid;

  return shadow;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief looks up a shadow document or creates it
////////////////////////////////////////////////////////////////////////////////

static TRI_shadow_document_t* LookupShadowDocument(TRI_shadow_document_store_t* const store,
                                                   TRI_doc_collection_t* collection,
                                                   TRI_doc_mptr_t const* mptr,
                                                   TRI_voc_cid_t cid, 
                                                   TRI_voc_did_t did) {
  union { TRI_shadow_document_t* s; TRI_shadow_document_t const* c; } cnv;
  TRI_shadow_document_t* shadow;
  TRI_shadow_document_t search;
  void* element;

  // check if we already know a parsed version
  search._cid = cid;
  search._did = did;
  cnv.c = TRI_LookupByElementAssociativePointer(&store->_base->_index, &search);
  shadow = cnv.s;

  if (shadow) {
    bool ok = store->verifyShadow(store, collection, mptr, shadow->_base->_data);
    if (ok) {
      ++shadow->_base->_rc;
      return shadow;
    }
    else {
     TRI_ReleaseShadowDocument(store, shadow);
    }
  }

  // parse the document
  element = store->createShadow(store, collection, mptr);
  if (!element) {
    return NULL;
  }

  shadow = CreateShadowDocument(element, cid, did, mptr->_rid);
  if (shadow) {
    // enter the element into the store
    TRI_InsertElementAssociativePointer(&store->_base->_index, shadow, true);
  }

  // might be NULL
  return shadow;
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
/// @brief initialises a shadow document storage
////////////////////////////////////////////////////////////////////////////////

TRI_shadow_document_store_t* TRI_CreateShadowDocumentStore (
  void* (*create) (TRI_shadow_document_store_t*, TRI_doc_collection_t*, TRI_doc_mptr_t const*),
  bool (*verify) (TRI_shadow_document_store_t*, TRI_doc_collection_t*, TRI_doc_mptr_t const*, void*),
  void (*destroy) (TRI_shadow_document_store_t*, TRI_shadow_document_t*)) {

  TRI_shadow_document_store_t* store;
  TRI_shadow_store_t* base;
  
  base = (TRI_shadow_store_t*) TRI_Allocate(sizeof(TRI_shadow_store_t));
  if (!base) {
    return NULL;
  }

  TRI_InitAssociativePointer(&base->_index,
                             NULL,
                             HashShadowDocumentElement,
                             NULL,
                             EqualShadowDocumentElement);

  store = (TRI_shadow_document_store_t*) TRI_Allocate(sizeof(TRI_shadow_document_store_t));
  if (!store) {
    TRI_FreeShadowStore(base);
    return NULL;
  }

  store->createShadow  = create;
  store->verifyShadow  = verify;
  store->destroyShadow = destroy;

  return store;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief destroys a shadow document storage, but does not free the pointers
////////////////////////////////////////////////////////////////////////////////

void TRI_DestroyShadowDocumentStore (TRI_shadow_document_store_t* const store) {
  assert(store);

  TRI_DestroyAssociativePointer(&store->_base->_index);
}

////////////////////////////////////////////////////////////////////////////////
/// @brief destroys a shadow document storage
////////////////////////////////////////////////////////////////////////////////

void TRI_FreeShadowDocumentStore (TRI_shadow_document_store_t* const store) {
  assert(store);

  TRI_FreeShadowStore(store->_base);
  TRI_Free(store);
}

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

TRI_shadow_document_t* TRI_FindShadowDocument (TRI_shadow_document_store_t* const store, 
                                               TRI_vocbase_t* vocbase, 
                                               TRI_voc_cid_t cid, 
                                               TRI_voc_did_t did) {
  TRI_vocbase_col_t const* col;
  TRI_doc_collection_t* collection;
  TRI_doc_mptr_t const* mptr;
  TRI_shadow_document_t* shadow;

  assert(store);

  // extract the collection
  col = TRI_LookupCollectionByIdVocBase(vocbase, cid);
  if (!col) {
    return NULL;
  }

  collection = col->_collection;

  // lock the collection
  collection->beginRead(collection);

  // find the document
  mptr = collection->read(collection, did);

  shadow = NULL;
  if (mptr) {
    TRI_LockMutex(&store->_base->_lock);
    shadow = LookupShadowDocument(store, collection, mptr, cid, did);
    TRI_UnlockMutex(&store->_base->_lock);
  }

  // unlock the collection
  collection->endRead(collection);
 
  // might be null
  return shadow;
}


////////////////////////////////////////////////////////////////////////////////
/// @brief releases a shadow document
////////////////////////////////////////////////////////////////////////////////

void TRI_ReleaseShadowDocument (TRI_shadow_document_store_t* const store, 
                                TRI_shadow_document_t* shadow) {
  assert(store);

  TRI_LockMutex(&store->_base->_lock);

  // release the element
  --shadow->_base->_rc;

  // need to destroy the element
  if (shadow->_base->_rc < 1) {
    TRI_RemoveElementAssociativePointer(&store->_base->_index, shadow);
    store->destroyShadow(store, shadow);
    TRI_Free(shadow->_base);
    TRI_Free(shadow);
  }

  TRI_UnlockMutex(&store->_base->_lock);
}
  
////////////////////////////////////////////////////////////////////////////////
/// @}
////////////////////////////////////////////////////////////////////////////////

// Local Variables:
// mode: outline-minor
// outline-regexp: "^\\(/// @brief\\|/// {@inheritDoc}\\|/// @addtogroup\\|// --SECTION--\\|/// @\\}\\)"
// End:
