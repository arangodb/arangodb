////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2014-2020 ArangoDB GmbH, Cologne, Germany
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
/// @author Copyright 2018, ArangoDB GmbH, Cologne, Germany
////////////////////////////////////////////////////////////////////////////////

#include "v8-users.h"

#include "ApplicationFeatures/ApplicationServer.h"
#include "Basics/VelocyPackHelper.h"
#include "GeneralServer/AuthenticationFeature.h"
#include "Graph/Graph.h"
#include "Graph/GraphManager.h"
#include "Graph/GraphOperations.h"
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

#include <velocypack/Slice.h>

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
  std::string graphName = TRI_ObjectToString(isolate, args[0]);
  if (graphName.empty()) {
    TRI_V8_THROW_EXCEPTION(TRI_ERROR_GRAPH_CREATE_MISSING_NAME);
  }
  bool dropCollections = false;
  if (args.Length() >= 2) {
    dropCollections = TRI_ObjectToBoolean(isolate, args[1]);
  }

  auto& vocbase = GetContextVocBase(isolate);
  auto ctx = transaction::V8Context::Create(vocbase, false);

  GraphManager gmngr{vocbase};
  auto graph = gmngr.lookupGraphByName(graphName);
  if (graph.fail()) {
    TRI_V8_THROW_EXCEPTION_MESSAGE(graph.errorNumber(), graph.errorMessage());
  }
  TRI_ASSERT(graph.get() != nullptr);

  OperationResult result = gmngr.removeGraph(*(graph.get()), true, dropCollections);

  VPackBuilder obj;
  obj.add(VPackValue(VPackValueType::Object, true));
  obj.add("removed", VPackValue(result.ok()));
  obj.close();
  TRI_V8_RETURN(TRI_VPackToV8(isolate, obj.slice()));
  TRI_V8_TRY_CATCH_END
}

static void JS_RenameGraphCollection(v8::FunctionCallbackInfo<v8::Value> const& args) {
  TRI_V8_TRY_CATCH_BEGIN(isolate);
  v8::HandleScope scope(isolate);

  if (args.Length() < 2) {
    TRI_V8_THROW_EXCEPTION_USAGE("_renameCollection(oldName, newName)");
  } else if (!args[0]->IsString()) {
    TRI_V8_THROW_EXCEPTION(TRI_ERROR_GRAPH_CREATE_MISSING_NAME);
  } else if (!args[1]->IsString()) {
    TRI_V8_THROW_EXCEPTION(TRI_ERROR_GRAPH_CREATE_MISSING_NAME);
  }
  std::string oldName = TRI_ObjectToString(isolate, args[0]);
  std::string newName = TRI_ObjectToString(isolate, args[1]);
  if (oldName.empty() || newName.empty()) {
    TRI_V8_THROW_EXCEPTION(TRI_ERROR_GRAPH_CREATE_MISSING_NAME);
  }

  auto& vocbase = GetContextVocBase(isolate);
  GraphManager gmngr{vocbase};
  bool r = gmngr.renameGraphCollection(oldName, newName);

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
  std::string graphName = TRI_ObjectToString(isolate, args[0]);
  if (graphName.empty()) {
    TRI_V8_THROW_EXCEPTION(TRI_ERROR_GRAPH_CREATE_MISSING_NAME);
  }

  auto& vocbase = GetContextVocBase(isolate);
  // check if graph already exists
  GraphManager gmngr{vocbase};
  bool r = gmngr.graphExists(graphName);

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
  std::string graphName = TRI_ObjectToString(isolate, args[0]);
  if (graphName.empty()) {
    TRI_V8_THROW_EXCEPTION(TRI_ERROR_GRAPH_CREATE_MISSING_NAME);
  }

  auto& vocbase = GetContextVocBase(isolate);

  GraphManager gmngr{vocbase};
  auto graph = gmngr.lookupGraphByName(graphName);
  if (graph.fail()) {
    TRI_V8_THROW_EXCEPTION_MESSAGE(graph.errorNumber(), graph.errorMessage());
  }
  TRI_ASSERT(graph.get() != nullptr);

  VPackBuilder result;
  result.openObject();
  graph.get()->graphForClient(result);
  result.close();
  VPackSlice resSlice = result.slice().get("graph");

  TRI_V8_RETURN(TRI_VPackToV8(isolate, resSlice));
  TRI_V8_TRY_CATCH_END
}

static void JS_GetGraphs(v8::FunctionCallbackInfo<v8::Value> const& args) {
  TRI_V8_TRY_CATCH_BEGIN(isolate);
  v8::HandleScope scope(isolate);

  auto& vocbase = GetContextVocBase(isolate);

  GraphManager gmngr{vocbase};
  VPackBuilder result;
  Result r = gmngr.readGraphs(result);

  if (r.fail()) {
    TRI_V8_THROW_EXCEPTION_MESSAGE(r.errorNumber(), r.errorMessage());
  }

  if (!result.isEmpty()) {
    auto ctx = std::make_shared<transaction::StandaloneContext>(vocbase);
    TRI_V8_RETURN(TRI_VPackToV8(isolate, result.slice().get("graphs"), ctx->getVPackOptions()));
  }

  TRI_V8_RETURN_UNDEFINED();
  TRI_V8_TRY_CATCH_END
}

static void JS_GetGraphKeys(v8::FunctionCallbackInfo<v8::Value> const& args) {
  TRI_V8_TRY_CATCH_BEGIN(isolate);
  v8::HandleScope scope(isolate);

  auto& vocbase = GetContextVocBase(isolate);

  GraphManager gmngr{vocbase};
  VPackBuilder result;
  Result r = gmngr.readGraphKeys(result);

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
  // TODO Needs to return a wrapped Graph!
  TRI_V8_TRY_CATCH_BEGIN(isolate);
  v8::HandleScope scope(isolate);

  if (args.Length() < 1) {
    TRI_V8_THROW_EXCEPTION_USAGE(
        "_create(graphName, edgeDefinitions, orphanCollections, options)");
  } else if (!args[0]->IsString()) {
    TRI_V8_THROW_EXCEPTION(TRI_ERROR_GRAPH_CREATE_MISSING_NAME);
  }
  std::string graphName = TRI_ObjectToString(isolate, args[0]);
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
  if (args.Length() >= 4 && !args[3]->IsNullOrUndefined()) {
    builder.add(VPackValue("options"));
    TRI_V8ToVPack(isolate, builder, args[3], true, true);
    builder.close();
  }

  builder.close();

  auto& vocbase = GetContextVocBase(isolate);

  GraphManager gmngr{vocbase};
  OperationResult r = gmngr.createGraph(builder.slice(), false);

  if (r.fail()) {
    TRI_V8_THROW_EXCEPTION_MESSAGE(r.errorNumber(), r.errorMessage());
  }

  auto graph = gmngr.lookupGraphByName(graphName);
  if (graph.fail()) {
    TRI_V8_THROW_EXCEPTION_MESSAGE(graph.errorNumber(), graph.errorMessage());
  }
  TRI_ASSERT(graph.get() != nullptr);

  VPackBuilder result;
  result.openObject();
  graph.get()->graphForClient(result);
  result.close();

  TRI_V8_RETURN(TRI_VPackToV8(isolate, result.slice()));
  TRI_V8_TRY_CATCH_END
}

static void JS_AddEdgeDefinitions(v8::FunctionCallbackInfo<v8::Value> const& args) {
  TRI_V8_TRY_CATCH_BEGIN(isolate);
  v8::HandleScope scope(isolate);

  if (args.Length() < 2) {
    TRI_V8_THROW_EXCEPTION_USAGE("_extendEdgeDefinitions(edgeDefinition)");
  }
  if (!args[0]->IsString()) {
    TRI_V8_THROW_EXCEPTION(TRI_ERROR_GRAPH_CREATE_MISSING_NAME);
  }
  std::string graphName = TRI_ObjectToString(isolate, args[0]);
  if (graphName.empty()) {
    TRI_V8_THROW_EXCEPTION(TRI_ERROR_GRAPH_CREATE_MISSING_NAME);
  }

  VPackBuilder edgeDefinition;
  TRI_V8ToVPack(isolate, edgeDefinition, args[1], false);

  auto& vocbase = GetContextVocBase(isolate);
  GraphManager gmngr{vocbase};
  auto graph = gmngr.lookupGraphByName(graphName);
  if (graph.fail()) {
    TRI_V8_THROW_EXCEPTION_MESSAGE(graph.errorNumber(), graph.errorMessage());
  }
  TRI_ASSERT(graph.get() != nullptr);

  auto ctx = transaction::V8Context::Create(vocbase, true);
  GraphOperations gops{*graph.get(), vocbase, ctx};
  OperationResult r = gops.addEdgeDefinition(edgeDefinition.slice(), false);

  if (r.fail()) {
    TRI_V8_THROW_EXCEPTION_MESSAGE(r.errorNumber(), r.errorMessage());
  }

  graph = gmngr.lookupGraphByName(graphName);
  if (graph.fail()) {
    TRI_V8_THROW_EXCEPTION_MESSAGE(graph.errorNumber(), graph.errorMessage());
  }
  TRI_ASSERT(graph.get() != nullptr);

  VPackBuilder result;
  result.openObject();
  graph.get()->graphForClient(result);
  result.close();

  TRI_V8_RETURN(TRI_VPackToV8(isolate, result.slice()));
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
  std::string graphName = TRI_ObjectToString(isolate, args[0]);
  if (graphName.empty()) {
    TRI_V8_THROW_EXCEPTION(TRI_ERROR_GRAPH_CREATE_MISSING_NAME);
  }

  VPackBuilder edgeDefinition;
  TRI_V8ToVPack(isolate, edgeDefinition, args[1], false);

  auto& vocbase = GetContextVocBase(isolate);
  GraphManager gmngr{vocbase};
  auto graph = gmngr.lookupGraphByName(graphName);
  if (graph.fail()) {
    TRI_V8_THROW_EXCEPTION_MESSAGE(graph.errorNumber(), graph.errorMessage());
  }
  TRI_ASSERT(graph.get() != nullptr);

  auto ctx = transaction::V8Context::Create(vocbase, true);
  GraphOperations gops{*graph.get(), vocbase, ctx};
  OperationResult r =
      gops.editEdgeDefinition(edgeDefinition.slice(), false,
                              edgeDefinition.slice().get("collection").copyString());

  if (r.fail()) {
    TRI_V8_THROW_EXCEPTION_MESSAGE(r.errorNumber(), r.errorMessage());
  }

  graph = gmngr.lookupGraphByName(graphName);
  if (graph.fail()) {
    TRI_V8_THROW_EXCEPTION_MESSAGE(graph.errorNumber(), graph.errorMessage());
  }
  TRI_ASSERT(graph.get() != nullptr);

  VPackBuilder result;
  result.openObject();
  graph.get()->graphForClient(result);
  result.close();

  TRI_V8_RETURN(TRI_VPackToV8(isolate, result.slice()));
  TRI_V8_TRY_CATCH_END
}

static void JS_RemoveVertexCollection(v8::FunctionCallbackInfo<v8::Value> const& args) {
  TRI_V8_TRY_CATCH_BEGIN(isolate);
  v8::HandleScope scope(isolate);

  if (args.Length() < 2) {
    TRI_V8_THROW_EXCEPTION_USAGE(
        "_removeVertexCollection(vertexName, dropCollection)");
  }
  if (!args[0]->IsString()) {
    TRI_V8_THROW_EXCEPTION(TRI_ERROR_GRAPH_CREATE_MISSING_NAME);
  }
  if (!args[1]->IsString()) {
    TRI_V8_THROW_EXCEPTION_USAGE(
        "_removeVertexCollection(vertexName, dropCollection)");
  }
  std::string graphName = TRI_ObjectToString(isolate, args[0]);
  std::string vertexName = TRI_ObjectToString(isolate, args[1]);
  if (graphName.empty()) {
    TRI_V8_THROW_EXCEPTION(TRI_ERROR_GRAPH_CREATE_MISSING_NAME);
  }
  if (vertexName.empty()) {
    TRI_V8_THROW_EXCEPTION_USAGE(
        "_removeVertexCollection(vertexName, dropCollection)");
  }
  bool dropCollection = false;
  if (args.Length() >= 3) {
    dropCollection = TRI_ObjectToBoolean(isolate, args[2]);
  }

  auto& vocbase = GetContextVocBase(isolate);
  GraphManager gmngr{vocbase};
  auto graph = gmngr.lookupGraphByName(graphName);
  if (graph.fail()) {
    TRI_V8_THROW_EXCEPTION_MESSAGE(graph.errorNumber(), graph.errorMessage());
  }
  TRI_ASSERT(graph.get() != nullptr);

  VPackBuilder builder;
  builder.openObject();
  builder.add("collection", VPackValue(vertexName));
  builder.close();

  auto ctx = transaction::V8Context::Create(vocbase, true);
  GraphOperations gops{*graph.get(), vocbase, ctx};
  OperationResult r = gops.eraseOrphanCollection(false, vertexName, dropCollection);

  if (r.fail()) {
    TRI_V8_THROW_EXCEPTION_MESSAGE(r.errorNumber(), r.errorMessage());
  }

  graph = gmngr.lookupGraphByName(graphName);
  if (graph.fail()) {
    TRI_V8_THROW_EXCEPTION_MESSAGE(graph.errorNumber(), graph.errorMessage());
  }
  TRI_ASSERT(graph.get() != nullptr);

  VPackBuilder result;
  result.openObject();
  graph.get()->graphForClient(result);
  result.close();

  TRI_V8_RETURN(TRI_VPackToV8(isolate, result.slice()));
  TRI_V8_TRY_CATCH_END
}

static void JS_AddVertexCollection(v8::FunctionCallbackInfo<v8::Value> const& args) {
  TRI_V8_TRY_CATCH_BEGIN(isolate);
  v8::HandleScope scope(isolate);

  if (args.Length() < 2) {
    TRI_V8_THROW_EXCEPTION_USAGE(
        "_addVertexCollection(vertexName, createCollection)");
  }
  if (!args[0]->IsString()) {
    TRI_V8_THROW_EXCEPTION(TRI_ERROR_GRAPH_CREATE_MISSING_NAME);
  }
  if (!args[1]->IsString()) {
    TRI_V8_THROW_EXCEPTION_USAGE(
        "_addVertexCollection(vertexName, createCollection)");
  }
  std::string graphName = TRI_ObjectToString(isolate, args[0]);
  std::string vertexName = TRI_ObjectToString(isolate, args[1]);
  if (graphName.empty()) {
    TRI_V8_THROW_EXCEPTION(TRI_ERROR_GRAPH_CREATE_MISSING_NAME);
  }
  if (vertexName.empty()) {
    TRI_V8_THROW_EXCEPTION_USAGE(
        "_addVertexCollection(vertexName, createCollection)");
  }
  bool createCollection = true;
  if (args.Length() >= 3) {
    createCollection = TRI_ObjectToBoolean(isolate, args[2]);
  }

  auto& vocbase = GetContextVocBase(isolate);
  auto ctx = transaction::V8Context::Create(vocbase, true);

  GraphManager gmngr{vocbase};
  auto graph = gmngr.lookupGraphByName(graphName);
  if (graph.fail()) {
    TRI_V8_THROW_EXCEPTION_MESSAGE(graph.errorNumber(), graph.errorMessage());
  }
  TRI_ASSERT(graph.get() != nullptr);

  GraphOperations gops{*graph.get(), vocbase, ctx};

  VPackBuilder builder;
  builder.openObject();
  builder.add("collection", VPackValue(vertexName));
  builder.close();

  OperationResult r = gops.addOrphanCollection(builder.slice(), false, createCollection);

  if (r.fail()) {
    TRI_V8_THROW_EXCEPTION_MESSAGE(r.errorNumber(), r.errorMessage());
  }

  graph = gmngr.lookupGraphByName(graphName);
  if (graph.fail()) {
    TRI_V8_THROW_EXCEPTION_MESSAGE(graph.errorNumber(), graph.errorMessage());
  }
  TRI_ASSERT(graph.get() != nullptr);

  VPackBuilder result;
  result.openObject();
  graph.get()->graphForClient(result);
  result.close();

  TRI_V8_RETURN(TRI_VPackToV8(isolate, result.slice()));
  TRI_V8_TRY_CATCH_END
}

static void JS_DropEdgeDefinition(v8::FunctionCallbackInfo<v8::Value> const& args) {
  TRI_V8_TRY_CATCH_BEGIN(isolate);
  v8::HandleScope scope(isolate);

  if (args.Length() < 2) {
    TRI_V8_THROW_EXCEPTION_USAGE(
        "_deleteEdgeDefinition(edgeCollection, dropCollection)");
  }
  if (!args[0]->IsString()) {
    TRI_V8_THROW_EXCEPTION(TRI_ERROR_GRAPH_CREATE_MISSING_NAME);
  }
  if (!args[1]->IsString()) {
    TRI_V8_THROW_EXCEPTION_USAGE(
        "_deleteEdgeDefinition(edgeCollection, dropCollection)");
  }
  std::string graphName = TRI_ObjectToString(isolate, args[0]);
  std::string edgeDefinitionName = TRI_ObjectToString(isolate, args[1]);
  if (graphName.empty()) {
    TRI_V8_THROW_EXCEPTION(TRI_ERROR_GRAPH_CREATE_MISSING_NAME);
  }
  if (edgeDefinitionName.empty()) {
    TRI_V8_THROW_EXCEPTION_USAGE(
        "_deleteEdgeDefinition(edgeCollection, dropCollection)");
  }

  bool dropCollections = false;
  if (args.Length() >= 3) {
    dropCollections = TRI_ObjectToBoolean(isolate, args[2]);
  }

  auto& vocbase = GetContextVocBase(isolate);

  GraphManager gmngr{vocbase};
  auto graph = gmngr.lookupGraphByName(graphName);
  if (graph.fail()) {
    TRI_V8_THROW_EXCEPTION_MESSAGE(graph.errorNumber(), graph.errorMessage());
  }
  TRI_ASSERT(graph.get() != nullptr);

  auto ctx = transaction::V8Context::Create(vocbase, true);
  GraphOperations gops{*graph.get(), vocbase, ctx};
  OperationResult r = gops.eraseEdgeDefinition(false, edgeDefinitionName, dropCollections);

  if (r.fail()) {
    TRI_V8_THROW_EXCEPTION_MESSAGE(r.errorNumber(), r.errorMessage());
  }

  graph = gmngr.lookupGraphByName(graphName);
  if (graph.fail()) {
    TRI_V8_THROW_EXCEPTION_MESSAGE(graph.errorNumber(), graph.errorMessage());
  }
  TRI_ASSERT(graph.get() != nullptr);

  VPackBuilder result;
  result.openObject();
  graph.get()->graphForClient(result);
  result.close();

  TRI_V8_RETURN(TRI_VPackToV8(isolate, result.slice()));
  TRI_V8_TRY_CATCH_END
}

static void InitV8GeneralGraphClass(v8::Handle<v8::Context> context, TRI_vocbase_t* vocbase,
                                    TRI_v8_global_t* v8g, v8::Isolate* isolate) {
  /* FULL API
   * _edgeCollections
   * _vertexCollections(bool excludeOrphans)
   * _EDGES
   * _INEDGES
   * _OUTEDGES
   * _edges
   * _vertices
   * _fromVertex(edgeId)
   * _toVertex(edgeId)
   * _getEdgeCollectionByName
   * _getVertexCollectionByName
   * _neighbors
   * _commonNeighbors
   * _countCommonNeighbors
   * _commonProperties
   * _countCommonProperties
   * _paths
   * _shortestPath
   * _distanceTo
   * _absoluteEccentricity
   * _farness
   * _absoluteCloseness
   * _eccentricity
   * _closeness
   * _absoluteBetweenness
   * _betweenness
   * _radius
   * _diameter
   * _orphanCollections
   * _renameVertexCollection
   * _getConnectingEdges
   */

  v8::Handle<v8::ObjectTemplate> rt;
  v8::Handle<v8::FunctionTemplate> ft;

  ft = v8::FunctionTemplate::New(isolate);
  ft->SetClassName(TRI_V8_ASCII_STRING(isolate, "ArangoGraph"));

  rt = ft->InstanceTemplate();
  rt->SetInternalFieldCount(2);

  TRI_AddMethodVocbase(isolate, rt,
                       TRI_V8_ASCII_STRING(isolate, "_addVertexCollection"),
                       JS_AddVertexCollection);
  TRI_AddMethodVocbase(isolate, rt,
                       TRI_V8_ASCII_STRING(isolate, "_deleteEdgeDefinition"),
                       JS_DropEdgeDefinition);
  TRI_AddMethodVocbase(isolate, rt,
                       TRI_V8_ASCII_STRING(isolate, "_editEdgeDefinitions"),
                       JS_EditEdgeDefinitions);
  TRI_AddMethodVocbase(isolate, rt,
                       TRI_V8_ASCII_STRING(isolate, "_extendEdgeDefinitions"),
                       JS_AddEdgeDefinitions);
  TRI_AddMethodVocbase(isolate, rt,
                       TRI_V8_ASCII_STRING(isolate, "_removeVertexCollection"),
                       JS_RemoveVertexCollection);

  v8g->GeneralGraphTempl.Reset(isolate, rt);
  ft->SetClassName(TRI_V8_ASCII_STRING(isolate, "ArangoGraphCtor"));
  TRI_AddGlobalFunctionVocbase(isolate,
                               TRI_V8_ASCII_STRING(isolate, "ArangoGraphCtor"),
                               ft->GetFunction(TRI_IGETC).FromMaybe(v8::Local<v8::Function>()), true);

  // TODO WE DO NOT NEED THIS. Update _create to return a graph object properly
  // register the global object
  v8::Handle<v8::Object> aa = rt->NewInstance(TRI_IGETC).FromMaybe(v8::Local<v8::Object>());
  if (!aa.IsEmpty()) {
    TRI_AddGlobalVariableVocbase(isolate,
                                 TRI_V8_ASCII_STRING(isolate, "ArangoGraph"), aa);
  }
}

#ifdef USE_ENTERPRISE
static void InitV8SmartGraphClass(v8::Handle<v8::Context> context, TRI_vocbase_t* vocbase,
                                  TRI_v8_global_t* v8g, v8::Isolate* isolate) {
  v8::Handle<v8::ObjectTemplate> rt;
  v8::Handle<v8::FunctionTemplate> ft;

  ft = v8::FunctionTemplate::New(isolate);
  ft->SetClassName(TRI_V8_ASCII_STRING(isolate, "ArangoSmartGraph"));

  rt = ft->InstanceTemplate();
  rt->SetInternalFieldCount(2);

  TRI_AddMethodVocbase(isolate, rt,
                       TRI_V8_ASCII_STRING(isolate, "_addVertexCollection"),
                       JS_AddVertexCollection);

  TRI_AddMethodVocbase(isolate, rt,
                       TRI_V8_ASCII_STRING(isolate, "_deleteEdgeDefinition"),
                       JS_DropEdgeDefinition);

  TRI_AddMethodVocbase(isolate, rt,
                       TRI_V8_ASCII_STRING(isolate, "_editEdgeDefinitions"),
                       JS_EditEdgeDefinitions);

  TRI_AddMethodVocbase(isolate, rt,
                       TRI_V8_ASCII_STRING(isolate, "_extendEdgeDefinitions"),
                       JS_AddEdgeDefinitions);

  TRI_AddMethodVocbase(isolate, rt,
                       TRI_V8_ASCII_STRING(isolate, "_removeVertexCollection"),
                       JS_RemoveVertexCollection);

  v8g->SmartGraphTempl.Reset(isolate, rt);
  ft->SetClassName(TRI_V8_ASCII_STRING(isolate, "ArangoSmartGraphCtor"));
  TRI_AddGlobalFunctionVocbase(isolate,
                               TRI_V8_ASCII_STRING(isolate,
                                                   "ArangoSmartGraphCtor"),
                               ft->GetFunction(TRI_IGETC).FromMaybe(v8::Local<v8::Function>()), true);

  // register the global object
  v8::Handle<v8::Object> aa = rt->NewInstance(TRI_IGETC).FromMaybe(v8::Local<v8::Object>());
  if (!aa.IsEmpty()) {
    TRI_AddGlobalVariableVocbase(
        isolate, TRI_V8_ASCII_STRING(isolate, "ArangoSmartGraph"), aa);
  }
}
#endif

static void InitV8GeneralGraphModule(v8::Handle<v8::Context> context, TRI_vocbase_t* vocbase,
                                     TRI_v8_global_t* v8g, v8::Isolate* isolate) {
  /* These functions still have a JS only implementation
   * JS ONLY:
   * _edgeDefinitions
   * _extendEdgeDefinitions
   * _relation
   * _registerCompatibilityFunctions
   */

  /* TODO
   * _create // SG => Potentially returns SG
   * _graph // SG => Potentially returns SG
   */
  v8::Handle<v8::ObjectTemplate> rt;
  v8::Handle<v8::FunctionTemplate> ft;
  ft = v8::FunctionTemplate::New(isolate);
  ft->SetClassName(TRI_V8_ASCII_STRING(isolate, "ArangoGeneralGraphModule"));
  rt = ft->InstanceTemplate();
  rt->SetInternalFieldCount(0);

  TRI_AddMethodVocbase(isolate, rt, TRI_V8_ASCII_STRING(isolate, "_create"), JS_CreateGraph);
  TRI_AddMethodVocbase(isolate, rt, TRI_V8_ASCII_STRING(isolate, "_drop"), JS_DropGraph);
  TRI_AddMethodVocbase(isolate, rt, TRI_V8_ASCII_STRING(isolate, "_exists"), JS_GraphExists);
  TRI_AddMethodVocbase(isolate, rt, TRI_V8_ASCII_STRING(isolate, "_graph"), JS_GetGraph);
  TRI_AddMethodVocbase(isolate, rt, TRI_V8_ASCII_STRING(isolate, "_list"), JS_GetGraphKeys);
  TRI_AddMethodVocbase(isolate, rt, TRI_V8_ASCII_STRING(isolate, "_listObjects"), JS_GetGraphs);
  TRI_AddMethodVocbase(isolate, rt,
                       TRI_V8_ASCII_STRING(isolate, "_renameCollection"),
                       JS_RenameGraphCollection);

  v8g->GeneralGraphModuleTempl.Reset(isolate, rt);
  ft->SetClassName(
      TRI_V8_ASCII_STRING(isolate, "ArangoGeneralGraphModuleCtor"));
  TRI_AddGlobalFunctionVocbase(
      isolate, TRI_V8_ASCII_STRING(isolate, "ArangoGeneralGraphModuleCtor"),
      ft->GetFunction(TRI_IGETC).FromMaybe(v8::Local<v8::Function>()), true);

  // register the global object
  v8::Handle<v8::Object> aa = rt->NewInstance(TRI_IGETC).FromMaybe(v8::Local<v8::Object>());
  if (!aa.IsEmpty()) {
    TRI_AddGlobalVariableVocbase(
        isolate, TRI_V8_ASCII_STRING(isolate, "ArangoGeneralGraphModule"), aa);
  }
}

void TRI_InitV8GeneralGraph(v8::Handle<v8::Context> context, TRI_vocbase_t* vocbase,
                            TRI_v8_global_t* v8g, v8::Isolate* isolate) {
  InitV8GeneralGraphModule(context, vocbase, v8g, isolate);
  InitV8GeneralGraphClass(context, vocbase, v8g, isolate);
#ifdef USE_ENTERPRISE
  InitV8SmartGraphClass(context, vocbase, v8g, isolate);
#endif
}
