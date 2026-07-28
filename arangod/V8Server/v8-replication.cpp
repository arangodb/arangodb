////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2014-2024 ArangoDB GmbH, Cologne, Germany
/// Copyright 2004-2014 triAGENS GmbH, Cologne, Germany
///
/// Licensed under the Business Source License 1.1 (the "License");
/// you may not use this file except in compliance with the License.
/// You may obtain a copy of the License at
///
///     https://github.com/arangodb/arangodb/blob/devel/LICENSE
///
/// Unless required by applicable law or agreed to in writing, software
/// distributed under the License is distributed on an "AS IS" BASIS,
/// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
/// See the License for the specific language governing permissions and
/// limitations under the License.
///
/// Copyright holder is ArangoDB GmbH, Cologne, Germany
///
////////////////////////////////////////////////////////////////////////////////

#ifndef USE_V8
#error this file is not supposed to be used in builds with -DUSE_V8=Off
#endif

#include "v8-replication.h"
#include "Basics/StringUtils.h"
#include "Cluster/ClusterFeature.h"
#include "Logger/LogMacros.h"
#include "Logger/Logger.h"
#include "Logger/LoggerStream.h"
#include "Replication/DatabaseReplicationApplier.h"
#include "Replication/DatabaseTailingSyncer.h"
#include "Replication/GlobalReplicationApplier.h"
#include "Replication/ReplicationApplierConfiguration.h"
#include "Replication/ReplicationFeature.h"
#include "RestServer/DatabaseFeature.h"
#include "StorageEngine/StorageEngine.h"
#include "Transaction/OperationOrigin.h"
#include "Transaction/V8Context.h"
#include "Utils/DatabaseGuard.h"
#include "V8/v8-conv.h"
#include "V8/v8-globals.h"
#include "V8/v8-utils.h"
#include "V8/v8-vpack.h"
#include "V8Server/v8-vocbaseprivate.h"

#include <velocypack/Builder.h>
#include <velocypack/Parser.h>
#include <velocypack/Slice.h>

using namespace arangodb;
using namespace arangodb::basics;
using namespace arangodb::rest;

////////////////////////////////////////////////////////////////////////////////
/// @brief get the state of the replication logger
////////////////////////////////////////////////////////////////////////////////

static void JS_StateLoggerReplication(
    v8::FunctionCallbackInfo<v8::Value> const& args) {
  // FIXME: use code in RestReplicationHandler and get rid of storage-engine
  //        dependent code here
  //
  TRI_V8_TRY_CATCH_BEGIN(isolate);
  v8::HandleScope scope(isolate);

  TRI_GET_GLOBALS();
  StorageEngine& engine = v8g->server().getFeature<DatabaseFeature>().engine();
  v8::Handle<v8::Object> result = v8::Object::New(isolate);
  TRI_vocbase_t& vocbase = GetContextVocBase(isolate);

  VPackBuilder builder;
  auto res = engine.createLoggerState(&vocbase, builder);
  if (res.fail()) {
    TRI_V8_THROW_EXCEPTION(res);
  }
  v8::Handle<v8::Value> resultValue = TRI_VPackToV8(isolate, builder.slice());
  result = v8::Handle<v8::Object>::Cast(resultValue);

  TRI_V8_RETURN(result);
  TRI_V8_TRY_CATCH_END
}

////////////////////////////////////////////////////////////////////////////////
/// @brief get the last WAL entries
////////////////////////////////////////////////////////////////////////////////

static void JS_LastLoggerReplication(
    v8::FunctionCallbackInfo<v8::Value> const& args) {
  TRI_V8_TRY_CATCH_BEGIN(isolate);
  v8::HandleScope scope(isolate);

  auto& vocbase = GetContextVocBase(isolate);

  if (args.Length() != 2) {
    TRI_V8_THROW_EXCEPTION_USAGE(
        "REPLICATION_LOGGER_LAST(<fromTick>, <toTick>)");
  }

  TRI_voc_tick_t tickStart = TRI_ObjectToUInt64(isolate, args[0], true);
  TRI_voc_tick_t tickEnd = TRI_ObjectToUInt64(isolate, args[1], true);
  if (tickEnd <= tickStart) {
    TRI_V8_THROW_EXCEPTION_USAGE("tickStart < tickEnd");
  }

  auto origin =
      transaction::OperationOriginREST{"returning last documents from WAL"};
  auto transactionContext =
      transaction::V8Context::create(vocbase, origin, true);
  VPackBuilder builder(transactionContext->getVPackOptions());
  TRI_GET_GLOBALS();
  StorageEngine& engine = v8g->server().getFeature<DatabaseFeature>().engine();
  Result res = engine.lastLogger(vocbase, tickStart, tickEnd, builder);
  v8::Handle<v8::Value> result;

  if (res.fail()) {
    result = v8::Null(isolate);
    TRI_V8_THROW_EXCEPTION(res);
  }

  result = TRI_VPackToV8(isolate, builder.slice(),
                         transactionContext->getVPackOptions());

  TRI_V8_RETURN(result);
  TRI_V8_TRY_CATCH_END
}

enum ApplierType { APPLIER_DATABASE, APPLIER_GLOBAL };

static ReplicationApplier* getContinuousApplier(v8::Isolate* isolate,
                                                ApplierType applierType) {
  ReplicationApplier* applier = nullptr;

  if (applierType == APPLIER_DATABASE) {
    // database-specific applier
    auto& vocbase = GetContextVocBase(isolate);

    applier = vocbase.replicationApplier();
  } else {
    // applier type global
    TRI_GET_GLOBALS();
    auto& replicationFeature = v8g->server().getFeature<ReplicationFeature>();
    applier = replicationFeature.globalReplicationApplier();
  }

  if (applier == nullptr) {
    THROW_ARANGO_EXCEPTION_MESSAGE(TRI_ERROR_INTERNAL,
                                   "unable to find replicationApplier");
  }

  return applier;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief configure the replication applier manually
////////////////////////////////////////////////////////////////////////////////

static void ConfigureApplierReplication(
    v8::FunctionCallbackInfo<v8::Value> const& args, ApplierType applierType) {
  TRI_V8_TRY_CATCH_BEGIN(isolate);
  v8::HandleScope scope(isolate);

  ReplicationApplier* applier = getContinuousApplier(isolate, applierType);

  // get current configuration
  ReplicationApplierConfiguration configuration = applier->configuration();

  if (args.Length() == 0) {
    // no argument: return the current configuration
    VPackBuilder builder;
    builder.openObject();
    configuration.toVelocyPack(builder, true, true);
    builder.close();

    v8::Handle<v8::Value> result = TRI_VPackToV8(isolate, builder.slice());

    TRI_V8_RETURN(result);
  }

  else {
    // set the configuration
    if (args.Length() != 1 || !args[0]->IsObject()) {
      TRI_V8_THROW_EXCEPTION_USAGE("properties(<properties>)");
    }

    VPackBuilder builder;
    TRI_V8ToVPack(isolate, builder, args[0], false);

    std::string databaseName;
    if (applierType == APPLIER_DATABASE) {
      auto& vocbase = GetContextVocBase(isolate);

      databaseName = vocbase.name();
    }

    // merge the passed configuration into the existing one
    configuration = ReplicationApplierConfiguration::fromVelocyPack(
        configuration, builder.slice(), databaseName);

    // will throw if invalid
    configuration.validate();

    // finally store the new configuration
    applier->reconfigure(configuration);

    // and return it
    builder.clear();
    builder.openObject();
    configuration.toVelocyPack(builder, true, true);
    builder.close();

    v8::Handle<v8::Value> result = TRI_VPackToV8(isolate, builder.slice());

    TRI_V8_RETURN(result);
  }
  TRI_V8_TRY_CATCH_END
}

static void JS_ConfigureApplierReplication(
    v8::FunctionCallbackInfo<v8::Value> const& args) {
  ConfigureApplierReplication(args, APPLIER_DATABASE);
}

static void JS_ConfigureGlobalApplierReplication(
    v8::FunctionCallbackInfo<v8::Value> const& args) {
  ConfigureApplierReplication(args, APPLIER_GLOBAL);
}

////////////////////////////////////////////////////////////////////////////////
/// @brief start the replication applier manually
////////////////////////////////////////////////////////////////////////////////

static void StartApplierReplication(
    v8::FunctionCallbackInfo<v8::Value> const& args, ApplierType applierType) {
  TRI_V8_TRY_CATCH_BEGIN(isolate);
  v8::HandleScope scope(isolate);

  if (args.Length() > 2) {
    TRI_V8_THROW_EXCEPTION_USAGE("start(<from>)");
  }

  TRI_voc_tick_t initialTick = 0;
  bool useTick = false;

  if (args.Length() >= 1) {
    initialTick = TRI_ObjectToUInt64(isolate, args[0], true);
    useTick = true;
  }

  ReplicationApplier* applier = getContinuousApplier(isolate, applierType);

  applier->startTailing(initialTick, useTick);

  TRI_V8_RETURN_TRUE();
  TRI_V8_TRY_CATCH_END
}

static void JS_StartApplierReplication(
    v8::FunctionCallbackInfo<v8::Value> const& args) {
  StartApplierReplication(args, APPLIER_DATABASE);
}

static void JS_StartGlobalApplierReplication(
    v8::FunctionCallbackInfo<v8::Value> const& args) {
  StartApplierReplication(args, APPLIER_GLOBAL);
}

////////////////////////////////////////////////////////////////////////////////
/// @brief shuts down the replication applier manually
////////////////////////////////////////////////////////////////////////////////

static void StopApplierReplication(
    v8::FunctionCallbackInfo<v8::Value> const& args, ApplierType applierType) {
  TRI_V8_TRY_CATCH_BEGIN(isolate);
  v8::HandleScope scope(isolate);

  if (args.Length() != 0) {
    TRI_V8_THROW_EXCEPTION_USAGE("stop()");
  }

  ReplicationApplier* applier = getContinuousApplier(isolate, applierType);

  applier->stopAndJoin();

  TRI_V8_RETURN_TRUE();
  TRI_V8_TRY_CATCH_END
}

static void JS_StopApplierReplication(
    v8::FunctionCallbackInfo<v8::Value> const& args) {
  StopApplierReplication(args, APPLIER_DATABASE);
}

static void JS_StopGlobalApplierReplication(
    v8::FunctionCallbackInfo<v8::Value> const& args) {
  StopApplierReplication(args, APPLIER_GLOBAL);
}

////////////////////////////////////////////////////////////////////////////////
/// @brief get the state of the replication applier
////////////////////////////////////////////////////////////////////////////////

static void StateApplierReplication(
    v8::FunctionCallbackInfo<v8::Value> const& args, ApplierType applierType) {
  TRI_V8_TRY_CATCH_BEGIN(isolate);
  v8::HandleScope scope(isolate);

  if (args.Length() != 0) {
    TRI_V8_THROW_EXCEPTION_USAGE("state()");
  }

  ReplicationApplier* applier = getContinuousApplier(isolate, applierType);

  VPackBuilder builder;
  builder.openObject();
  applier->toVelocyPack(builder);
  builder.close();

  v8::Handle<v8::Value> result = TRI_VPackToV8(isolate, builder.slice());

  TRI_V8_RETURN(result);
  TRI_V8_TRY_CATCH_END
}

static void StateApplierReplicationAll(
    v8::FunctionCallbackInfo<v8::Value> const& args, ApplierType applierType) {
  TRI_V8_TRY_CATCH_BEGIN(isolate);
  v8::HandleScope scope(isolate);

  if (args.Length() != 0) {
    TRI_V8_THROW_EXCEPTION_USAGE("stateAll()");
  }

  TRI_GET_GLOBALS();
  DatabaseFeature& databaseFeature =
      v8g->server().getFeature<DatabaseFeature>();

  VPackBuilder builder;
  builder.openObject();
  for (auto& name : databaseFeature.getDatabaseNames()) {
    auto vocbase = databaseFeature.useDatabase(name);

    if (vocbase == nullptr) {
      continue;
    }

    ReplicationApplier* applier = vocbase->replicationApplier();

    if (applier == nullptr) {
      continue;
    }

    builder.add(name, VPackValue(VPackValueType::Object));
    applier->toVelocyPack(builder);
    builder.close();
  }
  builder.close();
  v8::Handle<v8::Value> result = TRI_VPackToV8(isolate, builder.slice());

  TRI_V8_RETURN(result);
  TRI_V8_TRY_CATCH_END
}

static void JS_StateApplierReplication(
    v8::FunctionCallbackInfo<v8::Value> const& args) {
  StateApplierReplication(args, APPLIER_DATABASE);
}

static void JS_StateApplierReplicationAll(
    v8::FunctionCallbackInfo<v8::Value> const& args) {
  StateApplierReplicationAll(args, APPLIER_DATABASE);
}

static void JS_StateGlobalApplierReplication(
    v8::FunctionCallbackInfo<v8::Value> const& args) {
  StateApplierReplication(args, APPLIER_GLOBAL);
}

////////////////////////////////////////////////////////////////////////////////
/// @brief stop the replication applier and "forget" all state
////////////////////////////////////////////////////////////////////////////////

static void ForgetApplierReplication(
    v8::FunctionCallbackInfo<v8::Value> const& args, ApplierType applierType) {
  TRI_V8_TRY_CATCH_BEGIN(isolate);
  v8::HandleScope scope(isolate);

  if (args.Length() != 0) {
    TRI_V8_THROW_EXCEPTION_USAGE("forget()");
  }

  ReplicationApplier* applier = getContinuousApplier(isolate, applierType);

  applier->forget();

  TRI_V8_RETURN_TRUE();
  TRI_V8_TRY_CATCH_END
}

static void JS_ForgetApplierReplication(
    v8::FunctionCallbackInfo<v8::Value> const& args) {
  ForgetApplierReplication(args, APPLIER_DATABASE);
}

static void JS_ForgetGlobalApplierReplication(
    v8::FunctionCallbackInfo<v8::Value> const& args) {
  ForgetApplierReplication(args, APPLIER_GLOBAL);
}

static void JS_FailoverEnabledGlobalApplierReplication(
    v8::FunctionCallbackInfo<v8::Value> const& args) {
  TRI_V8_TRY_CATCH_BEGIN(isolate);
  v8::HandleScope scope(isolate);

  // response is hard-coded to false since 3.12
  TRI_V8_RETURN_FALSE();
  TRI_V8_TRY_CATCH_END
}

void TRI_InitV8Replication(v8::Isolate* isolate,
                           v8::Handle<v8::Context> context,
                           TRI_vocbase_t* vocbase, size_t threadNumber,
                           TRI_v8_global_t* v8g) {
  // replication functions. not intended to be used by end users

  // logger functions
  TRI_AddGlobalFunctionVocbase(
      isolate, TRI_V8_ASCII_STRING(isolate, "REPLICATION_LOGGER_STATE"),
      JS_StateLoggerReplication, true);
  TRI_AddGlobalFunctionVocbase(
      isolate, TRI_V8_ASCII_STRING(isolate, "REPLICATION_LOGGER_LAST"),
      JS_LastLoggerReplication, true);
  // applier functions
  TRI_AddGlobalFunctionVocbase(
      isolate, TRI_V8_ASCII_STRING(isolate, "REPLICATION_APPLIER_CONFIGURE"),
      JS_ConfigureApplierReplication, true);
  TRI_AddGlobalFunctionVocbase(
      isolate,
      TRI_V8_ASCII_STRING(isolate, "GLOBAL_REPLICATION_APPLIER_CONFIGURE"),
      JS_ConfigureGlobalApplierReplication, true);
  TRI_AddGlobalFunctionVocbase(
      isolate, TRI_V8_ASCII_STRING(isolate, "REPLICATION_APPLIER_START"),
      JS_StartApplierReplication, true);
  TRI_AddGlobalFunctionVocbase(
      isolate, TRI_V8_ASCII_STRING(isolate, "GLOBAL_REPLICATION_APPLIER_START"),
      JS_StartGlobalApplierReplication, true);
  TRI_AddGlobalFunctionVocbase(
      isolate, TRI_V8_ASCII_STRING(isolate, "REPLICATION_APPLIER_STOP"),
      JS_StopApplierReplication, true);
  TRI_AddGlobalFunctionVocbase(
      isolate, TRI_V8_ASCII_STRING(isolate, "GLOBAL_REPLICATION_APPLIER_STOP"),
      JS_StopGlobalApplierReplication, true);
  TRI_AddGlobalFunctionVocbase(
      isolate, TRI_V8_ASCII_STRING(isolate, "REPLICATION_APPLIER_STATE"),
      JS_StateApplierReplication, true);
  TRI_AddGlobalFunctionVocbase(
      isolate, TRI_V8_ASCII_STRING(isolate, "REPLICATION_APPLIER_STATE_ALL"),
      JS_StateApplierReplicationAll, true);
  TRI_AddGlobalFunctionVocbase(
      isolate, TRI_V8_ASCII_STRING(isolate, "GLOBAL_REPLICATION_APPLIER_STATE"),
      JS_StateGlobalApplierReplication, true);
  TRI_AddGlobalFunctionVocbase(
      isolate, TRI_V8_ASCII_STRING(isolate, "REPLICATION_APPLIER_FORGET"),
      JS_ForgetApplierReplication, true);
  TRI_AddGlobalFunctionVocbase(
      isolate,
      TRI_V8_ASCII_STRING(isolate, "GLOBAL_REPLICATION_APPLIER_FORGET"),
      JS_ForgetGlobalApplierReplication, true);
  TRI_AddGlobalFunctionVocbase(
      isolate,
      TRI_V8_ASCII_STRING(isolate,
                          "GLOBAL_REPLICATION_APPLIER_FAILOVER_ENABLED"),
      JS_FailoverEnabledGlobalApplierReplication, true);
}
