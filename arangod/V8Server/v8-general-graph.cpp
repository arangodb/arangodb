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

static void JS_DropGraph(v8::FunctionCallbackInfo<v8::Value> const& args) {
  TRI_V8_TRY_CATCH_BEGIN(isolate);
  v8::HandleScope scope(isolate);

  if (args.Length() < 1) {
    TRI_V8_THROW_EXCEPTION_USAGE("_drop(graphName, dropCollections)");
  } else if (!args[0]->IsString()) {
    TRI_V8_THROW_EXCEPTION(TRI_ERROR_GRAPH_CREATE_MISSING_NAME);
  }
  std::string graphName = TRI_ObjectToString(args[0]);
  if (graphName.empty()) {
    TRI_V8_THROW_EXCEPTION(TRI_ERROR_GRAPH_CREATE_MISSING_NAME);
  }
  bool dropCollections = false;
  if (args.Length() == 2) {
    dropCollections = TRI_ObjectToBoolean(args[1]);
  }

  auto& vocbase = GetContextVocBase(isolate);
  auto ctx = transaction::V8Context::Create(vocbase, false);

  std::shared_ptr<Graph const> graph;
  graph.reset(lookupGraphByName(ctx, graphName));

  ctx = transaction::V8Context::Create(vocbase, false);
  GraphOperations gops{*graph, ctx};
  OperationResult result = gops.removeGraph(true, dropCollections);

  VPackBuilder obj;
  if (result.fail()) {
    obj.add(VPackValue(VPackValueType::Object, true));
    obj.add("removed", VelocyPackHelper::BooleanValue(false));
    obj.close();
    TRI_V8_RETURN(TRI_VPackToV8(isolate, obj.slice()));
  }

  if (result.ok()) {
    obj.add(VPackValue(VPackValueType::Object, true));
    obj.add("removed", VelocyPackHelper::BooleanValue(true));
    obj.close();
    TRI_V8_RETURN(TRI_VPackToV8(isolate, obj.slice()));
  }
  TRI_V8_THROW_EXCEPTION(TRI_ERROR_GRAPH_NOT_FOUND);

  TRI_V8_RETURN_UNDEFINED();
  TRI_V8_TRY_CATCH_END
}

static void JS_RenameGraphCollection(
    v8::FunctionCallbackInfo<v8::Value> const& args) {
  TRI_V8_TRY_CATCH_BEGIN(isolate);
  v8::HandleScope scope(isolate);

  if (args.Length() < 2) {
    TRI_V8_THROW_EXCEPTION_USAGE("_renameCollection(oldName, newName)");
  } else if (!args[0]->IsString()) {
    TRI_V8_THROW_EXCEPTION(TRI_ERROR_GRAPH_CREATE_MISSING_NAME);
  }
  std::string oldName = TRI_ObjectToString(args[0]);
  std::string newName = TRI_ObjectToString(args[1]);
  if (oldName.empty() || newName.empty()) {
    TRI_V8_RETURN(false);
  }

  auto& vocbase = GetContextVocBase(isolate);
  auto ctx = transaction::V8Context::Create(vocbase, false);

  ctx = transaction::V8Context::Create(vocbase, false);
  GraphManager gmngr{ctx};
  bool r = false;
  r = gmngr.renameGraphCollection(oldName, newName);

  TRI_V8_RETURN(r);

  TRI_V8_RETURN_UNDEFINED();
  TRI_V8_TRY_CATCH_END
}

static void JS_GraphExists(v8::FunctionCallbackInfo<v8::Value> const& args) {
  TRI_V8_TRY_CATCH_BEGIN(isolate);
  v8::HandleScope scope(isolate);

  if (args.Length() < 1) {
    TRI_V8_THROW_EXCEPTION_USAGE("_exists(graphName)");
  } else if (!args[0]->IsString()) {
    TRI_V8_THROW_EXCEPTION(TRI_ERROR_GRAPH_CREATE_MISSING_NAME);
  }
  std::string graphName = TRI_ObjectToString(args[0]);
  if (graphName.empty()) {
    TRI_V8_THROW_EXCEPTION(TRI_ERROR_GRAPH_CREATE_MISSING_NAME);
  }

  auto& vocbase = GetContextVocBase(isolate);
  auto ctx = transaction::V8Context::Create(vocbase, false);

  // check if graph already exists
  GraphManager gmngr{ctx};
  bool r = false;
  r = gmngr.graphExists(graphName);

  TRI_V8_RETURN(r);

  TRI_V8_TRY_CATCH_END
}

static void JS_GetGraph(v8::FunctionCallbackInfo<v8::Value> const& args) {
  TRI_V8_TRY_CATCH_BEGIN(isolate);
  v8::HandleScope scope(isolate);

  if (args.Length() < 1) {
    TRI_V8_THROW_EXCEPTION_USAGE("_graph(graphName)");
  } else if (!args[0]->IsString()) {
    TRI_V8_THROW_EXCEPTION(TRI_ERROR_GRAPH_CREATE_MISSING_NAME);
  }
  std::string graphName = TRI_ObjectToString(args[0]);
  if (graphName.empty()) {
    TRI_V8_THROW_EXCEPTION(TRI_ERROR_GRAPH_CREATE_MISSING_NAME);
  }

  auto& vocbase = GetContextVocBase(isolate);
  auto ctx = transaction::V8Context::Create(vocbase, false);

  std::shared_ptr<Graph const> graph;
  graph.reset(lookupGraphByName(ctx, graphName));

  VPackBuilder result;
  graph->graphToVpack(result);
  VPackSlice resSlice = result.slice().get("graph");

  if (!result.isEmpty()) {
    TRI_V8_RETURN(TRI_VPackToV8(isolate, resSlice));
  }
  TRI_V8_THROW_EXCEPTION(TRI_ERROR_GRAPH_NOT_FOUND);

  TRI_V8_RETURN_UNDEFINED();
  TRI_V8_TRY_CATCH_END
}

static void JS_GetGraphs(v8::FunctionCallbackInfo<v8::Value> const& args) {
  TRI_V8_TRY_CATCH_BEGIN(isolate);
  v8::HandleScope scope(isolate);

  auto& vocbase = GetContextVocBase(isolate);
  auto ctx = transaction::V8Context::Create(vocbase, false);

  GraphManager gmngr{ctx};
  VPackBuilder result;
  OperationResult r = gmngr.readGraphs(result, arangodb::aql::PART_DEPENDENT);

  if (r.fail()) {
    TRI_V8_THROW_EXCEPTION_MESSAGE(r.errorNumber(), r.errorMessage());
  }

  if (!result.isEmpty()) {
    TRI_V8_RETURN(TRI_VPackToV8(isolate, result.slice().get("graphs")));
  }

  TRI_V8_RETURN_UNDEFINED();
  TRI_V8_TRY_CATCH_END
}

static void JS_GetGraphKeys(v8::FunctionCallbackInfo<v8::Value> const& args) {
  TRI_V8_TRY_CATCH_BEGIN(isolate);
  v8::HandleScope scope(isolate);

  auto& vocbase = GetContextVocBase(isolate);
  auto ctx = transaction::V8Context::Create(vocbase, false);

  GraphManager gmngr{ctx};
  VPackBuilder result;
  OperationResult r =
      gmngr.readGraphKeys(result, arangodb::aql::PART_DEPENDENT);

  if (r.fail()) {
    TRI_V8_THROW_EXCEPTION_MESSAGE(r.errorNumber(), r.errorMessage());
  }

  if (!result.isEmpty()) {
    TRI_V8_RETURN(TRI_VPackToV8(isolate, result.slice().get("graphs")));
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
    TRI_V8_THROW_EXCEPTION_MESSAGE(r.errorNumber(), r.errorMessage());
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

static void JS_AddEdgeDefinitions(
    v8::FunctionCallbackInfo<v8::Value> const& args) {
  TRI_V8_TRY_CATCH_BEGIN(isolate);
  v8::HandleScope scope(isolate);

  if (args.Length() < 2) {
    TRI_V8_THROW_EXCEPTION_USAGE("_extendEdgeDefinitions(edgeDefinition)");
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

  r = gops.addEdgeDefinition(edgeDefinition.slice(), false);

  if (r.fail()) {
    TRI_V8_THROW_EXCEPTION_MESSAGE(r.errorNumber(), r.errorMessage());
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

static void JS_EditEdgeDefinitions(
    v8::FunctionCallbackInfo<v8::Value> const& args) {
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

  r = gops.editEdgeDefinition(
      edgeDefinition.slice(), false,
      edgeDefinition.slice().get("collection").copyString());

  if (r.fail()) {
    TRI_V8_THROW_EXCEPTION_MESSAGE(r.errorNumber(), r.errorMessage());
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

static void JS_RemoveVertexCollection(
    v8::FunctionCallbackInfo<v8::Value> const& args) {
  TRI_V8_TRY_CATCH_BEGIN(isolate);
  v8::HandleScope scope(isolate);

  if (args.Length() < 2) {
    TRI_V8_THROW_EXCEPTION_USAGE(
        "_removeVertexCollection(vertexName, dropCollection)");
  }
  if (!args[0]->IsString()) {
    TRI_V8_THROW_EXCEPTION(TRI_ERROR_GRAPH_CREATE_MISSING_NAME);
  }
  std::string graphName = TRI_ObjectToString(args[0]);
  std::string vertexName = TRI_ObjectToString(args[1]);
  bool dropCollection = false;
  if (args.Length() == 3) {
    dropCollection = TRI_ObjectToBoolean(args[2]);
  }

  auto& vocbase = GetContextVocBase(isolate);
  auto ctx = transaction::V8Context::Create(vocbase, false);

  std::shared_ptr<Graph const> graph;
  graph.reset(lookupGraphByName(ctx, graphName));

  ctx = transaction::V8Context::Create(vocbase, false);
  GraphOperations gops{*graph, ctx};
  OperationResult r;

  VPackBuilder builder;
  builder.openObject();
  builder.add("collection", VPackValue(vertexName));
  builder.close();

  r = gops.eraseOrphanCollection(false, vertexName, dropCollection);

  if (r.fail()) {
    TRI_V8_THROW_EXCEPTION_MESSAGE(r.errorNumber(), r.errorMessage());
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

static void JS_AddVertexCollection(
    v8::FunctionCallbackInfo<v8::Value> const& args) {
  TRI_V8_TRY_CATCH_BEGIN(isolate);
  v8::HandleScope scope(isolate);

  if (args.Length() < 2) {
    TRI_V8_THROW_EXCEPTION_USAGE(
        "_addVertexCollection(vertexName, createCollection)");
  }
  if (!args[0]->IsString()) {
    TRI_V8_THROW_EXCEPTION(TRI_ERROR_GRAPH_CREATE_MISSING_NAME);
  }
  std::string graphName = TRI_ObjectToString(args[0]);
  std::string vertexName = TRI_ObjectToString(args[1]);
  bool createCollection = true;
  if (args.Length() == 3) {
    createCollection = TRI_ObjectToBoolean(args[2]);
  }

  auto& vocbase = GetContextVocBase(isolate);
  auto ctx = transaction::V8Context::Create(vocbase, false);

  std::shared_ptr<Graph const> graph;
  graph.reset(lookupGraphByName(ctx, graphName));

  ctx = transaction::V8Context::Create(vocbase, false);
  GraphOperations gops{*graph, ctx};
  OperationResult r;

  VPackBuilder builder;
  builder.openObject();
  builder.add("collection", VPackValue(vertexName));
  builder.close();

  r = gops.addOrphanCollection(builder.slice(), false, createCollection);

  if (r.fail()) {
    TRI_V8_THROW_EXCEPTION_MESSAGE(r.errorNumber(), r.errorMessage());
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

static void JS_DropEdgeDefinition(
    v8::FunctionCallbackInfo<v8::Value> const& args) {
  TRI_V8_TRY_CATCH_BEGIN(isolate);
  v8::HandleScope scope(isolate);

  if (args.Length() < 2) {
    TRI_V8_THROW_EXCEPTION_USAGE(
        "_deleteEdgeDefinitions(edgeCollection, dropCollection)");
  }
  if (!args[0]->IsString()) {
    TRI_V8_THROW_EXCEPTION(TRI_ERROR_GRAPH_CREATE_MISSING_NAME);
  }
  std::string graphName = TRI_ObjectToString(args[0]);
  std::string edgeDefinitionName = TRI_ObjectToString(args[1]);

  bool dropCollections = false;
  if (args.Length() == 3) {
    dropCollections = TRI_ObjectToBoolean(args[2]);
  }

  auto& vocbase = GetContextVocBase(isolate);
  auto ctx = transaction::V8Context::Create(vocbase, false);

  std::shared_ptr<Graph const> graph;
  graph.reset(lookupGraphByName(ctx, graphName));

  ctx = transaction::V8Context::Create(vocbase, false);
  GraphOperations gops{*graph, ctx};
  OperationResult r;

  r = gops.eraseEdgeDefinition(false, edgeDefinitionName, dropCollections);

  if (r.fail()) {
    TRI_V8_THROW_EXCEPTION_MESSAGE(r.errorNumber(), r.errorMessage());
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

  TRI_AddMethodVocbase(isolate, rt, TRI_V8_ASCII_STRING(isolate, "_list"),
                       JS_GetGraphKeys);

  TRI_AddMethodVocbase(isolate, rt, TRI_V8_ASCII_STRING(isolate, "_graph"),
                       JS_GetGraph);
  TRI_AddMethodVocbase(isolate, rt, TRI_V8_ASCII_STRING(isolate, "_exists"),
                       JS_GraphExists);

  TRI_AddMethodVocbase(isolate, rt,
                       TRI_V8_ASCII_STRING(isolate, "_renameCollection"),
                       JS_RenameGraphCollection);

  TRI_AddMethodVocbase(isolate, rt, TRI_V8_ASCII_STRING(isolate, "_create"),
                       JS_CreateGraph);

  TRI_AddMethodVocbase(isolate, rt, TRI_V8_ASCII_STRING(isolate, "_drop"),
                       JS_DropGraph);

  TRI_AddMethodVocbase(isolate, rt,
                       TRI_V8_ASCII_STRING(isolate, "_addVertexCollection"),
                       JS_AddVertexCollection);

  TRI_AddMethodVocbase(isolate, rt,
                       TRI_V8_ASCII_STRING(isolate, "_removeVertexCollection"),
                       JS_RemoveVertexCollection);

  TRI_AddMethodVocbase(isolate, rt,
                       TRI_V8_ASCII_STRING(isolate, "_extendEdgeDefinitions"),
                       JS_AddEdgeDefinitions);

  TRI_AddMethodVocbase(isolate, rt,
                       TRI_V8_ASCII_STRING(isolate, "_editEdgeDefinitions"),
                       JS_EditEdgeDefinitions);

  TRI_AddMethodVocbase(isolate, rt,
                       TRI_V8_ASCII_STRING(isolate, "_deleteEdgeDefinition"),
                       JS_DropEdgeDefinition);

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
