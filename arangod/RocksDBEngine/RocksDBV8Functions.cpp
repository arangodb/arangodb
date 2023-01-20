////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2014-2022 ArangoDB GmbH, Cologne, Germany
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
/// @author Jan Steemann
////////////////////////////////////////////////////////////////////////////////

#include "RocksDBV8Functions.h"
#include "ApplicationFeatures/ApplicationServer.h"
#include "Aql/Functions.h"
#include "Basics/Exceptions.h"
#include "Basics/Result.h"
#include "Basics/StaticStrings.h"
#include "Cluster/ClusterFeature.h"
#include "Cluster/ClusterMethods.h"
#include "Cluster/ServerState.h"
#include "RestServer/FlushFeature.h"
#include "RocksDBEngine/RocksDBCollection.h"
#include "RocksDBEngine/RocksDBCommon.h"
#include "RocksDBEngine/RocksDBEngine.h"
#include "RocksDBEngine/RocksDBReplicationContext.h"
#include "RocksDBEngine/RocksDBReplicationContextGuard.h"
#include "RocksDBEngine/RocksDBReplicationManager.h"
#include "RocksDBEngine/RocksDBSettingsManager.h"
#include "StorageEngine/EngineSelectorFeature.h"
#include "Utils/ExecContext.h"
#include "V8/v8-conv.h"
#include "V8/v8-globals.h"
#include "V8/v8-utils.h"
#include "V8/v8-vpack.h"
#include "V8Server/v8-collection.h"
#include "V8Server/v8-externals.h"
#include "VocBase/LogicalCollection.h"

#include <v8.h>

using namespace arangodb;

/// flush the WAL
static void JS_FlushWal(v8::FunctionCallbackInfo<v8::Value> const& args) {
  TRI_V8_TRY_CATCH_BEGIN(isolate);
  v8::HandleScope scope(isolate);
  auto context = TRI_IGETC;

  bool waitForSync = false;
  bool flushColumnFamilies = false;

  if (args.Length() > 0) {
    if (args[0]->IsObject()) {
      v8::Handle<v8::Object> obj =
          args[0]->ToObject(TRI_IGETC).FromMaybe(v8::Local<v8::Object>());
      if (TRI_HasProperty(context, isolate, obj,
                          StaticStrings::WaitForSyncString)) {
        waitForSync = TRI_ObjectToBoolean(
            isolate,
            obj->Get(context, TRI_V8_ASCII_STD_STRING(
                                  isolate, StaticStrings::WaitForSyncString))
                .FromMaybe(v8::Local<v8::Value>()));
      }
      if (TRI_HasProperty(context, isolate, obj, "waitForCollector")) {
        flushColumnFamilies = TRI_ObjectToBoolean(
            isolate,
            obj->Get(context, TRI_V8_ASCII_STRING(isolate, "waitForCollector"))
                .FromMaybe(v8::Local<v8::Value>()));
      }
    } else {
      waitForSync = TRI_ObjectToBoolean(isolate, args[0]);

      if (args.Length() > 1) {
        flushColumnFamilies = TRI_ObjectToBoolean(isolate, args[1]);
      }
    }
  }

  TRI_GET_SERVER_GLOBALS(ArangodServer);
  v8g->server().getFeature<EngineSelectorFeature>().engine().flushWal(
      waitForSync, flushColumnFamilies);
  TRI_V8_RETURN_TRUE();
  TRI_V8_TRY_CATCH_END
}

/// this is just a stub
static void JS_WaitCollectorWal(
    v8::FunctionCallbackInfo<v8::Value> const& args) {
  TRI_V8_TRY_CATCH_BEGIN(isolate);
  v8::HandleScope scope(isolate);

  if (ServerState::instance()->isCoordinator()) {
    TRI_V8_THROW_EXCEPTION(TRI_ERROR_NOT_IMPLEMENTED);
  }

  // this is just a stub
  TRI_V8_RETURN_TRUE();
  TRI_V8_TRY_CATCH_END
}

/// this is just a stub
static void JS_TransactionsWal(
    v8::FunctionCallbackInfo<v8::Value> const& args) {
  TRI_V8_TRY_CATCH_BEGIN(isolate);
  v8::HandleScope scope(isolate);

  if (ServerState::instance()->isCoordinator()) {
    TRI_V8_THROW_EXCEPTION(TRI_ERROR_NOT_IMPLEMENTED);
  }

  // this is just a stub
  TRI_V8_RETURN_TRUE();
  TRI_V8_TRY_CATCH_END
}

/// this is just a stub
static void JS_PropertiesWal(v8::FunctionCallbackInfo<v8::Value> const& args) {
  TRI_V8_TRY_CATCH_BEGIN(isolate);
  v8::HandleScope scope(isolate);

  if (ServerState::instance()->isCoordinator()) {
    TRI_V8_THROW_EXCEPTION(TRI_ERROR_NOT_IMPLEMENTED);
  }

  // this is just a stub
  TRI_V8_RETURN_TRUE();
  TRI_V8_TRY_CATCH_END
}

static void JS_RecalculateCounts(
    v8::FunctionCallbackInfo<v8::Value> const& args) {
  TRI_V8_TRY_CATCH_BEGIN(isolate);
  v8::HandleScope scope(isolate);

  auto* collection = UnwrapCollection(isolate, args.Holder());

  if (!collection) {
    TRI_V8_THROW_EXCEPTION_INTERNAL("cannot extract collection");
  }

  if (!ExecContext::current().canUseCollection(collection->name(),
                                               auth::Level::RW)) {
    TRI_V8_THROW_EXCEPTION(TRI_ERROR_FORBIDDEN);
  }

  auto* physical = toRocksDBCollection(*collection);

  v8::Handle<v8::Value> result = v8::Number::New(
      isolate, static_cast<double>(physical->recalculateCounts()));

  TRI_V8_RETURN(result);
  TRI_V8_TRY_CATCH_END
}

static void JS_CompactCollection(
    v8::FunctionCallbackInfo<v8::Value> const& args) {
  TRI_V8_TRY_CATCH_BEGIN(isolate);
  v8::HandleScope scope(isolate);

  auto* collection = UnwrapCollection(isolate, args.Holder());

  if (!collection) {
    TRI_V8_THROW_EXCEPTION_INTERNAL("cannot extract collection");
  }

  auto* physical = toRocksDBCollection(*collection);
  physical->compact();

  TRI_V8_RETURN_UNDEFINED();
  TRI_V8_TRY_CATCH_END
}

static void JS_EstimateCollectionSize(
    v8::FunctionCallbackInfo<v8::Value> const& args) {
  TRI_V8_TRY_CATCH_BEGIN(isolate);
  v8::HandleScope scope(isolate);

  auto* collection = UnwrapCollection(isolate, args.Holder());

  if (!collection) {
    TRI_V8_THROW_EXCEPTION_INTERNAL("cannot extract collection");
  }

  auto* physical = toRocksDBCollection(*collection);
  VPackBuilder builder;
  physical->estimateSize(builder);

  v8::Handle<v8::Value> result = TRI_VPackToV8(isolate, builder.slice());
  TRI_V8_RETURN(result);
  TRI_V8_TRY_CATCH_END
}

static void JS_WaitForEstimatorSync(
    v8::FunctionCallbackInfo<v8::Value> const& args) {
  TRI_V8_TRY_CATCH_BEGIN(isolate);
  v8::HandleScope scope(isolate);

  TRI_GET_SERVER_GLOBALS(ArangodServer);

  // release all unused ticks from flush feature
  v8g->server().getFeature<FlushFeature>().releaseUnusedTicks();

  // force-flush
  RocksDBEngine& engine =
      v8g->server().getFeature<EngineSelectorFeature>().engine<RocksDBEngine>();
  engine.settingsManager()->sync(/*force*/ true);

  v8g->server()
      .getFeature<EngineSelectorFeature>()
      .engine()
      .waitForEstimatorSync(std::chrono::seconds(10));

  TRI_V8_RETURN_TRUE();
  TRI_V8_TRY_CATCH_END
}

#ifdef ARANGODB_USE_GOOGLE_TESTS
static void JS_WalRecoveryStartSequence(
    v8::FunctionCallbackInfo<v8::Value> const& args) {
  TRI_V8_TRY_CATCH_BEGIN(isolate);
  v8::HandleScope scope(isolate);

  TRI_GET_SERVER_GLOBALS(ArangodServer);
  v8::Handle<v8::Value> result =
      TRI_V8UInt64String(isolate, v8g->server()
                                      .getFeature<EngineSelectorFeature>()
                                      .engine<RocksDBEngine>()
                                      .recoveryStartSequence());

  TRI_V8_RETURN(result);
  TRI_V8_TRY_CATCH_END
}
#endif

#ifdef ARANGODB_ENABLE_FAILURE_TESTS
static void JS_CollectionRevisionTreeCorrupt(
    v8::FunctionCallbackInfo<v8::Value> const& args) {
  TRI_V8_TRY_CATCH_BEGIN(isolate);
  v8::HandleScope scope(isolate);

  auto* collection = UnwrapCollection(isolate, args.Holder());

  if (!collection) {
    TRI_V8_THROW_EXCEPTION_INTERNAL("cannot extract collection");
  }

  if (args.Length() != 2) {
    TRI_V8_THROW_EXCEPTION_USAGE("_revisionTreeCorrupt(<count>, <hash>");
  }

  uint64_t count = TRI_ObjectToUInt64(isolate, args[0], true);
  uint64_t hash = TRI_ObjectToUInt64(isolate, args[1], true);

  auto* physical = toRocksDBCollection(*collection);
  physical->corruptRevisionTree(count, hash);

  TRI_V8_RETURN_UNDEFINED();
  TRI_V8_TRY_CATCH_END
}

static void JS_CollectionRevisionTreeVerification(
    v8::FunctionCallbackInfo<v8::Value> const& args) {
  TRI_V8_TRY_CATCH_BEGIN(isolate);
  v8::HandleScope scope(isolate);

  auto* collection = UnwrapCollection(isolate, args.Holder());

  if (!collection) {
    TRI_V8_THROW_EXCEPTION_INTERNAL("cannot extract collection");
  }

  std::unique_ptr<containers::RevisionTree> storedTree;
  std::unique_ptr<containers::RevisionTree> computedTree;

  {
    TRI_vocbase_t& vocbase = collection->vocbase();
    auto& server = vocbase.server();
    RocksDBEngine& engine =
        server.getFeature<EngineSelectorFeature>().engine<RocksDBEngine>();
    RocksDBReplicationManager* manager = engine.replicationManager();
    // the 600 and 17 are magic numbers here. we can put in any ttl and any
    // client id to proceed. the context created here is thrown away
    // immediately afterwards anyway.
    auto ctx = manager->createContext(engine, /*ttl*/ 600, SyncerId{17},
                                      ServerId{17}, "");
    TRI_ASSERT(ctx);
    try {
      auto* physical = toRocksDBCollection(*collection);
      auto batchId = ctx->id();
      storedTree = physical->revisionTree(ctx->snapshotTick());
      computedTree = physical->computeRevisionTree(batchId);
      ctx.setDeleted();
    } catch (...) {
      ctx.setDeleted();
      throw;
    }
  }

  VPackBuilder builder;
  {
    VPackObjectBuilder guard(&builder);
    if (storedTree != nullptr) {
      builder.add(VPackValue("stored"));
      storedTree->serialize(builder, /*onlyPopulated*/ false);
    } else {
      builder.add("stored", VPackValue(false));
    }
    if (computedTree != nullptr) {
      builder.add(VPackValue("computed"));
      computedTree->serialize(builder, /*onlyPopulated*/ false);
    } else {
      builder.add("computed", VPackValue(false));
    }
    if (storedTree != nullptr && computedTree != nullptr) {
      try {
        std::vector<std::pair<uint64_t, uint64_t>> diff =
            computedTree->diff(*storedTree);
        builder.add("equal", VPackValue(diff.empty()));
      } catch (std::exception const& ex) {
        builder.add("error", VPackValue(ex.what()));
      }
    }
  }

  v8::Handle<v8::Value> result = TRI_VPackToV8(isolate, builder.slice());
  TRI_V8_RETURN(result);
  TRI_V8_TRY_CATCH_END
}

static void JS_CollectionRevisionTreeRebuild(
    v8::FunctionCallbackInfo<v8::Value> const& args) {
  TRI_V8_TRY_CATCH_BEGIN(isolate);
  v8::HandleScope scope(isolate);

  auto* collection = UnwrapCollection(isolate, args.Holder());

  if (!collection) {
    TRI_V8_THROW_EXCEPTION_INTERNAL("cannot extract collection");
  }

  auto* physical = toRocksDBCollection(*collection);
  Result result = physical->rebuildRevisionTree();

  if (result.fail()) {
    TRI_V8_THROW_EXCEPTION_FULL(result.errorNumber(), result.errorMessage());
  }
  TRI_V8_RETURN_UNDEFINED();
  TRI_V8_TRY_CATCH_END
}
#endif

static void JS_CollectionRevisionTreeSummary(
    v8::FunctionCallbackInfo<v8::Value> const& args) {
  TRI_V8_TRY_CATCH_BEGIN(isolate);
  v8::HandleScope scope(isolate);

  auto* collection = UnwrapCollection(isolate, args.Holder());

  if (!collection) {
    TRI_V8_THROW_EXCEPTION_INTERNAL("cannot extract collection");
  }

  bool fromCollection = false;
  if (args.Length() > 0) {
    fromCollection = TRI_ObjectToBoolean(isolate, args[0]);
  }

  auto* physical = toRocksDBCollection(*collection);
  VPackBuilder builder;
  physical->revisionTreeSummary(builder, fromCollection);

  v8::Handle<v8::Value> result = TRI_VPackToV8(isolate, builder.slice());
  TRI_V8_RETURN(result);
  TRI_V8_TRY_CATCH_END
}

#ifdef ARANGODB_ENABLE_FAILURE_TESTS
static void JS_CollectionRevisionTreePendingUpdates(
    v8::FunctionCallbackInfo<v8::Value> const& args) {
  TRI_V8_TRY_CATCH_BEGIN(isolate);
  v8::HandleScope scope(isolate);

  auto* collection = UnwrapCollection(isolate, args.Holder());

  if (!collection) {
    TRI_V8_THROW_EXCEPTION_INTERNAL("cannot extract collection");
  }

  auto* physical = toRocksDBCollection(*collection);
  VPackBuilder builder;
  physical->revisionTreePendingUpdates(builder);

  v8::Handle<v8::Value> result = TRI_VPackToV8(isolate, builder.slice());
  TRI_V8_RETURN(result);
  TRI_V8_TRY_CATCH_END
}
#endif

void RocksDBV8Functions::registerResources(RocksDBEngine& engine) {
  ISOLATE;
  v8::HandleScope scope(isolate);

  TRI_GET_GLOBALS();

  // patch ArangoCollection object
  v8::Handle<v8::ObjectTemplate> rt =
      v8::Handle<v8::ObjectTemplate>::New(isolate, v8g->VocbaseColTempl);
  TRI_ASSERT(!rt.IsEmpty());

  TRI_AddMethodVocbase(isolate, rt,
                       TRI_V8_ASCII_STRING(isolate, "recalculateCount"),
                       JS_RecalculateCounts, true);
  TRI_AddMethodVocbase(isolate, rt, TRI_V8_ASCII_STRING(isolate, "compact"),
                       JS_CompactCollection);
  TRI_AddMethodVocbase(isolate, rt,
                       TRI_V8_ASCII_STRING(isolate, "estimatedSize"),
                       JS_EstimateCollectionSize);
  TRI_AddMethodVocbase(isolate, rt,
                       TRI_V8_ASCII_STRING(isolate, "_revisionTreeSummary"),
                       JS_CollectionRevisionTreeSummary);
#ifdef ARANGODB_ENABLE_FAILURE_TESTS
  TRI_AddMethodVocbase(
      isolate, rt, TRI_V8_ASCII_STRING(isolate, "_revisionTreePendingUpdates"),
      JS_CollectionRevisionTreePendingUpdates);
  // intentionally corrupting revision tree
  TRI_AddMethodVocbase(isolate, rt,
                       TRI_V8_ASCII_STRING(isolate, "_revisionTreeCorrupt"),
                       JS_CollectionRevisionTreeCorrupt);
  // get trees from RAM and freshly computed
  TRI_AddMethodVocbase(
      isolate, rt, TRI_V8_ASCII_STRING(isolate, "_revisionTreeVerification"),
      JS_CollectionRevisionTreeVerification);
  // rebuildRevisionTree
  TRI_AddMethodVocbase(isolate, rt,
                       TRI_V8_ASCII_STRING(isolate, "_revisionTreeRebuild"),
                       JS_CollectionRevisionTreeRebuild);
#endif

  // add global WAL handling functions
  TRI_AddGlobalFunctionVocbase(
      isolate, TRI_V8_ASCII_STRING(isolate, "WAL_FLUSH"), JS_FlushWal, true);
  TRI_AddGlobalFunctionVocbase(
      isolate, TRI_V8_ASCII_STRING(isolate, "WAL_WAITCOLLECTOR"),
      JS_WaitCollectorWal, true);
  TRI_AddGlobalFunctionVocbase(isolate,
                               TRI_V8_ASCII_STRING(isolate, "WAL_PROPERTIES"),
                               JS_PropertiesWal, true);
  TRI_AddGlobalFunctionVocbase(isolate,
                               TRI_V8_ASCII_STRING(isolate, "WAL_TRANSACTIONS"),
                               JS_TransactionsWal, true);
  TRI_AddGlobalFunctionVocbase(
      isolate, TRI_V8_ASCII_STRING(isolate, "WAIT_FOR_ESTIMATOR_SYNC"),
      JS_WaitForEstimatorSync, true);

  // only used for testing - not publicly documented!
#ifdef ARANGODB_USE_GOOGLE_TESTS
  TRI_AddGlobalFunctionVocbase(
      isolate, TRI_V8_ASCII_STRING(isolate, "WAL_RECOVERY_START_SEQUENCE"),
      JS_WalRecoveryStartSequence, true);

#endif
}
