////////////////////////////////////////////////////////////////////////////////
/// @brief V8 utility functions
///
/// @file
///
/// DISCLAIMER
///
/// Copyright 2004-2012 triagens GmbH, Cologne, Germany
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
/// @author Copyright 2011-2012, triAGENS GmbH, Cologne, Germany
////////////////////////////////////////////////////////////////////////////////

#ifndef TRIAGENS_V8_V8_OBJECTS_H
#define TRIAGENS_V8_V8_OBJECTS_H 1

#include "V8/v8-globals.h"

#include "BasicsC/json.h"
#include "VocBase/vocbase.h"

// -----------------------------------------------------------------------------
// --SECTION--                                              CONVERSION FUNCTIONS
// -----------------------------------------------------------------------------

// -----------------------------------------------------------------------------
// --SECTION--                                                  public functions
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @addtogroup V8Conversions
/// @{
////////////////////////////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////////////////////////
/// @brief converts identifier into a object reference
////////////////////////////////////////////////////////////////////////////////

v8::Handle<v8::Value> TRI_ObjectReference (TRI_voc_cid_t, TRI_voc_did_t);

////////////////////////////////////////////////////////////////////////////////
/// @brief extratcs identifiers from a object reference
////////////////////////////////////////////////////////////////////////////////

bool TRI_IdentifiersObjectReference (v8::Handle<v8::Value>, TRI_voc_cid_t&, TRI_voc_did_t&);

////////////////////////////////////////////////////////////////////////////////
/// @}
////////////////////////////////////////////////////////////////////////////////

// -----------------------------------------------------------------------------
// --SECTION--                                                           GENERAL
// -----------------------------------------------------------------------------

// -----------------------------------------------------------------------------
// --SECTION--                                                  public functions
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @addtogroup V8Conversions
/// @{
////////////////////////////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////////////////////////
/// @brief initialises the V8 conversion module
////////////////////////////////////////////////////////////////////////////////

void TRI_InitV8Conversions (v8::Handle<v8::Context>);

////////////////////////////////////////////////////////////////////////////////
/// @}
////////////////////////////////////////////////////////////////////////////////

#endif

// Local Variables:
// mode: outline-minor
// outline-regexp: "^\\(/// @brief\\|/// {@inheritDoc}\\|/// @addtogroup\\|// --SECTION--\\|/// @\\}\\)"
// End:
