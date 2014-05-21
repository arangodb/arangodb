////////////////////////////////////////////////////////////////////////////////
/// @brief V8 utility functions
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

#include "v8-execution.h"

#include "V8/v8-conv.h"

using namespace std;

// -----------------------------------------------------------------------------
// --SECTION--                                                 EXECUTION CONTEXT
// -----------------------------------------------------------------------------

// -----------------------------------------------------------------------------
// --SECTION--                                                      public types
// -----------------------------------------------------------------------------

// -----------------------------------------------------------------------------
// --SECTION--                                      constructors and destructors
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @brief creates a new execution context
////////////////////////////////////////////////////////////////////////////////

TRI_js_exec_context_t* TRI_CreateExecutionContext (char const* script,
                                                  size_t length) {
  v8::Isolate* isolate = v8::Isolate::GetCurrent();
  TRI_js_exec_context_t* ctx = new TRI_js_exec_context_t;
  ctx->_error = TRI_ERROR_NO_ERROR;

  // execute script inside the context
  v8::Handle<v8::Script> compiled = v8::Script::Compile(v8::String::New(script, (int) length),
                                                        v8::String::New("--script--"));

  // compilation failed, return
  if (compiled.IsEmpty()) {
    ctx->_error = TRI_ERROR_INTERNAL; 
    return ctx;
  }

  // compute the function
  v8::TryCatch tryCatch;
  v8::Handle<v8::Value> val = compiled->Run();

  if (tryCatch.HasCaught()) {
    if (tryCatch.CanContinue()) {
      ctx->_error = TRI_ERROR_INTERNAL; 
    }
    else {
      ctx->_error = TRI_ERROR_REQUEST_CANCELED; 
    }
    return ctx;
  }

  if (val.IsEmpty()) {
    ctx->_error = TRI_ERROR_INTERNAL; 
    return ctx;
  }

  ctx->_func = v8::Persistent<v8::Function>::New(isolate, v8::Handle<v8::Function>::Cast(val));
  ctx->_arguments = v8::Persistent<v8::Object>::New(isolate, v8::Object::New());
  ctx->_isolate = isolate;
  ctx->_error = TRI_ERROR_NO_ERROR;

  // return the handle
  return ctx;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief frees an new execution context
////////////////////////////////////////////////////////////////////////////////

void TRI_FreeExecutionContext (TRI_js_exec_context_t* ctx) {
  if (ctx->_error == TRI_ERROR_NO_ERROR) {
    ctx->_func.Dispose(ctx->_isolate);
    ctx->_func.Clear();

    ctx->_arguments.Dispose(ctx->_isolate);
    ctx->_arguments.Clear();
  }

  delete ctx;
}

// -----------------------------------------------------------------------------
// --SECTION--                                                  public functions
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @brief executes a result context
////////////////////////////////////////////////////////////////////////////////

TRI_json_t* TRI_ExecuteResultContext (TRI_js_exec_context_t* ctx) {
  assert(ctx->_error == TRI_ERROR_NO_ERROR);

  // convert back into a handle
  v8::Persistent<v8::Function> func = ctx->_func;

  v8::TryCatch tryCatch;

  // and execute the function
  v8::Handle<v8::Value> args[] = { ctx->_arguments };
  v8::Handle<v8::Value> result = func->Call(func, 1, args);

  if (tryCatch.HasCaught()) {
    if (tryCatch.CanContinue()) {
      ctx->_error = TRI_ERROR_INTERNAL;
    }
    else {
      ctx->_error = TRI_ERROR_REQUEST_CANCELED;
    }
    return NULL;
  }

  if (result.IsEmpty()) {
    ctx->_error = TRI_ERROR_INTERNAL;
    return NULL;
  }

  return TRI_ObjectToJson(result);
}

// -----------------------------------------------------------------------------
// --SECTION--                                                       END-OF-FILE
// -----------------------------------------------------------------------------

// Local Variables:
// mode: outline-minor
// outline-regexp: "/// @brief\\|/// {@inheritDoc}\\|/// @addtogroup\\|/// @page\\|// --SECTION--\\|/// @\\}"
// End:
