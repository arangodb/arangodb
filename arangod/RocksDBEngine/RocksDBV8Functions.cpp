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
/// @author Jan Steemann
////////////////////////////////////////////////////////////////////////////////

#include "RocksDBV8Functions.h"
#include "Aql/Functions.h"
#include "Basics/Exceptions.h"
#include "Basics/Result.h"
#include "Cluster/ServerState.h"
#include "RocksDBEngine/RocksDBCollection.h"
#include "RocksDBEngine/RocksDBCommon.h"
#include "RocksDBEngine/RocksDBEngine.h"
#include "StorageEngine/EngineSelectorFeature.h"
#include "V8/v8-conv.h"
#include "V8/v8-globals.h"
#include "V8/v8-utils.h"
#include "V8/v8-vpack.h"
#include "V8Server/v8-externals.h"
#include "VocBase/LogicalCollection.h"

#include <v8.h>

using namespace arangodb;

/// flush the WAL
static void JS_FlushWal(v8::FunctionCallbackInfo<v8::Value> const& args) {
  TRI_V8_TRY_CATCH_BEGIN(isolate);
  v8::HandleScope scope(isolate);

  bool waitForSync = false;
  bool waitForCollector = false;
  bool writeShutdownFile = false;

  if (args.Length() > 0) {
    if (args[0]->IsObject()) {
      v8::Handle<v8::Object> obj = args[0]->ToObject();
      if (obj->Has(TRI_V8_ASCII_STRING("waitForSync"))) {
        waitForSync =
            TRI_ObjectToBoolean(obj->Get(TRI_V8_ASCII_STRING("waitForSync")));
      }
      if (obj->Has(TRI_V8_ASCII_STRING("waitForCollector"))) {
        waitForCollector = TRI_ObjectToBoolean(
            obj->Get(TRI_V8_ASCII_STRING("waitForCollector")));
      }
      if (obj->Has(TRI_V8_ASCII_STRING("writeShutdownFile"))) {
        writeShutdownFile = TRI_ObjectToBoolean(
            obj->Get(TRI_V8_ASCII_STRING("writeShutdownFile")));
      }
    } else {
      waitForSync = TRI_ObjectToBoolean(args[0]);

      if (args.Length() > 1) {
        waitForCollector = TRI_ObjectToBoolean(args[1]);

        if (args.Length() > 2) {
          writeShutdownFile = TRI_ObjectToBoolean(args[2]);
        }
      }
    }
  }

  arangodb::Result ret =
      static_cast<RocksDBEngine*>(EngineSelectorFeature::ENGINE)->syncWal(
          waitForSync, waitForCollector, writeShutdownFile);
  if (!ret.ok()) {
    THROW_ARANGO_EXCEPTION_MESSAGE(ret.errorNumber(), ret.errorMessage());
  }
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

  arangodb::LogicalCollection* collection =
      TRI_UnwrapClass<arangodb::LogicalCollection>(args.Holder(),
                                                   WRP_VOCBASE_COL_TYPE);

  if (collection == nullptr) {
    TRI_V8_THROW_EXCEPTION_INTERNAL("cannot extract collection");
  }

  auto physical = toRocksDBCollection(collection);

  v8::Handle<v8::Value> result = v8::Number::New(
      isolate, static_cast<double>(physical->recalculateCounts()));

  TRI_V8_RETURN(result);
  TRI_V8_TRY_CATCH_END
}

static void JS_CompactCollection(
    v8::FunctionCallbackInfo<v8::Value> const& args) {
  TRI_V8_TRY_CATCH_BEGIN(isolate);
  v8::HandleScope scope(isolate);

  arangodb::LogicalCollection* collection =
      TRI_UnwrapClass<arangodb::LogicalCollection>(args.Holder(),
                                                   WRP_VOCBASE_COL_TYPE);

  if (collection == nullptr) {
    TRI_V8_THROW_EXCEPTION_INTERNAL("cannot extract collection");
  }

  RocksDBCollection* physical = toRocksDBCollection(collection);
  physical->compact();

  TRI_V8_RETURN_UNDEFINED();
  TRI_V8_TRY_CATCH_END
}

static void JS_EstimateCollectionSize(
    v8::FunctionCallbackInfo<v8::Value> const& args) {
  TRI_V8_TRY_CATCH_BEGIN(isolate);
  v8::HandleScope scope(isolate);

  arangodb::LogicalCollection* collection =
      TRI_UnwrapClass<arangodb::LogicalCollection>(args.Holder(),
                                                   WRP_VOCBASE_COL_TYPE);

  if (collection == nullptr) {
    TRI_V8_THROW_EXCEPTION_INTERNAL("cannot extract collection");
  }

  RocksDBCollection* physical = toRocksDBCollection(collection);
  VPackBuilder builder;
  physical->estimateSize(builder);

  v8::Handle<v8::Value> result = TRI_VPackToV8(isolate, builder.slice());
  TRI_V8_RETURN(result);
  TRI_V8_TRY_CATCH_END
}

void RocksDBV8Functions::registerResources() {
  ISOLATE;
  v8::HandleScope scope(isolate);

  TRI_GET_GLOBALS();

  // patch ArangoCollection object
  v8::Handle<v8::ObjectTemplate> rt =
      v8::Handle<v8::ObjectTemplate>::New(isolate, v8g->VocbaseColTempl);
  TRI_ASSERT(!rt.IsEmpty());

  TRI_AddMethodVocbase(isolate, rt, TRI_V8_ASCII_STRING("recalculateCount"),
                       JS_RecalculateCounts, true);
  TRI_AddMethodVocbase(isolate, rt, TRI_V8_ASCII_STRING("compact"),
                       JS_CompactCollection);
  TRI_AddMethodVocbase(isolate, rt, TRI_V8_ASCII_STRING("estimatedSize"),
                       JS_EstimateCollectionSize);

  // add global WAL handling functions
  TRI_AddGlobalFunctionVocbase(isolate, TRI_V8_ASCII_STRING("WAL_FLUSH"),
                               JS_FlushWal, true);
  TRI_AddGlobalFunctionVocbase(isolate,
                               TRI_V8_ASCII_STRING("WAL_WAITCOLLECTOR"),
                               JS_WaitCollectorWal, true);
  TRI_AddGlobalFunctionVocbase(isolate, TRI_V8_ASCII_STRING("WAL_PROPERTIES"),
                               JS_PropertiesWal, true);
  TRI_AddGlobalFunctionVocbase(isolate, TRI_V8_ASCII_STRING("WAL_TRANSACTIONS"),
                               JS_TransactionsWal, true);
}
