////////////////////////////////////////////////////////////////////////////////
/// @brief json shaper used to compute the shape of an json object
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
/// @author Martin Schoenert
/// @author Copyright 2014, ArangoDB GmbH, Cologne, Germany
/// @author Copyright 2006-2013, triAGENS GmbH, Cologne, Germany
////////////////////////////////////////////////////////////////////////////////

#ifndef ARANGODB_VOC_BASE_VOC__SHAPER_H
#define ARANGODB_VOC_BASE_VOC__SHAPER_H 1

#include "Basics/Common.h"

#include "ShapedJson/json-shaper.h"
#include "ShapedJson/shape-accessor.h"
#include "ShapedJson/shaped-json.h"
#include "VocBase/datafile.h"
#include "VocBase/document-collection.h"
#include "Wal/Marker.h"

struct TRI_document_collection_t;

// -----------------------------------------------------------------------------
// --SECTION--                                      constructors and destructors
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @brief creates a shaper
////////////////////////////////////////////////////////////////////////////////

TRI_shaper_t* TRI_CreateVocShaper (TRI_vocbase_t*,
                                   struct TRI_document_collection_t*);

////////////////////////////////////////////////////////////////////////////////
/// @brief destroys a shaper, but does not free the pointer
////////////////////////////////////////////////////////////////////////////////

void TRI_DestroyVocShaper (TRI_shaper_t*);

////////////////////////////////////////////////////////////////////////////////
/// @brief destroys a shaper and frees the pointer
////////////////////////////////////////////////////////////////////////////////

void TRI_FreeVocShaper (TRI_shaper_t*);

// -----------------------------------------------------------------------------
// --SECTION--                                                  public functions
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @brief initialises a vocshaper
////////////////////////////////////////////////////////////////////////////////

int TRI_InitVocShaper (TRI_shaper_t*);

////////////////////////////////////////////////////////////////////////////////
/// @brief move a shape marker, called during compaction
////////////////////////////////////////////////////////////////////////////////

int TRI_MoveMarkerVocShaper (TRI_shaper_t*,
                             TRI_df_marker_t*,
                             void*);

////////////////////////////////////////////////////////////////////////////////
/// @brief insert a shape, called when opening a collection
////////////////////////////////////////////////////////////////////////////////

int TRI_InsertShapeVocShaper (TRI_shaper_t*,
                              TRI_df_marker_t const*,
                              bool);

////////////////////////////////////////////////////////////////////////////////
/// @brief insert an attribute, called when opening a collection
////////////////////////////////////////////////////////////////////////////////

int TRI_InsertAttributeVocShaper (TRI_shaper_t*,
                                  TRI_df_marker_t const*,
                                  bool);

////////////////////////////////////////////////////////////////////////////////
/// @brief finds an accessor
////////////////////////////////////////////////////////////////////////////////

TRI_shape_access_t const* TRI_FindAccessorVocShaper (TRI_shaper_t*,
                                                     TRI_shape_sid_t,
                                                     TRI_shape_pid_t);

////////////////////////////////////////////////////////////////////////////////
/// @brief extracts a sub-shape
////////////////////////////////////////////////////////////////////////////////

bool TRI_ExtractShapedJsonVocShaper (TRI_shaper_t* s,
                                     TRI_shaped_json_t const* document,
                                     TRI_shape_sid_t sid,
                                     TRI_shape_pid_t pid,
                                     TRI_shaped_json_t* result,
                                     TRI_shape_t const** shape);

////////////////////////////////////////////////////////////////////////////////
/// @brief helper method for recursion for comparison
///
/// You must either supply (leftDocument, leftObject) or leftShaped.
/// You must either supply (rightDocument, rightObject) or rightShaped.
////////////////////////////////////////////////////////////////////////////////

int TRI_CompareShapeTypes (char const* leftDocument,
                           TRI_shaped_sub_t* leftObject,
                           TRI_shaped_json_t const* leftShaped,
                           TRI_shaper_t* leftShaper,
                           char const* rightDocument,
                           TRI_shaped_sub_t* rightObject,
                           TRI_shaped_json_t const* rightShaped,
                           TRI_shaper_t* rightShaper);

////////////////////////////////////////////////////////////////////////////////
/// @brief extracts the shape identifier pointer from a marker
////////////////////////////////////////////////////////////////////////////////

static inline void TRI_EXTRACT_SHAPE_IDENTIFIER_MARKER(
                   TRI_shape_sid_t& dst,
                   void const* src) {

  TRI_df_marker_t const* marker = static_cast<TRI_df_marker_t const*>(src);

  if (marker->_type == TRI_DOC_MARKER_KEY_DOCUMENT) {
    dst = static_cast<TRI_doc_document_key_marker_t const*>(src)->_shape;
  }
  else if (marker->_type == TRI_DOC_MARKER_KEY_EDGE) {
    dst = static_cast<TRI_doc_edge_key_marker_t const*>(src)->base._shape;
  }
  else if (marker->_type == TRI_WAL_MARKER_DOCUMENT) {
    dst = static_cast<triagens::wal::document_marker_t const*>(src)->_shape;
  }
  else if (marker->_type == TRI_WAL_MARKER_EDGE) {
    dst = static_cast<triagens::wal::edge_marker_t const*>(src)->_shape;
  }
  else {
    dst = 0;
  }
}

////////////////////////////////////////////////////////////////////////////////
/// @brief extracts the shaped JSON pointer from a marker
////////////////////////////////////////////////////////////////////////////////

static inline void TRI_EXTRACT_SHAPED_JSON_MARKER (TRI_shaped_json_t& dst,
                                                   void const* src) {
  if (static_cast<TRI_df_marker_t const*>(src)->_type ==
      TRI_DOC_MARKER_KEY_DOCUMENT) {
    dst._sid = static_cast<TRI_doc_document_key_marker_t const*>(src)->_shape;
    dst._data.length = static_cast<TRI_df_marker_t const*>(src)->_size
      - static_cast<TRI_doc_document_key_marker_t const*>(src)->_offsetJson;
    dst._data.data = const_cast<char*>(static_cast<char const*>(src))
      + static_cast<TRI_doc_document_key_marker_t const*>(src)->_offsetJson;
  }
  else if (static_cast<TRI_df_marker_t const*>(src)->_type ==
           TRI_DOC_MARKER_KEY_EDGE) {
    dst._sid = static_cast<TRI_doc_document_key_marker_t const*>(src)->_shape;
    dst._data.length = static_cast<TRI_df_marker_t const*>(src)->_size
      - static_cast<TRI_doc_document_key_marker_t const*>(src)->_offsetJson;
    dst._data.data = const_cast<char*>(static_cast<char const*>(src))
      + static_cast<TRI_doc_document_key_marker_t const*>(src)->_offsetJson;
  }
  else if (static_cast<TRI_df_marker_t const*>(src)->_type ==
           TRI_WAL_MARKER_DOCUMENT) {
    dst._sid = static_cast<triagens::wal::document_marker_t const*>(src)->_shape;
    dst._data.length = static_cast<TRI_df_marker_t const*>(src)->_size
      - static_cast<triagens::wal::document_marker_t const*>(src)->_offsetJson;
    dst._data.data = const_cast<char*>(static_cast<char const*>(src))
      + static_cast<triagens::wal::document_marker_t const*>(src)->_offsetJson;
  }
  else if (static_cast<TRI_df_marker_t const*>(src)->_type ==
           TRI_WAL_MARKER_EDGE) {
    dst._sid = static_cast<triagens::wal::edge_marker_t const*>(src)->_shape;
    dst._data.length = static_cast<TRI_df_marker_t const*>(src)->_size
      - static_cast<triagens::wal::edge_marker_t const*>(src)->_offsetJson;
    dst._data.data = const_cast<char*>(static_cast<char const*>(src))
      + static_cast<triagens::wal::edge_marker_t const*>(src)->_offsetJson;
  }
  else {
    dst._sid = 0;
    dst._data.length = 0;
    dst._data.data = nullptr;
  }
}

#endif

// -----------------------------------------------------------------------------
// --SECTION--                                                       END-OF-FILE
// -----------------------------------------------------------------------------

// Local Variables:
// mode: outline-minor
// outline-regexp: "/// @brief\\|/// {@inheritDoc}\\|/// @page\\|// --SECTION--\\|/// @\\}"
// End:
