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

#ifndef ARANGOD_VOC_BASE_SHAPE_ACCESSOR_H
#define ARANGOD_VOC_BASE_SHAPE_ACCESSOR_H 1

#include "Basics/Common.h"
#include "Basics/json.h"
#include "VocBase/shaped-json.h"

class VocShaper;

////////////////////////////////////////////////////////////////////////////////
/// @brief json shape access
////////////////////////////////////////////////////////////////////////////////

typedef struct TRI_shape_access_s {
  TRI_shape_sid_t _sid;  // shaped identifier of the shape we are looking at
  TRI_shape_pid_t _pid;  // path identifier of the attribute path

  TRI_shape_sid_t _resultSid;  // resulting shape
  void const** _code;          // bytecode
} TRI_shape_access_t;

////////////////////////////////////////////////////////////////////////////////
/// @brief free a shape accessor
////////////////////////////////////////////////////////////////////////////////

void TRI_FreeShapeAccessor(TRI_shape_access_t*);

////////////////////////////////////////////////////////////////////////////////
/// @brief creates a shape accessor
////////////////////////////////////////////////////////////////////////////////

TRI_shape_access_t* TRI_ShapeAccessor(VocShaper* shaper, TRI_shape_sid_t sid,
                                      TRI_shape_pid_t pid);

////////////////////////////////////////////////////////////////////////////////
/// @brief executes a shape accessor
////////////////////////////////////////////////////////////////////////////////

bool TRI_ExecuteShapeAccessor(TRI_shape_access_t const* accessor,
                              TRI_shaped_json_t const* shaped,
                              TRI_shaped_json_t* result);

////////////////////////////////////////////////////////////////////////////////
/// @brief prints a TRI_shape_t for debugging
////////////////////////////////////////////////////////////////////////////////

void TRI_PrintShapeAccessor(TRI_shape_access_t*);

#endif
