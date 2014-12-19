////////////////////////////////////////////////////////////////////////////////
/// @brief V8-vocbase bridge
///
/// @file
///
/// DISCLAIMER
///
/// Copyright 2014 ArangoDB GmbH, Cologne, Germany
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
/// @author Copyright 2014, ArangoDB GmbH, Cologne, Germany
/// @author Copyright 2011-2013, triAGENS GmbH, Cologne, Germany
////////////////////////////////////////////////////////////////////////////////

#include "v8-vocbaseprivate.h"

#include "V8/v8-conv.h"
#include "V8/v8-utils.h"
#include "Wal/LogfileManager.h"

#include "VocBase/replication-dump.h"
#include "Replication/InitialSyncer.h"

using namespace std;
using namespace triagens::basics;
using namespace triagens::arango;
using namespace triagens::rest;

// -----------------------------------------------------------------------------
// --SECTION--                                                       REPLICATION
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @brief get the state of the replication logger
////////////////////////////////////////////////////////////////////////////////

static void JS_StateLoggerReplication (const v8::FunctionCallbackInfo<v8::Value>& args) {
  v8::Isolate* isolate = args.GetIsolate();
  v8::HandleScope scope(isolate);

  triagens::wal::LogfileManagerState s = triagens::wal::LogfileManager::instance()->state();

  v8::Handle<v8::Object> result = v8::Object::New(isolate);

  v8::Handle<v8::Object> state = v8::Object::New(isolate);
  state->Set(TRI_V8_ASCII_STRING("running"),     v8::True(isolate));
  state->Set(TRI_V8_ASCII_STRING("lastLogTick"), V8TickId(isolate, s.lastTick));
  state->Set(TRI_V8_ASCII_STRING("totalEvents"), v8::Number::New(isolate, (double) s.numEvents));
  state->Set(TRI_V8_ASCII_STRING("time"),        TRI_V8_STD_STRING(s.timeString));
  result->Set(TRI_V8_ASCII_STRING("state"),      state);

  v8::Handle<v8::Object> server = v8::Object::New(isolate);
  server->Set(TRI_V8_ASCII_STRING("version"),  TRI_V8_ASCII_STRING(TRI_VERSION));
  server->Set(TRI_V8_ASCII_STRING("serverId"), TRI_V8_STD_STRING(StringUtils::itoa(TRI_GetIdServer()))); /// TODO
  result->Set(TRI_V8_ASCII_STRING("server"), server);
  
  v8::Handle<v8::Object> clients = v8::Object::New(isolate);
  result->Set(TRI_V8_ASCII_STRING("clients"), clients);

  TRI_V8_RETURN(result);
}

////////////////////////////////////////////////////////////////////////////////
/// @brief get the last WAL entries
////////////////////////////////////////////////////////////////////////////////

#ifdef TRI_ENABLE_MAINTAINER_MODE
static void JS_LastLoggerReplication (const v8::FunctionCallbackInfo<v8::Value>& args) {
  v8::Isolate* isolate = args.GetIsolate();
  v8::HandleScope scope(isolate);
  
  TRI_vocbase_t* vocbase = GetContextVocBase(isolate);

  if (vocbase == nullptr) {
    TRI_V8_THROW_EXCEPTION(TRI_ERROR_ARANGO_DATABASE_NOT_FOUND);
  }

  if (args.Length() != 2) {
    TRI_V8_THROW_EXCEPTION_USAGE("REPLICATION_LOGGER_LAST(<fromTick>, <toTick>)");
  }

  TRI_replication_dump_t dump(vocbase, 0, true); 
  TRI_voc_tick_t tickStart = TRI_ObjectToUInt64(args[0], true);
  TRI_voc_tick_t tickEnd = TRI_ObjectToUInt64(args[1], true);

  int res = TRI_DumpLogReplication(&dump, tickStart, tickEnd, true);

  if (res != TRI_ERROR_NO_ERROR) {
    TRI_V8_THROW_EXCEPTION(res);
  }

  TRI_json_t* json = TRI_JsonString(TRI_UNKNOWN_MEM_ZONE, dump._buffer->_buffer);

  if (json == nullptr) {
    TRI_V8_THROW_EXCEPTION_MEMORY();
  }

  v8::Handle<v8::Value> result = TRI_ObjectJson(isolate, json);
  TRI_FreeJson(TRI_UNKNOWN_MEM_ZONE, json);
  
  TRI_V8_RETURN(result);
}
#endif

////////////////////////////////////////////////////////////////////////////////
/// @brief sync data from a remote master
////////////////////////////////////////////////////////////////////////////////

static void JS_SynchroniseReplication (const v8::FunctionCallbackInfo<v8::Value>& args) {
  v8::Isolate* isolate = args.GetIsolate();
  v8::HandleScope scope(isolate);

  if (args.Length() != 1) {
    TRI_V8_THROW_EXCEPTION_USAGE("REPLICATION_SYNCHRONISE(<config>)");
  }

  TRI_vocbase_t* vocbase = GetContextVocBase(isolate);

  if (vocbase == nullptr) {
    TRI_V8_THROW_EXCEPTION(TRI_ERROR_ARANGO_DATABASE_NOT_FOUND);
  }

  // treat the argument as an object from now on
  v8::Handle<v8::Object> object = v8::Handle<v8::Object>::Cast(args[0]);

  string endpoint;
  if (object->Has(TRI_V8_ASCII_STRING("endpoint"))) {
    endpoint = TRI_ObjectToString(object->Get(TRI_V8_ASCII_STRING("endpoint")));
  }

  string database;
  if (object->Has(TRI_V8_ASCII_STRING("database"))) {
    database = TRI_ObjectToString(object->Get(TRI_V8_ASCII_STRING("database")));
  }
  else {
    database = string(vocbase->_name);
  }

  string username;
  if (object->Has(TRI_V8_ASCII_STRING("username"))) {
    username = TRI_ObjectToString(object->Get(TRI_V8_ASCII_STRING("username")));
  }

  string password;
  if (object->Has(TRI_V8_ASCII_STRING("password"))) {
    password = TRI_ObjectToString(object->Get(TRI_V8_ASCII_STRING("password")));
  }

  std::unordered_map<std::string, bool> restrictCollections;
  if (object->Has(TRI_V8_ASCII_STRING("restrictCollections")) && object->Get(TRI_V8_ASCII_STRING("restrictCollections"))->IsArray()) {
    v8::Handle<v8::Array> a = v8::Handle<v8::Array>::Cast(object->Get(TRI_V8_ASCII_STRING("restrictCollections")));

    const uint32_t n = a->Length();

    for (uint32_t i = 0; i < n; ++i) {
      v8::Handle<v8::Value> cname = a->Get(i);

      if (cname->IsString()) {
        restrictCollections.emplace(std::make_pair(TRI_ObjectToString(cname), true));
      }
    }
  }

  std::string restrictType;
  if (object->Has(TRI_V8_ASCII_STRING("restrictType"))) {
    restrictType = TRI_ObjectToString(object->Get(TRI_V8_ASCII_STRING("restrictType")));
  }

  bool verbose = true;
  if (object->Has(TRI_V8_ASCII_STRING("verbose"))) {
    verbose = TRI_ObjectToBoolean(object->Get(TRI_V8_ASCII_STRING("verbose")));
  }

  if (endpoint.empty()) {
    TRI_V8_THROW_EXCEPTION_PARAMETER("<endpoint> must be a valid endpoint");
  }

  if ((restrictType.empty() && ! restrictCollections.empty()) ||
      (! restrictType.empty() && restrictCollections.empty()) ||
      (! restrictType.empty() && restrictType != "include" && restrictType != "exclude")) {
    TRI_V8_THROW_EXCEPTION_PARAMETER("invalid value for <restrictCollections> or <restrictType>");
  }

  TRI_replication_applier_configuration_t config;
  TRI_InitConfigurationReplicationApplier(&config);
  config._endpoint = TRI_DuplicateString2Z(TRI_CORE_MEM_ZONE, endpoint.c_str(), endpoint.size());
  config._database = TRI_DuplicateString2Z(TRI_CORE_MEM_ZONE, database.c_str(), database.size());
  config._username = TRI_DuplicateString2Z(TRI_CORE_MEM_ZONE, username.c_str(), username.size());
  config._password = TRI_DuplicateString2Z(TRI_CORE_MEM_ZONE, password.c_str(), password.size());

  if (object->Has(TRI_V8_ASCII_STRING("chunkSize"))) {
    if (object->Get(TRI_V8_ASCII_STRING("chunkSize"))->IsNumber()) {
      config._chunkSize = TRI_ObjectToUInt64(object->Get(TRI_V8_ASCII_STRING("chunkSize")), true);
    }
  }
 
  if (object->Has(TRI_V8_ASCII_STRING("includeSystem"))) {
    if (object->Get(TRI_V8_ASCII_STRING("includeSystem"))->IsBoolean()) {
      config._includeSystem = TRI_ObjectToBoolean(object->Get(TRI_V8_ASCII_STRING("includeSystem")));
    }
  }

  string errorMsg = "";
  InitialSyncer syncer(vocbase, &config, restrictCollections, restrictType, verbose);
  TRI_DestroyConfigurationReplicationApplier(&config);

  int res = TRI_ERROR_NO_ERROR;
  v8::Handle<v8::Object> result = v8::Object::New(isolate);

  try {
    res = syncer.run(errorMsg);

    result->Set(TRI_V8_ASCII_STRING("lastLogTick"), V8TickId(isolate, syncer.getLastLogTick()));

    map<TRI_voc_cid_t, string>::const_iterator it;
    map<TRI_voc_cid_t, string> const& c = syncer.getProcessedCollections();

    uint32_t j = 0;
    v8::Handle<v8::Array> collections = v8::Array::New(isolate);
    for (it = c.begin(); it != c.end(); ++it) {
      const string cidString = StringUtils::itoa((*it).first);

      v8::Handle<v8::Object> ci = v8::Object::New(isolate);
      ci->Set(TRI_V8_ASCII_STRING("id"),   TRI_V8_STD_STRING(cidString));
      ci->Set(TRI_V8_ASCII_STRING("name"), TRI_V8_STD_STRING((*it).second));

      collections->Set(j++, ci);
    }

    result->Set(TRI_V8_ASCII_STRING("collections"), collections);
  }
  catch (...) {
  }

  if (res != TRI_ERROR_NO_ERROR) {
    TRI_V8_THROW_EXCEPTION_MESSAGE(res, "cannot sync from remote endpoint: " + errorMsg);
  }

  TRI_V8_RETURN(result);
}

////////////////////////////////////////////////////////////////////////////////
/// @brief return the server's id
////////////////////////////////////////////////////////////////////////////////

static void JS_ServerIdReplication (const v8::FunctionCallbackInfo<v8::Value>& args) {
  v8::Isolate* isolate = args.GetIsolate();
  v8::HandleScope scope(isolate);

  const string serverId = StringUtils::itoa(TRI_GetIdServer());
  TRI_V8_RETURN_STD_STRING(serverId);
}

////////////////////////////////////////////////////////////////////////////////
/// @brief configure the replication applier manually
////////////////////////////////////////////////////////////////////////////////

static void JS_ConfigureApplierReplication (const v8::FunctionCallbackInfo<v8::Value>& args) {
  v8::Isolate* isolate = args.GetIsolate();
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
    TRI_InitConfigurationReplicationApplier(&config);

    TRI_ReadLockReadWriteLock(&vocbase->_replicationApplier->_statusLock);
    TRI_CopyConfigurationReplicationApplier(&vocbase->_replicationApplier->_configuration, &config);
    TRI_ReadUnlockReadWriteLock(&vocbase->_replicationApplier->_statusLock);

    TRI_json_t* json = TRI_JsonConfigurationReplicationApplier(&config);
    TRI_DestroyConfigurationReplicationApplier(&config);

    if (json == nullptr) {
      TRI_V8_THROW_EXCEPTION_MEMORY();
    }

    v8::Handle<v8::Value> result = TRI_ObjectJson(isolate, json);
    TRI_FreeJson(TRI_CORE_MEM_ZONE, json);

    TRI_V8_RETURN(result);
  }

  else {
    // set the configuration

    if (args.Length() != 1 || ! args[0]->IsObject()) {
      TRI_V8_THROW_EXCEPTION_USAGE("REPLICATION_APPLIER_CONFIGURE(<configuration>)");
    }

    TRI_replication_applier_configuration_t config;
    TRI_InitConfigurationReplicationApplier(&config);

    // fill with previous configuration
    TRI_ReadLockReadWriteLock(&vocbase->_replicationApplier->_statusLock);
    TRI_CopyConfigurationReplicationApplier(&vocbase->_replicationApplier->_configuration, &config);
    TRI_ReadUnlockReadWriteLock(&vocbase->_replicationApplier->_statusLock);


    // treat the argument as an object from now on
    v8::Handle<v8::Object> object = v8::Handle<v8::Object>::Cast(args[0]);

    if (object->Has(TRI_V8_ASCII_STRING("endpoint"))) {
      if (object->Get(TRI_V8_ASCII_STRING("endpoint"))->IsString()) {
        string endpoint = TRI_ObjectToString(object->Get(TRI_V8_ASCII_STRING("endpoint")));

        if (config._endpoint != nullptr) {
          TRI_Free(TRI_CORE_MEM_ZONE, config._endpoint);
        }
        config._endpoint = TRI_DuplicateString2Z(TRI_CORE_MEM_ZONE, endpoint.c_str(), endpoint.size());
      }
    }

    if (object->Has(TRI_V8_ASCII_STRING("database"))) {
      if (object->Get(TRI_V8_ASCII_STRING("database"))->IsString()) {
        string database = TRI_ObjectToString(object->Get(TRI_V8_ASCII_STRING("database")));

        if (config._database != nullptr) {
          TRI_Free(TRI_CORE_MEM_ZONE, config._database);
        }
        config._database = TRI_DuplicateString2Z(TRI_CORE_MEM_ZONE, database.c_str(), database.size());
      }
    }
    else {
      if (config._database == nullptr) {
        // no database set, use current
        config._database = TRI_DuplicateStringZ(TRI_CORE_MEM_ZONE, vocbase->_name);
      }
    }

    TRI_ASSERT(config._database != nullptr);

    if (object->Has(TRI_V8_ASCII_STRING("username"))) {
      if (object->Get(TRI_V8_ASCII_STRING("username"))->IsString()) {
        string username = TRI_ObjectToString(object->Get(TRI_V8_ASCII_STRING("username")));

        if (config._username != nullptr) {
          TRI_Free(TRI_CORE_MEM_ZONE, config._username);
        }
        config._username = TRI_DuplicateString2Z(TRI_CORE_MEM_ZONE, username.c_str(), username.size());
      }
    }

    if (object->Has(TRI_V8_ASCII_STRING("password"))) {
      if (object->Get(TRI_V8_ASCII_STRING("password"))->IsString()) {
        string password = TRI_ObjectToString(object->Get(TRI_V8_ASCII_STRING("password")));

        if (config._password != nullptr) {
          TRI_Free(TRI_CORE_MEM_ZONE, config._password);
        }
        config._password = TRI_DuplicateString2Z(TRI_CORE_MEM_ZONE, password.c_str(), password.size());
      }
    }

    if (object->Has(TRI_V8_ASCII_STRING("requestTimeout"))) {
      if (object->Get(TRI_V8_ASCII_STRING("requestTimeout"))->IsNumber()) {
        config._requestTimeout = TRI_ObjectToDouble(object->Get(TRI_V8_ASCII_STRING("requestTimeout")));
      }
    }

    if (object->Has(TRI_V8_ASCII_STRING("connectTimeout"))) {
      if (object->Get(TRI_V8_ASCII_STRING("connectTimeout"))->IsNumber()) {
        config._connectTimeout = TRI_ObjectToDouble(object->Get(TRI_V8_ASCII_STRING("connectTimeout")));
      }
    }

    if (object->Has(TRI_V8_ASCII_STRING("ignoreErrors"))) {
      if (object->Get(TRI_V8_ASCII_STRING("ignoreErrors"))->IsNumber()) {
        config._ignoreErrors = TRI_ObjectToUInt64(object->Get(TRI_V8_ASCII_STRING("ignoreErrors")), false);
      }
    }

    if (object->Has(TRI_V8_ASCII_STRING("maxConnectRetries"))) {
      if (object->Get(TRI_V8_ASCII_STRING("maxConnectRetries"))->IsNumber()) {
        config._maxConnectRetries = TRI_ObjectToUInt64(object->Get(TRI_V8_ASCII_STRING("maxConnectRetries")), false);
      }
    }

    if (object->Has(TRI_V8_ASCII_STRING("sslProtocol"))) {
      if (object->Get(TRI_V8_ASCII_STRING("sslProtocol"))->IsNumber()) {
        config._sslProtocol = (uint32_t) TRI_ObjectToUInt64(object->Get(TRI_V8_ASCII_STRING("sslProtocol")), false);
      }
    }

    if (object->Has(TRI_V8_ASCII_STRING("chunkSize"))) {
      if (object->Get(TRI_V8_ASCII_STRING("chunkSize"))->IsNumber()) {
        config._chunkSize = TRI_ObjectToUInt64(object->Get(TRI_V8_ASCII_STRING("chunkSize")), true);
      }
    }

    if (object->Has(TRI_V8_ASCII_STRING("autoStart"))) {
      if (object->Get(TRI_V8_ASCII_STRING("autoStart"))->IsBoolean()) {
        config._autoStart = TRI_ObjectToBoolean(object->Get(TRI_V8_ASCII_STRING("autoStart")));
      }
    }

    if (object->Has(TRI_V8_ASCII_STRING("adaptivePolling"))) {
      if (object->Get(TRI_V8_ASCII_STRING("adaptivePolling"))->IsBoolean()) {
        config._adaptivePolling = TRI_ObjectToBoolean(object->Get(TRI_V8_ASCII_STRING("adaptivePolling")));
      }
    }
    
    if (object->Has(TRI_V8_ASCII_STRING("includeSystem"))) {
      if (object->Get(TRI_V8_ASCII_STRING("includeSystem"))->IsBoolean()) {
        config._includeSystem = TRI_ObjectToBoolean(object->Get(TRI_V8_ASCII_STRING("includeSystem")));
      }
    }
  
    if (object->Has(TRI_V8_ASCII_STRING("restrictCollections")) && object->Get(TRI_V8_ASCII_STRING("restrictCollections"))->IsArray()) {
      config._restrictCollections.clear();
      v8::Handle<v8::Array> a = v8::Handle<v8::Array>::Cast(object->Get(TRI_V8_ASCII_STRING("restrictCollections")));

      uint32_t const n = a->Length();

      for (uint32_t i = 0; i < n; ++i) {
        v8::Handle<v8::Value> cname = a->Get(i);

        if (cname->IsString()) {
          config._restrictCollections.insert(pair<string, bool>(TRI_ObjectToString(cname), true));
        }
      }
    }
    
    if (object->Has(TRI_V8_ASCII_STRING("restrictType"))) {
      config._restrictType = TRI_ObjectToString(object->Get(TRI_V8_ASCII_STRING("restrictType")));
    }
  
    if ((config._restrictType.empty() && ! config._restrictCollections.empty()) ||
        (! config._restrictType.empty() && config._restrictCollections.empty()) ||
        (! config._restrictType.empty() && config._restrictType != "include" && config._restrictType != "exclude")) {
      TRI_DestroyConfigurationReplicationApplier(&config);
      TRI_V8_THROW_EXCEPTION_PARAMETER("invalid value for <restrictCollections> or <restrictType>");
    }

    int res = TRI_ConfigureReplicationApplier(vocbase->_replicationApplier, &config);

    if (res != TRI_ERROR_NO_ERROR) {
      TRI_DestroyConfigurationReplicationApplier(&config);
      TRI_V8_THROW_EXCEPTION(res);
    }

    TRI_json_t* json = TRI_JsonConfigurationReplicationApplier(&config);
    TRI_DestroyConfigurationReplicationApplier(&config);

    if (json == nullptr) {
      TRI_V8_THROW_EXCEPTION_MEMORY();
    }

    v8::Handle<v8::Value> result = TRI_ObjectJson(isolate, json);
    TRI_FreeJson(TRI_CORE_MEM_ZONE, json);

    TRI_V8_RETURN(result);
  }
}

////////////////////////////////////////////////////////////////////////////////
/// @brief start the replication applier manually
////////////////////////////////////////////////////////////////////////////////

static void JS_StartApplierReplication (const v8::FunctionCallbackInfo<v8::Value>& args) {
  v8::Isolate* isolate = args.GetIsolate();
  v8::HandleScope scope(isolate);

  TRI_vocbase_t* vocbase = GetContextVocBase(isolate);

  if (vocbase == nullptr) {
    TRI_V8_THROW_EXCEPTION(TRI_ERROR_ARANGO_DATABASE_NOT_FOUND);
  }

  if (vocbase->_replicationApplier == nullptr) {
    TRI_V8_THROW_EXCEPTION(TRI_ERROR_INTERNAL);
  }

  if (args.Length() > 1) {
    TRI_V8_THROW_EXCEPTION_USAGE("REPLICATION_APPLIER_START(<from>)");
  }

  TRI_voc_tick_t initialTick = 0;
  bool useTick = false;

  if (args.Length() == 1) {
    initialTick = TRI_ObjectToUInt64(args[0], true);
    useTick = true;
  }

  int res = TRI_StartReplicationApplier(vocbase->_replicationApplier,
                                        initialTick,
                                        useTick);

  if (res != TRI_ERROR_NO_ERROR) {
    TRI_V8_THROW_EXCEPTION_MESSAGE(res, "cannot start replication applier");
  }

  TRI_V8_RETURN_TRUE();
}

////////////////////////////////////////////////////////////////////////////////
/// @brief shuts down the replication applier manually
////////////////////////////////////////////////////////////////////////////////

static void JS_ShutdownApplierReplication (const v8::FunctionCallbackInfo<v8::Value>& args) {
  v8::Isolate* isolate = args.GetIsolate();
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

  int res = TRI_ShutdownReplicationApplier(vocbase->_replicationApplier);

  if (res != TRI_ERROR_NO_ERROR) {
    TRI_V8_THROW_EXCEPTION_MESSAGE(res, "cannot shut down replication applier");
  }

  TRI_V8_RETURN_TRUE();
}

////////////////////////////////////////////////////////////////////////////////
/// @brief get the state of the replication applier
////////////////////////////////////////////////////////////////////////////////

static void JS_StateApplierReplication (const v8::FunctionCallbackInfo<v8::Value>& args) {
  v8::Isolate* isolate = args.GetIsolate();
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
}

////////////////////////////////////////////////////////////////////////////////
/// @brief stop the replication applier and "forget" all state
////////////////////////////////////////////////////////////////////////////////

static void JS_ForgetApplierReplication (const v8::FunctionCallbackInfo<v8::Value>& args) {
  v8::Isolate* isolate = args.GetIsolate();
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

  int res = TRI_ForgetReplicationApplier(vocbase->_replicationApplier);

  if (res != TRI_ERROR_NO_ERROR) {
    TRI_V8_THROW_EXCEPTION(res);
  }

  TRI_V8_RETURN_TRUE();
}



void TRI_InitV8replication (v8::Isolate* isolate,
                            v8::Handle<v8::Context> context,
                            TRI_server_t* server,
                            TRI_vocbase_t* vocbase,
                            JSLoader* loader,
                            const size_t threadNumber,
                            TRI_v8_global_t* v8g){

  // replication functions. not intended to be used by end users
  TRI_AddGlobalFunctionVocbase(isolate, context, TRI_V8_ASCII_STRING("REPLICATION_LOGGER_STATE"), JS_StateLoggerReplication, true);
#ifdef TRI_ENABLE_MAINTAINER_MODE
  TRI_AddGlobalFunctionVocbase(isolate, context, TRI_V8_ASCII_STRING("REPLICATION_LOGGER_LAST"), JS_LastLoggerReplication, true);
#endif
  TRI_AddGlobalFunctionVocbase(isolate, context, TRI_V8_ASCII_STRING("REPLICATION_SYNCHRONISE"), JS_SynchroniseReplication, true);
  TRI_AddGlobalFunctionVocbase(isolate, context, TRI_V8_ASCII_STRING("REPLICATION_SERVER_ID"), JS_ServerIdReplication, true);
  TRI_AddGlobalFunctionVocbase(isolate, context, TRI_V8_ASCII_STRING("REPLICATION_APPLIER_CONFIGURE"), JS_ConfigureApplierReplication, true);
  TRI_AddGlobalFunctionVocbase(isolate, context, TRI_V8_ASCII_STRING("REPLICATION_APPLIER_START"), JS_StartApplierReplication, true);
  TRI_AddGlobalFunctionVocbase(isolate, context, TRI_V8_ASCII_STRING("REPLICATION_APPLIER_SHUTDOWN"), JS_ShutdownApplierReplication, true);
  TRI_AddGlobalFunctionVocbase(isolate, context, TRI_V8_ASCII_STRING("REPLICATION_APPLIER_STATE"), JS_StateApplierReplication, true);
  TRI_AddGlobalFunctionVocbase(isolate, context, TRI_V8_ASCII_STRING("REPLICATION_APPLIER_FORGET"), JS_ForgetApplierReplication, true);
}
