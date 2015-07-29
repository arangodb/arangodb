////////////////////////////////////////////////////////////////////////////////
/// @brief V8 utility functions
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

#ifndef ARANGODB_V8_SERVER_V8_SHAPE_CONV_H
#define ARANGODB_V8_SERVER_V8_SHAPE_CONV_H 1

#include "Basics/Common.h"
#include "Basics/json.h"
#include "V8/v8-globals.h"
#include "VocBase/shaped-json.h"

class VocShaper;

// -----------------------------------------------------------------------------
// --SECTION--                                              CONVERSION FUNCTIONS
// -----------------------------------------------------------------------------

// -----------------------------------------------------------------------------
// --SECTION--                                                  public functions
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @brief converts a TRI_shaped_json_t into an existing V8 object
////////////////////////////////////////////////////////////////////////////////

v8::Handle<v8::Value> TRI_JsonShapeData (v8::Isolate* isolate,
                                         v8::Handle<v8::Value>,
                                         VocShaper*,
                                         TRI_shape_t const*,
                                         char const* data,
                                         size_t size);

////////////////////////////////////////////////////////////////////////////////
/// @brief converts a TRI_shaped_json_t into a new V8 object
////////////////////////////////////////////////////////////////////////////////

v8::Handle<v8::Value> TRI_JsonShapeData (v8::Isolate* isolate,
                                         VocShaper*,
                                         TRI_shape_t const*,
                                         char const* data,
                                         size_t size);

////////////////////////////////////////////////////////////////////////////////
/// @brief converts an V8 object to a TRI_shaped_json_t
////////////////////////////////////////////////////////////////////////////////

TRI_shaped_json_t* TRI_ShapedJsonV8Object (v8::Isolate* isolate,
                                           v8::Handle<v8::Value> const,
                                           VocShaper*,
                                           bool);

////////////////////////////////////////////////////////////////////////////////
/// @brief converts a V8 object to a TRI_shaped_json_t in place
////////////////////////////////////////////////////////////////////////////////

int TRI_FillShapedJsonV8Object (v8::Isolate* isolate,
                                v8::Handle<v8::Value> const,
                                TRI_shaped_json_t*,
                                VocShaper*,
                                bool);

#endif

// -----------------------------------------------------------------------------
// --SECTION--                                                       END-OF-FILE
// -----------------------------------------------------------------------------

// Local Variables:
// mode: outline-minor
// outline-regexp: "/// @brief\\|/// {@inheritDoc}\\|/// @page\\|// --SECTION--\\|/// @\\}"
// End:
