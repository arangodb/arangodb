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
/// @author Heiko Kernbach
////////////////////////////////////////////////////////////////////////////////

#include "v8-users.h"

#include "ApplicationFeatures/ApplicationServer.h"
#include "Basics/VelocyPackHelper.h"
#include "GeneralServer/AuthenticationFeature.h"
#include "Graph/GraphManager.h"
#include "RestServer/DatabaseFeature.h"
#include "Utils/ExecContext.h"
#include "Transaction/V8Context.h"

#include "V8/v8-conv.h"
#include "V8/v8-globals.h"
#include "V8/v8-utils.h"
#include "V8/v8-vpack.h"
#include "V8Server/v8-externals.h"
#include "V8Server/v8-vocbase.h"
#include "V8Server/v8-vocbaseprivate.h"
#include "V8Server/v8-vocindex.h"
#include "VocBase/LogicalCollection.h"

#include <velocypack/Builder.h>
#include <velocypack/HexDump.h>
#include <velocypack/Slice.h>
#include <velocypack/velocypack-aliases.h>

using namespace arangodb;
using namespace arangodb::basics;
using namespace arangodb::graph;
using namespace arangodb::rest;

static void JS_GetGraphs(v8::FunctionCallbackInfo<v8::Value> const& args) {
  TRI_V8_TRY_CATCH_BEGIN(isolate);
  v8::HandleScope scope(isolate);

  auto& vocbase = GetContextVocBase(isolate);
  auto ctx = transaction::V8Context::Create(vocbase, false);

  GraphManager gmngr{ctx};
  VPackBuilder result;
  gmngr.readGraphs(result, arangodb::aql::PART_DEPENDENT);

  if (!result.isEmpty()) {
    TRI_V8_RETURN(TRI_VPackToV8(isolate, result.slice()));
  }

  TRI_V8_RETURN_UNDEFINED();
  TRI_V8_TRY_CATCH_END
}

void TRI_InitV8GeneralGraph(v8::Handle<v8::Context> context,
                            TRI_vocbase_t* vocbase, TRI_v8_global_t* v8g,
                            v8::Isolate* isolate) {
  v8::Handle<v8::ObjectTemplate> rt;
  v8::Handle<v8::FunctionTemplate> ft;

  ft = v8::FunctionTemplate::New(isolate);
  ft->SetClassName(TRI_V8_ASCII_STRING(isolate, "ArangoGeneralGraph"));

  rt = ft->InstanceTemplate();
  rt->SetInternalFieldCount(2);

  TRI_AddMethodVocbase(
      isolate, rt, TRI_V8_ASCII_STRING(isolate, "_listObjects"), JS_GetGraphs);
  TRI_AddMethodVocbase(
          isolate, rt, TRI_V8_ASCII_STRING(isolate, "_list"), JS_GetGraphs);

  v8g->GeneralGraphsTempl.Reset(isolate, rt);
  ft->SetClassName(TRI_V8_ASCII_STRING(isolate, "ArangoGeneralGraphCtor"));
  TRI_AddGlobalFunctionVocbase(
      isolate, TRI_V8_ASCII_STRING(isolate, "ArangoGeneralGraphCtor"),
      ft->GetFunction(), true);

  // register the global object
  v8::Handle<v8::Object> aa = rt->NewInstance();
  if (!aa.IsEmpty()) {
    TRI_AddGlobalVariableVocbase(
        isolate, TRI_V8_ASCII_STRING(isolate, "ArangoGeneralGraph"), aa);
  }
}
