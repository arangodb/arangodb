////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2017 ArangoDB GmbH, Cologne, Germany
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
/// @author Simon Grätzer
////////////////////////////////////////////////////////////////////////////////

#include "v8-users.h"

#include "ApplicationFeatures/ApplicationServer.h"

#include "V8/v8-globals.h"
#include "V8/v8-vpack.h"
#include "V8/v8-conv.h"
#include "V8/v8-utils.h"
#include "V8/v8-vpack.h"
#include "V8Server/v8-externals.h"
#include "V8Server/v8-vocbase.h"
#include "V8Server/v8-vocbaseprivate.h"
#include "V8Server/v8-vocindex.h"
#include "VocBase/KeyGenerator.h"
#include "VocBase/LogicalCollection.h"
#include "VocBase/modes.h"

#include <velocypack/Builder.h>
#include <velocypack/HexDump.h>
#include <velocypack/Slice.h>
#include <velocypack/velocypack-aliases.h>

using namespace arangodb;
using namespace arangodb::basics;
using namespace arangodb::rest;



void TRI_InitV8Users(v8::Handle<v8::Context> context,
                           TRI_vocbase_t* vocbase, TRI_v8_global_t* v8g,
                           v8::Isolate* isolate,
                           v8::Handle<v8::ObjectTemplate> ArangoDBNS) {
  
  v8::Handle<v8::ObjectTemplate> rt;
  v8::Handle<v8::FunctionTemplate> ft;

  ft = v8::FunctionTemplate::New(isolate);
  ft->SetClassName(TRI_V8_ASCII_STRING("ArangoUsers"));

  rt = ft->InstanceTemplate();
  rt->SetInternalFieldCount(3);

  /*TRI_AddMethodVocbase(isolate, rt, TRI_V8_ASCII_STRING("save"),
                       JS_SaveUser);
  TRI_AddMethodVocbase(isolate, rt, TRI_V8_ASCII_STRING("replace"),
                       JS_ReplaceUser);
  TRI_AddMethodVocbase(isolate, rt, TRI_V8_ASCII_STRING("update"),
                       JS_UpdateUser);
  TRI_AddMethodVocbase(isolate, rt, TRI_V8_ASCII_STRING("remove"),
                       JS_RemoveUser);
  TRI_AddMethodVocbase(isolate, rt, TRI_V8_ASCII_STRING("document"),
                       JS_GetUser);
  TRI_AddMethodVocbase(isolate, rt, TRI_V8_ASCII_STRING("reload"),
                       JS_ReloadAuthData);
  TRI_AddMethodVocbase(isolate, rt, TRI_V8_ASCII_STRING("grantDatabase"),
                       JS_GrantDatabase);
  TRI_AddMethodVocbase(isolate, rt, TRI_V8_ASCII_STRING("revokeDatabase"),
                       JS_RevokeDatabase);
  TRI_AddMethodVocbase(isolate, rt, TRI_V8_ASCII_STRING("updateConfigData"),
                       JS_UpdateConfigData);
  TRI_AddMethodVocbase(isolate, rt, TRI_V8_ASCII_STRING("configData"),
                       JS_GetConfigData);
  TRI_AddMethodVocbase(isolate, rt, TRI_V8_ASCII_STRING("permission"),
                       JS_GetPermission);*/
  
  TRI_InitV8IndexCollection(isolate, rt);

  v8g->VocbaseColTempl.Reset(isolate, rt);
  TRI_AddGlobalFunctionVocbase(isolate, TRI_V8_ASCII_STRING("ArangoUsers"),
                               ft->GetFunction());
}
