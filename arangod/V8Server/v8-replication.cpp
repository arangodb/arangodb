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
#include "Replication/InitialSyncer.h"
#include "V8/v8-conv.h"
#include "V8/v8-globals.h"
#include "V8/v8-utils.h"
#include "V8Server/v8-vocbaseprivate.h"
#include "VocBase/replication-dump.h"
#include "Wal/LogfileManager.h"

using namespace arangodb;
using namespace arangodb::basics;
using namespace arangodb::rest;

////////////////////////////////////////////////////////////////////////////////
/// @brief get the state of the replication logger
////////////////////////////////////////////////////////////////////////////////

static void JS_StateLoggerReplication(
    v8::FunctionCallbackInfo<v8::Value> const& args) {
  TRI_V8_TRY_CATCH_BEGIN(isolate);
  v8::HandleScope scope(isolate);

  arangodb::wal::LogfileManagerState s =
      arangodb::wal::LogfileManager::instance()->state();

  v8::Handle<v8::Object> result = v8::Object::New(isolate);

  v8::Handle<v8::Object> state = v8::Object::New(isolate);
  state->Set(TRI_V8_ASCII_STRING("running"), v8::True(isolate));
  state->Set(TRI_V8_ASCII_STRING("lastLogTick"), V8TickId(isolate, s.lastTick));
  state->Set(TRI_V8_ASCII_STRING("totalEvents"),
             v8::Number::New(isolate, (double)s.numEvents));
  state->Set(TRI_V8_ASCII_STRING("time"), TRI_V8_STD_STRING(s.timeString));
  result->Set(TRI_V8_ASCII_STRING("state"), state);

  v8::Handle<v8::Object> server = v8::Object::New(isolate);
  server->Set(TRI_V8_ASCII_STRING("version"), TRI_V8_ASCII_STRING(TRI_VERSION));
  server->Set(TRI_V8_ASCII_STRING("serverId"),
              TRI_V8_STD_STRING(StringUtils::itoa(TRI_GetIdServer())));
  result->Set(TRI_V8_ASCII_STRING("server"), server);

  v8::Handle<v8::Object> clients = v8::Object::New(isolate);
  result->Set(TRI_V8_ASCII_STRING("clients"), clients);

  TRI_V8_RETURN(result);
  TRI_V8_TRY_CATCH_END
}

////////////////////////////////////////////////////////////////////////////////
/// @brief get the tick ranges that can be provided by the replication logger
////////////////////////////////////////////////////////////////////////////////

static void JS_TickRangesLoggerReplication(
    v8::FunctionCallbackInfo<v8::Value> const& args) {
  TRI_V8_TRY_CATCH_BEGIN(isolate);
  v8::HandleScope scope(isolate);

  auto const& ranges = arangodb::wal::LogfileManager::instance()->ranges();

  v8::Handle<v8::Array> result = v8::Array::New(isolate, (int)ranges.size());
  uint32_t i = 0;

  for (auto& it : ranges) {
    v8::Handle<v8::Object> df = v8::Object::New(isolate);

    df->ForceSet(TRI_V8_ASCII_STRING("datafile"),
                 TRI_V8_STD_STRING(it.filename));
    df->ForceSet(TRI_V8_ASCII_STRING("state"), TRI_V8_STD_STRING(it.state));
    df->ForceSet(TRI_V8_ASCII_STRING("tickMin"), V8TickId(isolate, it.tickMin));
    df->ForceSet(TRI_V8_ASCII_STRING("tickMax"), V8TickId(isolate, it.tickMax));

    result->Set(i++, df);
  }

  TRI_V8_RETURN(result);
  TRI_V8_TRY_CATCH_END
}

////////////////////////////////////////////////////////////////////////////////
/// @brief get the first tick that can be provided by the replication logger
////////////////////////////////////////////////////////////////////////////////

static void JS_FirstTickLoggerReplication(
    v8::FunctionCallbackInfo<v8::Value> const& args) {
  TRI_V8_TRY_CATCH_BEGIN(isolate);
  v8::HandleScope scope(isolate);

  auto const& ranges = arangodb::wal::LogfileManager::instance()->ranges();

  TRI_voc_tick_t tick = UINT64_MAX;

  for (auto& it : ranges) {
    if (it.tickMin == 0) {
      continue;
    }

    if (it.tickMin < tick) {
      tick = it.tickMin;
    }
  }

  if (tick == UINT64_MAX) {
    TRI_V8_RETURN(v8::Null(isolate));
  }

  TRI_V8_RETURN(V8TickId(isolate, tick));
  TRI_V8_TRY_CATCH_END
}

////////////////////////////////////////////////////////////////////////////////
/// @brief get the last WAL entries
////////////////////////////////////////////////////////////////////////////////

static void JS_LastLoggerReplication(
    v8::FunctionCallbackInfo<v8::Value> const& args) {
  TRI_V8_TRY_CATCH_BEGIN(isolate);
  v8::HandleScope scope(isolate);

  TRI_vocbase_t* vocbase = GetContextVocBase(isolate);

  if (vocbase == nullptr) {
    TRI_V8_THROW_EXCEPTION(TRI_ERROR_ARANGO_DATABASE_NOT_FOUND);
  }

  if (args.Length() != 2) {
    TRI_V8_THROW_EXCEPTION_USAGE(
        "REPLICATION_LOGGER_LAST(<fromTick>, <toTick>)");
  }

  TRI_replication_dump_t dump(vocbase, 0, true, 0);
  TRI_voc_tick_t tickStart = TRI_ObjectToUInt64(args[0], true);
  TRI_voc_tick_t tickEnd = TRI_ObjectToUInt64(args[1], true);

  int res = TRI_DumpLogReplication(&dump, std::unordered_set<TRI_voc_tid_t>(),
                                   0, tickStart, tickEnd, true);

  if (res != TRI_ERROR_NO_ERROR) {
    TRI_V8_THROW_EXCEPTION(res);
  }

  TRI_json_t* json =
      TRI_JsonString(TRI_UNKNOWN_MEM_ZONE, dump._buffer->_buffer);

  if (json == nullptr) {
    TRI_V8_THROW_EXCEPTION_MEMORY();
  }

  v8::Handle<v8::Value> result = TRI_ObjectJson(isolate, json);
  TRI_FreeJson(TRI_UNKNOWN_MEM_ZONE, json);

  TRI_V8_RETURN(result);
  TRI_V8_TRY_CATCH_END
}

////////////////////////////////////////////////////////////////////////////////
/// @brief sync data from a remote master
////////////////////////////////////////////////////////////////////////////////

static void JS_SynchronizeReplication(
    v8::FunctionCallbackInfo<v8::Value> const& args) {
  TRI_V8_TRY_CATCH_BEGIN(isolate);
  v8::HandleScope scope(isolate);

  if (args.Length() != 1 || !args[0]->IsObject()) {
    TRI_V8_THROW_EXCEPTION_USAGE("REPLICATION_SYNCHRONIZE(<config>)");
  }

  TRI_vocbase_t* vocbase = GetContextVocBase(isolate);

  if (vocbase == nullptr) {
    TRI_V8_THROW_EXCEPTION(TRI_ERROR_ARANGO_DATABASE_NOT_FOUND);
  }

  // treat the argument as an object from now on
  v8::Handle<v8::Object> object = v8::Handle<v8::Object>::Cast(args[0]);

  std::string endpoint;
  if (object->Has(TRI_V8_ASCII_STRING("endpoint"))) {
    endpoint = TRI_ObjectToString(object->Get(TRI_V8_ASCII_STRING("endpoint")));
  }

  std::string database;
  if (object->Has(TRI_V8_ASCII_STRING("database"))) {
    database = TRI_ObjectToString(object->Get(TRI_V8_ASCII_STRING("database")));
  } else {
    database = std::string(vocbase->_name);
  }

  std::string username;
  if (object->Has(TRI_V8_ASCII_STRING("username"))) {
    username = TRI_ObjectToString(object->Get(TRI_V8_ASCII_STRING("username")));
  }

  std::string password;
  if (object->Has(TRI_V8_ASCII_STRING("password"))) {
    password = TRI_ObjectToString(object->Get(TRI_V8_ASCII_STRING("password")));
  }

  std::unordered_map<std::string, bool> restrictCollections;
  if (object->Has(TRI_V8_ASCII_STRING("restrictCollections")) &&
      object->Get(TRI_V8_ASCII_STRING("restrictCollections"))->IsArray()) {
    v8::Handle<v8::Array> a = v8::Handle<v8::Array>::Cast(
        object->Get(TRI_V8_ASCII_STRING("restrictCollections")));

    uint32_t const n = a->Length();

    for (uint32_t i = 0; i < n; ++i) {
      v8::Handle<v8::Value> cname = a->Get(i);

      if (cname->IsString()) {
        restrictCollections.emplace(
            std::make_pair(TRI_ObjectToString(cname), true));
      }
    }
  }

  std::string restrictType;
  if (object->Has(TRI_V8_ASCII_STRING("restrictType"))) {
    restrictType =
        TRI_ObjectToString(object->Get(TRI_V8_ASCII_STRING("restrictType")));
  }

  bool verbose = true;
  if (object->Has(TRI_V8_ASCII_STRING("verbose"))) {
    verbose = TRI_ObjectToBoolean(object->Get(TRI_V8_ASCII_STRING("verbose")));
  }

  if (endpoint.empty()) {
    TRI_V8_THROW_EXCEPTION_PARAMETER("<endpoint> must be a valid endpoint");
  }

  if ((restrictType.empty() && !restrictCollections.empty()) ||
      (!restrictType.empty() && restrictCollections.empty()) ||
      (!restrictType.empty() && restrictType != "include" &&
       restrictType != "exclude")) {
    TRI_V8_THROW_EXCEPTION_PARAMETER(
        "invalid value for <restrictCollections> or <restrictType>");
  }

  TRI_replication_applier_configuration_t config;
  config._endpoint = endpoint;
  config._database = database;
  config._username = username;
  config._password = password;

  if (object->Has(TRI_V8_ASCII_STRING("chunkSize"))) {
    if (object->Get(TRI_V8_ASCII_STRING("chunkSize"))->IsNumber()) {
      config._chunkSize = TRI_ObjectToUInt64(
          object->Get(TRI_V8_ASCII_STRING("chunkSize")), true);
    }
  }

  if (object->Has(TRI_V8_ASCII_STRING("includeSystem"))) {
    if (object->Get(TRI_V8_ASCII_STRING("includeSystem"))->IsBoolean()) {
      config._includeSystem = TRI_ObjectToBoolean(
          object->Get(TRI_V8_ASCII_STRING("includeSystem")));
    }
  }

  if (object->Has(TRI_V8_ASCII_STRING("requireFromPresent"))) {
    if (object->Get(TRI_V8_ASCII_STRING("requireFromPresent"))->IsBoolean()) {
      config._requireFromPresent = TRI_ObjectToBoolean(
          object->Get(TRI_V8_ASCII_STRING("requireFromPresent")));
    }
  }

  bool incremental = false;
  if (object->Has(TRI_V8_ASCII_STRING("incremental"))) {
    if (object->Get(TRI_V8_ASCII_STRING("incremental"))->IsBoolean()) {
      incremental =
          TRI_ObjectToBoolean(object->Get(TRI_V8_ASCII_STRING("incremental")));
    }
  }
 
  bool keepBarrier = false;
  if (object->Has(TRI_V8_ASCII_STRING("keepBarrier"))) {
    keepBarrier = TRI_ObjectToBoolean(object->Get(TRI_V8_ASCII_STRING("keepBarrier")));
  }

  std::string shardFollower;
  if (object->Has(TRI_V8_ASCII_STRING("shardFollower"))) {
    if (object->Get(TRI_V8_ASCII_STRING("shardFollower"))->IsString()) {
      shardFollower =
          TRI_ObjectToString(object->Get(TRI_V8_ASCII_STRING("shardFollower")));
    }
  }

  std::string errorMsg = "";
  InitialSyncer syncer(vocbase, &config, restrictCollections, restrictType,
                       verbose);

  int res = TRI_ERROR_NO_ERROR;
  v8::Handle<v8::Object> result = v8::Object::New(isolate);

  try {
    res = syncer.run(errorMsg, incremental);

    if (keepBarrier) {
      result->Set(TRI_V8_ASCII_STRING("barrierId"),
                  V8TickId(isolate, syncer.stealBarrier()));
    }

    result->Set(TRI_V8_ASCII_STRING("lastLogTick"),
                V8TickId(isolate, syncer.getLastLogTick()));

    std::map<TRI_voc_cid_t, std::string>::const_iterator it;
    std::map<TRI_voc_cid_t, std::string> const& c =
        syncer.getProcessedCollections();

    uint32_t j = 0;
    v8::Handle<v8::Array> collections = v8::Array::New(isolate);
    for (it = c.begin(); it != c.end(); ++it) {
      std::string const cidString = StringUtils::itoa((*it).first);

      v8::Handle<v8::Object> ci = v8::Object::New(isolate);
      ci->Set(TRI_V8_ASCII_STRING("id"), TRI_V8_STD_STRING(cidString));
      ci->Set(TRI_V8_ASCII_STRING("name"), TRI_V8_STD_STRING((*it).second));

      collections->Set(j++, ci);
    }

    result->Set(TRI_V8_ASCII_STRING("collections"), collections);
  } catch (...) {
  }

  if (res != TRI_ERROR_NO_ERROR) {
    if (errorMsg.empty()) {
      TRI_V8_THROW_EXCEPTION_MESSAGE(res, "cannot sync from remote endpoint");
    } else {
      TRI_V8_THROW_EXCEPTION_MESSAGE(
          res, "cannot sync from remote endpoint: " + errorMsg);
    }
  }

  // Now check forSynchronousReplication flag and tell ClusterInfo
  // about a new follower.

  TRI_V8_RETURN(result);
  TRI_V8_TRY_CATCH_END
}

////////////////////////////////////////////////////////////////////////////////
/// @brief return the server's id
////////////////////////////////////////////////////////////////////////////////

static void JS_ServerIdReplication(
    v8::FunctionCallbackInfo<v8::Value> const& args) {
  TRI_V8_TRY_CATCH_BEGIN(isolate);
  v8::HandleScope scope(isolate);

  std::string const serverId = StringUtils::itoa(TRI_GetIdServer());
  TRI_V8_RETURN_STD_STRING(serverId);
  TRI_V8_TRY_CATCH_END
}

////////////////////////////////////////////////////////////////////////////////
/// @brief configure the replication applier manually
////////////////////////////////////////////////////////////////////////////////

static void JS_ConfigureApplierReplication(
    v8::FunctionCallbackInfo<v8::Value> const& args) {
  TRI_V8_TRY_CATCH_BEGIN(isolate);
  v8::HandleScope scope(isolate);

  TRI_vocbase_t* vocbase = GetContextVocBase(isolate);

  if (vocbase == nullptr) {
    TRI_V8_THROW_EXCEPTION(TRI_ERROR_ARANGO_DATABASE_NOT_FOUND);
  }

  if (vocbase->_replicationApplier == nullptr) {
    TRI_V8_THROW_EXCEPTION(TRI_ERROR_INTERNAL);
  }

  if (args.Length() == 0) {
    // no argument: return the current configuration

    TRI_replication_applier_configuration_t config;

    {
      READ_LOCKER(readLocker, vocbase->_replicationApplier->_statusLock);
      config.update(&vocbase->_replicationApplier->_configuration);
    }

    std::unique_ptr<TRI_json_t> json(config.toJson());

    if (json == nullptr) {
      TRI_V8_THROW_EXCEPTION_MEMORY();
    }

    v8::Handle<v8::Value> result = TRI_ObjectJson(isolate, json.get());

    TRI_V8_RETURN(result);
  }

  else {
    // set the configuration

    if (args.Length() != 1 || !args[0]->IsObject()) {
      TRI_V8_THROW_EXCEPTION_USAGE(
          "REPLICATION_APPLIER_CONFIGURE(<configuration>)");
    }

    TRI_replication_applier_configuration_t config;

    // fill with previous configuration
    {
      READ_LOCKER(readLocker, vocbase->_replicationApplier->_statusLock);
      config.update(&vocbase->_replicationApplier->_configuration);
    }

    // treat the argument as an object from now on
    v8::Handle<v8::Object> object = v8::Handle<v8::Object>::Cast(args[0]);

    if (object->Has(TRI_V8_ASCII_STRING("endpoint"))) {
      if (object->Get(TRI_V8_ASCII_STRING("endpoint"))->IsString()) {
        config._endpoint =
            TRI_ObjectToString(object->Get(TRI_V8_ASCII_STRING("endpoint")));
      }
    }

    if (object->Has(TRI_V8_ASCII_STRING("database"))) {
      if (object->Get(TRI_V8_ASCII_STRING("database"))->IsString()) {
        config._database =
            TRI_ObjectToString(object->Get(TRI_V8_ASCII_STRING("database")));
      }
    } else {
      if (config._database.empty()) {
        // no database set, use current
        config._database = std::string(vocbase->_name);
      }
    }

    if (object->Has(TRI_V8_ASCII_STRING("username"))) {
      if (object->Get(TRI_V8_ASCII_STRING("username"))->IsString()) {
        config._username =
            TRI_ObjectToString(object->Get(TRI_V8_ASCII_STRING("username")));
      }
    }

    if (object->Has(TRI_V8_ASCII_STRING("password"))) {
      if (object->Get(TRI_V8_ASCII_STRING("password"))->IsString()) {
        config._password =
            TRI_ObjectToString(object->Get(TRI_V8_ASCII_STRING("password")));
      }
    }

    if (object->Has(TRI_V8_ASCII_STRING("requestTimeout"))) {
      if (object->Get(TRI_V8_ASCII_STRING("requestTimeout"))->IsNumber()) {
        config._requestTimeout = TRI_ObjectToDouble(
            object->Get(TRI_V8_ASCII_STRING("requestTimeout")));
      }
    }

    if (object->Has(TRI_V8_ASCII_STRING("connectTimeout"))) {
      if (object->Get(TRI_V8_ASCII_STRING("connectTimeout"))->IsNumber()) {
        config._connectTimeout = TRI_ObjectToDouble(
            object->Get(TRI_V8_ASCII_STRING("connectTimeout")));
      }
    }

    if (object->Has(TRI_V8_ASCII_STRING("ignoreErrors"))) {
      if (object->Get(TRI_V8_ASCII_STRING("ignoreErrors"))->IsNumber()) {
        config._ignoreErrors = TRI_ObjectToUInt64(
            object->Get(TRI_V8_ASCII_STRING("ignoreErrors")), false);
      }
    }

    if (object->Has(TRI_V8_ASCII_STRING("maxConnectRetries"))) {
      if (object->Get(TRI_V8_ASCII_STRING("maxConnectRetries"))->IsNumber()) {
        config._maxConnectRetries = TRI_ObjectToUInt64(
            object->Get(TRI_V8_ASCII_STRING("maxConnectRetries")), false);
      }
    }

    if (object->Has(TRI_V8_ASCII_STRING("sslProtocol"))) {
      if (object->Get(TRI_V8_ASCII_STRING("sslProtocol"))->IsNumber()) {
        config._sslProtocol = (uint32_t)TRI_ObjectToUInt64(
            object->Get(TRI_V8_ASCII_STRING("sslProtocol")), false);
      }
    }

    if (object->Has(TRI_V8_ASCII_STRING("chunkSize"))) {
      if (object->Get(TRI_V8_ASCII_STRING("chunkSize"))->IsNumber()) {
        config._chunkSize = TRI_ObjectToUInt64(
            object->Get(TRI_V8_ASCII_STRING("chunkSize")), true);
      }
    }

    if (object->Has(TRI_V8_ASCII_STRING("autoStart"))) {
      if (object->Get(TRI_V8_ASCII_STRING("autoStart"))->IsBoolean()) {
        config._autoStart =
            TRI_ObjectToBoolean(object->Get(TRI_V8_ASCII_STRING("autoStart")));
      }
    }

    if (object->Has(TRI_V8_ASCII_STRING("adaptivePolling"))) {
      if (object->Get(TRI_V8_ASCII_STRING("adaptivePolling"))->IsBoolean()) {
        config._adaptivePolling = TRI_ObjectToBoolean(
            object->Get(TRI_V8_ASCII_STRING("adaptivePolling")));
      }
    }

    if (object->Has(TRI_V8_ASCII_STRING("autoResync"))) {
      if (object->Get(TRI_V8_ASCII_STRING("autoResync"))->IsBoolean()) {
        config._autoResync =
            TRI_ObjectToBoolean(object->Get(TRI_V8_ASCII_STRING("autoResync")));
      }
    }

    if (object->Has(TRI_V8_ASCII_STRING("includeSystem"))) {
      if (object->Get(TRI_V8_ASCII_STRING("includeSystem"))->IsBoolean()) {
        config._includeSystem = TRI_ObjectToBoolean(
            object->Get(TRI_V8_ASCII_STRING("includeSystem")));
      }
    }

    if (object->Has(TRI_V8_ASCII_STRING("requireFromPresent"))) {
      if (object->Get(TRI_V8_ASCII_STRING("requireFromPresent"))->IsBoolean()) {
        config._requireFromPresent = TRI_ObjectToBoolean(
            object->Get(TRI_V8_ASCII_STRING("requireFromPresent")));
      }
    }
    
    if (object->Has(TRI_V8_ASCII_STRING("incremental"))) {
      if (object->Get(TRI_V8_ASCII_STRING("incremental"))->IsBoolean()) {
        config._incremental = TRI_ObjectToBoolean(
            object->Get(TRI_V8_ASCII_STRING("incremental")));
      }
    }

    if (object->Has(TRI_V8_ASCII_STRING("verbose"))) {
      if (object->Get(TRI_V8_ASCII_STRING("verbose"))->IsBoolean()) {
        config._verbose =
            TRI_ObjectToBoolean(object->Get(TRI_V8_ASCII_STRING("verbose")));
      }
    }

    if (object->Has(TRI_V8_ASCII_STRING("restrictCollections")) &&
        object->Get(TRI_V8_ASCII_STRING("restrictCollections"))->IsArray()) {
      config._restrictCollections.clear();
      v8::Handle<v8::Array> a = v8::Handle<v8::Array>::Cast(
          object->Get(TRI_V8_ASCII_STRING("restrictCollections")));

      uint32_t const n = a->Length();

      for (uint32_t i = 0; i < n; ++i) {
        v8::Handle<v8::Value> cname = a->Get(i);

        if (cname->IsString()) {
          config._restrictCollections.insert(
              std::pair<std::string, bool>(TRI_ObjectToString(cname), true));
        }
      }
    }

    if (object->Has(TRI_V8_ASCII_STRING("restrictType"))) {
      config._restrictType =
          TRI_ObjectToString(object->Get(TRI_V8_ASCII_STRING("restrictType")));
    }

    if ((config._restrictType.empty() &&
         !config._restrictCollections.empty()) ||
        (!config._restrictType.empty() &&
         config._restrictCollections.empty()) ||
        (!config._restrictType.empty() && config._restrictType != "include" &&
         config._restrictType != "exclude")) {
      TRI_V8_THROW_EXCEPTION_PARAMETER(
          "invalid value for <restrictCollections> or <restrictType>");
    }

    if (object->Has(TRI_V8_ASCII_STRING("connectionRetryWaitTime"))) {
      if (object->Get(TRI_V8_ASCII_STRING("connectionRetryWaitTime"))
              ->IsNumber()) {
        double value = TRI_ObjectToDouble(
            object->Get(TRI_V8_ASCII_STRING("connectionRetryWaitTime")));
        if (value > 0.0) {
          config._connectionRetryWaitTime =
              static_cast<uint64_t>(value * 1000.0 * 1000.0);
        }
      }
    }

    if (object->Has(TRI_V8_ASCII_STRING("initialSyncMaxWaitTime"))) {
      if (object->Get(TRI_V8_ASCII_STRING("initialSyncMaxWaitTime"))
              ->IsNumber()) {
        double value = TRI_ObjectToDouble(
            object->Get(TRI_V8_ASCII_STRING("initialSyncMaxWaitTime")));
        if (value > 0.0) {
          config._initialSyncMaxWaitTime =
              static_cast<uint64_t>(value * 1000.0 * 1000.0);
        }
      }
    }

    if (object->Has(TRI_V8_ASCII_STRING("idleMinWaitTime"))) {
      if (object->Get(TRI_V8_ASCII_STRING("idleMinWaitTime"))->IsNumber()) {
        double value = TRI_ObjectToDouble(
            object->Get(TRI_V8_ASCII_STRING("idleMinWaitTime")));
        if (value > 0.0) {
          config._idleMinWaitTime =
              static_cast<uint64_t>(value * 1000.0 * 1000.0);
        }
      }
    }

    if (object->Has(TRI_V8_ASCII_STRING("idleMaxWaitTime"))) {
      if (object->Get(TRI_V8_ASCII_STRING("idleMaxWaitTime"))->IsNumber()) {
        double value = TRI_ObjectToDouble(
            object->Get(TRI_V8_ASCII_STRING("idleMaxWaitTime")));
        if (value > 0.0) {
          config._idleMaxWaitTime =
              static_cast<uint64_t>(value * 1000.0 * 1000.0);
        }
      }
    }

    if (object->Has(TRI_V8_ASCII_STRING("autoResyncRetries"))) {
      if (object->Get(TRI_V8_ASCII_STRING("autoResyncRetries"))->IsNumber()) {
        double value = TRI_ObjectToDouble(
            object->Get(TRI_V8_ASCII_STRING("autoResyncRetries")));
        config._autoResyncRetries = static_cast<uint64_t>(value);
      }
    }

    int res =
        TRI_ConfigureReplicationApplier(vocbase->_replicationApplier, &config);

    if (res != TRI_ERROR_NO_ERROR) {
      TRI_V8_THROW_EXCEPTION(res);
    }

    std::unique_ptr<TRI_json_t> json(config.toJson());

    if (json == nullptr) {
      TRI_V8_THROW_EXCEPTION_MEMORY();
    }

    v8::Handle<v8::Value> result = TRI_ObjectJson(isolate, json.get());

    TRI_V8_RETURN(result);
  }
  TRI_V8_TRY_CATCH_END
}

////////////////////////////////////////////////////////////////////////////////
/// @brief start the replication applier manually
////////////////////////////////////////////////////////////////////////////////

static void JS_StartApplierReplication(
    v8::FunctionCallbackInfo<v8::Value> const& args) {
  TRI_V8_TRY_CATCH_BEGIN(isolate);
  v8::HandleScope scope(isolate);

  TRI_vocbase_t* vocbase = GetContextVocBase(isolate);

  if (vocbase == nullptr) {
    TRI_V8_THROW_EXCEPTION(TRI_ERROR_ARANGO_DATABASE_NOT_FOUND);
  }

  if (vocbase->_replicationApplier == nullptr) {
    TRI_V8_THROW_EXCEPTION(TRI_ERROR_INTERNAL);
  }

  if (args.Length() > 2) {
    TRI_V8_THROW_EXCEPTION_USAGE("REPLICATION_APPLIER_START(<from>)");
  }

  TRI_voc_tick_t initialTick = 0;
  bool useTick = false;

  if (args.Length() >= 1) {
    initialTick = TRI_ObjectToUInt64(args[0], true);
    useTick = true;
  }

  TRI_voc_tick_t barrierId = 0;
  if (args.Length() >= 2) {
    barrierId = TRI_ObjectToUInt64(args[1], true);
  }

  int res = vocbase->_replicationApplier->start(initialTick, useTick, barrierId);

  if (res != TRI_ERROR_NO_ERROR) {
    TRI_V8_THROW_EXCEPTION_MESSAGE(res, "cannot start replication applier");
  }

  TRI_V8_RETURN_TRUE();
  TRI_V8_TRY_CATCH_END
}

////////////////////////////////////////////////////////////////////////////////
/// @brief shuts down the replication applier manually
////////////////////////////////////////////////////////////////////////////////

static void JS_ShutdownApplierReplication(
    v8::FunctionCallbackInfo<v8::Value> const& args) {
  TRI_V8_TRY_CATCH_BEGIN(isolate);
  v8::HandleScope scope(isolate);

  if (args.Length() != 0) {
    TRI_V8_THROW_EXCEPTION_USAGE("REPLICATION_APPLIER_SHUTDOWN()");
  }

  TRI_vocbase_t* vocbase = GetContextVocBase(isolate);

  if (vocbase == nullptr) {
    TRI_V8_THROW_EXCEPTION(TRI_ERROR_ARANGO_DATABASE_NOT_FOUND);
  }

  if (vocbase->_replicationApplier == nullptr) {
    TRI_V8_THROW_EXCEPTION(TRI_ERROR_INTERNAL);
  }

  int res = vocbase->_replicationApplier->shutdown();

  if (res != TRI_ERROR_NO_ERROR) {
    TRI_V8_THROW_EXCEPTION_MESSAGE(res, "cannot shut down replication applier");
  }

  TRI_V8_RETURN_TRUE();
  TRI_V8_TRY_CATCH_END
}

////////////////////////////////////////////////////////////////////////////////
/// @brief get the state of the replication applier
////////////////////////////////////////////////////////////////////////////////

static void JS_StateApplierReplication(
    v8::FunctionCallbackInfo<v8::Value> const& args) {
  TRI_V8_TRY_CATCH_BEGIN(isolate);
  v8::HandleScope scope(isolate);

  if (args.Length() != 0) {
    TRI_V8_THROW_EXCEPTION_USAGE("REPLICATION_APPLIER_STATE()");
  }

  TRI_vocbase_t* vocbase = GetContextVocBase(isolate);

  if (vocbase == nullptr) {
    TRI_V8_THROW_EXCEPTION(TRI_ERROR_ARANGO_DATABASE_NOT_FOUND);
  }

  if (vocbase->_replicationApplier == nullptr) {
    TRI_V8_THROW_EXCEPTION(TRI_ERROR_INTERNAL);
  }

  TRI_json_t* json = TRI_JsonReplicationApplier(vocbase->_replicationApplier);

  if (json == nullptr) {
    TRI_V8_THROW_EXCEPTION_MEMORY();
  }

  v8::Handle<v8::Value> result = TRI_ObjectJson(isolate, json);
  TRI_FreeJson(TRI_CORE_MEM_ZONE, json);

  TRI_V8_RETURN(result);
  TRI_V8_TRY_CATCH_END
}

////////////////////////////////////////////////////////////////////////////////
/// @brief stop the replication applier and "forget" all state
////////////////////////////////////////////////////////////////////////////////

static void JS_ForgetApplierReplication(
    v8::FunctionCallbackInfo<v8::Value> const& args) {
  TRI_V8_TRY_CATCH_BEGIN(isolate);
  v8::HandleScope scope(isolate);

  if (args.Length() != 0) {
    TRI_V8_THROW_EXCEPTION_USAGE("REPLICATION_APPLIER_FORGET()");
  }

  TRI_vocbase_t* vocbase = GetContextVocBase(isolate);

  if (vocbase == nullptr) {
    TRI_V8_THROW_EXCEPTION(TRI_ERROR_ARANGO_DATABASE_NOT_FOUND);
  }

  if (vocbase->_replicationApplier == nullptr) {
    TRI_V8_THROW_EXCEPTION(TRI_ERROR_INTERNAL);
  }

  int res = vocbase->_replicationApplier->forget();

  if (res != TRI_ERROR_NO_ERROR) {
    TRI_V8_THROW_EXCEPTION(res);
  }

  TRI_V8_RETURN_TRUE();
  TRI_V8_TRY_CATCH_END
}

void TRI_InitV8Replication(v8::Isolate* isolate,
                           v8::Handle<v8::Context> context,
                           TRI_server_t* server, TRI_vocbase_t* vocbase,
                           JSLoader* loader, size_t threadNumber,
                           TRI_v8_global_t* v8g) {
  // replication functions. not intended to be used by end users
  TRI_AddGlobalFunctionVocbase(isolate, context,
                               TRI_V8_ASCII_STRING("REPLICATION_LOGGER_STATE"),
                               JS_StateLoggerReplication, true);
  TRI_AddGlobalFunctionVocbase(isolate, context,
                               TRI_V8_ASCII_STRING("REPLICATION_LOGGER_LAST"),
                               JS_LastLoggerReplication, true);
  TRI_AddGlobalFunctionVocbase(
      isolate, context, TRI_V8_ASCII_STRING("REPLICATION_LOGGER_TICK_RANGES"),
      JS_TickRangesLoggerReplication, true);
  TRI_AddGlobalFunctionVocbase(
      isolate, context, TRI_V8_ASCII_STRING("REPLICATION_LOGGER_FIRST_TICK"),
      JS_FirstTickLoggerReplication, true);
  TRI_AddGlobalFunctionVocbase(isolate, context,
                               TRI_V8_ASCII_STRING("REPLICATION_SYNCHRONIZE"),
                               JS_SynchronizeReplication, true);
  TRI_AddGlobalFunctionVocbase(isolate, context,
                               TRI_V8_ASCII_STRING("REPLICATION_SERVER_ID"),
                               JS_ServerIdReplication, true);
  TRI_AddGlobalFunctionVocbase(
      isolate, context, TRI_V8_ASCII_STRING("REPLICATION_APPLIER_CONFIGURE"),
      JS_ConfigureApplierReplication, true);
  TRI_AddGlobalFunctionVocbase(isolate, context,
                               TRI_V8_ASCII_STRING("REPLICATION_APPLIER_START"),
                               JS_StartApplierReplication, true);
  TRI_AddGlobalFunctionVocbase(
      isolate, context, TRI_V8_ASCII_STRING("REPLICATION_APPLIER_SHUTDOWN"),
      JS_ShutdownApplierReplication, true);
  TRI_AddGlobalFunctionVocbase(isolate, context,
                               TRI_V8_ASCII_STRING("REPLICATION_APPLIER_STATE"),
                               JS_StateApplierReplication, true);
  TRI_AddGlobalFunctionVocbase(
      isolate, context, TRI_V8_ASCII_STRING("REPLICATION_APPLIER_FORGET"),
      JS_ForgetApplierReplication, true);
}
