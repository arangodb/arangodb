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
////////////////////////////////////////////////////////////////////////////////

#include <stdlib.h>
#include <string.h>
#include <cstdint>
#include <string>

#include <v8.h>

#include "ApplicationFeatures/ApplicationServer.h"
#include "ApplicationFeatures/V8SecurityFeature.h"
#include "Basics/Common.h"
#include "Basics/debugging.h"
#include "V8/v8-globals.h"

#ifdef _WIN32
#include "Basics/win-utils.h"
#else
#ifdef __APPLE__
#include <crt_externs.h>
#define environ (*_NSGetEnviron())
#elif !defined(_MSC_VER)
extern char** environ;
#endif
#endif

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
  arangodb::V8SecurityFeature& v8security =
      v8g->_server.getFeature<arangodb::V8SecurityFeature>();
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

#ifndef _WIN32
  v8::String::Utf8Value const key(isolate, property);
  char const* val = getenv(*key);
  if (val) {
    TRI_V8_RETURN_STRING(val);
  }
#else  // _WIN32
  v8::String::Value key(isolate, property);
  WCHAR buffer[32767];  // The maximum size allowed for environment variables.
  DWORD result = GetEnvironmentVariableW(reinterpret_cast<const WCHAR*>(*key),
                                         buffer, sizeof(buffer));

  // ERROR_ENVVAR_NOT_FOUND is possibly returned.
  // If result >= sizeof buffer the buffer was too small. That should never
  // happen. If result == 0 and result != ERROR_SUCCESS the variable was not
  // not found.
  if ((result > 0 || GetLastError() == ERROR_SUCCESS) && result < sizeof(buffer)) {
    uint16_t const* two_byte_buffer = reinterpret_cast<uint16_t const*>(buffer);
    TRI_V8_RETURN(TRI_V8_STRING_UTF16(isolate, two_byte_buffer, result));
  }
#endif
  auto context = TRI_IGETC;
  // Not found.  Fetch from prototype.
  TRI_V8_RETURN(args.Data().As<v8::Object>()->Get(context, property).FromMaybe(v8::Local<v8::Value>()));
}

static void EnvSetter(v8::Local<v8::Name> property, v8::Local<v8::Value> value,
                      const v8::PropertyCallbackInfo<v8::Value>& args) {
  v8::Isolate* isolate = args.GetIsolate();
  v8::HandleScope scope(isolate);

  if (!canExpose(isolate, property)) {
    TRI_V8_RETURN(value);
  }
#ifndef _WIN32
  v8::String::Utf8Value key(isolate, property);
  v8::String::Utf8Value val(isolate, value);
  setenv(*key, *val, 1);
#else  // _WIN32
  v8::String::Value key(isolate, property);
  v8::String::Value val(isolate, value);
  WCHAR* key_ptr = reinterpret_cast<WCHAR*>(*key);
  // Environment variables that start with '=' are read-only.
  if (key_ptr[0] != L'=') {
    SetEnvironmentVariableW(key_ptr, reinterpret_cast<WCHAR*>(*val));
  }
#endif
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
#ifndef _WIN32
  v8::String::Utf8Value key(isolate, property);
  if (getenv(*key)) {
    rc = 0;
  }
#else  // _WIN32
  v8::String::Value key(isolate, property);
  WCHAR* key_ptr = reinterpret_cast<WCHAR*>(*key);
  SetLastError(ERROR_SUCCESS);

  if (GetEnvironmentVariableW(key_ptr, nullptr, 0) > 0 || GetLastError() == ERROR_SUCCESS) {
    rc = 0;
    if (key_ptr[0] == L'=') {
      // Environment variables that start with '=' are hidden and read-only.
      rc = static_cast<int32_t>(v8::ReadOnly) |
           static_cast<int32_t>(v8::DontDelete) | static_cast<int32_t>(v8::DontEnum);
    }
  }
#endif
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
#ifndef _WIN32
  v8::String::Utf8Value key(isolate, property);
  bool rc = getenv(*key) != nullptr;
  if (rc) {
    unsetenv(*key);
  }
#else
  bool rc = true;
  v8::String::Value key(isolate, property);
  WCHAR* key_ptr = reinterpret_cast<WCHAR*>(*key);
  if (key_ptr[0] == L'=' || !SetEnvironmentVariableW(key_ptr, nullptr)) {
    // Deletion failed. Return true if the key wasn't there in the first place,
    // false if it is still there.
    rc = GetEnvironmentVariableW(key_ptr, nullptr, 0) == 0 && GetLastError() != ERROR_SUCCESS;
  }
#endif
  if (rc) {
    TRI_V8_RETURN_TRUE();
  }
  TRI_V8_RETURN_FALSE();
}

static void EnvEnumerator(const v8::PropertyCallbackInfo<v8::Array>& args) {
  v8::Isolate* isolate = args.GetIsolate();
  v8::HandleScope scope(isolate);
  auto context = TRI_IGETC;
#ifndef _WIN32
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
#else  // _WIN32
  WCHAR* environment = GetEnvironmentStringsW();
  if (environment == nullptr) return;  // This should not happen.
  v8::Local<v8::Array> envarr = v8::Array::New(isolate);
  WCHAR* p = environment;
  int i = 0;
  while (*p != 0) {
    WCHAR* s;
    if (*p == L'=') {
      // If the key starts with '=' it is a hidden environment variable.
      p += wcslen(p) + 1;
      continue;
    } else {
      s = wcschr(p, L'=');
    }
    if (!s) {
      s = p + wcslen(p);
    }
    uint16_t const* two_byte_buffer = reinterpret_cast<uint16_t const*>(p);
    size_t const two_byte_buffer_len = s - p;
    auto value = TRI_V8_STRING_UTF16(isolate, two_byte_buffer, (int)two_byte_buffer_len);

    if (canExpose(isolate, value)) {
      envarr->Set(context, i, value).FromMaybe(false);
    }
    p = s + wcslen(s) + 1;
    i++;
  }
  FreeEnvironmentStringsW(environment);
#endif

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
  // rt->SetInternalFieldCount(3);

  rt->SetHandler(v8::NamedPropertyHandlerConfiguration(EnvGetter, EnvSetter, EnvQuery,
                                                       EnvDeleter, EnvEnumerator,
                                                       v8::Object::New(isolate)));

  v8g->EnvTempl.Reset(isolate, rt);
  TRI_AddGlobalFunctionVocbase(isolate, TRI_V8_ASCII_STRING(isolate, "ENV"),
                               ft->GetFunction(TRI_IGETC).FromMaybe(v8::Local<v8::Function>()));
}
