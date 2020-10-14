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
/// @author Dr. Frank Celler
////////////////////////////////////////////////////////////////////////////////

#include "v8-replication.h"
#include "ApplicationFeatures/ApplicationServer.h"
#include "Basics/StringUtils.h"
#include "Cluster/ClusterFeature.h"
#include "Logger/LogMacros.h"
#include "Logger/Logger.h"
#include "Logger/LoggerStream.h"
#include "Replication/DatabaseInitialSyncer.h"
#include "Replication/DatabaseReplicationApplier.h"
#include "Replication/DatabaseTailingSyncer.h"
#include "Replication/GlobalInitialSyncer.h"
#include "Replication/GlobalReplicationApplier.h"
#include "Replication/ReplicationApplierConfiguration.h"
#include "Replication/ReplicationFeature.h"
#include "Rest/Version.h"
#include "RestServer/DatabaseFeature.h"
#include "RestServer/ServerIdFeature.h"
#include "StorageEngine/EngineSelectorFeature.h"
#include "StorageEngine/StorageEngine.h"
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
#include <velocypack/velocypack-aliases.h>

using namespace arangodb;
using namespace arangodb::basics;
using namespace arangodb::rest;

////////////////////////////////////////////////////////////////////////////////
/// @brief get the state of the replication logger
////////////////////////////////////////////////////////////////////////////////

static void JS_StateLoggerReplication(v8::FunctionCallbackInfo<v8::Value> const& args) {
  // FIXME: use code in RestReplicationHandler and get rid of storage-engine
  //        dependent code here
  //
  TRI_V8_TRY_CATCH_BEGIN(isolate);
  v8::HandleScope scope(isolate);

  StorageEngine* engine = EngineSelectorFeature::ENGINE;
  v8::Handle<v8::Object> result = v8::Object::New(isolate);
  TRI_vocbase_t& vocbase = GetContextVocBase(isolate);

  VPackBuilder builder;
  auto res = engine->createLoggerState(&vocbase, builder);
  if (res.fail()) {
    TRI_V8_THROW_EXCEPTION(res);
  }
  v8::Handle<v8::Value> resultValue = TRI_VPackToV8(isolate, builder.slice());
  result = v8::Handle<v8::Object>::Cast(resultValue);

  TRI_V8_RETURN(result);
  TRI_V8_TRY_CATCH_END
}

////////////////////////////////////////////////////////////////////////////////
/// @brief get the tick ranges that can be provided by the replication logger
////////////////////////////////////////////////////////////////////////////////

static void JS_TickRangesLoggerReplication(v8::FunctionCallbackInfo<v8::Value> const& args) {
  TRI_V8_TRY_CATCH_BEGIN(isolate);
  v8::HandleScope scope(isolate);
  v8::Handle<v8::Array> result;

  VPackBuilder builder;
  Result res = EngineSelectorFeature::ENGINE->createTickRanges(builder);
  if (res.fail()) {
    TRI_V8_THROW_EXCEPTION(res);
  }

  v8::Handle<v8::Value> resultValue = TRI_VPackToV8(isolate, builder.slice());
  result = v8::Handle<v8::Array>::Cast(resultValue);

  TRI_V8_RETURN(result);
  TRI_V8_TRY_CATCH_END
}

////////////////////////////////////////////////////////////////////////////////
/// @brief get the first tick that can be provided by the replication logger
////////////////////////////////////////////////////////////////////////////////

static void JS_FirstTickLoggerReplication(v8::FunctionCallbackInfo<v8::Value> const& args) {
  TRI_V8_TRY_CATCH_BEGIN(isolate);
  v8::HandleScope scope(isolate);

  TRI_voc_tick_t tick = UINT64_MAX;
  Result res = EngineSelectorFeature::ENGINE->firstTick(tick);
  if (res.fail()) {
    TRI_V8_THROW_EXCEPTION(res);
  }

  if (tick == UINT64_MAX) {
    TRI_V8_RETURN(v8::Null(isolate));
  }

  TRI_V8_RETURN(TRI_V8UInt64String<TRI_voc_tick_t>(isolate, tick));
  TRI_V8_TRY_CATCH_END
}

////////////////////////////////////////////////////////////////////////////////
/// @brief get the last WAL entries
////////////////////////////////////////////////////////////////////////////////

static void JS_LastLoggerReplication(v8::FunctionCallbackInfo<v8::Value> const& args) {
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

  auto transactionContext = transaction::V8Context::Create(vocbase, true);
  VPackBuilder builder(transactionContext->getVPackOptions());
  Result res = EngineSelectorFeature::ENGINE->lastLogger(vocbase, tickStart, tickEnd, builder);
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

////////////////////////////////////////////////////////////////////////////////
/// @brief sync data from a remote master
////////////////////////////////////////////////////////////////////////////////

static void SynchronizeReplication(v8::FunctionCallbackInfo<v8::Value> const& args,
                                   ApplierType applierType) {
  TRI_V8_TRY_CATCH_BEGIN(isolate);
  v8::HandleScope scope(isolate);
  auto context = TRI_IGETC;

  if (args.Length() != 1 || !args[0]->IsObject()) {
    TRI_V8_THROW_EXCEPTION_USAGE("synchronize(<configuration>)");
  }

  // treat the argument as an object from now on
  v8::Handle<v8::Object> object = v8::Handle<v8::Object>::Cast(args[0]);
  VPackBuilder builder;
  int res = TRI_V8ToVPack(isolate, builder, args[0], false);

  if (res != TRI_ERROR_NO_ERROR) {
    TRI_V8_THROW_EXCEPTION(res);
  }

  auto& vocbase = GetContextVocBase(isolate);
  std::string databaseName;

  if (applierType == APPLIER_DATABASE) {
    databaseName = vocbase.name();
  }

  bool keepBarrier = false;

  if (TRI_HasProperty(context, isolate, object, "keepBarrier")) {
    keepBarrier = TRI_ObjectToBoolean(
        isolate,
        object->Get(TRI_IGETC, TRI_V8_ASCII_STRING(isolate, "keepBarrier"))
            .FromMaybe(v8::Local<v8::Value>()));
  }

  TRI_GET_GLOBALS();
  ReplicationApplierConfiguration configuration =
      ReplicationApplierConfiguration::fromVelocyPack(v8g->_server, builder.slice(), databaseName);
  configuration.validate();

  v8::Handle<v8::Object> result = v8::Object::New(isolate);
  std::shared_ptr<InitialSyncer> syncer;

  if (applierType == APPLIER_DATABASE) {
    // database-specific synchronization
    syncer.reset(new DatabaseInitialSyncer(vocbase, configuration));

    if (TRI_HasProperty(context, isolate, object, "leaderId")) {
      syncer->setLeaderId(TRI_ObjectToString(
          isolate,
          object->Get(TRI_IGETC, TRI_V8_ASCII_STRING(isolate, "leaderId"))
              .FromMaybe(v8::Local<v8::Value>())));
    }
  } else if (applierType == APPLIER_GLOBAL) {
    configuration._skipCreateDrop = false;
    syncer.reset(new GlobalInitialSyncer(configuration));
  } else {
    TRI_ASSERT(false);
  }

  try {
    Result r = syncer->run(configuration._incremental);

    if (r.fail()) {
      LOG_TOPIC("3d58b", DEBUG, Logger::REPLICATION)
          << "initial sync failed for database '" << vocbase.name()
          << "': " << r.errorMessage();
      TRI_V8_THROW_EXCEPTION_MESSAGE(r.errorNumber(),
                                     "cannot sync from remote endpoint: " + r.errorMessage() +
                                         ". last progress message was: '" +
                                         syncer->progress() + "'");
    }

    if (keepBarrier) {
      result->Set(context,
                  TRI_V8_ASCII_STRING(isolate, "barrierId"),
                  TRI_V8UInt64String<TRI_voc_tick_t>(isolate, syncer->stealBarrier())).FromMaybe(false);
    }

    result->Set(context,
                TRI_V8_ASCII_STRING(isolate, "lastLogTick"),
                TRI_V8UInt64String<TRI_voc_tick_t>(isolate, syncer->getLastLogTick())).FromMaybe(false);

    std::map<TRI_voc_cid_t, std::string>::const_iterator it;
    std::map<TRI_voc_cid_t, std::string> const& c = syncer->getProcessedCollections();

    uint32_t j = 0;
    v8::Handle<v8::Array> collections = v8::Array::New(isolate);
    for (it = c.begin(); it != c.end(); ++it) {
      std::string const cidString = StringUtils::itoa((*it).first);

      v8::Handle<v8::Object> ci = v8::Object::New(isolate);
      ci->Set(context, TRI_V8_ASCII_STRING(isolate, "id"), TRI_V8_STD_STRING(isolate, cidString)).FromMaybe(false);
      ci->Set(context, TRI_V8_ASCII_STRING(isolate, "name"),
              TRI_V8_STD_STRING(isolate, (*it).second)).FromMaybe(false);

      collections->Set(context, j++, ci).FromMaybe(false);
    }

    result->Set(context, TRI_V8_ASCII_STRING(isolate, "collections"), collections).FromMaybe(false);
  } catch (arangodb::basics::Exception const& ex) {
    TRI_V8_THROW_EXCEPTION_MESSAGE(
        ex.code(), std::string("cannot sync from remote endpoint: ") +
                       ex.what() + ". last progress message was: '" +
                       syncer->progress() + "'");
  } catch (std::exception const& ex) {
    TRI_V8_THROW_EXCEPTION_MESSAGE(
        TRI_ERROR_INTERNAL, std::string("cannot sync from remote endpoint: ") +
                                ex.what() + ". last progress message was: '" +
                                syncer->progress() + "'");
  } catch (...) {
    TRI_V8_THROW_EXCEPTION_MESSAGE(
        TRI_ERROR_INTERNAL,
        std::string("cannot sync from remote endpoint: unknown exception. last "
                    "progress message was: '") +
            syncer->progress() + "'");
  }

  // Now check forSynchronousReplication flag and tell ClusterInfo
  // about a new follower.

  TRI_V8_RETURN(result);
  TRI_V8_TRY_CATCH_END
}

static void JS_SynchronizeReplication(v8::FunctionCallbackInfo<v8::Value> const& args) {
  SynchronizeReplication(args, APPLIER_DATABASE);
}

static void JS_SynchronizeGlobalReplication(v8::FunctionCallbackInfo<v8::Value> const& args) {
  SynchronizeReplication(args, APPLIER_GLOBAL);
}

////////////////////////////////////////////////////////////////////////////////
/// @brief return the server's id
////////////////////////////////////////////////////////////////////////////////

static void JS_ServerIdReplication(v8::FunctionCallbackInfo<v8::Value> const& args) {
  TRI_V8_TRY_CATCH_BEGIN(isolate);
  v8::HandleScope scope(isolate);

  std::string const serverId = StringUtils::itoa(ServerIdFeature::getId().id());
  TRI_V8_RETURN_STD_STRING(serverId);
  TRI_V8_TRY_CATCH_END
}

static ReplicationApplier* getContinuousApplier(v8::Isolate* isolate, ApplierType applierType) {
  ReplicationApplier* applier = nullptr;

  if (applierType == APPLIER_DATABASE) {
    // database-specific applier
    auto& vocbase = GetContextVocBase(isolate);

    applier = vocbase.replicationApplier();
  } else {
    // applier type global
    TRI_GET_GLOBALS();
    auto& replicationFeature = v8g->_server.getFeature<ReplicationFeature>();
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

static void ConfigureApplierReplication(v8::FunctionCallbackInfo<v8::Value> const& args,
                                        ApplierType applierType) {
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
    int res = TRI_V8ToVPack(isolate, builder, args[0], false);

    if (res != TRI_ERROR_NO_ERROR) {
      TRI_V8_THROW_EXCEPTION(res);
    }

    std::string databaseName;
    if (applierType == APPLIER_DATABASE) {
      auto& vocbase = GetContextVocBase(isolate);

      databaseName = vocbase.name();
    }

    // merge the passed configuration into the existing one
    configuration =
        ReplicationApplierConfiguration::fromVelocyPack(configuration,
                                                        builder.slice(), databaseName);

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

static void JS_ConfigureApplierReplication(v8::FunctionCallbackInfo<v8::Value> const& args) {
  ConfigureApplierReplication(args, APPLIER_DATABASE);
}

static void JS_ConfigureGlobalApplierReplication(v8::FunctionCallbackInfo<v8::Value> const& args) {
  ConfigureApplierReplication(args, APPLIER_GLOBAL);
}

////////////////////////////////////////////////////////////////////////////////
/// @brief start the replication applier manually
////////////////////////////////////////////////////////////////////////////////

static void StartApplierReplication(v8::FunctionCallbackInfo<v8::Value> const& args,
                                    ApplierType applierType) {
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

  TRI_voc_tick_t barrierId = 0;
  if (args.Length() >= 2) {
    barrierId = TRI_ObjectToUInt64(isolate, args[1], true);
  }

  ReplicationApplier* applier = getContinuousApplier(isolate, applierType);

  applier->startTailing(initialTick, useTick, barrierId);

  TRI_V8_RETURN_TRUE();
  TRI_V8_TRY_CATCH_END
}

static void JS_StartApplierReplication(v8::FunctionCallbackInfo<v8::Value> const& args) {
  StartApplierReplication(args, APPLIER_DATABASE);
}

static void JS_StartGlobalApplierReplication(v8::FunctionCallbackInfo<v8::Value> const& args) {
  StartApplierReplication(args, APPLIER_GLOBAL);
}

////////////////////////////////////////////////////////////////////////////////
/// @brief shuts down the replication applier manually
////////////////////////////////////////////////////////////////////////////////

static void StopApplierReplication(v8::FunctionCallbackInfo<v8::Value> const& args,
                                   ApplierType applierType) {
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

static void JS_StopApplierReplication(v8::FunctionCallbackInfo<v8::Value> const& args) {
  StopApplierReplication(args, APPLIER_DATABASE);
}

static void JS_StopGlobalApplierReplication(v8::FunctionCallbackInfo<v8::Value> const& args) {
  StopApplierReplication(args, APPLIER_GLOBAL);
}

////////////////////////////////////////////////////////////////////////////////
/// @brief get the state of the replication applier
////////////////////////////////////////////////////////////////////////////////

static void StateApplierReplication(v8::FunctionCallbackInfo<v8::Value> const& args,
                                    ApplierType applierType) {
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

static void StateApplierReplicationAll(v8::FunctionCallbackInfo<v8::Value> const& args,
                                       ApplierType applierType) {
  TRI_V8_TRY_CATCH_BEGIN(isolate);
  v8::HandleScope scope(isolate);

  if (args.Length() != 0) {
    TRI_V8_THROW_EXCEPTION_USAGE("stateAll()");
  }

  TRI_GET_GLOBALS();
  DatabaseFeature& databaseFeature = v8g->_server.getFeature<DatabaseFeature>();

  VPackBuilder builder;
  builder.openObject();
  for (auto& name : databaseFeature.getDatabaseNames()) {
    TRI_vocbase_t* vocbase = databaseFeature.lookupDatabase(name);

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

static void JS_StateApplierReplication(v8::FunctionCallbackInfo<v8::Value> const& args) {
  StateApplierReplication(args, APPLIER_DATABASE);
}

static void JS_StateApplierReplicationAll(v8::FunctionCallbackInfo<v8::Value> const& args) {
  StateApplierReplicationAll(args, APPLIER_DATABASE);
}

static void JS_StateGlobalApplierReplication(v8::FunctionCallbackInfo<v8::Value> const& args) {
  StateApplierReplication(args, APPLIER_GLOBAL);
}

////////////////////////////////////////////////////////////////////////////////
/// @brief stop the replication applier and "forget" all state
////////////////////////////////////////////////////////////////////////////////

static void ForgetApplierReplication(v8::FunctionCallbackInfo<v8::Value> const& args,
                                     ApplierType applierType) {
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

static void JS_ForgetApplierReplication(v8::FunctionCallbackInfo<v8::Value> const& args) {
  ForgetApplierReplication(args, APPLIER_DATABASE);
}

static void JS_ForgetGlobalApplierReplication(v8::FunctionCallbackInfo<v8::Value> const& args) {
  ForgetApplierReplication(args, APPLIER_GLOBAL);
}

static void JS_FailoverEnabledGlobalApplierReplication(v8::FunctionCallbackInfo<v8::Value> const& args) {
  TRI_V8_TRY_CATCH_BEGIN(isolate);
  v8::HandleScope scope(isolate);

  auto replicationFeature = ReplicationFeature::INSTANCE;
  if (replicationFeature != nullptr && replicationFeature->isActiveFailoverEnabled()) {
    TRI_V8_RETURN_TRUE();
  }
  TRI_V8_RETURN_FALSE();
  TRI_V8_TRY_CATCH_END
}

void TRI_InitV8Replication(v8::Isolate* isolate, v8::Handle<v8::Context> context,
                           TRI_vocbase_t* vocbase, size_t threadNumber,
                           TRI_v8_global_t* v8g) {
  // replication functions. not intended to be used by end users

  // logger functions
  TRI_AddGlobalFunctionVocbase(isolate,
                               TRI_V8_ASCII_STRING(isolate,
                                                   "REPLICATION_LOGGER_STATE"),
                               JS_StateLoggerReplication, true);
  TRI_AddGlobalFunctionVocbase(isolate,
                               TRI_V8_ASCII_STRING(isolate,
                                                   "REPLICATION_LOGGER_LAST"),
                               JS_LastLoggerReplication, true);
  TRI_AddGlobalFunctionVocbase(
      isolate, TRI_V8_ASCII_STRING(isolate, "REPLICATION_LOGGER_TICK_RANGES"),
      JS_TickRangesLoggerReplication, true);
  TRI_AddGlobalFunctionVocbase(
      isolate, TRI_V8_ASCII_STRING(isolate, "REPLICATION_LOGGER_FIRST_TICK"),
      JS_FirstTickLoggerReplication, true);

  // applier functions
  TRI_AddGlobalFunctionVocbase(
      isolate, TRI_V8_ASCII_STRING(isolate, "REPLICATION_APPLIER_CONFIGURE"),
      JS_ConfigureApplierReplication, true);
  TRI_AddGlobalFunctionVocbase(
      isolate,
      TRI_V8_ASCII_STRING(isolate, "GLOBAL_REPLICATION_APPLIER_CONFIGURE"),
      JS_ConfigureGlobalApplierReplication, true);
  TRI_AddGlobalFunctionVocbase(isolate,
                               TRI_V8_ASCII_STRING(isolate,
                                                   "REPLICATION_APPLIER_START"),
                               JS_StartApplierReplication, true);
  TRI_AddGlobalFunctionVocbase(
      isolate, TRI_V8_ASCII_STRING(isolate, "GLOBAL_REPLICATION_APPLIER_START"),
      JS_StartGlobalApplierReplication, true);
  TRI_AddGlobalFunctionVocbase(isolate,
                               TRI_V8_ASCII_STRING(isolate,
                                                   "REPLICATION_APPLIER_STOP"),
                               JS_StopApplierReplication, true);
  TRI_AddGlobalFunctionVocbase(
      isolate, TRI_V8_ASCII_STRING(isolate, "GLOBAL_REPLICATION_APPLIER_STOP"),
      JS_StopGlobalApplierReplication, true);
  TRI_AddGlobalFunctionVocbase(isolate,
                               TRI_V8_ASCII_STRING(isolate,
                                                   "REPLICATION_APPLIER_STATE"),
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
      isolate, TRI_V8_ASCII_STRING(isolate, "GLOBAL_REPLICATION_APPLIER_FORGET"),
      JS_ForgetGlobalApplierReplication, true);
  TRI_AddGlobalFunctionVocbase(
      isolate,
      TRI_V8_ASCII_STRING(isolate,
                          "GLOBAL_REPLICATION_APPLIER_FAILOVER_ENABLED"),
      JS_FailoverEnabledGlobalApplierReplication, true);

  // other functions
  TRI_AddGlobalFunctionVocbase(isolate,
                               TRI_V8_ASCII_STRING(isolate,
                                                   "REPLICATION_SYNCHRONIZE"),
                               JS_SynchronizeReplication, true);
  TRI_AddGlobalFunctionVocbase(
      isolate, TRI_V8_ASCII_STRING(isolate, "GLOBAL_REPLICATION_SYNCHRONIZE"),
      JS_SynchronizeGlobalReplication, true);
  TRI_AddGlobalFunctionVocbase(isolate,
                               TRI_V8_ASCII_STRING(isolate,
                                                   "REPLICATION_SERVER_ID"),
                               JS_ServerIdReplication, true);
}
