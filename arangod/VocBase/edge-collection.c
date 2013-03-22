////////////////////////////////////////////////////////////////////////////////
/// @brief edge collection functionality
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
/// @author Jan Steemann
/// @author Copyright 2011-2013, triAGENS GmbH, Cologne, Germany
////////////////////////////////////////////////////////////////////////////////

#include "edge-collection.h"

#include "BasicsC/logging.h"

#include "VocBase/document-collection.h"
#include "VocBase/index.h"

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
/// @brief find the edges index of a document collection
////////////////////////////////////////////////////////////////////////////////

static TRI_multi_pointer_t* FindEdgesIndex (TRI_document_collection_t* const document) {
  size_t i, n;

  if (document->base.base._info._type != TRI_COL_TYPE_EDGE) {
    // collection is not an edge collection... caller must handle that
    return NULL;
  }

  n = document->_allIndexes._length;
  for (i = 0; i < n; ++i) {
    TRI_index_t* idx = (TRI_index_t*) TRI_AtVectorPointer(&document->_allIndexes, i);
    if (idx->_type == TRI_IDX_TYPE_EDGE_INDEX) {
      TRI_edge_index_t* edgesIndex = (TRI_edge_index_t*) idx;

      return &edgesIndex->_edges;
    }
  }

  // collection does not have an edges index... caller must handle that
  return NULL;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief return whether an edge is self-reflexive
////////////////////////////////////////////////////////////////////////////////

static bool IsReflexive (const TRI_edge_header_t* const edge) {
  return ((edge->_flags & TRI_EDGE_BIT_REFLEXIVE) > 0);
}

////////////////////////////////////////////////////////////////////////////////
/// @brief find edges matching search criteria and add them to the result
/// this function is called two times for each edge query:
/// the first call (with matchType 1) will query the index with the originally
/// requested direction, whereas the second call will query the index with the
/// opposite direction (with matchType 2 or 3) to find all counterparts
////////////////////////////////////////////////////////////////////////////////

static bool FindEdges (const TRI_edge_direction_e direction,
                       TRI_multi_pointer_t* idx,
                       TRI_vector_pointer_t* result,
                       TRI_edge_header_t* entry,
                       const int matchType) {
  TRI_vector_pointer_t found;
  TRI_edge_header_t* edge;

  entry->_flags = TRI_LookupFlagsEdge(direction);
  found = TRI_LookupByKeyMultiPointer(TRI_UNKNOWN_MEM_ZONE, idx, entry);

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
      }

      TRI_PushBackVectorPointer(result, CONST_CAST(edge->_mptr));
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
/// @brief compose edge flags aggregate out of only the direction
////////////////////////////////////////////////////////////////////////////////

TRI_edge_flags_t TRI_LookupFlagsEdge (const TRI_edge_direction_e direction) {
  if (direction == TRI_EDGE_IN) {
    return TRI_EDGE_BIT_DIRECTION_IN;
  }
  if (direction == TRI_EDGE_OUT) {
    return TRI_EDGE_BIT_DIRECTION_OUT;
  }

  // invalid direction type
  assert(false);
  return 0;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief compose edge flags aggregate out of multiple individual parameters
////////////////////////////////////////////////////////////////////////////////

TRI_edge_flags_t TRI_FlagsEdge (const TRI_edge_direction_e direction,
                                const bool isReflexive) {
  TRI_edge_flags_t result = TRI_LookupFlagsEdge(direction);

  if (isReflexive) {
    result |= TRI_EDGE_BIT_REFLEXIVE;
  }

  return result;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief looks up edges
////////////////////////////////////////////////////////////////////////////////

TRI_vector_pointer_t TRI_LookupEdgesDocumentCollection (TRI_document_collection_t* document,
                                                        TRI_edge_direction_e direction,
                                                        TRI_voc_cid_t cid,
                                                        TRI_voc_key_t key) {
  TRI_vector_pointer_t result;
  TRI_edge_header_t entry;
  TRI_multi_pointer_t* edgesIndex;

  // search criteria
  entry._mptr = NULL;
  entry._cid = cid;
  entry._searchKey._key = key;

  // initialise the result vector
  TRI_InitVectorPointer(&result, TRI_UNKNOWN_MEM_ZONE);

  edgesIndex = FindEdgesIndex(document);
  if (edgesIndex == NULL) {
    LOG_ERROR("collection does not have an edges index");
    return result;
  }

  if (direction == TRI_EDGE_IN) {
    // get all edges with a matching IN vertex
    FindEdges(TRI_EDGE_IN, edgesIndex, &result, &entry, 1);
  }
  else if (direction == TRI_EDGE_OUT) {
    // get all edges with a matching OUT vertex
    FindEdges(TRI_EDGE_OUT, edgesIndex, &result, &entry, 1);
  }
  else if (direction == TRI_EDGE_ANY) {
    // get all edges with a matching IN vertex
    FindEdges(TRI_EDGE_IN, edgesIndex, &result, &entry, 1);
    // add all non-reflexive edges with a matching OUT vertex
    FindEdges(TRI_EDGE_OUT, edgesIndex, &result, &entry, 3);
  }

  return result;
}

////////////////////////////////////////////////////////////////////////////////
/// @}
////////////////////////////////////////////////////////////////////////////////

// Local Variables:
// mode: outline-minor
// outline-regexp: "/// @brief\\|/// {@inheritDoc}\\|/// @addtogroup\\|/// @page\\|// --SECTION--\\|/// @\\}"
// End:
