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
#include "Graph/Graph.h"
#include "Graph/GraphCache.h"
#include "Graph/GraphManager.h"
#include "RestServer/DatabaseFeature.h"
#include "Transaction/V8Context.h"
#include "Utils/ExecContext.h"

#include "V8/v8-conv.h"
#include "V8/v8-globals.h"
#include "V8/v8-utils.h"
#include "V8/v8-vpack.h"
#include "V8Server/v8-externals.h"
#include "V8Server/v8-vocbase.h"
#include "V8Server/v8-vocbaseprivate.h"
#include "V8Server/v8-vocindex.h"
#include "VocBase/LogicalCollection.h"

#include <arangod/Graph/GraphOperations.h>
#include <arangod/VocBase/Graphs.h>
#include <velocypack/Builder.h>
#include <velocypack/HexDump.h>
#include <velocypack/Slice.h>
#include <velocypack/velocypack-aliases.h>

using namespace arangodb;
using namespace arangodb::velocypack;
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
  OperationResult r = gmngr.readGraphs(result, arangodb::aql::PART_DEPENDENT);

  if (r.fail()) {
    TRI_V8_THROW_EXCEPTION(r.errorNumber());
  }

  if (!result.isEmpty()) {
    TRI_V8_RETURN(TRI_VPackToV8(isolate, result.slice()));
  }

  TRI_V8_RETURN_UNDEFINED();
  TRI_V8_TRY_CATCH_END
}

static void JS_CreateGraph(v8::FunctionCallbackInfo<v8::Value> const& args) {
  TRI_V8_TRY_CATCH_BEGIN(isolate);
  v8::HandleScope scope(isolate);

  if (args.Length() < 1) {
    TRI_V8_THROW_EXCEPTION_USAGE(
        "_create(graphName, edgeDefinitions, orphanCollections, options)");
  } else if (!args[0]->IsString()) {
    TRI_V8_THROW_EXCEPTION(TRI_ERROR_GRAPH_CREATE_MISSING_NAME);
  }
  std::string graphName = TRI_ObjectToString(args[0]);
  if (graphName.empty()) {
    TRI_V8_THROW_EXCEPTION(TRI_ERROR_GRAPH_CREATE_MISSING_NAME);
  }

  VPackBuilder builder;
  builder.openObject();

  builder.add("name", VPackValue(graphName));
  if (args.Length() >= 2 && !args[1]->IsNullOrUndefined()) {
    builder.add(VPackValue(StaticStrings::GraphEdgeDefinitions));
    TRI_V8ToVPack(isolate, builder, args[1], true, true);
    builder.close();
  }
  if (args.Length() >= 3 && !args[2]->IsNullOrUndefined()) {
    builder.add(VPackValue(StaticStrings::GraphOrphans));
    TRI_V8ToVPack(isolate, builder, args[2], true, true);
    builder.close();
  }
  if (args.Length() >= 4 && !args[2]->IsNullOrUndefined()) {
    builder.add(VPackValue("options"));
    TRI_V8ToVPack(isolate, builder, args[3], true, true);
    builder.close();
  }

  builder.close();

  auto& vocbase = GetContextVocBase(isolate);
  auto ctx = transaction::V8Context::Create(vocbase, false);

  GraphManager gmngr{ctx};
  OperationResult r = gmngr.createGraph(builder.slice(), false);

  if (r.fail()) {
    TRI_V8_THROW_EXCEPTION(r.errorNumber());
  }

  ctx = transaction::V8Context::Create(vocbase, false);

  // TODO: use cache?
  std::shared_ptr<Graph const> graph;
  graph.reset(lookupGraphByName(ctx, graphName));
  VPackBuilder result;
  graph->graphToVpack(result);

  if (!result.isEmpty()) {
    TRI_V8_RETURN(TRI_VPackToV8(isolate, result.slice()));
  }

  TRI_V8_RETURN_UNDEFINED();
  TRI_V8_TRY_CATCH_END
}

static void JS_EditEdgeDefinitions(v8::FunctionCallbackInfo<v8::Value> const& args) {
  TRI_V8_TRY_CATCH_BEGIN(isolate);
  v8::HandleScope scope(isolate);

  if (args.Length() < 2) {
    TRI_V8_THROW_EXCEPTION_USAGE("_editEdgeDefinitions(edgeDefinition)");
  }
  if (!args[0]->IsString()) {
    TRI_V8_THROW_EXCEPTION(TRI_ERROR_GRAPH_CREATE_MISSING_NAME);
  }
  std::string graphName = TRI_ObjectToString(args[0]);

  VPackBuilder edgeDefinition;
  if (args.Length() == 2) {
    TRI_V8ToVPack(isolate, edgeDefinition, args[1], false);
  }

  auto& vocbase = GetContextVocBase(isolate);
  auto ctx = transaction::V8Context::Create(vocbase, false);

  std::shared_ptr<Graph const> graph;
  graph.reset(lookupGraphByName(ctx, graphName));

  ctx = transaction::V8Context::Create(vocbase, false);
  GraphOperations gops{*graph, ctx};
  OperationResult r;

  if (r.fail()) {
    TRI_V8_THROW_EXCEPTION(r.errorNumber());
  }

  r = gops.editEdgeDefinition(
      edgeDefinition.slice(), false,
      edgeDefinition.slice().get("collection").copyString());

  if (r.fail()) {
    TRI_V8_THROW_EXCEPTION(r.errorNumber());
  }
  ctx = transaction::V8Context::Create(vocbase, false);

  // TODO: use cache?
  graph.reset(lookupGraphByName(ctx, graphName));
  VPackBuilder result;
  graph->graphToVpack(result);

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

  TRI_AddMethodVocbase(isolate, rt, TRI_V8_ASCII_STRING(isolate, "_create"),
                       JS_CreateGraph);
  TRI_AddMethodVocbase(isolate, rt, TRI_V8_ASCII_STRING(isolate, "_editEdgeDefinitions"),
                       JS_EditEdgeDefinitions);

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
