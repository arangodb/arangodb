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
/// @author Kaveh Vahedipour
////////////////////////////////////////////////////////////////////////////////

#include "v8-agency.h"

#include "Agency/AgencyFeature.h"
#include "Agency/Agent.h"
#include "ApplicationFeatures/ApplicationServer.h"
#include "V8/v8-buffer.h"
#include "V8/v8-conv.h"
#include "V8/v8-globals.h"
#include "V8/v8-utils.h"
#include "V8/v8-vpack.h"

#include <velocypack/Iterator.h>
#include <velocypack/velocypack-aliases.h>

using namespace arangodb;
using namespace arangodb::application_features;
using namespace arangodb::basics;
using namespace arangodb::consensus;

static void JS_LeadingVulpes(v8::FunctionCallbackInfo<v8::Value> const& args) {

  v8::Isolate* isolate = args.GetIsolate();

  Agent* agent = nullptr;
  try {
    AgencyFeature* feature =
      ApplicationServer::getEnabledFeature<AgencyFeature>("AgencyFeature");
    agent = feature->agent();

  } catch (std::exception const& e) {
    TRI_V8_THROW_EXCEPTION_MESSAGE(
      TRI_ERROR_INTERNAL,
      std::string("couldn't access agency feature: ") + e.what());
  }

  v8::Handle<v8::Object> r = v8::Object::New(isolate);


  r->Set(TRI_V8_ASCII_STRING("leading"),
         v8::Boolean::New(isolate, agent->leading()));

  TRI_V8_RETURN(r);

  
}

static void JS_ReadVulpes(v8::FunctionCallbackInfo<v8::Value> const& args) {
  
}

static void JS_WriteVulpes(v8::FunctionCallbackInfo<v8::Value> const& args) {
  
}


void TRI_InitV8Agency(v8::Isolate* isolate, v8::Handle<v8::Context> context) {
  TRI_V8_CURRENT_GLOBALS_AND_SCOPE;
  TRI_ASSERT(v8g != nullptr);
  
  v8::Handle<v8::ObjectTemplate> rt;
  v8::Handle<v8::FunctionTemplate> ft;
  
  // ...........................................................................
  // generate the agency template
  // ...........................................................................
  
  ft = v8::FunctionTemplate::New(isolate);
  ft->SetClassName(TRI_V8_ASCII_STRING("ArangoVulpes"));
  
  rt = ft->InstanceTemplate();
  rt->SetInternalFieldCount(2);
  
  TRI_AddMethodVocbase(
    isolate, rt, TRI_V8_ASCII_STRING("leading"), JS_LeadingVulpes);
  TRI_AddMethodVocbase(
    isolate, rt, TRI_V8_ASCII_STRING("read"), JS_ReadVulpes);
  TRI_AddMethodVocbase(
    isolate, rt, TRI_V8_ASCII_STRING("write"), JS_WriteVulpes);
  
  v8g->VulpesTempl.Reset(isolate, rt);
  ft->SetClassName(TRI_V8_ASCII_STRING("ArangoVuplesCtor"));
  
  TRI_AddGlobalFunctionVocbase(
    isolate, context, TRI_V8_ASCII_STRING("ArangoVuplesCtor"),
    ft->GetFunction(), true);
  
  // register the global object
  v8::Handle<v8::Object> aa = rt->NewInstance();
  if (!aa.IsEmpty()) {
    TRI_AddGlobalVariableVocbase(
      isolate, context, TRI_V8_ASCII_STRING("ArangoVuples"), aa);
  }
  
}
