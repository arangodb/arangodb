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

#include "MMFilesV8Functions.h"
#include "Aql/Functions.h"
#include "Basics/Exceptions.h"
#include "Cluster/ClusterFeature.h"
#include "Cluster/ClusterMethods.h"
#include "Cluster/ServerState.h"
#include "MMFiles/MMFilesCollection.h"
#include "MMFiles/MMFilesEngine.h"
#include "MMFiles/MMFilesLogfileManager.h"
#include "StorageEngine/EngineSelectorFeature.h"
#include "StorageEngine/PhysicalCollection.h"
#include "StorageEngine/StorageEngine.h"
#include "Transaction/V8Context.h"
#include "Utils/OperationOptions.h"
#include "Utils/SingleCollectionTransaction.h"
#include "V8/v8-conv.h"
#include "V8/v8-globals.h"
#include "V8/v8-utils.h"
#include "V8Server/v8-collection.h"
#include "V8Server/v8-externals.h"
#include "V8Server/v8-vocbaseprivate.h"
#include "VocBase/LogicalCollection.h"

#include <v8.h>

using namespace arangodb;

/// @brief rotate the active journal of the collection
static void JS_RotateVocbaseCol(v8::FunctionCallbackInfo<v8::Value> const& args) {
  TRI_V8_TRY_CATCH_BEGIN(isolate);
  v8::HandleScope scope(isolate);

  PREVENT_EMBEDDED_TRANSACTION();

  auto* collection = UnwrapCollection(isolate, args.Holder());

  if (!collection) {
    TRI_V8_THROW_EXCEPTION_INTERNAL("cannot extract collection");
  }

  SingleCollectionTransaction trx(transaction::V8Context::Create(collection->vocbase(), true),
                                  *collection, AccessMode::Type::WRITE);
  Result res = trx.begin();

  if (!res.ok()) {
    TRI_V8_THROW_EXCEPTION(res);
  }

  MMFilesCollection* mcoll = static_cast<MMFilesCollection*>(collection->getPhysical());
  res.reset(mcoll->rotateActiveJournal());
  trx.finish(res);

  if (!res.ok()) {
    res.reset(res.errorNumber(),
              std::string("could not rotate journal: ") + res.errorMessage());
    TRI_V8_THROW_EXCEPTION(res);
  }

  TRI_V8_RETURN_UNDEFINED();
  TRI_V8_TRY_CATCH_END
}

/// @brief returns information about the datafiles
/// the collection must be unloaded.
static void JS_DatafilesVocbaseCol(v8::FunctionCallbackInfo<v8::Value> const& args) {
  TRI_V8_TRY_CATCH_BEGIN(isolate);
  v8::HandleScope scope(isolate);
  auto context = TRI_IGETC;

  auto* collection = UnwrapCollection(isolate, args.Holder());

  if (!collection) {
    TRI_V8_THROW_EXCEPTION_INTERNAL("cannot extract collection");
  }

  TRI_THROW_SHARDING_COLLECTION_NOT_YET_IMPLEMENTED(collection);

  // TODO: move this into engine
  StorageEngine* engine = EngineSelectorFeature::ENGINE;
  auto status = collection->getStatusLocked();

  if (status != TRI_VOC_COL_STATUS_UNLOADED && status != TRI_VOC_COL_STATUS_CORRUPTED) {
    TRI_V8_THROW_EXCEPTION(TRI_ERROR_ARANGO_COLLECTION_NOT_UNLOADED);
  }

  auto structure = dynamic_cast<MMFilesEngine*>(engine)->scanCollectionDirectory(
      collection->getPhysical()->path());

  // build result
  v8::Handle<v8::Object> result = v8::Object::New(isolate);

  // journals
  v8::Handle<v8::Array> journals = v8::Array::New(isolate);
  result->Set(context, TRI_V8_ASCII_STRING(isolate, "journals"), journals).FromMaybe(false);

  uint32_t i = 0;
  for (auto& it : structure.journals) {
    journals->Set(context, i++, TRI_V8_STD_STRING(isolate, it)).FromMaybe(false);
  }

  // compactors
  v8::Handle<v8::Array> compactors = v8::Array::New(isolate);
  result->Set(context, TRI_V8_ASCII_STRING(isolate, "compactors"), compactors).FromMaybe(false);

  i = 0;
  for (auto& it : structure.compactors) {
    compactors->Set(context, i++, TRI_V8_STD_STRING(isolate, it)).FromMaybe(false);
  }

  // datafiles
  v8::Handle<v8::Array> datafiles = v8::Array::New(isolate);
  result->Set(context, TRI_V8_ASCII_STRING(isolate, "datafiles"), datafiles).FromMaybe(false);

  i = 0;
  for (auto& it : structure.datafiles) {
    datafiles->Set(context, i++, TRI_V8_STD_STRING(isolate, it)).FromMaybe(false);
  }

  TRI_V8_RETURN(result);
  TRI_V8_TRY_CATCH_END
}

/// @brief returns information about the datafiles
/// Returns information about the datafiles. The collection must be unloaded.
static void JS_DatafileScanVocbaseCol(v8::FunctionCallbackInfo<v8::Value> const& args) {
  TRI_V8_TRY_CATCH_BEGIN(isolate);
  v8::HandleScope scope(isolate);
  auto context = TRI_IGETC;
  auto* collection = UnwrapCollection(isolate, args.Holder());

  if (!collection) {
    TRI_V8_THROW_EXCEPTION_INTERNAL("cannot extract collection");
  }

  if (args.Length() != 1) {
    TRI_V8_THROW_EXCEPTION_USAGE("datafileScan(<path>)");
  }

  std::string path = TRI_ObjectToString(isolate, args[0]);

  v8::Handle<v8::Object> result;
  {
    auto status = collection->getStatusLocked();

    if (status != TRI_VOC_COL_STATUS_UNLOADED && status != TRI_VOC_COL_STATUS_CORRUPTED) {
      TRI_V8_THROW_EXCEPTION(TRI_ERROR_ARANGO_COLLECTION_NOT_UNLOADED);
    }

    DatafileScan scan = MMFilesDatafile::scan(path);

    // build result
    result = v8::Object::New(isolate);

    result->Set(context,
                TRI_V8_ASCII_STRING(isolate, "currentSize"),
                v8::Number::New(isolate, scan.currentSize)).FromMaybe(false);
    result->Set(context,
                TRI_V8_ASCII_STRING(isolate, "maximalSize"),
                v8::Number::New(isolate, scan.maximalSize)).FromMaybe(false);
    result->Set(context,
                TRI_V8_ASCII_STRING(isolate, "endPosition"),
                v8::Number::New(isolate, scan.endPosition)).FromMaybe(false);
    result->Set(context,
                TRI_V8_ASCII_STRING(isolate, "numberMarkers"),
                v8::Number::New(isolate, scan.numberMarkers)).FromMaybe(false);
    result->Set(context,
                TRI_V8_ASCII_STRING(isolate, "status"),
                v8::Number::New(isolate, scan.status)).FromMaybe(false);
    result->Set(context,
                TRI_V8_ASCII_STRING(isolate, "isSealed"),
                v8::Boolean::New(isolate, scan.isSealed)).FromMaybe(false);

    v8::Handle<v8::Array> entries = v8::Array::New(isolate);
    result->Set(context,
                TRI_V8_ASCII_STRING(isolate, "entries"), entries).FromMaybe(false);

    uint32_t i = 0;
    for (auto const& entry : scan.entries) {
      v8::Handle<v8::Object> o = v8::Object::New(isolate);

      o->Set(context,
             TRI_V8_ASCII_STRING(isolate, "position"),
             v8::Number::New(isolate, entry.position)).FromMaybe(false);
      o->Set(context,
             TRI_V8_ASCII_STRING(isolate, "size"),
             v8::Number::New(isolate, entry.size)).FromMaybe(false);
      o->Set(context,
             TRI_V8_ASCII_STRING(isolate, "realSize"),
             v8::Number::New(isolate, entry.realSize)).FromMaybe(false);
      o->Set(context,
             TRI_V8_ASCII_STRING(isolate, "tick"),
             TRI_V8UInt64String<TRI_voc_tick_t>(isolate, entry.tick)).FromMaybe(false);
      o->Set(context,
             TRI_V8_ASCII_STRING(isolate, "type"),
             v8::Number::New(isolate, static_cast<int>(entry.type))).FromMaybe(false);
      o->Set(context,
             TRI_V8_ASCII_STRING(isolate, "status"),
             v8::Number::New(isolate, static_cast<int>(entry.status))).FromMaybe(false);

      if (!entry.key.empty()) {
        o->Set(context,
               TRI_V8_ASCII_STRING(isolate, "key"),
               TRI_V8_STD_STRING(isolate, entry.key)).FromMaybe(false);
      }

      if (entry.typeName != nullptr) {
        o->Set(context,
               TRI_V8_ASCII_STRING(isolate, "typeName"),
               TRI_V8_ASCII_STRING(isolate, entry.typeName)).FromMaybe(false);
      }

      if (!entry.diagnosis.empty()) {
        o->Set(context,
               TRI_V8_ASCII_STRING(isolate, "diagnosis"),
               TRI_V8_STD_STRING(isolate, entry.diagnosis)).FromMaybe(false);
      }

      entries->Set(context, i++, o).FromMaybe(false);
    }
  }

  TRI_V8_RETURN(result);
  TRI_V8_TRY_CATCH_END
}

/// @brief tries to repair a datafile
static void JS_TryRepairDatafileVocbaseCol(v8::FunctionCallbackInfo<v8::Value> const& args) {
  TRI_V8_TRY_CATCH_BEGIN(isolate);
  v8::HandleScope scope(isolate);

  auto* collection = UnwrapCollection(isolate, args.Holder());

  if (!collection) {
    TRI_V8_THROW_EXCEPTION_INTERNAL("cannot extract collection");
  }

  TRI_THROW_SHARDING_COLLECTION_NOT_YET_IMPLEMENTED(collection);

  if (args.Length() != 1) {
    TRI_V8_THROW_EXCEPTION_USAGE("tryRepairDatafile(<datafile>)");
  }

  std::string path = TRI_ObjectToString(isolate, args[0]);
  auto status = collection->getStatusLocked();

  if (status != TRI_VOC_COL_STATUS_UNLOADED && status != TRI_VOC_COL_STATUS_CORRUPTED) {
    TRI_V8_THROW_EXCEPTION(TRI_ERROR_ARANGO_COLLECTION_NOT_UNLOADED);
  }

  bool result = MMFilesDatafile::tryRepair(path);

  if (result) {
    TRI_V8_RETURN_TRUE();
  }

  TRI_V8_RETURN_FALSE();
  TRI_V8_TRY_CATCH_END
}

/// @brief truncates a datafile
static void JS_TruncateDatafileVocbaseCol(v8::FunctionCallbackInfo<v8::Value> const& args) {
  TRI_V8_TRY_CATCH_BEGIN(isolate);
  v8::HandleScope scope(isolate);

  auto* collection = UnwrapCollection(isolate, args.Holder());

  if (!collection) {
    TRI_V8_THROW_EXCEPTION_INTERNAL("cannot extract collection");
  }

  TRI_THROW_SHARDING_COLLECTION_NOT_YET_IMPLEMENTED(collection);

  if (args.Length() != 2) {
    TRI_V8_THROW_EXCEPTION_USAGE("truncateDatafile(<datafile>, <size>)");
  }

  std::string path = TRI_ObjectToString(isolate, args[0]);
  size_t size = (size_t)TRI_ObjectToInt64(isolate, args[1]);
  auto status = collection->getStatusLocked();

  if (status != TRI_VOC_COL_STATUS_UNLOADED && status != TRI_VOC_COL_STATUS_CORRUPTED) {
    TRI_V8_THROW_EXCEPTION(TRI_ERROR_ARANGO_COLLECTION_NOT_UNLOADED);
  }

  int res = MMFilesDatafile::truncate(path, static_cast<uint32_t>(size));

  if (res != TRI_ERROR_NO_ERROR) {
    TRI_V8_THROW_EXCEPTION_MESSAGE(res, "cannot truncate datafile");
  }

  TRI_V8_RETURN_UNDEFINED();
  TRI_V8_TRY_CATCH_END
}

static void JS_PropertiesWal(v8::FunctionCallbackInfo<v8::Value> const& args) {
  TRI_V8_TRY_CATCH_BEGIN(isolate);
  v8::HandleScope scope(isolate);
  auto context = TRI_IGETC;

  if (args.Length() > 1 || (args.Length() == 1 && !args[0]->IsObject())) {
    TRI_V8_THROW_EXCEPTION_USAGE("properties(<object>)");
  }

  auto l = MMFilesLogfileManager::instance();

  if (args.Length() == 1) {
    // set the properties
    v8::Handle<v8::Object> object = v8::Handle<v8::Object>::Cast(args[0]);
    if (TRI_HasProperty(context, isolate, object, "allowOversizeEntries")) {
      bool value = TRI_ObjectToBoolean(
          isolate,
          object->Get(context, TRI_V8_ASCII_STRING(isolate, "allowOversizeEntries")).FromMaybe(v8::Local<v8::Value>()));
      l->allowOversizeEntries(value);
    }

    if (TRI_HasProperty(context, isolate, object, "logfileSize")) {
      uint32_t value = static_cast<
        uint32_t>(TRI_ObjectToUInt64(isolate,
                                     object->Get(context,
                                                 TRI_V8_ASCII_STRING(isolate,"logfileSize")
                                                 ).FromMaybe(v8::Local<v8::Value>()),
                                     true));
      l->filesize(value);
    }

    if (TRI_HasProperty(context, isolate, object, "historicLogfiles")) {
      uint32_t value = static_cast<
        uint32_t>(TRI_ObjectToUInt64(isolate,
                                     object->Get(context,
                                                 TRI_V8_ASCII_STRING(isolate,  "historicLogfiles")
                                                 ).FromMaybe(v8::Local<v8::Value>()),
                                     true));
      l->historicLogfiles(value);
    }

    if (TRI_HasProperty(context, isolate, object, "reserveLogfiles")) {
      uint32_t value = static_cast<
        uint32_t>(TRI_ObjectToUInt64(isolate,
                                     object->Get(context,
                                                 TRI_V8_ASCII_STRING(isolate, "reserveLogfiles")
                                                 ).FromMaybe(v8::Local<v8::Value>()), true));
      l->reserveLogfiles(value);
    }

    if (TRI_HasProperty(context, isolate, object, "throttleWait")) {
      uint64_t value =
        TRI_ObjectToUInt64(isolate, object->Get(context,
                                                TRI_V8_ASCII_STRING(isolate, "throttleWait")
                                                ).FromMaybe(v8::Local<v8::Value>()), true);
      l->maxThrottleWait(value);
    }

    if (TRI_HasProperty(context, isolate, object, "throttleWhenPending")) {
      uint64_t value = TRI_ObjectToUInt64(
          isolate,
          object->Get(context,
                      TRI_V8_ASCII_STRING(isolate, "throttleWhenPending")
                      ).FromMaybe(v8::Local<v8::Value>()), true);
      l->throttleWhenPending(value);
    }
  }

  v8::Handle<v8::Object> result = v8::Object::New(isolate);
  result->Set(context,
              TRI_V8_ASCII_STRING(isolate, "allowOversizeEntries"),
              v8::Boolean::New(isolate, l->allowOversizeEntries())).FromMaybe(false);
  result->Set(context,
              TRI_V8_ASCII_STRING(isolate, "logfileSize"),
              v8::Number::New(isolate, l->filesize())).FromMaybe(false);
  result->Set(context,
              TRI_V8_ASCII_STRING(isolate, "historicLogfiles"),
              v8::Number::New(isolate, l->historicLogfiles())).FromMaybe(false);
  result->Set(context,
              TRI_V8_ASCII_STRING(isolate, "reserveLogfiles"),
              v8::Number::New(isolate, l->reserveLogfiles())).FromMaybe(false);
  result->Set(context,
              TRI_V8_ASCII_STRING(isolate, "syncInterval"),
              v8::Number::New(isolate, (double)l->syncInterval())).FromMaybe(false);
  result->Set(context,
              TRI_V8_ASCII_STRING(isolate, "throttleWait"),
              v8::Number::New(isolate, (double)l->maxThrottleWait())).FromMaybe(false);
  result->Set(context,
              TRI_V8_ASCII_STRING(isolate, "throttleWhenPending"),
              v8::Number::New(isolate, (double)l->throttleWhenPending())).FromMaybe(false);

  TRI_V8_RETURN(result);
  TRI_V8_TRY_CATCH_END
}

static void JS_FlushWal(v8::FunctionCallbackInfo<v8::Value> const& args) {
  TRI_V8_TRY_CATCH_BEGIN(isolate);
  v8::Local<v8::Context> context = isolate->GetCurrentContext();
  v8::HandleScope scope(isolate);

  bool waitForSync = false;
  bool waitForCollector = false;
  bool writeShutdownFile = false;
  double maxWaitTime = -1.0;

  if (args.Length() > 0) {
    if (args[0]->IsObject()) {
      v8::Handle<v8::Object> obj =
          args[0]->ToObject(TRI_IGETC).FromMaybe(v8::Local<v8::Object>());
      if (TRI_HasProperty(context, isolate, obj, "waitForSync")) {
        waitForSync = TRI_ObjectToBoolean(isolate,
                                          obj->Get(context,
                                                   TRI_V8_ASCII_STRING(isolate, "waitForSync")
                                                   ).FromMaybe(v8::Local<v8::Value>()));
      }
      if (TRI_HasProperty(context, isolate, obj, "waitForCollector")) {
        waitForCollector = TRI_ObjectToBoolean(isolate,
                                               obj->Get(context,
                                                        TRI_V8_ASCII_STRING(isolate, "waitForCollector")
                                                        ).FromMaybe(v8::Local<v8::Value>()));
      }
      if (TRI_HasProperty(context, isolate, obj, "writeShutdownFile")) {
        writeShutdownFile = TRI_ObjectToBoolean(isolate,
                                                obj->Get(context,
                                                         TRI_V8_ASCII_STRING(isolate, "writeShutdownFile")
                                                         ).FromMaybe(v8::Local<v8::Value>()));
      }
      if (TRI_HasProperty(context, isolate, obj, "maxWaitTime")) {
        maxWaitTime = TRI_ObjectToDouble(isolate,
                                         obj->Get(context,
                                                  TRI_V8_ASCII_STRING(isolate, "maxWaitTime")
                                                  ).FromMaybe(v8::Local<v8::Value>()));
      }
    } else {
      waitForSync = TRI_ObjectToBoolean(isolate, args[0]);

      if (args.Length() > 1) {
        waitForCollector = TRI_ObjectToBoolean(isolate, args[1]);

        if (args.Length() > 2) {
          writeShutdownFile = TRI_ObjectToBoolean(isolate, args[2]);

          if (args.Length() > 3) {
            maxWaitTime = TRI_ObjectToDouble(isolate, args[3]);
          }
        }
      }
    }
  }

  int res;

  if (ServerState::instance()->isCoordinator()) {
    TRI_GET_GLOBALS();
    auto& feature = v8g->_server.getFeature<ClusterFeature>();
    res = flushWalOnAllDBServers(feature, waitForSync, waitForCollector, maxWaitTime);

    if (res != TRI_ERROR_NO_ERROR) {
      TRI_V8_THROW_EXCEPTION(res);
    }
    TRI_V8_RETURN_TRUE();
  }

  res = MMFilesLogfileManager::instance()->flush(waitForSync, waitForCollector,
                                                 writeShutdownFile, maxWaitTime);

  if (res != TRI_ERROR_NO_ERROR) {
    if (res == TRI_ERROR_LOCK_TIMEOUT) {
      // improved diagnostic message for this special case
      TRI_V8_THROW_EXCEPTION_MESSAGE(
          res, "timed out waiting for WAL flush operation");
    }
    TRI_V8_THROW_EXCEPTION(res);
  }

  TRI_V8_RETURN_TRUE();
  TRI_V8_TRY_CATCH_END
}

/// @brief wait for WAL collector to finish operations for the specified
/// collection
static void JS_WaitCollectorWal(v8::FunctionCallbackInfo<v8::Value> const& args) {
  TRI_V8_TRY_CATCH_BEGIN(isolate);
  v8::HandleScope scope(isolate);

  if (ServerState::instance()->isCoordinator()) {
    TRI_V8_THROW_EXCEPTION(TRI_ERROR_NOT_IMPLEMENTED);
  }

  auto& vocbase = GetContextVocBase(isolate);

  if (args.Length() < 1) {
    TRI_V8_THROW_EXCEPTION_USAGE(
        "WAL_WAITCOLLECTOR(<collection-id>, <timeout>)");
  }

  std::string const name = TRI_ObjectToString(isolate, args[0]);
  auto col = vocbase.lookupCollection(name);

  if (col == nullptr) {
    TRI_V8_THROW_EXCEPTION(TRI_ERROR_ARANGO_DATA_SOURCE_NOT_FOUND);
  }

  double timeout = 30.0;
  if (args.Length() > 1) {
    timeout = TRI_ObjectToDouble(isolate, args[1]);
  }

  int res = MMFilesLogfileManager::instance()->waitForCollectorQueue(col->id(), timeout);

  if (res != TRI_ERROR_NO_ERROR) {
    TRI_V8_THROW_EXCEPTION(res);
  }

  TRI_V8_RETURN_TRUE();
  TRI_V8_TRY_CATCH_END
}

/// @brief get information about the currently running transactions
static void JS_TransactionsWal(v8::FunctionCallbackInfo<v8::Value> const& args) {
  TRI_V8_TRY_CATCH_BEGIN(isolate);
  v8::HandleScope scope(isolate);
  auto context = TRI_IGETC;

  auto const& info = MMFilesLogfileManager::instance()->runningTransactions();

  v8::Handle<v8::Object> result = v8::Object::New(isolate);
  
  result->Set(context,
              TRI_V8_ASCII_STRING(isolate, "runningTransactions"),
              v8::Number::New(isolate, static_cast<double>(std::get<0>(info)))).FromMaybe(false);

  // lastCollectedId
  {
    auto value = std::get<1>(info);
    if (value.id() == std::numeric_limits<MMFilesWalLogfile::IdType::BaseType>::max()) {
      result->Set(context, TRI_V8_ASCII_STRING(isolate, "minLastCollected"), v8::Null(isolate)).FromMaybe(false);
    } else {
      result
          ->Set(context, TRI_V8_ASCII_STRING(isolate, "minLastCollected"),
                TRI_V8UInt64String<TRI_voc_tick_t>(isolate, static_cast<TRI_voc_tick_t>(
                                                                value.id())))
          .FromMaybe(false);
    }
  }

  // lastSealedId
  {
    auto value = std::get<2>(info);
    if (value.id() == std::numeric_limits<MMFilesWalLogfile::IdType::BaseType>::max()) {
      result->Set(context, TRI_V8_ASCII_STRING(isolate, "minLastSealed"), v8::Null(isolate)).FromMaybe(false);
    } else {
      result
          ->Set(context, TRI_V8_ASCII_STRING(isolate, "minLastSealed"),
                TRI_V8UInt64String<TRI_voc_tick_t>(isolate, static_cast<TRI_voc_tick_t>(
                                                                value.id())))
          .FromMaybe(false);
    }
  }

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

void MMFilesV8Functions::registerResources() {
  ISOLATE;
  v8::HandleScope scope(isolate);

  TRI_GET_GLOBALS();

  // patch ArangoCollection object
  v8::Handle<v8::ObjectTemplate> rt =
      v8::Handle<v8::ObjectTemplate>::New(isolate, v8g->VocbaseColTempl);
  TRI_ASSERT(!rt.IsEmpty());

  TRI_AddMethodVocbase(isolate, rt, TRI_V8_ASCII_STRING(isolate, "datafiles"),
                       JS_DatafilesVocbaseCol, true);
  TRI_AddMethodVocbase(isolate, rt, TRI_V8_ASCII_STRING(isolate, "datafileScan"),
                       JS_DatafileScanVocbaseCol, true);
  TRI_AddMethodVocbase(isolate, rt, TRI_V8_ASCII_STRING(isolate, "rotate"), JS_RotateVocbaseCol);
  TRI_AddMethodVocbase(isolate, rt,
                       TRI_V8_ASCII_STRING(isolate, "truncateDatafile"),
                       JS_TruncateDatafileVocbaseCol, true);
  TRI_AddMethodVocbase(isolate, rt,
                       TRI_V8_ASCII_STRING(isolate, "tryRepairDatafile"),
                       JS_TryRepairDatafileVocbaseCol, true);

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
