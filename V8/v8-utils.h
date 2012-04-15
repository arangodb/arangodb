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

#ifndef TRIAGENS_V8_V8_UTILS_H
#define TRIAGENS_V8_V8_UTILS_H 1

#include "V8/v8-globals.h"

#include "BasicsC/json.h"
#include "V8/v8-c-utils.h"

// -----------------------------------------------------------------------------
// --SECTION--                                                           GENERAL
// -----------------------------------------------------------------------------

// -----------------------------------------------------------------------------
// --SECTION--                                                  public constants
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @addtogroup V8Utils
/// @{
////////////////////////////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////////////////////////
/// @brief slot for a type
////////////////////////////////////////////////////////////////////////////////

static int const SLOT_CLASS_TYPE = 0;

////////////////////////////////////////////////////////////////////////////////
/// @brief slot for a "C++ class"
////////////////////////////////////////////////////////////////////////////////

static int const SLOT_CLASS = 1;

////////////////////////////////////////////////////////////////////////////////
/// @}
////////////////////////////////////////////////////////////////////////////////

// -----------------------------------------------------------------------------
// --SECTION--                                                  public functions
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @addtogroup V8Utils
/// @{
////////////////////////////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////////////////////////
/// @brief unwraps a C++ class given a v8::Object
////////////////////////////////////////////////////////////////////////////////

template<class T>
static T* TRI_UnwrapClass (v8::Handle<v8::Object> obj, int32_t type) {
  if (obj->InternalFieldCount() <= SLOT_CLASS) {
    return 0;
  }

  if (obj->GetInternalField(SLOT_CLASS_TYPE)->Int32Value() != type) {
    return 0;
  }

  return static_cast<T*>(v8::Handle<v8::External>::Cast(obj->GetInternalField(SLOT_CLASS))->Value());
}

////////////////////////////////////////////////////////////////////////////////
/// @brief adds attributes to array
////////////////////////////////////////////////////////////////////////////////

void TRI_AugmentObject (v8::Handle<v8::Value> value, TRI_json_t const* json);

////////////////////////////////////////////////////////////////////////////////
/// @brief reports an exception
////////////////////////////////////////////////////////////////////////////////

std::string TRI_StringifyV8Exception (v8::TryCatch* tryCatch);

////////////////////////////////////////////////////////////////////////////////
/// @brief prints an exception and stacktrace
////////////////////////////////////////////////////////////////////////////////

void TRI_LogV8Exception (v8::TryCatch* tryCatch);

////////////////////////////////////////////////////////////////////////////////
/// @brief reads a file into the current context
////////////////////////////////////////////////////////////////////////////////

bool TRI_LoadJavaScriptFile (v8::Handle<v8::Context> context, char const* filename);

////////////////////////////////////////////////////////////////////////////////
/// @brief reads all files from a directory into the current context
////////////////////////////////////////////////////////////////////////////////

bool TRI_LoadJavaScriptDirectory (v8::Handle<v8::Context> context, char const* path);

////////////////////////////////////////////////////////////////////////////////
/// @brief executes a file in the current context
////////////////////////////////////////////////////////////////////////////////

bool TRI_ExecuteJavaScriptFile (v8::Handle<v8::Context> context, char const* filename);

////////////////////////////////////////////////////////////////////////////////
/// @brief executes all files from a directory in the current context
////////////////////////////////////////////////////////////////////////////////

bool TRI_ExecuteJavaScriptDirectory (v8::Handle<v8::Context> context, char const* path);

////////////////////////////////////////////////////////////////////////////////
/// @brief executes a string within a V8 context
////////////////////////////////////////////////////////////////////////////////

bool TRI_ExecuteStringVocBase (v8::Handle<v8::Context> context,
                               v8::Handle<v8::String> source,
                               v8::Handle<v8::Value> name,
                               bool printResult);

////////////////////////////////////////////////////////////////////////////////
/// @brief stores the V8 utils function inside the global variable
////////////////////////////////////////////////////////////////////////////////

void TRI_InitV8Utils (v8::Handle<v8::Context> context, std::string const& path);

////////////////////////////////////////////////////////////////////////////////
/// @}
////////////////////////////////////////////////////////////////////////////////

#endif

// Local Variables:
// mode: outline-minor
// outline-regexp: "^\\(/// @brief\\|/// {@inheritDoc}\\|/// @addtogroup\\|// --SECTION--\\|/// @\\}\\)"
// End:
