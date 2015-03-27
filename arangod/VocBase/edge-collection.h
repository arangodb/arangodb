////////////////////////////////////////////////////////////////////////////////
/// @brief edge collection
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
/// @author Copyright 2011-2013, triAGENS GmbH, Cologne, Germany
////////////////////////////////////////////////////////////////////////////////

#ifndef ARANGODB_VOC_BASE_EDGE__COLLECTION_H
#define ARANGODB_VOC_BASE_EDGE__COLLECTION_H 1

#include "Basics/Common.h"
#include "Basics/Exceptions.h"
#include "VocBase/voc-types.h"
#include "VocBase/document-collection.h"

struct TRI_index_s;

// -----------------------------------------------------------------------------
// --SECTION--                                                   EDGE COLLECTION
// -----------------------------------------------------------------------------

// -----------------------------------------------------------------------------
// --SECTION--                                                      public types
// -----------------------------------------------------------------------------

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
  TRI_voc_cid_t _cid; // from or to, depending on the direction
  TRI_voc_key_t _key;
}
TRI_edge_header_t;

////////////////////////////////////////////////////////////////////////////////
/// @brief edge index iterator
////////////////////////////////////////////////////////////////////////////////

struct TRI_edge_index_iterator_t {
  TRI_edge_index_iterator_t (TRI_edge_direction_e direction,
                             TRI_voc_cid_t cid,
                             TRI_voc_key_t key) 
    : _direction(direction),
      _edge({ cid, nullptr }) {

    TRI_ASSERT(key != nullptr);
    _edge._key = TRI_DuplicateStringZ(TRI_UNKNOWN_MEM_ZONE, key);
    if (_edge._key == nullptr) {
      THROW_ARANGO_EXCEPTION(TRI_ERROR_OUT_OF_MEMORY);
    }
  }

  ~TRI_edge_index_iterator_t () {
    if (_edge._key != nullptr) {
      TRI_FreeString(TRI_UNKNOWN_MEM_ZONE, _edge._key);
    }
  }

  TRI_edge_direction_e const _direction;
  TRI_edge_header_t          _edge;
};

// -----------------------------------------------------------------------------
// --SECTION--                                                       EDGES INDEX
// -----------------------------------------------------------------------------

// -----------------------------------------------------------------------------
// --SECTION--                                                  public functions
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @brief looks up edges
////////////////////////////////////////////////////////////////////////////////

std::vector<TRI_doc_mptr_copy_t> TRI_LookupEdgesDocumentCollection (
                                                  struct TRI_document_collection_t*,
                                                  TRI_edge_direction_e,
                                                  TRI_voc_cid_t,
                                                  TRI_voc_key_t);

////////////////////////////////////////////////////////////////////////////////
/// @brief looks up edges using the index, restarting at the edge pointed at
/// by next
////////////////////////////////////////////////////////////////////////////////
      
void TRI_LookupEdgeIndex (struct TRI_index_s*,
                          TRI_edge_index_iterator_t const*,
                          std::vector<TRI_doc_mptr_copy_t>&,
                          void*&,
                          size_t);

#endif

// -----------------------------------------------------------------------------
// --SECTION--                                                       END-OF-FILE
// -----------------------------------------------------------------------------

// Local Variables:
// mode: outline-minor
// outline-regexp: "/// @brief\\|/// {@inheritDoc}\\|/// @page\\|// --SECTION--\\|/// @\\}"
// End:
