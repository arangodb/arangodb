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
#include "Basics/ReadLocker.h"
#include "Cluster/ClusterComm.h"
#include "Cluster/ClusterFeature.h"
#include "Replication/DatabaseTailingSyncer.h"
#include "Replication/DatabaseInitialSyncer.h"
#include "Replication/DatabaseReplicationApplier.h"
#include "Replication/ReplicationApplierConfiguration.h"
#include "Rest/Version.h"
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

static void JS_StateLoggerReplication(
    v8::FunctionCallbackInfo<v8::Value> const& args) {
  // FIXME: use code in RestReplicationHandler and get rid of storage-engine
  //        depended code here
  //        
  TRI_V8_TRY_CATCH_BEGIN(isolate);
  v8::HandleScope scope(isolate);

  StorageEngine* engine = EngineSelectorFeature::ENGINE;
  v8::Handle<v8::Object> result = v8::Object::New(isolate);

  VPackBuilder builder;
  auto res = engine->createLoggerState(nullptr,builder);
  if(res.fail()){
    TRI_V8_THROW_EXCEPTION(res);
  }
  v8::Handle<v8::Value>resultValue = TRI_VPackToV8(isolate, builder.slice());
  result = v8::Handle<v8::Object>::Cast(resultValue);

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
  v8::Handle<v8::Array> result;

  VPackBuilder builder;
  Result res = EngineSelectorFeature::ENGINE->createTickRanges(builder);
  if (res.fail()) {
    TRI_V8_THROW_EXCEPTION(res);
  }

  v8::Handle<v8::Value>resultValue = TRI_VPackToV8(isolate, builder.slice());
  result = v8::Handle<v8::Array>::Cast(resultValue);

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

  TRI_voc_tick_t tick = UINT64_MAX;
  Result res  = EngineSelectorFeature::ENGINE->firstTick(tick);
  if(res.fail()){
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

static void JS_LastLoggerReplication( v8::FunctionCallbackInfo<v8::Value> const& args) {
  TRI_V8_TRY_CATCH_BEGIN(isolate);
  v8::HandleScope scope(isolate);

  TRI_vocbase_t* vocbase = GetContextVocBase(isolate);

  if (vocbase == nullptr) {
    TRI_V8_THROW_EXCEPTION(TRI_ERROR_ARANGO_DATABASE_NOT_FOUND);
  }

  if (args.Length() != 2) {
    TRI_V8_THROW_EXCEPTION_USAGE("REPLICATION_LOGGER_LAST(<fromTick>, <toTick>)");
  }

  TRI_voc_tick_t tickStart = TRI_ObjectToUInt64(args[0], true);
  TRI_voc_tick_t tickEnd = TRI_ObjectToUInt64(args[1], true);
  if (tickEnd <= tickStart) {
    TRI_V8_THROW_EXCEPTION_USAGE("tickStart < tickEnd");
  }

  auto transactionContext = transaction::V8Context::Create(vocbase, false);
  auto builderSPtr = std::make_shared<VPackBuilder>();
  Result res = EngineSelectorFeature::ENGINE->lastLogger(
    vocbase, transactionContext, tickStart, tickEnd, builderSPtr);

  v8::Handle<v8::Value> result;
  if(res.fail()){
    result = v8::Null(isolate);
    TRI_V8_THROW_EXCEPTION(res);
  }

  result = TRI_VPackToV8(isolate, builderSPtr->slice(),
                         transactionContext->getVPackOptions());

  TRI_V8_RETURN(result);
  TRI_V8_TRY_CATCH_END
}

static void addReplicationAuthentication(v8::Isolate* isolate,
    v8::Handle<v8::Object> object,
    ReplicationApplierConfiguration &config) {
  bool hasUsernamePassword = false;
  if (object->Has(TRI_V8_ASCII_STRING(isolate, "username"))) {
    if (object->Get(TRI_V8_ASCII_STRING(isolate, "username"))->IsString()) {
      hasUsernamePassword = true;
      config._username =
        TRI_ObjectToString(object->Get(TRI_V8_ASCII_STRING(isolate, "username")));
    }
  }

  if (object->Has(TRI_V8_ASCII_STRING(isolate, "password"))) {
    if (object->Get(TRI_V8_ASCII_STRING(isolate, "password"))->IsString()) {
      hasUsernamePassword = true;
      config._password =
        TRI_ObjectToString(object->Get(TRI_V8_ASCII_STRING(isolate, "password")));
    }
  }
  if (!hasUsernamePassword) {
    auto cluster = application_features::ApplicationServer::getFeature<ClusterFeature>("Cluster");
    if (cluster->isEnabled()) {
      auto cc = ClusterComm::instance();
      if (cc != nullptr) {
        // nullptr happens only during controlled shutdown
        config._jwt = ClusterComm::instance()->jwt();
      }
    }
  }
}

static void addConnectionSettings(v8::Isolate* isolate,
    v8::Handle<v8::Object> object,
    ReplicationApplierConfiguration &config) {

  if (object->Has(TRI_V8_ASCII_STRING(isolate, "sslProtocol"))) {
    if (object->Get(TRI_V8_ASCII_STRING(isolate, "sslProtocol"))->IsNumber()) {
      config._sslProtocol = static_cast<uint32_t>(TRI_ObjectToUInt64(
          object->Get(TRI_V8_ASCII_STRING(isolate, "sslProtocol")), false));
    }
  }
    
  if (object->Has(TRI_V8_ASCII_STRING(isolate, "requestTimeout"))) {
    if (object->Get(TRI_V8_ASCII_STRING(isolate, "requestTimeout"))->IsNumber()) {
      config._requestTimeout = TRI_ObjectToDouble(
          object->Get(TRI_V8_ASCII_STRING(isolate, "requestTimeout")));
    }
  }

  if (object->Has(TRI_V8_ASCII_STRING(isolate, "connectTimeout"))) {
    if (object->Get(TRI_V8_ASCII_STRING(isolate, "connectTimeout"))->IsNumber()) {
      config._connectTimeout = TRI_ObjectToDouble(
          object->Get(TRI_V8_ASCII_STRING(isolate, "connectTimeout")));
    }
  }

  if (object->Has(TRI_V8_ASCII_STRING(isolate, "chunkSize"))) {
    if (object->Get(TRI_V8_ASCII_STRING(isolate, "chunkSize"))->IsNumber()) {
      config._chunkSize = TRI_ObjectToUInt64(
          object->Get(TRI_V8_ASCII_STRING(isolate, "chunkSize")), true);
    }
  }

  if (object->Has(TRI_V8_ASCII_STRING(isolate, "maxConnectRetries"))) {
    if (object->Get(TRI_V8_ASCII_STRING(isolate, "maxConnectRetries"))->IsNumber()) {
      config._maxConnectRetries = TRI_ObjectToUInt64(
          object->Get(TRI_V8_ASCII_STRING(isolate, "maxConnectRetries")), false);
    }
  }
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
  if (object->Has(TRI_V8_ASCII_STRING(isolate, "endpoint"))) {
    endpoint = TRI_ObjectToString(object->Get(TRI_V8_ASCII_STRING(isolate, "endpoint")));
  }

  std::string database;
  if (object->Has(TRI_V8_ASCII_STRING(isolate, "database"))) {
    database = TRI_ObjectToString(object->Get(TRI_V8_ASCII_STRING(isolate, "database")));
  } else {
    database = vocbase->name();
  }

  std::unordered_map<std::string, bool> restrictCollections;
  if (object->Has(TRI_V8_ASCII_STRING(isolate, "restrictCollections")) &&
      object->Get(TRI_V8_ASCII_STRING(isolate, "restrictCollections"))->IsArray()) {
    v8::Handle<v8::Array> a = v8::Handle<v8::Array>::Cast(
        object->Get(TRI_V8_ASCII_STRING(isolate, "restrictCollections")));

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
  if (object->Has(TRI_V8_ASCII_STRING(isolate, "restrictType"))) {
    restrictType =
        TRI_ObjectToString(object->Get(TRI_V8_ASCII_STRING(isolate, "restrictType")));
  }
  if ((restrictType.empty() && !restrictCollections.empty()) ||
      (!restrictType.empty() && restrictCollections.empty()) ||
      (!restrictType.empty() && restrictType != "include" &&
       restrictType != "exclude")) {
    TRI_V8_THROW_EXCEPTION_PARAMETER("invalid value for <restrictCollections> or <restrictType>");
  }
  Syncer::RestrictType rType = Syncer::RESTRICT_NONE;
  if (restrictType == "include") {
    rType = Syncer::RESTRICT_INCLUDE;
  } else if (restrictType == "exclude") {
    rType = Syncer::RESTRICT_EXCLUDE;
  }

  bool verbose = true;
  if (object->Has(TRI_V8_ASCII_STRING(isolate, "verbose"))) {
    verbose = TRI_ObjectToBoolean(object->Get(TRI_V8_ASCII_STRING(isolate, "verbose")));
  }

  bool skipCreateDrop = false;
  if (object->Has(TRI_V8_ASCII_STRING(isolate, "skipCreateDrop"))) {
    skipCreateDrop = TRI_ObjectToBoolean(object->Get(TRI_V8_ASCII_STRING(isolate, "skipCreateDrop")));
  }
    
  if (endpoint.empty()) {
    TRI_V8_THROW_EXCEPTION_PARAMETER("<endpoint> must be a valid endpoint");
  }



  ReplicationApplierConfiguration config;
  config._endpoint = endpoint;
  config._database = database;

  addReplicationAuthentication(isolate, object, config);
  addConnectionSettings(isolate, object, config);

  if (object->Has(TRI_V8_ASCII_STRING(isolate, "includeSystem"))) {
    if (object->Get(TRI_V8_ASCII_STRING(isolate, "includeSystem"))->IsBoolean()) {
      config._includeSystem = TRI_ObjectToBoolean(
          object->Get(TRI_V8_ASCII_STRING(isolate, "includeSystem")));
    }
  }

  if (object->Has(TRI_V8_ASCII_STRING(isolate, "requireFromPresent"))) {
    if (object->Get(TRI_V8_ASCII_STRING(isolate, "requireFromPresent"))->IsBoolean()) {
      config._requireFromPresent = TRI_ObjectToBoolean(
          object->Get(TRI_V8_ASCII_STRING(isolate, "requireFromPresent")));
    }
  }

  bool incremental = false;
  if (object->Has(TRI_V8_ASCII_STRING(isolate, "incremental"))) {
    if (object->Get(TRI_V8_ASCII_STRING(isolate, "incremental"))->IsBoolean()) {
      incremental =
          TRI_ObjectToBoolean(object->Get(TRI_V8_ASCII_STRING(isolate, "incremental")));
    }
  }

  bool keepBarrier = false;
  if (object->Has(TRI_V8_ASCII_STRING(isolate, "keepBarrier"))) {
    keepBarrier =
        TRI_ObjectToBoolean(object->Get(TRI_V8_ASCII_STRING(isolate, "keepBarrier")));
  }

  if (object->Has(TRI_V8_ASCII_STRING(isolate, "useCollectionId"))) {
    config._useCollectionId =
        TRI_ObjectToBoolean(object->Get(TRI_V8_ASCII_STRING(isolate, "useCollectionId")));
  }

  std::string leaderId;
  if (object->Has(TRI_V8_ASCII_STRING(isolate, "leaderId"))) {
    leaderId = TRI_ObjectToString(object->Get(TRI_V8_ASCII_STRING(isolate, "leaderId")));
  }

  std::string errorMsg = "";
  DatabaseInitialSyncer syncer(vocbase, &config, restrictCollections, rType,
                               verbose, skipCreateDrop);
  if (!leaderId.empty()) {
    syncer.setLeaderId(leaderId);
  }

  int res = TRI_ERROR_NO_ERROR;
  v8::Handle<v8::Object> result = v8::Object::New(isolate);

  try {
    res = syncer.run(errorMsg, incremental);

    if (keepBarrier) {
      result->Set(TRI_V8_ASCII_STRING(isolate, "barrierId"),
                  TRI_V8UInt64String<TRI_voc_tick_t>(isolate, syncer.stealBarrier()));
    }

    result->Set(TRI_V8_ASCII_STRING(isolate, "lastLogTick"),
                TRI_V8UInt64String<TRI_voc_tick_t>(isolate, syncer.getLastLogTick()));

    std::map<TRI_voc_cid_t, std::string>::const_iterator it;
    std::map<TRI_voc_cid_t, std::string> const& c =
        syncer.getProcessedCollections();

    uint32_t j = 0;
    v8::Handle<v8::Array> collections = v8::Array::New(isolate);
    for (it = c.begin(); it != c.end(); ++it) {
      std::string const cidString = StringUtils::itoa((*it).first);

      v8::Handle<v8::Object> ci = v8::Object::New(isolate);
      ci->Set(TRI_V8_ASCII_STRING(isolate, "id"), TRI_V8_STD_STRING(isolate, cidString));
      ci->Set(TRI_V8_ASCII_STRING(isolate, "name"), TRI_V8_STD_STRING(isolate, (*it).second));

      collections->Set(j++, ci);
    }

    result->Set(TRI_V8_ASCII_STRING(isolate, "collections"), collections);
  } catch (arangodb::basics::Exception const& ex) {
    res = ex.code();
    if (errorMsg.empty()) {
      errorMsg = std::string("caught exception: ") + ex.what();
    }
  } catch (std::exception const& ex) {
    res = TRI_ERROR_INTERNAL;
    if (errorMsg.empty()) {
      errorMsg = std::string("caught exception: ") + ex.what();
    }
  } catch (...) {
    res = TRI_ERROR_INTERNAL;
    if (errorMsg.empty()) {
      errorMsg = "caught unknown exception";
    }
  }

  if (res != TRI_ERROR_NO_ERROR) {
    if (errorMsg.empty()) {
      TRI_V8_THROW_EXCEPTION_MESSAGE(res, "cannot sync from remote endpoint. last progress message was '" + syncer.progress() + "'");
    } else {
      TRI_V8_THROW_EXCEPTION_MESSAGE(
          res, "cannot sync from remote endpoint: " + errorMsg + ". last progress message was '" + syncer.progress() + "'");
    }
  }

  // Now check forSynchronousReplication flag and tell ClusterInfo
  // about a new follower.

  TRI_V8_RETURN(result);
  TRI_V8_TRY_CATCH_END
}

/// @brief finalize the synchronization of a collection by tailing the WAL
/// and filtering on the collection name until no more data is available
static void JS_SynchronizeReplicationFinalize(
    v8::FunctionCallbackInfo<v8::Value> const& args) {
  TRI_V8_TRY_CATCH_BEGIN(isolate);
  v8::HandleScope scope(isolate);

  if (args.Length() != 1 || !args[0]->IsObject()) {
    TRI_V8_THROW_EXCEPTION_USAGE("REPLICATION_SYNCHRONIZE_FINALIZE(<config>)");
  }

  // treat the argument as an object from now on
  v8::Handle<v8::Object> object = v8::Handle<v8::Object>::Cast(args[0]);

  std::string endpoint;
  if (object->Has(TRI_V8_ASCII_STRING(isolate, "endpoint"))) {
    endpoint = TRI_ObjectToString(object->Get(TRI_V8_ASCII_STRING(isolate, "endpoint")));
  }
  
  if (endpoint.empty()) {
    TRI_V8_THROW_EXCEPTION_PARAMETER("<endpoint> must be a valid endpoint");
  }

  std::string database;
  if (object->Has(TRI_V8_ASCII_STRING(isolate, "database"))) {
    database = TRI_ObjectToString(object->Get(TRI_V8_ASCII_STRING(isolate, "database")));
  }
  if (database.empty()) {
    TRI_V8_THROW_EXCEPTION_PARAMETER("<database> must be a valid database name");
  }
  
  std::string collection;
  if (object->Has(TRI_V8_ASCII_STRING(isolate, "collection"))) {
    collection = TRI_ObjectToString(object->Get(TRI_V8_ASCII_STRING(isolate, "collection")));
  }
  if (collection.empty()) {
    TRI_V8_THROW_EXCEPTION_PARAMETER("<collection> must be a valid collection name");
  }
  
  TRI_voc_tick_t fromTick = 0;  
  if (object->Has(TRI_V8_ASCII_STRING(isolate, "from"))) {
    fromTick = TRI_ObjectToUInt64(object->Get(TRI_V8_ASCII_STRING(isolate, "from")), true);
  }
  if (fromTick == 0) {
    TRI_V8_THROW_EXCEPTION_PARAMETER("<from> must be a valid start tick");
  }

  ReplicationApplierConfiguration config;
  config._endpoint = endpoint;
  config._database = database;

  addReplicationAuthentication(isolate, object, config);
  addConnectionSettings(isolate, object, config);

  if (object->Has(TRI_V8_ASCII_STRING(isolate, "requireFromPresent"))) {
    if (object->Get(TRI_V8_ASCII_STRING(isolate, "requireFromPresent"))->IsBoolean()) {
      config._requireFromPresent = TRI_ObjectToBoolean(
          object->Get(TRI_V8_ASCII_STRING(isolate, "requireFromPresent")));
    }
  }
  
  if (object->Has(TRI_V8_ASCII_STRING(isolate, "verbose"))) {
    config._verbose = TRI_ObjectToBoolean(object->Get(TRI_V8_ASCII_STRING(isolate, "verbose")));
  }

 
  std::string leaderId;
  if (object->Has(TRI_V8_ASCII_STRING(isolate, "leaderId"))) {
    leaderId = TRI_ObjectToString(object->Get(TRI_V8_ASCII_STRING(isolate, "leaderId")));
  }

  DatabaseGuard guard(database);

  std::string errorMsg = "";
  DatabaseTailingSyncer syncer(guard.database(), &config, fromTick, true, 0);
  
  if (!leaderId.empty()) {
    syncer.setLeaderId(leaderId);
  }

  int res = TRI_ERROR_NO_ERROR;
  v8::Handle<v8::Object> result = v8::Object::New(isolate);

  try {
    res = syncer.syncCollectionFinalize(errorMsg, collection);
  } catch (arangodb::basics::Exception const& ex) {
    res = ex.code();
    if (errorMsg.empty()) {
      errorMsg = std::string("caught exception: ") + ex.what();
    }
  } catch (std::exception const& ex) {
    res = TRI_ERROR_INTERNAL;
    if (errorMsg.empty()) {
      errorMsg = std::string("caught exception: ") + ex.what();
    }
  } catch (...) {
    res = TRI_ERROR_INTERNAL;
    if (errorMsg.empty()) {
      errorMsg = "caught unknown exception";
    }
  }

  if (res != TRI_ERROR_NO_ERROR) {
    if (errorMsg.empty()) {
      TRI_V8_THROW_EXCEPTION_MESSAGE(res, std::string("cannot sync data for shard '") + collection + "' from remote endpoint");
    } else {
      TRI_V8_THROW_EXCEPTION_MESSAGE(res, std::string("cannot sync data for shard '") + collection + "' from remote endpoint: " + errorMsg);
    }
  }

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

  std::string const serverId = StringUtils::itoa(ServerIdFeature::getId());
  TRI_V8_RETURN_STD_STRING(serverId);
  TRI_V8_TRY_CATCH_END
}

enum ApplierType {
  APPLIER_DATABASE,
  APPLIER_GLOBAL
};

static ReplicationApplier* getApplier(v8::Isolate* isolate, ApplierType applierType) {
  ReplicationApplier* applier = nullptr;
  
  if (applierType == APPLIER_DATABASE) {
    // database-specific applier
    TRI_vocbase_t* vocbase = GetContextVocBase(isolate);

    if (vocbase == nullptr) {
      THROW_ARANGO_EXCEPTION(TRI_ERROR_ARANGO_DATABASE_NOT_FOUND);
    }

    applier = vocbase->replicationApplier();
  } else {
    // applier type global
  }

  if (applier == nullptr) {
    THROW_ARANGO_EXCEPTION_MESSAGE(TRI_ERROR_INTERNAL, "unable to find replicationApplier");
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

  ReplicationApplier* applier = getApplier(isolate, applierType);
    
  // get current configuration 
  ReplicationApplierConfiguration configuration = applier->configuration();

  if (args.Length() == 0) {
    // no argument: return the current configuration
    VPackBuilder builder;
    builder.openObject();
    configuration.toVelocyPack(builder, true);
    builder.close();

    v8::Handle<v8::Value> result = TRI_VPackToV8(isolate, builder.slice());

    TRI_V8_RETURN(result);
  }

  else {
    // set the configuration
    if (args.Length() != 1 || !args[0]->IsObject()) {
      TRI_V8_THROW_EXCEPTION_USAGE(
          "REPLICATION_APPLIER_CONFIGURE(<configuration>)");
    }

    // treat the argument as an object from now on
    v8::Handle<v8::Object> object = v8::Handle<v8::Object>::Cast(args[0]);

    if (object->Has(TRI_V8_ASCII_STRING(isolate, "endpoint"))) {
      if (object->Get(TRI_V8_ASCII_STRING(isolate, "endpoint"))->IsString()) {
        configuration._endpoint =
            TRI_ObjectToString(object->Get(TRI_V8_ASCII_STRING(isolate, "endpoint")));
      }
    }

    if (object->Has(TRI_V8_ASCII_STRING(isolate, "database"))) {
      if (object->Get(TRI_V8_ASCII_STRING(isolate, "database"))->IsString()) {
        configuration._database =
            TRI_ObjectToString(object->Get(TRI_V8_ASCII_STRING(isolate, "database")));
      }
    } else {
      if (configuration._database.empty()) {
        // no database set, use current
        TRI_vocbase_t* vocbase = GetContextVocBase(isolate);

        if (vocbase == nullptr) {
          THROW_ARANGO_EXCEPTION(TRI_ERROR_ARANGO_DATABASE_NOT_FOUND);
        }

        configuration._database = vocbase->name();
      }
    }

    addReplicationAuthentication(isolate, object, configuration);
    addConnectionSettings(isolate, object, configuration);

    if (object->Has(TRI_V8_ASCII_STRING(isolate, "ignoreErrors"))) {
      if (object->Get(TRI_V8_ASCII_STRING(isolate, "ignoreErrors"))->IsNumber()) {
        configuration._ignoreErrors = TRI_ObjectToUInt64(
            object->Get(TRI_V8_ASCII_STRING(isolate, "ignoreErrors")), false);
      }
    }

    if (object->Has(TRI_V8_ASCII_STRING(isolate, "lockTimeoutRetries"))) {
      if (object->Get(TRI_V8_ASCII_STRING(isolate, "lockTimeoutRetries"))->IsNumber()) {
        configuration._lockTimeoutRetries = TRI_ObjectToUInt64(
            object->Get(TRI_V8_ASCII_STRING(isolate, "lockTimeoutRetries")), false);
      }
    }

    if (object->Has(TRI_V8_ASCII_STRING(isolate, "autoStart"))) {
      if (object->Get(TRI_V8_ASCII_STRING(isolate, "autoStart"))->IsBoolean()) {
        configuration._autoStart =
            TRI_ObjectToBoolean(object->Get(TRI_V8_ASCII_STRING(isolate, "autoStart")));
      }
    }

    if (object->Has(TRI_V8_ASCII_STRING(isolate, "adaptivePolling"))) {
      if (object->Get(TRI_V8_ASCII_STRING(isolate, "adaptivePolling"))->IsBoolean()) {
        configuration._adaptivePolling = TRI_ObjectToBoolean(
            object->Get(TRI_V8_ASCII_STRING(isolate, "adaptivePolling")));
      }
    }

    if (object->Has(TRI_V8_ASCII_STRING(isolate, "autoResync"))) {
      if (object->Get(TRI_V8_ASCII_STRING(isolate, "autoResync"))->IsBoolean()) {
        configuration._autoResync =
            TRI_ObjectToBoolean(object->Get(TRI_V8_ASCII_STRING(isolate, "autoResync")));
      }
    }

    if (object->Has(TRI_V8_ASCII_STRING(isolate, "includeSystem"))) {
      if (object->Get(TRI_V8_ASCII_STRING(isolate, "includeSystem"))->IsBoolean()) {
        configuration._includeSystem = TRI_ObjectToBoolean(
            object->Get(TRI_V8_ASCII_STRING(isolate, "includeSystem")));
      }
    }

    if (object->Has(TRI_V8_ASCII_STRING(isolate, "requireFromPresent"))) {
      if (object->Get(TRI_V8_ASCII_STRING(isolate, "requireFromPresent"))->IsBoolean()) {
        configuration._requireFromPresent = TRI_ObjectToBoolean(
            object->Get(TRI_V8_ASCII_STRING(isolate, "requireFromPresent")));
      }
    }

    if (object->Has(TRI_V8_ASCII_STRING(isolate, "incremental"))) {
      if (object->Get(TRI_V8_ASCII_STRING(isolate, "incremental"))->IsBoolean()) {
        configuration._incremental = TRI_ObjectToBoolean(
            object->Get(TRI_V8_ASCII_STRING(isolate, "incremental")));
      }
    }

    if (object->Has(TRI_V8_ASCII_STRING(isolate, "verbose"))) {
      if (object->Get(TRI_V8_ASCII_STRING(isolate, "verbose"))->IsBoolean()) {
        configuration._verbose =
            TRI_ObjectToBoolean(object->Get(TRI_V8_ASCII_STRING(isolate, "verbose")));
      }
    }

    if (object->Has(TRI_V8_ASCII_STRING(isolate, "restrictCollections")) &&
        object->Get(TRI_V8_ASCII_STRING(isolate, "restrictCollections"))->IsArray()) {
      configuration._restrictCollections.clear();
      v8::Handle<v8::Array> a = v8::Handle<v8::Array>::Cast(
          object->Get(TRI_V8_ASCII_STRING(isolate, "restrictCollections")));

      uint32_t const n = a->Length();

      for (uint32_t i = 0; i < n; ++i) {
        v8::Handle<v8::Value> cname = a->Get(i);

        if (cname->IsString()) {
          configuration._restrictCollections.insert(
              std::pair<std::string, bool>(TRI_ObjectToString(cname), true));
        }
      }
    }

    if (object->Has(TRI_V8_ASCII_STRING(isolate, "restrictType"))) {
      configuration._restrictType =
          TRI_ObjectToString(object->Get(TRI_V8_ASCII_STRING(isolate, "restrictType")));
    }

    if ((configuration._restrictType.empty() &&
         !configuration._restrictCollections.empty()) ||
        (!configuration._restrictType.empty() &&
         configuration._restrictCollections.empty()) ||
        (!configuration._restrictType.empty() && configuration._restrictType != "include" &&
         configuration._restrictType != "exclude")) {
      TRI_V8_THROW_EXCEPTION_PARAMETER(
          "invalid value for <restrictCollections> or <restrictType>");
    }

    if (object->Has(TRI_V8_ASCII_STRING(isolate, "connectionRetryWaitTime"))) {
      if (object->Get(TRI_V8_ASCII_STRING(isolate, "connectionRetryWaitTime"))
              ->IsNumber()) {
        double value = TRI_ObjectToDouble(
            object->Get(TRI_V8_ASCII_STRING(isolate, "connectionRetryWaitTime")));
        if (value > 0.0) {
          configuration._connectionRetryWaitTime =
              static_cast<uint64_t>(value * 1000.0 * 1000.0);
        }
      }
    }

    if (object->Has(TRI_V8_ASCII_STRING(isolate, "initialSyncMaxWaitTime"))) {
      if (object->Get(TRI_V8_ASCII_STRING(isolate, "initialSyncMaxWaitTime"))
              ->IsNumber()) {
        double value = TRI_ObjectToDouble(
            object->Get(TRI_V8_ASCII_STRING(isolate, "initialSyncMaxWaitTime")));
        if (value > 0.0) {
          configuration._initialSyncMaxWaitTime =
              static_cast<uint64_t>(value * 1000.0 * 1000.0);
        }
      }
    }

    if (object->Has(TRI_V8_ASCII_STRING(isolate, "idleMinWaitTime"))) {
      if (object->Get(TRI_V8_ASCII_STRING(isolate, "idleMinWaitTime"))->IsNumber()) {
        double value = TRI_ObjectToDouble(
            object->Get(TRI_V8_ASCII_STRING(isolate, "idleMinWaitTime")));
        if (value > 0.0) {
          configuration._idleMinWaitTime =
              static_cast<uint64_t>(value * 1000.0 * 1000.0);
        }
      }
    }

    if (object->Has(TRI_V8_ASCII_STRING(isolate, "idleMaxWaitTime"))) {
      if (object->Get(TRI_V8_ASCII_STRING(isolate, "idleMaxWaitTime"))->IsNumber()) {
        double value = TRI_ObjectToDouble(
            object->Get(TRI_V8_ASCII_STRING(isolate, "idleMaxWaitTime")));
        if (value > 0.0) {
          configuration._idleMaxWaitTime =
              static_cast<uint64_t>(value * 1000.0 * 1000.0);
        }
      }
    }

    if (object->Has(TRI_V8_ASCII_STRING(isolate, "autoResyncRetries"))) {
      if (object->Get(TRI_V8_ASCII_STRING(isolate, "autoResyncRetries"))->IsNumber()) {
        double value = TRI_ObjectToDouble(
            object->Get(TRI_V8_ASCII_STRING(isolate, "autoResyncRetries")));
        configuration._autoResyncRetries = static_cast<uint64_t>(value);
      }
    }

    applier->reconfigure(configuration);

    VPackBuilder builder;
    builder.openObject();
    configuration.toVelocyPack(builder, true);
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

static void StartApplierReplication(v8::FunctionCallbackInfo<v8::Value> const& args,
                                    ApplierType applierType) {
  TRI_V8_TRY_CATCH_BEGIN(isolate);
  v8::HandleScope scope(isolate);
  
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
  
  ReplicationApplier* applier = getApplier(isolate, applierType);

  applier->start(initialTick, useTick, barrierId);

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

static void ShutdownApplierReplication(v8::FunctionCallbackInfo<v8::Value> const& args,
                                       ApplierType applierType) {
  TRI_V8_TRY_CATCH_BEGIN(isolate);
  v8::HandleScope scope(isolate);

  if (args.Length() != 0) {
    TRI_V8_THROW_EXCEPTION_USAGE("REPLICATION_APPLIER_SHUTDOWN()");
  }

  ReplicationApplier* applier = getApplier(isolate, applierType);

  applier->shutdown();

  TRI_V8_RETURN_TRUE();
  TRI_V8_TRY_CATCH_END
}

static void JS_ShutdownApplierReplication(
    v8::FunctionCallbackInfo<v8::Value> const& args) {
  ShutdownApplierReplication(args, APPLIER_DATABASE);
}

static void JS_ShutdownGlobalApplierReplication(
    v8::FunctionCallbackInfo<v8::Value> const& args) {
  ShutdownApplierReplication(args, APPLIER_GLOBAL);
}

////////////////////////////////////////////////////////////////////////////////
/// @brief get the state of the replication applier
////////////////////////////////////////////////////////////////////////////////

static void StateApplierReplication(v8::FunctionCallbackInfo<v8::Value> const& args,
                                    ApplierType applierType) {
  TRI_V8_TRY_CATCH_BEGIN(isolate);
  v8::HandleScope scope(isolate);

  if (args.Length() != 0) {
    TRI_V8_THROW_EXCEPTION_USAGE("REPLICATION_APPLIER_STATE()");
  }

  ReplicationApplier* applier = getApplier(isolate, applierType);

  VPackBuilder builder;
  builder.openObject();
  applier->toVelocyPack(builder);
  builder.close();

  v8::Handle<v8::Value> result = TRI_VPackToV8(isolate, builder.slice());

  TRI_V8_RETURN(result);
  TRI_V8_TRY_CATCH_END
}

static void JS_StateApplierReplication(
    v8::FunctionCallbackInfo<v8::Value> const& args) {
  StateApplierReplication(args, APPLIER_DATABASE);
}

static void JS_StateGlobalApplierReplication(
    v8::FunctionCallbackInfo<v8::Value> const& args) {
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
    TRI_V8_THROW_EXCEPTION_USAGE("REPLICATION_APPLIER_FORGET()");
  }

  ReplicationApplier* applier = getApplier(isolate, applierType);

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

void TRI_InitV8Replication(v8::Isolate* isolate,
                           v8::Handle<v8::Context> context,
                           TRI_vocbase_t* vocbase,
                           size_t threadNumber, TRI_v8_global_t* v8g) {
  // replication functions. not intended to be used by end users

  // logger functions
  TRI_AddGlobalFunctionVocbase(isolate,
                               TRI_V8_ASCII_STRING(isolate, "REPLICATION_LOGGER_STATE"),
                               JS_StateLoggerReplication, true);
  TRI_AddGlobalFunctionVocbase(isolate,
                               TRI_V8_ASCII_STRING(isolate, "REPLICATION_LOGGER_LAST"),
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
      isolate, TRI_V8_ASCII_STRING(isolate, "GLOBAL_REPLICATION_APPLIER_CONFIGURE"),
      JS_ConfigureGlobalApplierReplication, true);
  TRI_AddGlobalFunctionVocbase(isolate,
                               TRI_V8_ASCII_STRING(isolate, "REPLICATION_APPLIER_START"),
                               JS_StartApplierReplication, true);
  TRI_AddGlobalFunctionVocbase(isolate,
                               TRI_V8_ASCII_STRING(isolate, "GLOBAL_REPLICATION_APPLIER_START"),
                               JS_StartGlobalApplierReplication, true);
  TRI_AddGlobalFunctionVocbase(
      isolate, TRI_V8_ASCII_STRING(isolate, "REPLICATION_APPLIER_SHUTDOWN"),
      JS_ShutdownApplierReplication, true);
  TRI_AddGlobalFunctionVocbase(
      isolate, TRI_V8_ASCII_STRING(isolate, "GLOBAL_REPLICATION_APPLIER_SHUTDOWN"),
      JS_ShutdownGlobalApplierReplication, true);
  TRI_AddGlobalFunctionVocbase(isolate,
                               TRI_V8_ASCII_STRING(isolate, "REPLICATION_APPLIER_STATE"),
                               JS_StateApplierReplication, true);
  TRI_AddGlobalFunctionVocbase(isolate,
                               TRI_V8_ASCII_STRING(isolate, "GLOBAL_REPLICATION_APPLIER_STATE"),
                               JS_StateGlobalApplierReplication, true);
  TRI_AddGlobalFunctionVocbase(
      isolate, TRI_V8_ASCII_STRING(isolate, "REPLICATION_APPLIER_FORGET"),
      JS_ForgetApplierReplication, true);
  TRI_AddGlobalFunctionVocbase(
      isolate, TRI_V8_ASCII_STRING(isolate, "GLOBAL_REPLICATION_APPLIER_FORGET"),
      JS_ForgetGlobalApplierReplication, true);
  
  // other functions
  TRI_AddGlobalFunctionVocbase(isolate,
                               TRI_V8_ASCII_STRING(isolate, "REPLICATION_SYNCHRONIZE"),
                               JS_SynchronizeReplication, true);
  TRI_AddGlobalFunctionVocbase(isolate,
                               TRI_V8_ASCII_STRING(isolate, "REPLICATION_SYNCHRONIZE_FINALIZE"),
                               JS_SynchronizeReplicationFinalize, true);
  TRI_AddGlobalFunctionVocbase(isolate,
                               TRI_V8_ASCII_STRING(isolate, "REPLICATION_SERVER_ID"),
                               JS_ServerIdReplication, true);
}
