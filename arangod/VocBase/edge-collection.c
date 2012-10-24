////////////////////////////////////////////////////////////////////////////////
/// @brief edge collection
///
/// @file
///
/// DISCLAIMER
///
/// Copyright 2010-2011 triagens GmbH, Cologne, Germany
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
/// @author Copyright 2011, triagens GmbH, Cologne, Germany
////////////////////////////////////////////////////////////////////////////////

#include "edge-collection.h"

// -----------------------------------------------------------------------------
// --SECTION--                                                       EDGES INDEX
// -----------------------------------------------------------------------------

// -----------------------------------------------------------------------------
// --SECTION--                                                 private functions
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @addtogroup VocBase
/// @{
////////////////////////////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////////////////////////
/// @brief hashes an edge header
////////////////////////////////////////////////////////////////////////////////

static uint64_t HashElementEdge (TRI_multi_pointer_t* array, void const* data) {
  TRI_edge_header_t const* h;
  uint64_t hash[3];

  h = data;

  hash[0] = h->_direction;
  hash[1] = h->_cid;
  hash[2] = TRI_FnvHashString((char const*) h->_key);  // h->_did;

  return TRI_FnvHashPointer(hash, sizeof(hash));  
}

////////////////////////////////////////////////////////////////////////////////
/// @brief checks if key and element match
////////////////////////////////////////////////////////////////////////////////

static bool IsEqualKeyEdge (TRI_multi_pointer_t* array, void const* left, void const* right) {
  TRI_edge_header_t const* l;
  TRI_edge_header_t const* r;

  l = left;
  r = right;

  //return l->_direction == r->_direction && l->_cid == r->_cid && l->_did == r->_did;  
  return l->_direction == r->_direction && l->_cid == r->_cid && (strcmp(l->_key, r->_key) == 0);  
}

////////////////////////////////////////////////////////////////////////////////
/// @brief checks for elements are equal
////////////////////////////////////////////////////////////////////////////////

static bool IsEqualElementEdge (TRI_multi_pointer_t* array, void const* left, void const* right) {
  TRI_edge_header_t const* l;
  TRI_edge_header_t const* r;

  l = left;
  r = right;

  //return l->_mptr == r->_mptr && l->_direction == r->_direction && l->_cid == r->_cid && l->_did == r->_did;
  return l->_mptr == r->_mptr && l->_direction == r->_direction && l->_cid == r->_cid && (strcmp(l->_key, r->_key) == 0);
}

////////////////////////////////////////////////////////////////////////////////
/// @}
////////////////////////////////////////////////////////////////////////////////

// -----------------------------------------------------------------------------
// --SECTION--                                                  public functions
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @addtogroup VocBase
/// @{
////////////////////////////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////////////////////////
/// @brief initialise the edges index of a collection
////////////////////////////////////////////////////////////////////////////////

void TRI_InitEdgesDocumentCollection (TRI_document_collection_t* collection) {
  TRI_InitMultiPointer(&collection->_edgesIndex,
                       TRI_UNKNOWN_MEM_ZONE, 
                       HashElementEdge,
                       HashElementEdge,
                       IsEqualKeyEdge,
                       IsEqualElementEdge);
}

////////////////////////////////////////////////////////////////////////////////
/// @brief free all edges in the collection's edges index
////////////////////////////////////////////////////////////////////////////////

void TRI_FreeEdgesDocumentCollection (TRI_document_collection_t* collection) {
  size_t i, n;

  // free all elements in the edges index
  n = collection->_edgesIndex._nrAlloc;
  for (i = 0; i < n; ++i) {
    void* element = collection->_edgesIndex._table[i];
    if (element) {
      TRI_Free(TRI_UNKNOWN_MEM_ZONE, element);
    }
  }
  TRI_DestroyMultiPointer(&collection->_edgesIndex);
}

////////////////////////////////////////////////////////////////////////////////
/// @brief insert an edge into the edges index
////////////////////////////////////////////////////////////////////////////////
  
int TRI_InsertEdgeDocumentCollection (TRI_document_collection_t* collection,
                                      TRI_doc_mptr_t const* header) {
  TRI_edge_header_t* entryIn;
  TRI_edge_header_t* entryOut;
  TRI_edge_header_t* entryAny;
  TRI_edge_header_t* entryAny2 = NULL;
  TRI_doc_edge_key_marker_t const* edge;
  bool needEntryAny2;
    
  edge = header->_data;

  // do we need one or two entries into for direction ANY?
  needEntryAny2 = (edge->_toCid != edge->_fromCid || strcmp(((char*) edge) + edge->_offsetToKey, ((char*) edge) + edge->_offsetFromKey) != 0);

  // allocate 3 or 4 edge headers and return early if memory allocation fails

  // IN
  entryIn = TRI_Allocate(TRI_UNKNOWN_MEM_ZONE, sizeof(TRI_edge_header_t), true);
  if (entryIn == NULL) {
    return TRI_set_errno(TRI_ERROR_OUT_OF_MEMORY);
  }

  // OUT
  entryOut = TRI_Allocate(TRI_UNKNOWN_MEM_ZONE, sizeof(TRI_edge_header_t), true);
  if (entryOut == NULL) {
    TRI_Free(TRI_UNKNOWN_MEM_ZONE, entryIn);

    return TRI_set_errno(TRI_ERROR_OUT_OF_MEMORY);
  }
  
  // ANY
  entryAny = TRI_Allocate(TRI_UNKNOWN_MEM_ZONE, sizeof(TRI_edge_header_t), true);
  if (entryAny == NULL) {
    TRI_Free(TRI_UNKNOWN_MEM_ZONE, entryIn);
    TRI_Free(TRI_UNKNOWN_MEM_ZONE, entryOut);
    
    return TRI_set_errno(TRI_ERROR_OUT_OF_MEMORY);
  }

  if (needEntryAny2) {
    entryAny2 = TRI_Allocate(TRI_UNKNOWN_MEM_ZONE, sizeof(TRI_edge_header_t), true);
    if (entryAny2 == NULL) {
      TRI_Free(TRI_UNKNOWN_MEM_ZONE, entryIn);
      TRI_Free(TRI_UNKNOWN_MEM_ZONE, entryOut);
      TRI_Free(TRI_UNKNOWN_MEM_ZONE, entryAny);

      return TRI_set_errno(TRI_ERROR_OUT_OF_MEMORY);
    }
  }

  // we have allocated all necessary memory by here

  // IN
  assert(entryIn);
  entryIn->_mptr = header;
  entryIn->_direction = TRI_EDGE_IN;
  entryIn->_cid = edge->_toCid;
  //entryIn->_did = edge->_toDid;
  entryIn->_key = ((char*) edge) + edge->_offsetToKey;
  
  TRI_InsertElementMultiPointer(&collection->_edgesIndex, entryIn, true);

  // OUT
  assert(entryOut);
  entryOut->_mptr = header;
  entryOut->_direction = TRI_EDGE_OUT;
  entryOut->_cid = edge->_fromCid;
  //entryOut->_did = edge->_fromDid;
  entryOut->_key = ((char*) edge) + edge->_offsetFromKey;

  TRI_InsertElementMultiPointer(&collection->_edgesIndex, entryOut, true);

  // ANY
  assert(entryAny);
  entryAny->_mptr = header;
  entryAny->_direction = TRI_EDGE_ANY;
  entryAny->_cid = edge->_toCid;
  //entryAny->_did = edge->_toDid;
  entryAny->_key = ((char*) edge) + edge->_offsetToKey;

  TRI_InsertElementMultiPointer(&collection->_edgesIndex, entryAny, true);

  // ANY (reversed)
  if (needEntryAny2) {
    assert(entryAny2);
    entryAny2->_mptr = header;
    entryAny2->_direction = TRI_EDGE_ANY;
    entryAny2->_cid = edge->_fromCid;
    //entryAny2->_did = edge->_fromDid;
    entryAny2->_key = ((char*) edge) + edge->_offsetFromKey;

    TRI_InsertElementMultiPointer(&collection->_edgesIndex, entryAny2, true);
  }

  return TRI_ERROR_NO_ERROR;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief remove an edge from the edges index
////////////////////////////////////////////////////////////////////////////////
  
void TRI_DeleteEdgeDocumentCollection (TRI_document_collection_t* collection,
                                       TRI_doc_mptr_t const* header) {
  TRI_edge_header_t entry;
  TRI_edge_header_t* old;
  TRI_doc_edge_key_marker_t const* edge;
  
  edge = header->_data;

  memset(&entry, 0, sizeof(entry));
  entry._mptr = header;

  // IN
  entry._direction = TRI_EDGE_IN;
  entry._cid = edge->_toCid;
  //entry._did = edge->_toDid;
  entry._key = ((char*) edge) + edge->_offsetToKey;
  old = TRI_RemoveElementMultiPointer(&collection->_edgesIndex, &entry);

  if (old != NULL) {
    TRI_Free(TRI_UNKNOWN_MEM_ZONE, old);
  }

  // OUT
  entry._direction = TRI_EDGE_OUT;
  entry._cid = edge->_fromCid;
  //entry._did = edge->_fromDid;
  entry._key = ((char*) edge) + edge->_offsetFromKey;
  old = TRI_RemoveElementMultiPointer(&collection->_edgesIndex, &entry);

  if (old != NULL) {
    TRI_Free(TRI_UNKNOWN_MEM_ZONE, old);
  }

  // ANY
  entry._direction = TRI_EDGE_ANY;
  entry._cid = edge->_toCid;
  //entry._did = edge->_toDid;
  entry._key = ((char*) edge) + edge->_offsetToKey;
  old = TRI_RemoveElementMultiPointer(&collection->_edgesIndex, &entry);

  if (old != NULL) {
    TRI_Free(TRI_UNKNOWN_MEM_ZONE, old);
  }

  if (edge->_toCid != edge->_fromCid || strcmp(((char*) edge) + edge->_offsetToKey, ((char*) edge) + edge->_offsetFromKey) != 0) {
    entry._direction = TRI_EDGE_ANY;
    entry._cid = edge->_fromCid;
    //entry._did = edge->_fromDid;
    entry._key = ((char*) edge) + edge->_offsetFromKey;
    old = TRI_RemoveElementMultiPointer(&collection->_edgesIndex, &entry);

    if (old != NULL) {
      TRI_Free(TRI_UNKNOWN_MEM_ZONE, old);
    }
  }
}

////////////////////////////////////////////////////////////////////////////////
/// @brief looks up edges
////////////////////////////////////////////////////////////////////////////////

TRI_vector_pointer_t TRI_LookupEdgesDocumentCollection (TRI_document_collection_t* collection,
                                                        TRI_edge_direction_e direction,
                                                        TRI_voc_cid_t cid,
                                                        TRI_voc_key_t key) {
  union { TRI_doc_mptr_t* v; TRI_doc_mptr_t const* c; } cnv;
  TRI_vector_pointer_t result;
  TRI_edge_header_t entry;
  TRI_vector_pointer_t found;
  size_t i;

  TRI_InitVectorPointer(&result, TRI_UNKNOWN_MEM_ZONE);

  entry._direction = direction;
  entry._cid = cid;
  entry._key = key;

  found = TRI_LookupByKeyMultiPointer(TRI_UNKNOWN_MEM_ZONE, &collection->_edgesIndex, &entry);

  for (i = 0;  i < found._length;  ++i) {
    cnv.c = ((TRI_edge_header_t*) found._buffer[i])->_mptr;

    TRI_PushBackVectorPointer(&result, cnv.v);
  }

  TRI_DestroyVectorPointer(&found);

  return result;
}

////////////////////////////////////////////////////////////////////////////////
/// @}
////////////////////////////////////////////////////////////////////////////////

// Local Variables:
// mode: outline-minor
// outline-regexp: "^\\(/// @brief\\|/// {@inheritDoc}\\|/// @addtogroup\\|// --SECTION--\\|/// @\\}\\)"
// End:
