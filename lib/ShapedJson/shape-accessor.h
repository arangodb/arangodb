////////////////////////////////////////////////////////////////////////////////
/// @brief shape accessor
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

#ifndef TRIAGENS_SHAPED_JSON_SHAPE_ACCESSOR_H
#define TRIAGENS_SHAPED_JSON_SHAPE_ACCESSOR_H 1

#include "BasicsC/common.h"

#include "BasicsC/json.h"
#include "ShapedJson/shaped-json.h"

#ifdef __cplusplus
extern "C" {
#endif

// -----------------------------------------------------------------------------
// --SECTION--                                                      public types
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @addtogroup Json
/// @{
////////////////////////////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////////////////////////
/// @brief json shape access
////////////////////////////////////////////////////////////////////////////////

typedef struct TRI_shape_access_s {
  TRI_shape_sid_t _sid;                 // shaped identifier of the shape we are looking at
  TRI_shape_pid_t _pid;                 // path identifier of the attribute path

  TRI_shape_t const* _shape;            // resulting shape
  void* const* _code;                   // bytecode

  TRI_memory_zone_t* _memoryZone;
}
TRI_shape_access_t;

////////////////////////////////////////////////////////////////////////////////
/// @brief size of short strings
////////////////////////////////////////////////////////////////////////////////

typedef enum {
  TRI_SHAPE_AC_DONE = 1,
  TRI_SHAPE_AC_SHAPE_PTR = 2,
  TRI_SHAPE_AC_OFFSET_FIX = 3,
  TRI_SHAPE_AC_OFFSET_VAR = 4
}
TRI_shape_ac_bc_e;

////////////////////////////////////////////////////////////////////////////////
/// @}
////////////////////////////////////////////////////////////////////////////////

// -----------------------------------------------------------------------------
// --SECTION--                                                  public functions
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @addtogroup Json
/// @{
////////////////////////////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////////////////////////
/// @brief free a shape accessor
////////////////////////////////////////////////////////////////////////////////

void TRI_FreeShapeAccessor (TRI_shape_access_t*);

////////////////////////////////////////////////////////////////////////////////
/// @brief creates a shape accessor
////////////////////////////////////////////////////////////////////////////////

TRI_shape_access_t* TRI_ShapeAccessor (struct TRI_shaper_s* shaper,
                                       TRI_shape_sid_t sid,
                                       TRI_shape_pid_t pid);

////////////////////////////////////////////////////////////////////////////////
/// @brief executes a shape accessor
////////////////////////////////////////////////////////////////////////////////

bool TRI_ExecuteShapeAccessor (TRI_shape_access_t const* accessor,
                               TRI_shaped_json_t const* shaped,
                               TRI_shaped_json_t* result);

////////////////////////////////////////////////////////////////////////////////
/// @brief prints a TRI_shape_t for debugging
////////////////////////////////////////////////////////////////////////////////

void TRI_PrintShapeAccessor (TRI_shape_access_t*);

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
