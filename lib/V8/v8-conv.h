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

#ifndef ARANGODB_V8_V8__CONV_H
#define ARANGODB_V8_V8__CONV_H 1

#include "Basics/Common.h"

#include "V8/v8-globals.h"

#include "Basics/json.h"
#include "ShapedJson/json-shaper.h"

// -----------------------------------------------------------------------------
// --SECTION--                                              CONVERSION FUNCTIONS
// -----------------------------------------------------------------------------

// -----------------------------------------------------------------------------
// --SECTION--                                                  public functions
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @brief pushes the names of an associative char* array into a V8 array
////////////////////////////////////////////////////////////////////////////////

v8::Handle<v8::Array> TRI_ArrayAssociativePointer (TRI_associative_pointer_t const*);

////////////////////////////////////////////////////////////////////////////////
/// @brief converts a TRI_json_t into a V8 object
////////////////////////////////////////////////////////////////////////////////

v8::Handle<v8::Value> TRI_ObjectJson (TRI_json_t const*);

////////////////////////////////////////////////////////////////////////////////
/// @brief converts a TRI_shaped_json_t into an existing V8 object
////////////////////////////////////////////////////////////////////////////////

v8::Handle<v8::Value> TRI_JsonShapeData (v8::Handle<v8::Value>,
                                         TRI_shaper_t*,
                                         TRI_shape_t const*,
                                         char const* data,
                                         size_t size);

////////////////////////////////////////////////////////////////////////////////
/// @brief converts a TRI_shaped_json_t into a new V8 object
////////////////////////////////////////////////////////////////////////////////

v8::Handle<v8::Value> TRI_JsonShapeData (TRI_shaper_t*,
                                         TRI_shape_t const*,
                                         char const* data,
                                         size_t size);

////////////////////////////////////////////////////////////////////////////////
/// @brief converts an V8 object to a TRI_shaped_json_t
////////////////////////////////////////////////////////////////////////////////

TRI_shaped_json_t* TRI_ShapedJsonV8Object (v8::Handle<v8::Value> const,
                                           TRI_shaper_t*,
                                           bool);

////////////////////////////////////////////////////////////////////////////////
/// @brief converts a V8 object to a TRI_shaped_json_t in place
////////////////////////////////////////////////////////////////////////////////

int TRI_FillShapedJsonV8Object (v8::Handle<v8::Value> const,
                                TRI_shaped_json_t*,
                                TRI_shaper_t*,
                                bool);

////////////////////////////////////////////////////////////////////////////////
/// @brief convert a V8 value to a json_t value
////////////////////////////////////////////////////////////////////////////////

TRI_json_t* TRI_ObjectToJson (v8::Handle<v8::Value> const);

////////////////////////////////////////////////////////////////////////////////
/// @brief convert a V8 object to a json_t value
/// this function makes some assumption about the v8 object to achieve
/// substantial speedups (in comparison to TRI_ObjectToJson)
/// use for ShapedJson v8 objects only or for lists of ShapedJson v8 objects!
////////////////////////////////////////////////////////////////////////////////

TRI_json_t* TRI_ShapedJsonToJson (v8::Handle<v8::Value> const);

////////////////////////////////////////////////////////////////////////////////
/// @brief converts an V8 object to a string
////////////////////////////////////////////////////////////////////////////////

std::string TRI_ObjectToString (v8::Handle<v8::Value> const);

////////////////////////////////////////////////////////////////////////////////
/// @brief converts an V8 object to a character
////////////////////////////////////////////////////////////////////////////////

char TRI_ObjectToCharacter (v8::Handle<v8::Value> const,
                            bool& error);

////////////////////////////////////////////////////////////////////////////////
/// @brief converts an V8 object to an int64_t
////////////////////////////////////////////////////////////////////////////////

int64_t TRI_ObjectToInt64 (v8::Handle<v8::Value> const);

////////////////////////////////////////////////////////////////////////////////
/// @brief converts an V8 object to a uint64_t
////////////////////////////////////////////////////////////////////////////////

uint64_t TRI_ObjectToUInt64 (v8::Handle<v8::Value> const,
                             const bool);

////////////////////////////////////////////////////////////////////////////////
/// @brief converts a V8 object to a double
////////////////////////////////////////////////////////////////////////////////

double TRI_ObjectToDouble (v8::Handle<v8::Value> const);

////////////////////////////////////////////////////////////////////////////////
/// @brief converts a V8 object to a double with error handling
////////////////////////////////////////////////////////////////////////////////

double TRI_ObjectToDouble (v8::Handle<v8::Value> const,
                           bool& error);

////////////////////////////////////////////////////////////////////////////////
/// @brief converts a V8 object to a boolean
////////////////////////////////////////////////////////////////////////////////

bool TRI_ObjectToBoolean (v8::Handle<v8::Value> const);

// -----------------------------------------------------------------------------
// --SECTION--                                                           GENERAL
// -----------------------------------------------------------------------------

// -----------------------------------------------------------------------------
// --SECTION--                                                  public functions
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @brief initialises the V8 conversion module
////////////////////////////////////////////////////////////////////////////////

void TRI_InitV8Conversions (v8::Handle<v8::Context>);

#endif

// -----------------------------------------------------------------------------
// --SECTION--                                                       END-OF-FILE
// -----------------------------------------------------------------------------

// Local Variables:
// mode: outline-minor
// outline-regexp: "/// @brief\\|/// {@inheritDoc}\\|/// @page\\|// --SECTION--\\|/// @\\}"
// End:
