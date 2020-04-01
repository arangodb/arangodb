////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2018 ArangoDB GmbH, Cologne, Germany
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

#include "ClusterV8Functions.h"
#include "Aql/Functions.h"
#include "Basics/Exceptions.h"
#include "Basics/Result.h"
#include "Cluster/ClusterFeature.h"
#include "Cluster/ClusterMethods.h"
#include "Cluster/ServerState.h"
#include "Indexes/Index.h"
#include "StorageEngine/EngineSelectorFeature.h"
#include "StorageEngine/StorageEngine.h"
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
  bool waitForCollector = false;
  double maxWaitTime = -1.0;

  if (args.Length() > 0) {
    if (args[0]->IsObject()) {
      v8::Handle<v8::Object> obj =
          args[0]->ToObject(TRI_IGETC).FromMaybe(v8::Local<v8::Object>());
      if (TRI_HasProperty(context, isolate, obj, "waitForSync")) {
        waitForSync = TRI_ObjectToBoolean(
            isolate,
            obj->Get(context,
                     TRI_V8_ASCII_STRING(isolate, "waitForSync")
                     ).FromMaybe(v8::Local<v8::Value>()));
      }
      if (TRI_HasProperty(context, isolate, obj, "waitForCollector")) {
        waitForCollector = TRI_ObjectToBoolean(
            isolate,
            obj->Get(context,
                     TRI_V8_ASCII_STRING(isolate, "waitForCollector")
                     ).FromMaybe(v8::Local<v8::Value>()));
      }
      if (TRI_HasProperty(context, isolate, obj, "maxWaitTime")) {
        maxWaitTime = TRI_ObjectToDouble(
            isolate,
            obj->Get(context,
                     TRI_V8_ASCII_STRING(isolate, "maxWaitTime")
                     ).FromMaybe(v8::Local<v8::Value>()));
      }
    } else {
      waitForSync = TRI_ObjectToBoolean(isolate, args[0]);

      if (args.Length() > 1) {
        waitForCollector = TRI_ObjectToBoolean(isolate, args[1]);
        if (args.Length() > 3) {  // ignore writeShutdownFile
          maxWaitTime = TRI_ObjectToDouble(isolate, args[3]);
        }
      }
    }
  }

  TRI_GET_GLOBALS();
  auto& feature = v8g->_server.getFeature<ClusterFeature>();
  int res = flushWalOnAllDBServers(feature, waitForSync, waitForCollector, maxWaitTime);
  if (res != TRI_ERROR_NO_ERROR) {
    TRI_V8_THROW_EXCEPTION(res);
  }
  TRI_V8_RETURN_TRUE();

  TRI_V8_TRY_CATCH_END
}

/// this is just a stub
static void JS_WaitCollectorWal(v8::FunctionCallbackInfo<v8::Value> const& args) {
  TRI_V8_TRY_CATCH_BEGIN(isolate);
  v8::HandleScope scope(isolate);

  TRI_V8_THROW_EXCEPTION(TRI_ERROR_NOT_IMPLEMENTED);

  TRI_V8_TRY_CATCH_END
}

/// this is just a stub
static void JS_TransactionsWal(v8::FunctionCallbackInfo<v8::Value> const& args) {
  TRI_V8_TRY_CATCH_BEGIN(isolate);
  v8::HandleScope scope(isolate);

  TRI_V8_THROW_EXCEPTION(TRI_ERROR_NOT_IMPLEMENTED);

  TRI_V8_TRY_CATCH_END
}

/// this is just a stub
static void JS_PropertiesWal(v8::FunctionCallbackInfo<v8::Value> const& args) {
  TRI_V8_TRY_CATCH_BEGIN(isolate);
  v8::HandleScope scope(isolate);

  TRI_V8_THROW_EXCEPTION(TRI_ERROR_NOT_IMPLEMENTED);

  TRI_V8_TRY_CATCH_END
}

static void JS_RecalculateCounts(v8::FunctionCallbackInfo<v8::Value> const& args) {
  TRI_V8_TRY_CATCH_BEGIN(isolate);
  v8::HandleScope scope(isolate);

  // this is just a stub
  TRI_V8_RETURN_TRUE();
  TRI_V8_TRY_CATCH_END
}

static void JS_CompactCollection(v8::FunctionCallbackInfo<v8::Value> const& args) {
  TRI_V8_TRY_CATCH_BEGIN(isolate);
  v8::HandleScope scope(isolate);

  // this is just a stub
  TRI_V8_RETURN_UNDEFINED();
  TRI_V8_TRY_CATCH_END
}

static void JS_EstimateCollectionSize(v8::FunctionCallbackInfo<v8::Value> const& args) {
  TRI_V8_TRY_CATCH_BEGIN(isolate);
  v8::HandleScope scope(isolate);

  auto* collection = UnwrapCollection(isolate, args.Holder());

  if (!collection) {
    TRI_V8_THROW_EXCEPTION_INTERNAL("cannot extract collection");
  }

  VPackBuilder builder;
  builder.openObject();
  builder.add("documents", VPackValue(0));
  builder.add("indexes", VPackValue(VPackValueType::Object));

  for (auto& i : collection->getIndexes()) {
    builder.add(std::to_string(i->id().id()), VPackValue(0));
  }

  builder.close();
  builder.add("total", VPackValue(0));
  builder.close();

  v8::Handle<v8::Value> result = TRI_VPackToV8(isolate, builder.slice());
  TRI_V8_RETURN(result);
  TRI_V8_TRY_CATCH_END
}

static void JS_WaitForEstimatorSync(v8::FunctionCallbackInfo<v8::Value> const& args) {
  TRI_V8_TRY_CATCH_BEGIN(isolate);
  v8::HandleScope scope(isolate);

  EngineSelectorFeature::ENGINE->waitForEstimatorSync(std::chrono::seconds(10));

  TRI_V8_RETURN_TRUE();
  TRI_V8_TRY_CATCH_END
}

void ClusterV8Functions::registerResources() {
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
  TRI_AddMethodVocbase(isolate, rt, TRI_V8_ASCII_STRING(isolate, "compact"), JS_CompactCollection);
  TRI_AddMethodVocbase(isolate, rt,
                       TRI_V8_ASCII_STRING(isolate, "estimatedSize"),
                       JS_EstimateCollectionSize);

  // add global WAL handling functions
  TRI_AddGlobalFunctionVocbase(isolate,
                               TRI_V8_ASCII_STRING(isolate, "WAL_FLUSH"),
                               JS_FlushWal, true);
  TRI_AddGlobalFunctionVocbase(isolate,
                               TRI_V8_ASCII_STRING(isolate,
                                                   "WAL_WAITCOLLECTOR"),
                               JS_WaitCollectorWal, true);
  TRI_AddGlobalFunctionVocbase(isolate,
                               TRI_V8_ASCII_STRING(isolate, "WAL_PROPERTIES"),
                               JS_PropertiesWal, true);
  TRI_AddGlobalFunctionVocbase(isolate,
                               TRI_V8_ASCII_STRING(isolate, "WAL_TRANSACTIONS"),
                               JS_TransactionsWal, true);
  TRI_AddGlobalFunctionVocbase(isolate,
                               TRI_V8_ASCII_STRING(isolate,
                                                   "WAIT_FOR_ESTIMATOR_SYNC"),
                               JS_WaitForEstimatorSync, true);
}
