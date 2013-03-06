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

#ifndef TRIAGENS_DURHAM_VOC_BASE_EDGE_COLLECTION_H
#define TRIAGENS_DURHAM_VOC_BASE_EDGE_COLLECTION_H 1

#include "VocBase/document-collection.h"

#ifdef __cplusplus
extern "C" {
#endif

// -----------------------------------------------------------------------------
// --SECTION--                                                   EDGE COLLECTION
// -----------------------------------------------------------------------------

// -----------------------------------------------------------------------------
// --SECTION--                                                   private defines
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @addtogroup VocBase
/// @{
////////////////////////////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////////////////////////
/// @brief special bit that can be set within edge flags
/// this bit will be set if the edge is an in-marker
////////////////////////////////////////////////////////////////////////////////

#define TRI_EDGE_BIT_DIRECTION_IN  ((TRI_edge_flags_t) (1 << 1))

////////////////////////////////////////////////////////////////////////////////
/// @brief special bit that can be set within edge flags
/// this bit will be set if the edge is an out-marker
////////////////////////////////////////////////////////////////////////////////

#define TRI_EDGE_BIT_DIRECTION_OUT ((TRI_edge_flags_t) (1 << 2))

////////////////////////////////////////////////////////////////////////////////
/// @brief special bit that can be set within edge flags
/// this bit will be set if the edge is self-reflexive (i.e. _from and _to are
/// the same)
////////////////////////////////////////////////////////////////////////////////

#define TRI_EDGE_BIT_REFLEXIVE     ((TRI_edge_flags_t) (1 << 3))

////////////////////////////////////////////////////////////////////////////////
/// @brief combination of the two directional bits
////////////////////////////////////////////////////////////////////////////////

#define TRI_EDGE_BITS_DIRECTION    (TRI_EDGE_BIT_DIRECTION_IN | TRI_EDGE_BIT_DIRECTION_OUT)

////////////////////////////////////////////////////////////////////////////////
/// @}
////////////////////////////////////////////////////////////////////////////////

// -----------------------------------------------------------------------------
// --SECTION--                                                      public types
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @addtogroup VocBase
/// @{
////////////////////////////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////////////////////////
/// @brief typedef for edge flags
/// the type is an integer which indicates the edge direction (IN/OUT)
/// plus two extra bits that indicate whether the edge is self-reflexive and 
/// whether the edge is directed
////////////////////////////////////////////////////////////////////////////////

typedef uint8_t TRI_edge_flags_t;

////////////////////////////////////////////////////////////////////////////////
/// @brief edge from and to
////////////////////////////////////////////////////////////////////////////////

typedef struct TRI_document_edge_s {
  TRI_voc_cid_t _fromCid;
  TRI_voc_key_t _fromKey;

  TRI_voc_cid_t _toCid;
  TRI_voc_key_t _toKey;
}
TRI_document_edge_t;

////////////////////////////////////////////////////////////////////////////////
/// @brief edge direction
////////////////////////////////////////////////////////////////////////////////

typedef enum {
  TRI_EDGE_ANY    = 0, // can only be used for searching
  TRI_EDGE_IN     = 1,
  TRI_EDGE_OUT    = 2
}
TRI_edge_direction_e;

////////////////////////////////////////////////////////////////////////////////
/// @brief index entry for edges
////////////////////////////////////////////////////////////////////////////////

typedef struct TRI_edge_header_s {
  TRI_doc_mptr_t const* _mptr;
  TRI_voc_cid_t _cid; // from or to, depending on the direction
  union { TRI_voc_key_t _key; TRI_voc_size_t _offsetKey; } _searchKey;
  TRI_edge_flags_t _flags;
}
TRI_edge_header_t;

////////////////////////////////////////////////////////////////////////////////
/// @}
////////////////////////////////////////////////////////////////////////////////

// -----------------------------------------------------------------------------
// --SECTION--                                                       EDGES INDEX
// -----------------------------------------------------------------------------

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

TRI_edge_flags_t TRI_LookupFlagsEdge (const TRI_edge_direction_e);

////////////////////////////////////////////////////////////////////////////////
/// @brief compose edge flags aggregate out of multiple individual parameters
////////////////////////////////////////////////////////////////////////////////

TRI_edge_flags_t TRI_FlagsEdge (const TRI_edge_direction_e, 
                                const bool); 

////////////////////////////////////////////////////////////////////////////////
/// @brief looks up edges
////////////////////////////////////////////////////////////////////////////////

TRI_vector_pointer_t TRI_LookupEdgesDocumentCollection (TRI_document_collection_t*,
                                                        TRI_edge_direction_e,
                                                        TRI_voc_cid_t,
                                                        TRI_voc_key_t);

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
