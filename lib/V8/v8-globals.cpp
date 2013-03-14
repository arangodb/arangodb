////////////////////////////////////////////////////////////////////////////////
/// @brief V8 globals
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
/// @author Jan Steemann
/// @author Copyright 2012-2013, triAGENS GmbH, Cologne, Germany
////////////////////////////////////////////////////////////////////////////////

#include "v8-globals.h"

// -----------------------------------------------------------------------------
// --SECTION--                                                  GLOBAL FUNCTIONS
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @addtogroup V8Globals
/// @{
////////////////////////////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////////////////////////
/// @brief add a method to a prototype object
////////////////////////////////////////////////////////////////////////////////

void TRI_AddProtoMethodVocbase (v8::Handle<v8::Template> tpl,
                                const char* const name,
                                v8::Handle<v8::Value>(*func)(v8::Arguments const&),
                                const bool isHidden) {
  if (isHidden) {
    // hidden method
    tpl->Set(TRI_V8_SYMBOL(name), v8::FunctionTemplate::New(func), v8::DontEnum);
  }
  else {
    // normal method
    tpl->Set(TRI_V8_SYMBOL(name), v8::FunctionTemplate::New(func));
  }
}

////////////////////////////////////////////////////////////////////////////////
/// @brief add a method to an object
////////////////////////////////////////////////////////////////////////////////

void TRI_AddMethodVocbase (v8::Handle<v8::ObjectTemplate> tpl,
                           const char* const name,
                           v8::Handle<v8::Value>(*func)(v8::Arguments const&),
                           const bool isHidden) {
  if (isHidden) {
    // hidden method
    tpl->Set(TRI_V8_SYMBOL(name), v8::FunctionTemplate::New(func), v8::DontEnum);
  }
  else {
    // normal method
    tpl->Set(TRI_V8_SYMBOL(name), v8::FunctionTemplate::New(func));
  }
}

////////////////////////////////////////////////////////////////////////////////
/// @brief add a global function to the given context
////////////////////////////////////////////////////////////////////////////////

void TRI_AddGlobalFunctionVocbase (v8::Handle<v8::Context> context,
                                   const char* const name,
                                   v8::Handle<v8::Value>(*func)(v8::Arguments const&)) {
  // all global functions are read-only
  context->Global()->Set(TRI_V8_SYMBOL(name), v8::FunctionTemplate::New(func)->GetFunction(), v8::ReadOnly);
}

////////////////////////////////////////////////////////////////////////////////
/// @brief add a global function to the given context
////////////////////////////////////////////////////////////////////////////////

void TRI_AddGlobalFunctionVocbase (v8::Handle<v8::Context> context,
                                   const char* const name,
                                   v8::Handle<v8::Function> func) {
  // all global functions are read-only
  context->Global()->Set(TRI_V8_SYMBOL(name), func, v8::ReadOnly);
}

////////////////////////////////////////////////////////////////////////////////
/// @}
////////////////////////////////////////////////////////////////////////////////

// Local Variables:
// mode: outline-minor
// outline-regexp: "/// @brief\\|/// {@inheritDoc}\\|/// @addtogroup\\|/// @page\\|// --SECTION--\\|/// @\\}"
// End:
