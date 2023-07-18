////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2014-2023 ArangoDB GmbH, Cologne, Germany
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
/// @author Markus Pfeiffer
////////////////////////////////////////////////////////////////////////////////

#include "v8-pregel.h"

#include "ApplicationFeatures/ApplicationServer.h"
#include "Cluster/ServerState.h"
#include "Pregel/AggregatorHandler.h"
#include "Pregel/Conductor/Conductor.h"
#include "Pregel/ExecutionNumber.h"
#include "Pregel/PregelFeature.h"
#include "Pregel/PregelOptions.h"
#include "Pregel/StatusWriter/CollectionStatusWriter.h"
#include "Pregel/Worker/Worker.h"
#include "Utils/OperationResult.h"
#include "V8/v8-conv.h"
#include "V8/v8-globals.h"
#include "V8/v8-utils.h"
#include "V8/v8-vpack.h"
#include "V8Server/v8-vocbaseprivate.h"

#include <velocypack/Builder.h>
#include <velocypack/Slice.h>
#include <velocypack/Iterator.h>

using namespace arangodb;

static void JS_PregelStart(v8::FunctionCallbackInfo<v8::Value> const& args) {
  TRI_V8_TRY_CATCH_BEGIN(isolate);
  v8::HandleScope scope(isolate);
  ServerState* ss = ServerState::instance();
  if (ss->isRunningInCluster() && !ss->isCoordinator()) {
    TRI_V8_THROW_EXCEPTION_USAGE(
        "Only call on coordinator or in single server mode");
  }

  // check the arguments
  uint32_t const argLength = args.Length();
  if (argLength < 3 || !args[0]->IsString()) {
    // TODO extend this for named graphs, use the Graph class
    TRI_V8_THROW_EXCEPTION_USAGE(
        "_pregelStart(<algorithm>, <vertexCollections>,"
        "<edgeCollections>[, {maxGSS:100, ...}]");
  }
  auto parse = [](v8::Isolate* isolate, v8::Local<v8::Value> const& value,
                  std::vector<std::string>& out) {
    auto context = TRI_IGETC;
    v8::Handle<v8::Array> array = v8::Handle<v8::Array>::Cast(value);
    uint32_t const n = array->Length();
    for (uint32_t i = 0; i < n; ++i) {
      v8::Handle<v8::Value> obj =
          array->Get(context, i).FromMaybe(v8::Local<v8::Value>());
      if (obj->IsString()) {
        out.push_back(TRI_ObjectToString(isolate, obj));
      }
    }
  };

  std::string algorithm = TRI_ObjectToString(isolate, args[0]);
  std::vector<std::string> paramVertices, paramEdges;
  if (args[1]->IsArray()) {
    parse(isolate, args[1], paramVertices);
  } else if (args[1]->IsString()) {
    paramVertices.push_back(TRI_ObjectToString(isolate, args[1]));
  } else {
    TRI_V8_THROW_EXCEPTION_USAGE(
        "Specify an array of vertex collections (or a string)");
  }
  if (paramVertices.size() == 0) {
    TRI_V8_THROW_EXCEPTION_USAGE("Specify at least one vertex collection");
  }
  if (args[2]->IsArray()) {
    parse(isolate, args[2], paramEdges);
  } else if (args[2]->IsString()) {
    paramEdges.push_back(TRI_ObjectToString(isolate, args[2]));
  } else {
    TRI_V8_THROW_EXCEPTION_USAGE(
        "Specify an array of edge collections (or a string)");
  }
  if (paramEdges.size() == 0) {
    TRI_V8_THROW_EXCEPTION_USAGE("Specify at least one edge collection");
  }
  VPackBuilder paramBuilder;
  if (argLength >= 4 && args[3]->IsObject()) {
    TRI_V8ToVPack(isolate, paramBuilder, args[3], false);
  }

  std::unordered_map<std::string, std::vector<std::string>>
      paramEdgeCollectionRestrictions;
  if (paramBuilder.slice().isObject()) {
    VPackSlice s = paramBuilder.slice().get("edgeCollectionRestrictions");
    if (s.isObject()) {
      for (auto const& it : VPackObjectIterator(s)) {
        if (!it.value.isArray()) {
          continue;
        }
        auto& restrictions =
            paramEdgeCollectionRestrictions[it.key.copyString()];
        for (auto const& it2 : VPackArrayIterator(it.value)) {
          restrictions.emplace_back(it2.copyString());
        }
      }
    }
  }
  auto pregelOptions = pregel::PregelOptions{
      .algorithm = algorithm,
      .userParameters = paramBuilder,
      .graphSource = {
          {pregel::GraphCollectionNames{.vertexCollections = paramVertices,
                                        .edgeCollections = paramEdges}},
          {paramEdgeCollectionRestrictions}}};

  auto& vocbase = GetContextVocBase(isolate);
  if (!vocbase.server().hasFeature<arangodb::pregel::PregelFeature>()) {
    TRI_V8_THROW_EXCEPTION_MESSAGE(TRI_ERROR_FAILED, "pregel is not enabled");
  }
  auto& pregel = vocbase.server().getFeature<arangodb::pregel::PregelFeature>();
  auto res = pregel.startExecution(vocbase, pregelOptions);
  if (res.fail()) {
    TRI_V8_THROW_EXCEPTION(res.result());
  }

  auto result = TRI_V8UInt64String<uint64_t>(isolate, res.get().value);
  TRI_V8_RETURN(result);

  TRI_V8_TRY_CATCH_END
}

static void JS_PregelStatus(v8::FunctionCallbackInfo<v8::Value> const& args) {
  TRI_V8_TRY_CATCH_BEGIN(isolate);
  auto handlePregelHistoryV8Result = [&](ResultT<OperationResult> const& result,
                                         bool onlyReturnFirstAqlResultEntry =
                                             false) -> void {
    if (result.fail()) {
      // check outer ResultT
      TRI_V8_THROW_EXCEPTION_MESSAGE(result.errorNumber(),
                                     result.errorMessage());
    }
    if (result.get().fail()) {
      // check inner OperationResult
      std::string message = std::string{result.get().errorMessage()};
      if (result.get().errorNumber() == TRI_ERROR_ARANGO_DOCUMENT_NOT_FOUND) {
        // For reasons, not all OperationResults deliver the expected message.
        // Therefore, we need set up the message properly and manually here.
        message = Result(TRI_ERROR_ARANGO_DOCUMENT_NOT_FOUND).errorMessage();
      }

      TRI_V8_THROW_EXCEPTION_MESSAGE(result.get().errorNumber(), message);
    }
    if (result.get().hasSlice()) {
      if (result->slice().isNone()) {
        // Truncate does not deliver a proper slice in a Cluster.
        TRI_V8_RETURN(TRI_VPackToV8(isolate, VPackSlice::trueSlice()));
      } else {
        if (onlyReturnFirstAqlResultEntry) {
          TRI_ASSERT(result->slice().isArray());
          // due to AQL returning "null" values in case a document does not
          // exist ....
          if (result->slice().at(0).isNull()) {
            Result nf = Result(TRI_ERROR_ARANGO_DOCUMENT_NOT_FOUND);
            TRI_V8_THROW_EXCEPTION_MESSAGE(nf.errorNumber(), nf.errorMessage());
          } else {
            TRI_V8_RETURN(TRI_VPackToV8(isolate, result.get().slice().at(0)));
          }
        } else {
          TRI_V8_RETURN(TRI_VPackToV8(isolate, result.get().slice()));
        }
      }
    } else {
      // Should always have a slice, doing this check to be sure.
      // (e.g. a truncate might not return a Slice)
      TRI_V8_RETURN(TRI_VPackToV8(isolate, VPackSlice::trueSlice()));
    }
  };
  v8::HandleScope scope(isolate);

  auto& vocbase = GetContextVocBase(isolate);
  if (!vocbase.server().hasFeature<arangodb::pregel::PregelFeature>()) {
    TRI_V8_THROW_EXCEPTION_MESSAGE(TRI_ERROR_FAILED, "pregel is not enabled");
  }

  // check the arguments
  uint32_t const argLength = args.Length();
  if (argLength == 0) {
    pregel::statuswriter::CollectionStatusWriter cWriter{vocbase};
    handlePregelHistoryV8Result(cWriter.readAllNonExpiredResults());
    return;
  }

  if (argLength != 1 || (!args[0]->IsNumber() && !args[0]->IsString())) {
    // TODO extend this for named graphs, use the Graph class
    TRI_V8_THROW_EXCEPTION_USAGE("_pregelStatus(<executionNum>]");
  }

  auto executionNum = arangodb::pregel::ExecutionNumber{
      TRI_ObjectToUInt64(isolate, args[0], true)};
  pregel::statuswriter::CollectionStatusWriter cWriter{vocbase, executionNum};
  handlePregelHistoryV8Result(cWriter.readResult(), true);
  return;

  return;
  TRI_V8_TRY_CATCH_END
}

static void JS_PregelCancel(v8::FunctionCallbackInfo<v8::Value> const& args) {
  TRI_V8_TRY_CATCH_BEGIN(isolate);
  v8::HandleScope scope(isolate);

  // check the arguments
  uint32_t const argLength = args.Length();
  if (argLength != 1 || !(args[0]->IsNumber() || args[0]->IsString())) {
    // TODO extend this for named graphs, use the Graph class
    TRI_V8_THROW_EXCEPTION_USAGE("_pregelCancel(<executionNum>)");
  }

  auto& vocbase = GetContextVocBase(isolate);
  if (!vocbase.server().hasFeature<arangodb::pregel::PregelFeature>()) {
    TRI_V8_THROW_EXCEPTION_MESSAGE(TRI_ERROR_FAILED, "pregel is not enabled");
  }
  auto& pregel = vocbase.server().getFeature<arangodb::pregel::PregelFeature>();

  auto executionNum = arangodb::pregel::ExecutionNumber{
      TRI_ObjectToUInt64(isolate, args[0], true)};

  auto canceled = pregel.cancel(executionNum);
  if (canceled.fail()) {
    TRI_V8_THROW_EXCEPTION_MESSAGE(canceled.errorNumber(),
                                   canceled.errorMessage());
  }

  TRI_V8_RETURN_UNDEFINED();
  TRI_V8_TRY_CATCH_END
}

static void JS_PregelAQLResult(
    v8::FunctionCallbackInfo<v8::Value> const& args) {
  TRI_V8_TRY_CATCH_BEGIN(isolate);
  v8::HandleScope scope(isolate);

  // check the arguments
  uint32_t const argLength = args.Length();
  if (argLength <= 1 || !(args[0]->IsNumber() || args[0]->IsString())) {
    // TODO extend this for named graphs, use the Graph class
    TRI_V8_THROW_EXCEPTION_USAGE("_pregelAqlResult(<executionNum>[, <withId])");
  }

  bool withId = false;
  if (argLength == 2) {
    withId = TRI_ObjectToBoolean(isolate, args[1]);
  }

  auto& vocbase = GetContextVocBase(isolate);
  if (!vocbase.server().hasFeature<arangodb::pregel::PregelFeature>()) {
    TRI_V8_THROW_EXCEPTION_MESSAGE(TRI_ERROR_FAILED, "pregel is not enabled");
  }
  auto& pregel = vocbase.server().getFeature<arangodb::pregel::PregelFeature>();

  auto executionNum = arangodb::pregel::ExecutionNumber{
      TRI_ObjectToUInt64(isolate, args[0], true)};
  if (ServerState::instance()->isSingleServerOrCoordinator()) {
    VPackBuilder docs;
    auto c = pregel.conductor(executionNum);
    if (!c) {
      // check for actor run
      auto pregelResults = pregel.getResults(executionNum);
      if (!pregelResults.ok()) {
        TRI_V8_THROW_EXCEPTION_USAGE("Execution number is invalid");
        return;
      }
      {
        VPackArrayBuilder ab(&docs);
        docs.add(VPackArrayIterator(pregelResults.get().results.slice()));
      }
      TRI_V8_THROW_EXCEPTION_USAGE("Execution number is invalid");
    } else {
      c->collectAQLResults(docs, withId);
    }
    if (docs.isEmpty()) {
      TRI_V8_RETURN_NULL();
    }
    TRI_ASSERT(docs.slice().isArray());

    VPackOptions resultOptions = VPackOptions::Defaults;
    auto documents = TRI_VPackToV8(isolate, docs.slice(), &resultOptions);
    TRI_V8_RETURN(documents);
  } else {
    TRI_V8_THROW_EXCEPTION_USAGE("Only valid on the coordinator");
  }

  TRI_V8_RETURN_UNDEFINED();
  TRI_V8_TRY_CATCH_END
}

static void JS_PregelHistory(v8::FunctionCallbackInfo<v8::Value> const& args) {
  TRI_V8_TRY_CATCH_BEGIN(isolate);
  v8::HandleScope scope(isolate);

  auto handlePregelHistoryV8Result = [&](ResultT<OperationResult> const& result,
                                         bool onlyReturnFirstAqlResultEntry =
                                             false) -> void {
    if (result.fail()) {
      // check outer ResultT
      TRI_V8_THROW_EXCEPTION_MESSAGE(result.errorNumber(),
                                     result.errorMessage());
    }
    if (result.get().fail()) {
      // check inner OperationResult
      std::string message = std::string{result.get().errorMessage()};
      if (result.get().errorNumber() == TRI_ERROR_ARANGO_DOCUMENT_NOT_FOUND) {
        // For reasons, not all OperationResults deliver the expected message.
        // Therefore, we need set up the message properly and manually here.
        message = Result(TRI_ERROR_ARANGO_DOCUMENT_NOT_FOUND).errorMessage();
      }

      TRI_V8_THROW_EXCEPTION_MESSAGE(result.get().errorNumber(), message);
    }
    if (result.get().hasSlice()) {
      if (result->slice().isNone()) {
        // Truncate does not deliver a proper slice in a Cluster.
        TRI_V8_RETURN(TRI_VPackToV8(isolate, VPackSlice::trueSlice()));
      } else {
        if (onlyReturnFirstAqlResultEntry) {
          TRI_ASSERT(result->slice().isArray());
          // due to AQL returning "null" values in case a document does not
          // exist ....
          if (result->slice().at(0).isNull()) {
            Result nf = Result(TRI_ERROR_ARANGO_DOCUMENT_NOT_FOUND);
            TRI_V8_THROW_EXCEPTION_MESSAGE(nf.errorNumber(), nf.errorMessage());
          } else {
            TRI_V8_RETURN(TRI_VPackToV8(isolate, result.get().slice().at(0)));
          }
        } else {
          TRI_V8_RETURN(TRI_VPackToV8(isolate, result.get().slice()));
        }
      }
    } else {
      // Should always have a slice, doing this check to be sure.
      // (e.g. a truncate might not return a Slice)
      TRI_V8_RETURN(TRI_VPackToV8(isolate, VPackSlice::trueSlice()));
    }
  };

  auto& vocbase = GetContextVocBase(isolate);
  if (!vocbase.server().hasFeature<arangodb::pregel::PregelFeature>()) {
    TRI_V8_THROW_EXCEPTION_MESSAGE(TRI_ERROR_FAILED, "pregel is not enabled");
  }
  auto& pregel = vocbase.server().getFeature<arangodb::pregel::PregelFeature>();
  if (pregel.isStopping()) {
    handlePregelHistoryV8Result({Result(TRI_ERROR_SHUTTING_DOWN)});
    return;
  }

  // check the arguments
  uint32_t const argLength = args.Length();
  if (argLength == 0) {
    // Read all pregel history entries
    pregel::statuswriter::CollectionStatusWriter cWriter{vocbase};
    handlePregelHistoryV8Result(cWriter.readAllResults());
    return;
  }

  if (argLength != 1 || (!args[0]->IsNumber() && !args[0]->IsString())) {
    // TODO extend this for named graphs, use the Graph class
    TRI_V8_THROW_EXCEPTION_USAGE("_pregelHistory(<executionNum>]");
  }

  // Read single history entry
  auto executionNumber = arangodb::pregel::ExecutionNumber{
      TRI_ObjectToUInt64(isolate, args[0], true)};

  pregel::statuswriter::CollectionStatusWriter cWriter{vocbase,
                                                       executionNumber};
  handlePregelHistoryV8Result(cWriter.readResult(), true);
  return;
  TRI_V8_TRY_CATCH_END
}

static void JS_PregelHistoryRemove(
    v8::FunctionCallbackInfo<v8::Value> const& args) {
  TRI_V8_TRY_CATCH_BEGIN(isolate);
  v8::HandleScope scope(isolate);

  auto handlePregelHistoryV8Result = [&](ResultT<OperationResult> const& result,
                                         bool onlyReturnFirstAqlResultEntry =
                                             false) -> void {
    if (result.fail()) {
      // check outer ResultT
      TRI_V8_THROW_EXCEPTION_MESSAGE(result.errorNumber(),
                                     result.errorMessage());
    }
    if (result.get().fail()) {
      // check inner OperationResult
      std::string message = std::string{result.get().errorMessage()};
      if (result.get().errorNumber() == TRI_ERROR_ARANGO_DOCUMENT_NOT_FOUND) {
        // For reasons, not all OperationResults deliver the expected message.
        // Therefore, we need set up the message properly and manually here.
        message = Result(TRI_ERROR_ARANGO_DOCUMENT_NOT_FOUND).errorMessage();
      }

      TRI_V8_THROW_EXCEPTION_MESSAGE(result.get().errorNumber(), message);
    }
    if (result.get().hasSlice()) {
      if (result->slice().isNone()) {
        // Truncate does not deliver a proper slice in a Cluster.
        TRI_V8_RETURN(TRI_VPackToV8(isolate, VPackSlice::trueSlice()));
      } else {
        if (onlyReturnFirstAqlResultEntry) {
          TRI_ASSERT(result->slice().isArray());
          // due to AQL returning "null" values in case a document does not
          // exist ....
          if (result->slice().at(0).isNull()) {
            Result nf = Result(TRI_ERROR_ARANGO_DOCUMENT_NOT_FOUND);
            TRI_V8_THROW_EXCEPTION_MESSAGE(nf.errorNumber(), nf.errorMessage());
          } else {
            TRI_V8_RETURN(TRI_VPackToV8(isolate, result.get().slice().at(0)));
          }
        } else {
          TRI_V8_RETURN(TRI_VPackToV8(isolate, result.get().slice()));
        }
      }
    } else {
      // Should always have a slice, doing this check to be sure.
      // (e.g. a truncate might not return a Slice)
      TRI_V8_RETURN(TRI_VPackToV8(isolate, VPackSlice::trueSlice()));
    }
  };

  auto& vocbase = GetContextVocBase(isolate);
  if (!vocbase.server().hasFeature<arangodb::pregel::PregelFeature>()) {
    TRI_V8_THROW_EXCEPTION_MESSAGE(TRI_ERROR_FAILED, "pregel is not enabled");
  }
  auto& pregel = vocbase.server().getFeature<arangodb::pregel::PregelFeature>();
  if (pregel.isStopping()) {
    handlePregelHistoryV8Result({Result(TRI_ERROR_SHUTTING_DOWN)});
    return;
  }

  // check the arguments
  uint32_t const argLength = args.Length();
  if (argLength == 0) {
    // Delete all pregel history entries
    pregel::statuswriter::CollectionStatusWriter cWriter{vocbase};
    handlePregelHistoryV8Result(cWriter.deleteAllResults());
    return;
  }

  if (argLength != 1 || (!args[0]->IsNumber() && !args[0]->IsString())) {
    // TODO extend this for named graphs, use the Graph class
    TRI_V8_THROW_EXCEPTION_USAGE("_pregelHistoryRemove(<executionNum>]");
  }

  // Delete single history entry
  auto executionNumber = arangodb::pregel::ExecutionNumber{
      TRI_ObjectToUInt64(isolate, args[0], true)};
  pregel::statuswriter::CollectionStatusWriter cWriter{vocbase,
                                                       executionNumber};
  handlePregelHistoryV8Result(cWriter.deleteResult());
  return;
  TRI_V8_TRY_CATCH_END
}

void TRI_InitV8Pregel(v8::Isolate* isolate,
                      v8::Handle<v8::ObjectTemplate> ArangoDBNS) {
  TRI_AddMethodVocbase(isolate, ArangoDBNS,
                       TRI_V8_ASCII_STRING(isolate, "_pregelStart"),
                       JS_PregelStart);
  TRI_AddMethodVocbase(isolate, ArangoDBNS,
                       TRI_V8_ASCII_STRING(isolate, "_pregelStatus"),
                       JS_PregelStatus);
  TRI_AddMethodVocbase(isolate, ArangoDBNS,
                       TRI_V8_ASCII_STRING(isolate, "_pregelCancel"),
                       JS_PregelCancel);
  TRI_AddMethodVocbase(isolate, ArangoDBNS,
                       TRI_V8_ASCII_STRING(isolate, "_pregelAqlResult"),
                       JS_PregelAQLResult);
  TRI_AddMethodVocbase(isolate, ArangoDBNS,
                       TRI_V8_ASCII_STRING(isolate, "_pregelHistory"),
                       JS_PregelHistory);
  TRI_AddMethodVocbase(isolate, ArangoDBNS,
                       TRI_V8_ASCII_STRING(isolate, "_pregelHistoryRemove"),
                       JS_PregelHistoryRemove);
}
