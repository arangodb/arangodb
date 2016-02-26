////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2014-2016 ArangoDB GmbH, Cologne, Germany
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
////////////////////////////////////////////////////////////////////////////////

#ifndef ARANGOD_VOC_BASE_EDGE_COLLECTION_H
#define ARANGOD_VOC_BASE_EDGE_COLLECTION_H 1

#include "Basics/Common.h"
#include "Basics/Exceptions.h"
#include "VocBase/voc-types.h"
#include "VocBase/document-collection.h"

////////////////////////////////////////////////////////////////////////////////
/// @brief edge direction
////////////////////////////////////////////////////////////////////////////////

typedef enum {
  TRI_EDGE_ANY = 0,  // can only be used for searching
  TRI_EDGE_IN = 1,
  TRI_EDGE_OUT = 2
} TRI_edge_direction_e;

////////////////////////////////////////////////////////////////////////////////
/// @brief edge index iterator
///        NOTE: Make sure the input Slice stays valid as long as this iterator
///              is in use.
////////////////////////////////////////////////////////////////////////////////

struct TRI_edge_index_iterator_t {
  TRI_edge_index_iterator_t(TRI_edge_direction_e direction,
                            arangodb::velocypack::Slice const& edge)
      : _direction(direction), _edge(edge) {}

  TRI_edge_direction_e const _direction;
  arangodb::velocypack::Slice const _edge;
};

////////////////////////////////////////////////////////////////////////////////
/// @brief looks up edges
///        DEPRECATED
////////////////////////////////////////////////////////////////////////////////

std::vector<TRI_doc_mptr_copy_t> TRI_LookupEdgesDocumentCollection(
    arangodb::Transaction*, struct TRI_document_collection_t*,
    TRI_edge_direction_e, TRI_voc_cid_t, std::string const&);

#endif
