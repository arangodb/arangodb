////////////////////////////////////////////////////////////////////////////////
/// @brief edge collection functionality
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
/// @author Jan Steemann
/// @author Copyright 2011, triagens GmbH, Cologne, Germany
////////////////////////////////////////////////////////////////////////////////

#include "edge-collection.h"

// -----------------------------------------------------------------------------
// --SECTION--                                                       EDGES INDEX
// -----------------------------------------------------------------------------

// -----------------------------------------------------------------------------
// --SECTION--                                                   private defines
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @brief special bit that can be set within edge flags
/// this bit will be set if the edge is an in-marker
////////////////////////////////////////////////////////////////////////////////

#define BIT_DIRECTION_IN  ((TRI_edge_flags_t) (1 << 1))

////////////////////////////////////////////////////////////////////////////////
/// @brief special bit that can be set within edge flags
/// this bit will be set if the edge is an out-marker
////////////////////////////////////////////////////////////////////////////////

#define BIT_DIRECTION_OUT ((TRI_edge_flags_t) (1 << 2))

////////////////////////////////////////////////////////////////////////////////
/// @brief special bit that can be set within edge flags
/// this bit will be set if the edge is self-reflexive (i.e. _from and _to are
/// the same)
////////////////////////////////////////////////////////////////////////////////

#define BIT_REFLEXIVE     ((TRI_edge_flags_t) (1 << 3))

////////////////////////////////////////////////////////////////////////////////
/// @brief special bit that can be set within edge flags
/// this bit will be set if the edge is bidirectional
////////////////////////////////////////////////////////////////////////////////

#define BIT_BIDIRECTIONAL ((TRI_edge_flags_t) (1 << 4))

////////////////////////////////////////////////////////////////////////////////
/// @brief combination of the two directional bits
////////////////////////////////////////////////////////////////////////////////

#define BITS_DIRECTION    (BIT_DIRECTION_IN | BIT_DIRECTION_OUT)

// -----------------------------------------------------------------------------
// --SECTION--                                                 private functions
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @addtogroup VocBase
/// @{
////////////////////////////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////////////////////////
/// @brief return whether an edge is bi-directional
////////////////////////////////////////////////////////////////////////////////

static bool IsBidirectional (const TRI_edge_header_t* const edge) {
  return ((edge->_flags & BIT_BIDIRECTIONAL) > 0);
}

////////////////////////////////////////////////////////////////////////////////
/// @brief return whether an edge is self-reflexive
////////////////////////////////////////////////////////////////////////////////

static bool IsReflexive (const TRI_edge_header_t* const edge) {
  return ((edge->_flags & BIT_REFLEXIVE) > 0);
}

////////////////////////////////////////////////////////////////////////////////
/// @brief compose edge flags aggregate out of only the direction
////////////////////////////////////////////////////////////////////////////////

static TRI_edge_flags_t MakeLookupFlags (const TRI_edge_direction_e direction) {
  if (direction == TRI_EDGE_IN) {
    return BIT_DIRECTION_IN;
  }
  if (direction == TRI_EDGE_OUT) {
    return BIT_DIRECTION_OUT;
  }

  // invalid direction type
  assert(false);
  return 0;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief compose edge flags aggregate out of multiple individual parameters
////////////////////////////////////////////////////////////////////////////////

static TRI_edge_flags_t MakeFlags (const TRI_edge_direction_e direction, 
                                   const bool isReflexive, 
                                   const bool isBidirectional) {
  TRI_edge_flags_t result = MakeLookupFlags(direction);

  if (isReflexive) {
    result |= BIT_REFLEXIVE;
  }
  if (isBidirectional) {
    result |= BIT_BIDIRECTIONAL;
  }

  return result;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief hashes an edge header
////////////////////////////////////////////////////////////////////////////////

static uint64_t HashElementEdge (TRI_multi_pointer_t* array, void const* data) {
  TRI_edge_header_t const* h;
  uint64_t hash[3];

  h = data;

  // only include directional bits for hashing, exclude special bits
  hash[0] = (uint64_t) (h->_flags & BITS_DIRECTION); 
  hash[1] = h->_cid;
  hash[2] = TRI_FnvHashString((char const*) h->_key); 

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

  // only include directional flags, exclude special bits
  return ((l->_flags & BITS_DIRECTION) == (r->_flags & BITS_DIRECTION)) &&
         (l->_cid == r->_cid) && 
         (strcmp(l->_key, r->_key) == 0);  
}

////////////////////////////////////////////////////////////////////////////////
/// @brief checks for elements are equal
////////////////////////////////////////////////////////////////////////////////

static bool IsEqualElementEdge (TRI_multi_pointer_t* array, void const* left, void const* right) {
  TRI_edge_header_t const* l;
  TRI_edge_header_t const* r;

  l = left;
  r = right;

  // only include directional flags, exclude special bits
  return (l->_mptr == r->_mptr) && 
         ((l->_flags & BITS_DIRECTION) == (r->_flags & BITS_DIRECTION)) && 
         (l->_cid == r->_cid) && 
         (strcmp(l->_key, r->_key) == 0);
}

////////////////////////////////////////////////////////////////////////////////
/// @brief find edges matching search criteria and add them to the result
/// this function is called two times for each edge query:
/// the first call (with matchType 1) will query the index with the originally
/// requested direction, whereas the second call will query the index with the
/// opposite direction (with matchType 2 or 3) to find all counterparts
////////////////////////////////////////////////////////////////////////////////

static bool FindEdges (const TRI_edge_direction_e direction,
                       TRI_multi_pointer_t* index, 
                       TRI_vector_pointer_t* result,
                       TRI_edge_header_t* entry,
                       const int matchType) {
  union { TRI_doc_mptr_t* v; TRI_doc_mptr_t const* c; } cnv;
  TRI_vector_pointer_t found;
  TRI_edge_header_t* edge;
  
  entry->_flags = MakeLookupFlags(direction);
  found = TRI_LookupByKeyMultiPointer(TRI_UNKNOWN_MEM_ZONE, index, entry);

  if (found._length > 0) {
    size_t i;

    if (result->_capacity == 0) {
      int res;

      // if result vector is still empty and we have results, re-init the 
      // result vector to a "good" size. this will save later reallocations
      res = TRI_InitVectorPointer2(result, TRI_UNKNOWN_MEM_ZONE, found._length);
      if (res != TRI_ERROR_NO_ERROR) {
        TRI_DestroyVectorPointer(&found);
        TRI_set_errno(res);

        return false;
      }
    }

    // add all results found
    for (i = 0;  i < found._length;  ++i) {
      edge = (TRI_edge_header_t*) found._buffer[i];
      cnv.c = edge->_mptr;

      // the following queries will use the following sequences of matchTypes:
      // inEdges(): 1, 2,  outEdges(): 1, 2,  edges(): 1, 3

      // if matchType is 1, we'll return all found edges without further filtering
      //
      // if matchType is 2 (inEdges or outEdges query), the direction is reversed. 
      // We'll exclude all self-reflexive edges now (we already got them in iteration 1), 
      // and alsoexclude all unidirectional edges
      //
      // if matchType is 3, the direction is also reversed. We'll exclude all
      // self-reflexive edges now (we already got them in iteration 1)

      if (matchType > 1) {
        // if the edge is self-reflexive, we have already found it in iteration 1
        // we must skip it here, otherwise we would produce duplicates
        if (IsReflexive(edge)) {
          continue;
        }

        // in type 2 we are only interested in the counterparts of bidirectional edges
        if (matchType == 2 && ! IsBidirectional(edge)) {
          continue;
        }
      }

      TRI_PushBackVectorPointer(result, cnv.v);
    }
  } 
    
  
  TRI_DestroyVectorPointer(&found);

  return true;
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
  TRI_doc_edge_key_marker_t const* edge;
  bool isReflexive;
  bool isBidirectional;

  edge = header->_data;

  // is the edge self-reflexive (_from & _to are identical)?
  isReflexive = (edge->_toCid == edge->_fromCid && strcmp(((char*) edge) + edge->_offsetToKey, ((char*) edge) + edge->_offsetFromKey) == 0);
  isBidirectional = edge->_isBidirectional;

  // allocate all edge headers and return early if memory allocation fails

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
  
  // we have allocated all necessary memory by here

  // IN
  assert(entryIn);
  entryIn->_mptr = header;
  entryIn->_flags = MakeFlags(TRI_EDGE_IN, isReflexive, isBidirectional);
  entryIn->_cid = edge->_toCid;
  entryIn->_key = ((char*) edge) + edge->_offsetToKey;
  
  TRI_InsertElementMultiPointer(&collection->_edgesIndex, entryIn, true);

  // OUT
  assert(entryOut);
  entryOut->_mptr = header;
  entryOut->_flags = MakeFlags(TRI_EDGE_OUT, isReflexive, isBidirectional);
  entryOut->_cid = edge->_fromCid;
  entryOut->_key = ((char*) edge) + edge->_offsetFromKey;

  TRI_InsertElementMultiPointer(&collection->_edgesIndex, entryOut, true);

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
  bool isReflexive;
  bool isBidirectional;
  
  edge = header->_data;
  
  // is the edge self-reflexive (_from & _to are identical)?
  isReflexive = (edge->_toCid == edge->_fromCid && strcmp(((char*) edge) + edge->_offsetToKey, ((char*) edge) + edge->_offsetFromKey) == 0);
  isBidirectional = edge->_isBidirectional;

  entry._mptr = header;

  // IN
  entry._flags = MakeLookupFlags(TRI_EDGE_IN);
  entry._cid = edge->_toCid;
  entry._key = ((char*) edge) + edge->_offsetToKey;
  old = TRI_RemoveElementMultiPointer(&collection->_edgesIndex, &entry);

  if (old != NULL) {
    TRI_Free(TRI_UNKNOWN_MEM_ZONE, old);
  }

  // OUT
  entry._flags = MakeLookupFlags(TRI_EDGE_OUT);
  entry._cid = edge->_fromCid;
  entry._key = ((char*) edge) + edge->_offsetFromKey;
  old = TRI_RemoveElementMultiPointer(&collection->_edgesIndex, &entry);

  if (old != NULL) {
    TRI_Free(TRI_UNKNOWN_MEM_ZONE, old);
  }
}
////////////////////////////////////////////////////////////////////////////////
/// @brief looks up edges
////////////////////////////////////////////////////////////////////////////////

TRI_vector_pointer_t TRI_LookupEdgesDocumentCollection (TRI_document_collection_t* collection,
                                                        TRI_edge_direction_e direction,
                                                        TRI_voc_cid_t cid,
                                                        TRI_voc_key_t key) {
  TRI_vector_pointer_t result;
  TRI_edge_header_t entry;

  // search criteria 
  entry._mptr = 0;
  entry._cid = cid;
  entry._key = key;

  // initialise the result vector
  TRI_InitVectorPointer(&result, TRI_UNKNOWN_MEM_ZONE);

  if (direction == TRI_EDGE_IN) {
    // get all edges with a matching IN vertex
    FindEdges(TRI_EDGE_IN, &collection->_edgesIndex, &result, &entry, 1);
    // add all bidirectional edges with a matching OUT vertex
    FindEdges(TRI_EDGE_OUT, &collection->_edgesIndex, &result, &entry, 2);
  }
  else if (direction == TRI_EDGE_OUT) {
    // get all edges with a matching OUT vertex
    FindEdges(TRI_EDGE_OUT, &collection->_edgesIndex, &result, &entry, 1);
    // add all bidirectional edges with a matching IN vertex
    FindEdges(TRI_EDGE_IN, &collection->_edgesIndex, &result, &entry, 2);
  }
  else if (direction == TRI_EDGE_ANY) {
    // get all edges with a matching IN vertex
    FindEdges(TRI_EDGE_IN, &collection->_edgesIndex, &result, &entry, 1);
    // add all non-reflexive edges with a matching OUT vertex
    FindEdges(TRI_EDGE_OUT, &collection->_edgesIndex, &result, &entry, 3);
  }

  return result;
}

////////////////////////////////////////////////////////////////////////////////
/// @}
////////////////////////////////////////////////////////////////////////////////

// Local Variables:
// mode: outline-minor
// outline-regexp: "^\\(/// @brief\\|/// {@inheritDoc}\\|/// @addtogroup\\|// --SECTION--\\|/// @\\}\\)"
// End:
