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

#include "BasicsC/common.h"

#include "ShapedJson/json-shaper.h"
#include "ShapedJson/shape-accessor.h"
#include "ShapedJson/shaped-json.h"
#include "VocBase/shape-collection.h"
#include "VocBase/datafile.h"

#ifdef __cplusplus
extern "C" {
#endif

// -----------------------------------------------------------------------------
// --SECTION--                                                      public types
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @addtogroup VocBase
/// @{
////////////////////////////////////////////////////////////////////////////////

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
/// @brief creates persistent shaper
////////////////////////////////////////////////////////////////////////////////

TRI_shaper_t* TRI_CreateVocShaper (TRI_vocbase_t*,
                                   char const* path,
                                   char const* name,
                                   const bool waitForSync,
                                   const bool isVolatile);

////////////////////////////////////////////////////////////////////////////////
/// @brief destroys an persistent shaper, but does not free the pointer
////////////////////////////////////////////////////////////////////////////////

void TRI_DestroyVocShaper (TRI_shaper_t*);

////////////////////////////////////////////////////////////////////////////////
/// @brief destroys an persistent shaper and frees the pointer
////////////////////////////////////////////////////////////////////////////////

void TRI_FreeVocShaper (TRI_shaper_t*);

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
/// @brief returns the underlying collection
////////////////////////////////////////////////////////////////////////////////

TRI_shape_collection_t* TRI_CollectionVocShaper (TRI_shaper_t*);

////////////////////////////////////////////////////////////////////////////////
/// @brief opens a persistent shaper
////////////////////////////////////////////////////////////////////////////////

TRI_shaper_t* TRI_OpenVocShaper (TRI_vocbase_t*,
                                 char const* filename);

////////////////////////////////////////////////////////////////////////////////
/// @brief closes a persistent shaper
////////////////////////////////////////////////////////////////////////////////

int TRI_CloseVocShaper (TRI_shaper_t*);

////////////////////////////////////////////////////////////////////////////////
/// @brief closes a persistent shaper
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
/// @}
////////////////////////////////////////////////////////////////////////////////

#ifdef __cplusplus
}
#endif

#endif

// -----------------------------------------------------------------------------
// --SECTION--                                                       END-OF-FILE
// -----------------------------------------------------------------------------

// Local Variables:
// mode: outline-minor
// outline-regexp: "/// @brief\\|/// {@inheritDoc}\\|/// @addtogroup\\|/// @page\\|// --SECTION--\\|/// @\\}"
// End:
