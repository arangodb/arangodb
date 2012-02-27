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

#include "shadow-data.h"

// -----------------------------------------------------------------------------
// --SECTION--                                                       SHADOW DATA
// -----------------------------------------------------------------------------

// -----------------------------------------------------------------------------
// --SECTION--                                                 private functions
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @addtogroup VocBase
/// @{
////////////////////////////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////////////////////////
/// @brief hashs an element
////////////////////////////////////////////////////////////////////////////////

static uint64_t HashElement (TRI_associative_pointer_t* array, void const* e) {
  TRI_shadow_t const* element = e;

  return element->_did;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief tests if two elements are equal
////////////////////////////////////////////////////////////////////////////////

static bool EqualElement (TRI_associative_pointer_t* array, void const* l, void const* r) {
  TRI_shadow_t const* left = l;
  TRI_shadow_t const* right = r;

  return left->_cid == right->_cid && left->_did == right->_did;
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
/// @brief initialisases a shadow data storage
////////////////////////////////////////////////////////////////////////////////

void TRI_InitShadowDataStore (TRI_shadow_store_t* store,
                              void* (*create) (TRI_shadow_store_t*, TRI_doc_collection_t*, TRI_doc_mptr_t const*),
                              bool (*verify) (TRI_shadow_store_t*, TRI_doc_collection_t*, TRI_doc_mptr_t const*, void*),
                              void (*destroy) (TRI_shadow_store_t*, TRI_shadow_t*)) {

  TRI_InitAssociativePointer(&store->_index,
                             NULL,
                             HashElement,
                             NULL,
                             EqualElement);

  store->createShadow = create;
  store->verifyShadow = verify;
  store->destroyShadow = destroy;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief destroys a shadow data storage, but does not free the pointer
////////////////////////////////////////////////////////////////////////////////

void TRI_DestroyShadowDataStore (TRI_shadow_store_t* store) {
  TRI_DestroyAssociativePointer(&store->_index);
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
/// @brief looks up or create shadow data
////////////////////////////////////////////////////////////////////////////////

TRI_shadow_t* TRI_FindShadowData (TRI_shadow_store_t* store, TRI_vocbase_t* vocbase, TRI_voc_cid_t cid, TRI_voc_did_t did) {
  union { TRI_shadow_t* s; TRI_shadow_t const* c; } cnv;
  TRI_vocbase_col_t const* col;
  TRI_doc_collection_t* collection;
  TRI_doc_mptr_t const* mptr;
  TRI_shadow_t search;
  TRI_shadow_t* shadow;
  void* element;
  bool ok;

  // extract the collection
  col = TRI_LookupCollectionByIdVocBase(vocbase, cid);

  if (col == NULL) {
    return NULL;
  }

  collection = col->_collection;

  // lock the collection
  collection->beginRead(collection);

  // find the document
  mptr = collection->read(collection, did);

  if (mptr == NULL) {
    collection->endRead(collection);
    return NULL;
  }

  // check if we already know a parsed version
  TRI_LockMutex(&store->_lock);

  search._cid = cid;
  search._did = did;
  cnv.c = TRI_LookupByElementAssociativePointer(&store->_index, &search);
  shadow = cnv.s;

  if (shadow != NULL) {
    ok = store->verifyShadow(store, collection, mptr, shadow->_data);

    if (ok) {
      ++shadow->_rc;

      TRI_UnlockMutex(&store->_lock);
      collection->endRead(collection);

      return shadow;
    }
    else {
     TRI_ReleaseShadowData(store, shadow);
    }
  }

  // parse the document
  element = store->createShadow(store, collection, mptr);
  
  if (element == NULL) {
    TRI_UnlockMutex(&store->_lock);
    collection->endRead(collection);
    return NULL;
  }

  // enter the element into the store
  shadow = TRI_Allocate(sizeof(TRI_shadow_t));

  if (shadow == NULL) {
    TRI_UnlockMutex(&store->_lock);
    collection->endRead(collection);
    
    return NULL;
  }

  shadow->_rc = 1;
  shadow->_cid = cid;
  shadow->_did = did;
  shadow->_rid = mptr->_rid;
  shadow->_data = element;

  TRI_InsertElementAssociativePointer(&store->_index, shadow, true);

  // use element, unlock the store and the collection
  ++shadow->_rc;

  TRI_UnlockMutex(&store->_lock);
  collection->endRead(collection);

  // and return the element
  return shadow;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief releases shadow data
////////////////////////////////////////////////////////////////////////////////

void TRI_ReleaseShadowData (TRI_shadow_store_t* store, TRI_shadow_t* shadow) {
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
  
////////////////////////////////////////////////////////////////////////////////
/// @}
////////////////////////////////////////////////////////////////////////////////

// Local Variables:
// mode: outline-minor
// outline-regexp: "^\\(/// @brief\\|/// {@inheritDoc}\\|/// @addtogroup\\|// --SECTION--\\|/// @\\}\\)"
// End:
