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
// --SECTION--                                                      public types
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @brief constructor
////////////////////////////////////////////////////////////////////////////////

TRI_v8_global_s::TRI_v8_global_s (v8::Isolate* isolate)
  : JSBarriers(),

    ErrorTempl(),
    GeneralCursorTempl(),
    ShapedJsonTempl(),
    TransactionTempl(),
    VocbaseColTempl(),
    VocbaseTempl(),

    BufferTempl(),
    FastBufferConstructor(),

    BufferConstant(),
    DeleteConstant(),
    GetConstant(),
    HeadConstant(),
    OptionsConstant(),
    PatchConstant(),
    PostConstant(),
    PutConstant(),

    AddressKey(),
    BodyFromFileKey(),
    BodyKey(),
    ClientKey(),
    CodeKey(),
    ContentTypeKey(),
    DomainKey(),
    ErrorKey(),
    ErrorMessageKey(),
    ErrorNumKey(),
    HeadersKey(),
    HttpOnlyKey(),
    IdKey(),
    IsSystemKey(),
    IsVolatileKey(),
    JournalSizeKey(),
    KeyOptionsKey(),
    LengthKey(),
    LifeTimeKey(),
    NameKey(),
    ParametersKey(),
    PathKey(),
    PrefixKey(),
    PortKey(),
    ProtocolKey(),
    RequestBodyKey(),
    RequestTypeKey(),
    ResponseCodeKey(),
    SecureKey(),
    ServerKey(),
    SuffixKey(),
    TransformationsKey(),
    UrlKey(),
    UserKey(),
    ValueKey(),
    WaitForSyncKey(),

    _FromKey(),
    _IdKey(),
    _KeyKey(),
    _OldRevKey(),
    _RevKey(),
    _ToKey(),

    DocumentIdRegex(),
    DocumentKeyRegex(),
    IdRegex(),
    IndexIdRegex(),

    _currentTransaction(0),
    _vocbase(0) {
  v8::HandleScope scope;

  BufferConstant = v8::Persistent<v8::String>::New(isolate, TRI_V8_SYMBOL("Buffer"));
  DeleteConstant = v8::Persistent<v8::String>::New(isolate, TRI_V8_SYMBOL("DELETE"));
  GetConstant = v8::Persistent<v8::String>::New(isolate, TRI_V8_SYMBOL("GET"));
  HeadConstant = v8::Persistent<v8::String>::New(isolate, TRI_V8_SYMBOL("HEAD"));
  OptionsConstant = v8::Persistent<v8::String>::New(isolate, TRI_V8_SYMBOL("OPTIONS"));
  PatchConstant = v8::Persistent<v8::String>::New(isolate, TRI_V8_SYMBOL("PATCH"));
  PostConstant = v8::Persistent<v8::String>::New(isolate, TRI_V8_SYMBOL("POST"));
  PutConstant = v8::Persistent<v8::String>::New(isolate, TRI_V8_SYMBOL("PUT"));
  
  AddressKey = v8::Persistent<v8::String>::New(isolate, TRI_V8_SYMBOL("address"));
  BodyFromFileKey = v8::Persistent<v8::String>::New(isolate, TRI_V8_SYMBOL("bodyFromFile"));
  BodyKey = v8::Persistent<v8::String>::New(isolate, TRI_V8_SYMBOL("body"));
  ClientKey = v8::Persistent<v8::String>::New(isolate, TRI_V8_SYMBOL("client"));
  CodeKey = v8::Persistent<v8::String>::New(isolate, TRI_V8_SYMBOL("code"));
  ContentTypeKey = v8::Persistent<v8::String>::New(isolate, TRI_V8_SYMBOL("contentType"));
  CookiesKey = v8::Persistent<v8::String>::New(isolate, TRI_V8_SYMBOL("cookies"));
  DomainKey = v8::Persistent<v8::String>::New(isolate, TRI_V8_SYMBOL("domain"));
  ErrorKey = v8::Persistent<v8::String>::New(isolate, TRI_V8_SYMBOL("error"));
  ErrorMessageKey = v8::Persistent<v8::String>::New(isolate, TRI_V8_SYMBOL("errorMessage"));
  ErrorNumKey = v8::Persistent<v8::String>::New(isolate, TRI_V8_SYMBOL("errorNum"));
  HeadersKey = v8::Persistent<v8::String>::New(isolate, TRI_V8_SYMBOL("headers"));
  HttpOnlyKey = v8::Persistent<v8::String>::New(isolate, TRI_V8_SYMBOL("httpOnly"));
  IdKey = v8::Persistent<v8::String>::New(isolate, TRI_V8_SYMBOL("id"));
  IsSystemKey = v8::Persistent<v8::String>::New(isolate, TRI_V8_SYMBOL("isSystem"));
  IsVolatileKey = v8::Persistent<v8::String>::New(isolate, TRI_V8_SYMBOL("isVolatile"));
  JournalSizeKey = v8::Persistent<v8::String>::New(isolate, TRI_V8_SYMBOL("journalSize"));
  KeyOptionsKey = v8::Persistent<v8::String>::New(isolate, TRI_V8_SYMBOL("keyOptions"));
  LengthKey = v8::Persistent<v8::String>::New(isolate, TRI_V8_SYMBOL("length"));
  LifeTimeKey = v8::Persistent<v8::String>::New(isolate, TRI_V8_SYMBOL("lifeTime"));
  NameKey = v8::Persistent<v8::String>::New(isolate, TRI_V8_SYMBOL("name"));
  ParametersKey = v8::Persistent<v8::String>::New(isolate, TRI_V8_SYMBOL("parameters"));
  PathKey = v8::Persistent<v8::String>::New(isolate, TRI_V8_SYMBOL("path"));
  PrefixKey = v8::Persistent<v8::String>::New(isolate, TRI_V8_SYMBOL("prefix"));
  PortKey = v8::Persistent<v8::String>::New(isolate, TRI_V8_SYMBOL("port"));
  ProtocolKey = v8::Persistent<v8::String>::New(isolate, TRI_V8_SYMBOL("protocol"));
  RequestBodyKey = v8::Persistent<v8::String>::New(isolate, TRI_V8_SYMBOL("requestBody"));
  RequestTypeKey = v8::Persistent<v8::String>::New(isolate, TRI_V8_SYMBOL("requestType"));
  ResponseCodeKey = v8::Persistent<v8::String>::New(isolate, TRI_V8_SYMBOL("responseCode"));
  SecureKey = v8::Persistent<v8::String>::New(isolate, TRI_V8_SYMBOL("secure"));
  ServerKey = v8::Persistent<v8::String>::New(isolate, TRI_V8_SYMBOL("server"));
  SuffixKey = v8::Persistent<v8::String>::New(isolate, TRI_V8_SYMBOL("suffix"));
  TransformationsKey = v8::Persistent<v8::String>::New(isolate, TRI_V8_SYMBOL("transformations"));
  UrlKey = v8::Persistent<v8::String>::New(isolate, TRI_V8_SYMBOL("url"));
  UserKey = v8::Persistent<v8::String>::New(isolate, TRI_V8_SYMBOL("user"));
  ValueKey = v8::Persistent<v8::String>::New(isolate, TRI_V8_SYMBOL("value"));
  WaitForSyncKey = v8::Persistent<v8::String>::New(isolate, TRI_V8_SYMBOL("waitForSync"));
  
  _FromKey = v8::Persistent<v8::String>::New(isolate, TRI_V8_SYMBOL("_from"));
  _IdKey = v8::Persistent<v8::String>::New(isolate, TRI_V8_SYMBOL("_id"));
  _KeyKey = v8::Persistent<v8::String>::New(isolate, TRI_V8_SYMBOL("_key"));
  _OldRevKey = v8::Persistent<v8::String>::New(isolate, TRI_V8_SYMBOL("_oldRev"));
  _RevKey = v8::Persistent<v8::String>::New(isolate, TRI_V8_SYMBOL("_rev"));
  _ToKey = v8::Persistent<v8::String>::New(isolate, TRI_V8_SYMBOL("_to"));
}

////////////////////////////////////////////////////////////////////////////////
/// @brief destructor
////////////////////////////////////////////////////////////////////////////////

TRI_v8_global_s::~TRI_v8_global_s () {
  regfree(&DocumentIdRegex);
  regfree(&DocumentKeyRegex);
  regfree(&IndexIdRegex);
  regfree(&IdRegex);
}

// -----------------------------------------------------------------------------
// --SECTION--                                                  GLOBAL FUNCTIONS
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @brief creates a global context
////////////////////////////////////////////////////////////////////////////////

TRI_v8_global_t* TRI_CreateV8Globals(v8::Isolate* isolate) {
  TRI_v8_global_t* v8g = (TRI_v8_global_t*) isolate->GetData();

  if (v8g == 0) {
    v8g = new TRI_v8_global_t(isolate);
    isolate->SetData(v8g);
  }

  return v8g;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief adds a method to an object
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
/// @brief adds a global function to the given context
////////////////////////////////////////////////////////////////////////////////

void TRI_AddGlobalFunctionVocbase (v8::Handle<v8::Context> context,
                                   const char* const name,
                                   v8::Handle<v8::Value>(*func)(v8::Arguments const&)) {
  // all global functions are read-only
  context->Global()->Set(TRI_V8_SYMBOL(name), v8::FunctionTemplate::New(func)->GetFunction(), v8::ReadOnly);
}

////////////////////////////////////////////////////////////////////////////////
/// @brief adds a global function to the given context
////////////////////////////////////////////////////////////////////////////////

void TRI_AddGlobalFunctionVocbase (v8::Handle<v8::Context> context,
                                   const char* const name,
                                   v8::Handle<v8::Function> func) {
  // all global functions are read-only
  context->Global()->Set(TRI_V8_SYMBOL(name), func, v8::ReadOnly);
}

////////////////////////////////////////////////////////////////////////////////
/// @brief adds a global variable to the given context
////////////////////////////////////////////////////////////////////////////////

void TRI_AddGlobalVariableVocbase (v8::Handle<v8::Context> context,
                                   const char* const name,
                                   v8::Handle<v8::Value> value) {
  // all global functions are read-only
  context->Global()->Set(TRI_V8_SYMBOL(name), value, v8::ReadOnly);
}

// -----------------------------------------------------------------------------
// --SECTION--                                                       END-OF-FILE
// -----------------------------------------------------------------------------

// Local Variables:
// mode: outline-minor
// outline-regexp: "/// @brief\\|/// {@inheritDoc}\\|/// @addtogroup\\|/// @page\\|// --SECTION--\\|/// @\\}"
// End:
