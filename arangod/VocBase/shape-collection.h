////////////////////////////////////////////////////////////////////////////////
/// @brief (binary) shape collection
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
/// @author Copyright 2011-2013, triAGENS GmbH, Cologne, Germany
////////////////////////////////////////////////////////////////////////////////

#ifndef TRIAGENS_VOC_BASE_SHAPE_COLLECTION_H
#define TRIAGENS_VOC_BASE_SHAPE_COLLECTION_H 1

#include "BasicsC/common.h"

#include "VocBase/vocbase.h"
#include "VocBase/collection.h"

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
/// @brief shape collection
///
/// A shape collection is a collection of binary shapes. There is no versioning
/// or relationship between the shapes. The data is directly synced to disks.
/// Therefore no special management thread is needed. It is not possible to
/// delete entries, once they are created. The only query supported is a
/// full scan.
///
/// Calls to @ref TRI_WriteShapeCollection are synchronised using the _lock
/// of a shape collection.
////////////////////////////////////////////////////////////////////////////////

typedef struct TRI_shape_collection_s {
  TRI_collection_t base;

  TRI_mutex_t _lock;
}
TRI_shape_collection_t;

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
/// @brief creates a new collection
////////////////////////////////////////////////////////////////////////////////

TRI_shape_collection_t* TRI_CreateShapeCollection (TRI_vocbase_t*,
                                                   char const* path,
                                                   TRI_col_info_t* parameter);

////////////////////////////////////////////////////////////////////////////////
/// @brief frees the memory allocated, but does not free the pointer
///
/// Note that the collection must be closed first.
////////////////////////////////////////////////////////////////////////////////

void TRI_DestroyShapeCollection (TRI_shape_collection_t*);

////////////////////////////////////////////////////////////////////////////////
/// @brief frees the memory allocated and frees the pointer
////////////////////////////////////////////////////////////////////////////////

void TRI_FreeShapeCollection (TRI_shape_collection_t* collection);

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
/// @brief writes an element splitted into marker and body to file
////////////////////////////////////////////////////////////////////////////////

int TRI_WriteShapeCollection (TRI_shape_collection_t* collection,
                              TRI_df_marker_t* marker,
                              TRI_voc_size_t markerSize,
                              void const* body,
                              TRI_voc_size_t bodySize,
                              TRI_df_marker_t** result);

////////////////////////////////////////////////////////////////////////////////
/// @brief opens an existing collection
////////////////////////////////////////////////////////////////////////////////

TRI_shape_collection_t* TRI_OpenShapeCollection (TRI_vocbase_t*,
                                                 char const* path);

////////////////////////////////////////////////////////////////////////////////
/// @brief closes a collection
////////////////////////////////////////////////////////////////////////////////

bool TRI_CloseShapeCollection (TRI_shape_collection_t* collection);

////////////////////////////////////////////////////////////////////////////////
/// @}
////////////////////////////////////////////////////////////////////////////////

#ifdef __cplusplus
}
#endif

#endif

// Local Variables:
// mode: outline-minor
// outline-regexp: "/// @brief\\|/// {@inheritDoc}\\|/// @addtogroup\\|/// @page\\|// --SECTION--\\|/// @\\}"
// End:

