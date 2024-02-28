////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2014-2024 ArangoDB GmbH, Cologne, Germany
/// Copyright 2004-2014 triAGENS GmbH, Cologne, Germany
///
/// Licensed under the Business Source License 1.1 (the "License");
/// you may not use this file except in compliance with the License.
/// You may obtain a copy of the License at
///
///     https://github.com/arangodb/arangodb/blob/devel/LICENSE
///
/// Unless required by applicable law or agreed to in writing, software
/// distributed under the License is distributed on an "AS IS" BASIS,
/// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
/// See the License for the specific language governing permissions and
/// limitations under the License.
///
/// Copyright holder is ArangoDB GmbH, Cologne, Germany
///
/// @author Wilfried Goesgens
////////////////////////////////////////////////////////////////////////////////

#ifndef USE_V8
#error this file is not supposed to be used in builds with -DUSE_V8=Off
#endif

#include <stdlib.h>
#include <string.h>
#include <cstdint>
#include <string>

#include <v8.h>

#include "ApplicationFeatures/ApplicationServer.h"
#include "V8/V8SecurityFeature.h"
#include "Basics/Common.h"
#include "Basics/debugging.h"
#include "V8/v8-globals.h"

extern char** environ;

static bool canExpose(v8::Isolate* isolate, v8::Local<v8::Name> property) {
  if (property->IsSymbol()) {
    return false;
  }
  v8::String::Utf8Value const key(isolate, property);
  std::string utf8String(*key, key.length());
  if (utf8String == "hasOwnProperty") {
    return true;
  }

  TRI_GET_GLOBALS();
  arangodb::V8SecurityFeature& v8security = v8g->_v8security;
  return v8security.shouldExposeEnvironmentVariable(isolate, utf8String);
}

static void EnvGetter(v8::Local<v8::Name> property,
                      const v8::PropertyCallbackInfo<v8::Value>& args) {
  v8::Isolate* isolate = args.GetIsolate();
  v8::HandleScope scope(isolate);
  if (property->IsSymbol()) {
    return args.GetReturnValue().SetUndefined();
  }
  if (!canExpose(isolate, property)) {
    return args.GetReturnValue().SetUndefined();
  }

  v8::String::Utf8Value const key(isolate, property);
  char const* val = getenv(*key);
  if (val) {
    TRI_V8_RETURN_STRING(val);
  }

  auto context = TRI_IGETC;
  // Not found.  Fetch from prototype.
  TRI_V8_RETURN(args.Data()
                    .As<v8::Object>()
                    ->Get(context, property)
                    .FromMaybe(v8::Local<v8::Value>()));
}

static void EnvSetter(v8::Local<v8::Name> property, v8::Local<v8::Value> value,
                      const v8::PropertyCallbackInfo<v8::Value>& args) {
  v8::Isolate* isolate = args.GetIsolate();
  v8::HandleScope scope(isolate);

  if (!canExpose(isolate, property)) {
    TRI_V8_RETURN(value);
  }
  v8::String::Utf8Value key(isolate, property);
  v8::String::Utf8Value val(isolate, value);
  setenv(*key, *val, 1);
  // Whether it worked or not, always return rval.
  TRI_V8_RETURN(value);
}

static void EnvQuery(v8::Local<v8::Name> property,
                     const v8::PropertyCallbackInfo<v8::Integer>& args) {
  v8::Isolate* isolate = args.GetIsolate();
  v8::HandleScope scope(isolate);

  int32_t rc = -1;  // Not found unless proven otherwise.
  if (!canExpose(isolate, property)) {
    return;
  }
  v8::String::Utf8Value key(isolate, property);
  if (getenv(*key)) {
    rc = 0;
  }
  if (rc != -1) {
    TRI_V8_RETURN(rc);
  }
}

static void EnvDeleter(v8::Local<v8::Name> property,
                       const v8::PropertyCallbackInfo<v8::Boolean>& args) {
  v8::Isolate* isolate = args.GetIsolate();
  v8::HandleScope scope(isolate);
  if (!canExpose(isolate, property)) {
    TRI_V8_RETURN_FALSE();
  }
  v8::String::Utf8Value key(isolate, property);
  bool rc = getenv(*key) != nullptr;
  if (rc) {
    unsetenv(*key);
  }
  if (rc) {
    TRI_V8_RETURN_TRUE();
  }
  TRI_V8_RETURN_FALSE();
}

static void EnvEnumerator(const v8::PropertyCallbackInfo<v8::Array>& args) {
  v8::Isolate* isolate = args.GetIsolate();
  v8::HandleScope scope(isolate);
  auto context = TRI_IGETC;
  int size = 0;
  while (environ[size]) {
    size++;
  }

  v8::Local<v8::Array> envarr = v8::Array::New(isolate);
  int j = 0;
  for (int i = 0; i < size; ++i) {
    char const* var = environ[i];
    char const* s = strchr(var, '=');
    size_t const length = s ? s - var : strlen(var);
    v8::Local<v8::String> name = TRI_V8_PAIR_STRING(isolate, var, length);
    if (canExpose(isolate, name)) {
      envarr->Set(context, j++, name).FromMaybe(false);
    }
  }

  TRI_V8_RETURN(envarr);
}

////////////////////////////////////////////////////////////////////////////////
/// @brief stores the V8 utils functions inside the global variable
////////////////////////////////////////////////////////////////////////////////

void TRI_InitV8Env(v8::Isolate* isolate, v8::Handle<v8::Context> context) {
  TRI_v8_global_t* v8g = TRI_GetV8Globals(isolate);

  v8::Handle<v8::ObjectTemplate> rt;
  v8::Handle<v8::FunctionTemplate> ft;

  ft = v8::FunctionTemplate::New(isolate);
  ft->SetClassName(TRI_V8_ASCII_STRING(isolate, "ENV"));

  rt = ft->InstanceTemplate();

  rt->SetHandler(v8::NamedPropertyHandlerConfiguration(
      EnvGetter, EnvSetter, EnvQuery, EnvDeleter, EnvEnumerator,
      v8::Object::New(isolate)));

  v8g->EnvTempl.Reset(isolate, rt);
  TRI_AddGlobalFunctionVocbase(
      isolate, TRI_V8_ASCII_STRING(isolate, "ENV"),
      ft->GetFunction(TRI_IGETC).FromMaybe(v8::Local<v8::Function>()));
}
