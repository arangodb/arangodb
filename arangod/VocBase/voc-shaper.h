////////////////////////////////////////////////////////////////////////////////
/// @brief json shaper used to compute the shape of an json object
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
/// @author Martin Schoenert
/// @author Copyright 2006-2013, triAGENS GmbH, Cologne, Germany
////////////////////////////////////////////////////////////////////////////////

#ifndef TRIAGENS_VOC_BASE_VOC_SHAPER_H
#define TRIAGENS_VOC_BASE_VOC_SHAPER_H 1

#include "Basics/Common.h"

#include "ShapedJson/json-shaper.h"
#include "ShapedJson/shape-accessor.h"
#include "ShapedJson/shaped-json.h"
#include "VocBase/datafile.h"
#include "VocBase/document-collection.h"
#include "Wal/Marker.h"

struct TRI_document_collection_s;

// -----------------------------------------------------------------------------
// --SECTION--                                                      public types
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @brief datafile attribute marker
////////////////////////////////////////////////////////////////////////////////

typedef struct TRI_df_attribute_marker_s {
  TRI_df_marker_t base;

  TRI_shape_aid_t _aid;
  TRI_shape_size_t _size;

  // char name[]
}
TRI_df_attribute_marker_t;

////////////////////////////////////////////////////////////////////////////////
/// @brief datafile shape marker
////////////////////////////////////////////////////////////////////////////////

typedef struct TRI_df_shape_marker_s {
  TRI_df_marker_t base;
}
TRI_df_shape_marker_t;

// -----------------------------------------------------------------------------
// --SECTION--                                      constructors and destructors
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @brief creates a shaper
////////////////////////////////////////////////////////////////////////////////

TRI_shaper_t* TRI_CreateVocShaper (TRI_vocbase_t*, 
                                   struct TRI_document_collection_s*);

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
/// @brief destroys a shaper, but does not free the pointer
////////////////////////////////////////////////////////////////////////////////

int TRI_InitVocShaper (TRI_shaper_t*);

////////////////////////////////////////////////////////////////////////////////
/// @brief move a shape marker, called during compaction
////////////////////////////////////////////////////////////////////////////////

int TRI_MoveMarkerVocShaper (TRI_shaper_t*, 
                             TRI_df_marker_t*);

////////////////////////////////////////////////////////////////////////////////
/// @brief insert a shape, called when opening a collection
////////////////////////////////////////////////////////////////////////////////

int TRI_InsertShapeVocShaper (TRI_shaper_t*,
                              TRI_df_marker_t const*);

////////////////////////////////////////////////////////////////////////////////
/// @brief insert an attribute, called when opening a collection
////////////////////////////////////////////////////////////////////////////////

int TRI_InsertAttributeVocShaper (TRI_shaper_t*,
                                  TRI_df_marker_t const*);

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

int TRI_CompareShapeTypes (TRI_doc_mptr_t* leftDocument,
                           TRI_shaped_sub_t* leftObject,
                           TRI_shaped_json_t const* leftShaped,
                           TRI_doc_mptr_t* rightDocument,
                           TRI_shaped_sub_t* rightObject,
                           TRI_shaped_json_t const* rightShaped,
                           TRI_shaper_t* leftShaper,
                           TRI_shaper_t* rightShaper);
  
////////////////////////////////////////////////////////////////////////////////
/// @brief extracts the shaped JSON pointer from a marker
////////////////////////////////////////////////////////////////////////////////

static inline void TRI_EXTRACT_SHAPED_JSON_MARKER (TRI_shaped_json_t& dst,
                                                   void const* src) {
  if (((TRI_df_marker_t const*) (src))->_type == TRI_DOC_MARKER_KEY_DOCUMENT) {
    (dst)._sid = ((TRI_doc_document_key_marker_t*) (src))->_shape;
    (dst)._data.length = ((TRI_df_marker_t*) (src))->_size - ((TRI_doc_document_key_marker_t*) (src))->_offsetJson;
    (dst)._data.data = (((char*) (src)) + ((TRI_doc_document_key_marker_t*) (src))->_offsetJson);
  }
  else if (((TRI_df_marker_t const*) (src))->_type == TRI_DOC_MARKER_KEY_EDGE) {
    (dst)._sid = ((TRI_doc_document_key_marker_t*) (src))->_shape;
    (dst)._data.length = ((TRI_df_marker_t*) (src))->_size - ((TRI_doc_document_key_marker_t*) (src))->_offsetJson;
    (dst)._data.data = (((char*) (src)) + ((TRI_doc_document_key_marker_t*) (src))->_offsetJson);
  }
  else if (((TRI_df_marker_t const*) (src))->_type == TRI_WAL_MARKER_DOCUMENT) {
    (dst)._sid = ((triagens::wal::document_marker_t*) (src))->_shape;
    (dst)._data.length = ((TRI_df_marker_t*) (src))->_size - ((triagens::wal::document_marker_t*) (src))->_offsetJson;
    (dst)._data.data = (((char*) (src)) + ((triagens::wal::document_marker_t*) (src))->_offsetJson);
  }
  else if (((TRI_df_marker_t const*) (src))->_type == TRI_WAL_MARKER_EDGE) {
    (dst)._sid = ((triagens::wal::edge_marker_t*) (src))->_shape;
    (dst)._data.length = ((TRI_df_marker_t*) (src))->_size - ((triagens::wal::edge_marker_t*) (src))->_offsetJson;
    (dst)._data.data = (((char*) (src)) + ((triagens::wal::edge_marker_t*) (src))->_offsetJson);
  }
  else {
    (dst)._sid = 0;
    (dst)._data.length = 0;
    (dst)._data.data = NULL;
  }
}

#endif

// -----------------------------------------------------------------------------
// --SECTION--                                                       END-OF-FILE
// -----------------------------------------------------------------------------

// Local Variables:
// mode: outline-minor
// outline-regexp: "/// @brief\\|/// {@inheritDoc}\\|/// @addtogroup\\|/// @page\\|// --SECTION--\\|/// @\\}"
// End:
