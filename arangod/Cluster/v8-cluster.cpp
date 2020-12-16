////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2014-2018 ArangoDB GmbH, Cologne, Germany
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

#include "v8-cluster.h"

#include <velocypack/Iterator.h>
#include <velocypack/velocypack-aliases.h>

#include "Agency/AgencyComm.h"
#include "Agency/AsyncAgencyComm.h"
#include "ApplicationFeatures/ApplicationServer.h"
#include "Basics/Exceptions.h"
#include "Basics/StringBuffer.h"
#include "Basics/StringUtils.h"
#include "Cluster/ClusterFeature.h"
#include "Cluster/ClusterInfo.h"
#include "Cluster/ServerState.h"
#include "GeneralServer/AuthenticationFeature.h"
#include "Network/NetworkFeature.h"
#include "Network/Methods.h"
#include "Replication/ReplicationFeature.h"
#include "Rest/GeneralRequest.h"
#include "Sharding/ShardDistributionReporter.h"
#include "V8/v8-buffer.h"
#include "V8/v8-conv.h"
#include "V8/v8-globals.h"
#include "V8/v8-utils.h"
#include "V8/v8-vpack.h"
#include "V8Server/v8-vocbaseprivate.h"
#include "VocBase/LogicalCollection.h"
#include "VocBase/ticks.h"

using namespace arangodb;
using namespace arangodb::basics;

////////////////////////////////////////////////////////////////////////////////
/// @brief creates a v8 exception object
////////////////////////////////////////////////////////////////////////////////

#define THROW_AGENCY_EXCEPTION(data) \
  CreateAgencyException(args, data); \
  return;

static void onlyInCluster() {
  if (ServerState::instance()->isRunningInCluster()) {
    return;
  }

  THROW_ARANGO_EXCEPTION_MESSAGE(TRI_ERROR_INTERNAL, "ArangoDB is not running in cluster mode");
}

static void onlyInClusterOrActiveFailover() {
  auto replicationFeature = ReplicationFeature::INSTANCE;
  if (replicationFeature != nullptr && replicationFeature->isActiveFailoverEnabled()) {
    // active failover enabled
    return;
  }

  return onlyInCluster();
}

static void CreateAgencyException(v8::FunctionCallbackInfo<v8::Value> const& args,
                                  AgencyCommResult const& result) {
  v8::Isolate* isolate = args.GetIsolate();
  TRI_V8_CURRENT_GLOBALS_AND_SCOPE;
  auto context = TRI_IGETC;

  std::string const errorDetails = result.errorDetails();
  v8::Handle<v8::String> errorMessage = TRI_V8_STD_STRING(isolate, errorDetails);
  if (errorMessage.IsEmpty()) {
    isolate->ThrowException(v8::Object::New(isolate));
    return;
  }
  v8::Handle<v8::Object> errorObject =
      v8::Exception::Error(errorMessage)->ToObject(TRI_IGETC).FromMaybe(v8::Local<v8::Object>());
  if (errorObject.IsEmpty()) {
    isolate->ThrowException(v8::Object::New(isolate));
    return;
  }

  errorObject->Set(context,
                   TRI_V8_STD_STRING(isolate, StaticStrings::Code),
                   v8::Number::New(isolate, result.httpCode())).FromMaybe(false);
  errorObject->Set(context,
                   TRI_V8_STD_STRING(isolate, StaticStrings::ErrorNum),
                   v8::Number::New(isolate, result.errorCode())).FromMaybe(false);
  errorObject->Set(context,
                   TRI_V8_STD_STRING(isolate, StaticStrings::ErrorMessage), errorMessage).FromMaybe(false);
  errorObject->Set(context,
                   TRI_V8_STD_STRING(isolate, StaticStrings::Error), v8::True(isolate)).FromMaybe(false);

  TRI_GET_GLOBAL(ArangoErrorTempl, v8::ObjectTemplate);
  v8::Handle<v8::Value> proto = ArangoErrorTempl->NewInstance(TRI_IGETC).FromMaybe(v8::Local<v8::Value>());
  if (!proto.IsEmpty()) {
    errorObject->SetPrototype(TRI_IGETC, proto).FromMaybe(false);
  }

  args.GetIsolate()->ThrowException(errorObject);
}

////////////////////////////////////////////////////////////////////////////////
/// @brief compares and swaps a value in the agency
////////////////////////////////////////////////////////////////////////////////

static void JS_CasAgency(v8::FunctionCallbackInfo<v8::Value> const& args) {
  TRI_V8_TRY_CATCH_BEGIN(isolate);
  v8::HandleScope scope(isolate);

  onlyInClusterOrActiveFailover();

  if (args.Length() < 3) {
    TRI_V8_THROW_EXCEPTION_USAGE(
        "cas(<key>, <oldValue>, <newValue>, <ttl>, <timeout>, <throw>)");
  }

  std::string const key = TRI_ObjectToString(isolate, args[0]);

  VPackBuilder oldBuilder;
  int res = TRI_V8ToVPack(isolate, oldBuilder, args[1], false);

  if (res != TRI_ERROR_NO_ERROR) {
    TRI_V8_THROW_EXCEPTION_PARAMETER("cannot convert <oldValue> to VPack");
  }

  VPackBuilder newBuilder;
  res = TRI_V8ToVPack(isolate, newBuilder, args[2], false);

  if (res != TRI_ERROR_NO_ERROR) {
    TRI_V8_THROW_EXCEPTION_PARAMETER("cannot convert <newValue> to VPack");
  }

  double ttl = 0.0;
  if (args.Length() > 3) {
    ttl = TRI_ObjectToDouble(isolate, args[3]);
  }

  double timeout = 1.0;
  if (args.Length() > 4) {
    timeout = TRI_ObjectToDouble(isolate, args[4]);
  }

  bool shouldThrow = false;
  if (args.Length() > 5) {
    shouldThrow = TRI_ObjectToBoolean(isolate, args[5]);
  }

  TRI_GET_GLOBALS();
  AgencyComm comm(v8g->_server);
  AgencyCommResult result =
      comm.casValue(key, oldBuilder.slice(), newBuilder.slice(), ttl, timeout);

  if (!result.successful()) {
    if (!shouldThrow) {
      TRI_V8_RETURN_FALSE();
    }

    THROW_AGENCY_EXCEPTION(result);
  }

  TRI_V8_RETURN_TRUE();
  TRI_V8_TRY_CATCH_END
}

////////////////////////////////////////////////////////////////////////////////
/// @brief creates a directory in the agency
////////////////////////////////////////////////////////////////////////////////

static void JS_CreateDirectoryAgency(v8::FunctionCallbackInfo<v8::Value> const& args) {
  TRI_V8_TRY_CATCH_BEGIN(isolate);
  v8::HandleScope scope(isolate);

  onlyInClusterOrActiveFailover();

  if (args.Length() != 1) {
    TRI_V8_THROW_EXCEPTION_USAGE("createDirectory(<key>)");
  }

  std::string const key = TRI_ObjectToString(isolate, args[0]);

  TRI_GET_GLOBALS();
  AgencyComm comm(v8g->_server);
  AgencyCommResult result = comm.createDirectory(key);

  if (!result.successful()) {
    THROW_AGENCY_EXCEPTION(result);
  }

  TRI_V8_RETURN_TRUE();
  TRI_V8_TRY_CATCH_END
}

////////////////////////////////////////////////////////////////////////////////
/// @brief whether or not the agency is enabled
////////////////////////////////////////////////////////////////////////////////

static void JS_IsEnabledAgency(v8::FunctionCallbackInfo<v8::Value> const& args) {
  TRI_V8_TRY_CATCH_BEGIN(isolate)
  v8::HandleScope scope(isolate);

  if (args.Length() != 0) {
    TRI_V8_THROW_EXCEPTION_USAGE("isEnabled()");
  }

  if (AsyncAgencyCommManager::isEnabled()) {
    TRI_V8_RETURN_TRUE();
  }

  TRI_V8_RETURN_FALSE();
  TRI_V8_TRY_CATCH_END
}

////////////////////////////////////////////////////////////////////////////////
/// @brief increase the version number
////////////////////////////////////////////////////////////////////////////////

static void JS_IncreaseVersionAgency(v8::FunctionCallbackInfo<v8::Value> const& args) {
  TRI_V8_TRY_CATCH_BEGIN(isolate)
  v8::HandleScope scope(isolate);

  onlyInClusterOrActiveFailover();

  if (args.Length() != 1) {
    TRI_V8_THROW_EXCEPTION_USAGE("increaseVersion(<key>)");
  }

  std::string const key = TRI_ObjectToString(isolate, args[0]);

  TRI_GET_GLOBALS();
  AgencyComm comm(v8g->_server);
  if (!comm.increaseVersion(key)) {
    TRI_V8_THROW_EXCEPTION_MESSAGE(TRI_ERROR_INTERNAL,
                                   "unable to increase version");
  }

  TRI_V8_RETURN_TRUE();
  TRI_V8_TRY_CATCH_END
}

////////////////////////////////////////////////////////////////////////////////
/// @brief gets a value from the agency
////////////////////////////////////////////////////////////////////////////////

static void JS_GetAgency(v8::FunctionCallbackInfo<v8::Value> const& args) {
  TRI_V8_TRY_CATCH_BEGIN(isolate)
  v8::HandleScope scope(isolate);
  auto context = TRI_IGETC;

  onlyInClusterOrActiveFailover();

  if (args.Length() < 1) {
    TRI_V8_THROW_EXCEPTION_USAGE("get(<key>)");
  }

  std::string const key = TRI_ObjectToString(isolate, args[0]);
  TRI_GET_GLOBALS();
  AgencyComm comm(v8g->_server);
  AgencyCommResult result = comm.getValues(key);

  if (!result.successful()) {
    THROW_AGENCY_EXCEPTION(result);
  }

  v8::Handle<v8::Object> l = v8::Object::New(isolate);

  // return just the value for each key

  for (auto const& a : VPackArrayIterator(result.slice())) {
    for (auto const& o : VPackObjectIterator(a)) {
      std::string const key = o.key.copyString();
      VPackSlice const slice = o.value;

      if (!slice.isNone()) {
        l->Set(context, TRI_V8_STD_STRING(isolate, key), TRI_VPackToV8(isolate, slice)).FromMaybe(false);
      }
    }
  }

  TRI_V8_RETURN(l);
  TRI_V8_TRY_CATCH_END
}

////////////////////////////////////////////////////////////////////////////////
/// @brief read transaction to the agency
////////////////////////////////////////////////////////////////////////////////

static void JS_APIAgency(std::string const& envelope,
                         v8::FunctionCallbackInfo<v8::Value> const& args) {
  TRI_V8_TRY_CATCH_BEGIN(isolate)
  v8::HandleScope scope(isolate);

  onlyInClusterOrActiveFailover();

  if (args.Length() < 1) {
    TRI_V8_THROW_EXCEPTION_USAGE(std::string(envelope) + "([[...]])");
  }

  VPackBuilder builder;
  int res = TRI_V8ToVPack(isolate, builder, args[0], false);

  if (res != TRI_ERROR_NO_ERROR) {
    TRI_V8_THROW_EXCEPTION_PARAMETER("cannot convert query to JSON");
  }

  TRI_GET_GLOBALS();
  AgencyComm comm(v8g->_server);
  AgencyCommResult result =
      comm.sendWithFailover(arangodb::rest::RequestType::POST,
                            AgencyCommHelper::CONNECTION_OPTIONS._requestTimeout,
                            std::string("/_api/agency/") + envelope, builder.slice());

  if (!result.successful()) {
    THROW_AGENCY_EXCEPTION(result);
  }

  try {
    result.setVPack(VPackParser::fromJson(result.body()));
    result._body.clear();
  } catch (std::exception const& e) {
    LOG_TOPIC("57115", ERR, Logger::AGENCYCOMM) << "Error transforming result: " << e.what();
    result.clear();
  } catch (...) {
    LOG_TOPIC("ec86e", ERR, Logger::AGENCYCOMM)
        << "Error transforming result: out of memory";
    result.clear();
  }

  auto l = TRI_VPackToV8(isolate, result.slice());

  TRI_V8_RETURN(l);
  TRI_V8_TRY_CATCH_END
}
static void JS_ReadAgency(v8::FunctionCallbackInfo<v8::Value> const& args) {
  JS_APIAgency("read", args);
}
static void JS_WriteAgency(v8::FunctionCallbackInfo<v8::Value> const& args) {
  JS_APIAgency("write", args);
}
static void JS_TransactAgency(v8::FunctionCallbackInfo<v8::Value> const& args) {
  JS_APIAgency("transact", args);
}
static void JS_TransientAgency(v8::FunctionCallbackInfo<v8::Value> const& args) {
  JS_APIAgency("transient", args);
}

////////////////////////////////////////////////////////////////////////////////
/// @brief acquires a write-lock in the agency
////////////////////////////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////////////////////////
/// @brief removes a value from the agency
////////////////////////////////////////////////////////////////////////////////

static void JS_RemoveAgency(v8::FunctionCallbackInfo<v8::Value> const& args) {
  TRI_V8_TRY_CATCH_BEGIN(isolate);
  v8::HandleScope scope(isolate);

  onlyInClusterOrActiveFailover();

  if (args.Length() < 1) {
    TRI_V8_THROW_EXCEPTION_USAGE("remove(<key>, <recursive>)");
  }

  std::string const key = TRI_ObjectToString(isolate, args[0]);
  bool recursive = false;

  if (args.Length() > 1) {
    recursive = TRI_ObjectToBoolean(isolate, args[1]);
  }

  TRI_GET_GLOBALS();
  AgencyComm comm(v8g->_server);
  AgencyCommResult result = comm.removeValues(key, recursive);

  if (!result.successful()) {
    THROW_AGENCY_EXCEPTION(result);
  }

  TRI_V8_RETURN_TRUE();
  TRI_V8_TRY_CATCH_END
}

////////////////////////////////////////////////////////////////////////////////
/// @brief sets a value in the agency
////////////////////////////////////////////////////////////////////////////////

static void JS_SetAgency(v8::FunctionCallbackInfo<v8::Value> const& args) {
  TRI_V8_TRY_CATCH_BEGIN(isolate);
  v8::HandleScope scope(isolate);

  onlyInClusterOrActiveFailover();

  if (args.Length() < 2) {
    TRI_V8_THROW_EXCEPTION_USAGE("set(<key>, <value>, <ttl>)");
  }

  std::string const key = TRI_ObjectToString(isolate, args[0]);

  VPackBuilder builder;
  int res = TRI_V8ToVPack(isolate, builder, args[1], false);

  if (res != TRI_ERROR_NO_ERROR) {
    TRI_V8_THROW_EXCEPTION_PARAMETER("cannot convert <value> to JSON");
  }

  double ttl = 0.0;
  if (args.Length() > 2) {
    ttl = TRI_ObjectToDouble(isolate, args[2]);
  }

  TRI_GET_GLOBALS();
  AgencyComm comm(v8g->_server);
  AgencyCommResult result = comm.setValue(key, builder.slice(), ttl);

  if (!result.successful()) {
    THROW_AGENCY_EXCEPTION(result);
  }

  TRI_V8_RETURN_TRUE();
  TRI_V8_TRY_CATCH_END
}

////////////////////////////////////////////////////////////////////////////////
/// @brief returns the agency summery
////////////////////////////////////////////////////////////////////////////////

static void JS_Agency(v8::FunctionCallbackInfo<v8::Value> const& args) {
  TRI_V8_TRY_CATCH_BEGIN(isolate)
  v8::HandleScope scope(isolate);

  onlyInClusterOrActiveFailover();

  if (args.Length() > 0) {
    TRI_V8_THROW_EXCEPTION_USAGE("agency()");
  }

  VPackBuilder builder;

  TRI_GET_GLOBALS();
  AgencyComm comm(v8g->_server);
  AgencyCommResult result =
      comm.sendWithFailover(arangodb::rest::RequestType::GET,
                            AgencyCommHelper::CONNECTION_OPTIONS._requestTimeout,
                            std::string("/_api/agency/config"), builder.slice());

  if (!result.successful()) {
    THROW_AGENCY_EXCEPTION(result);
  }

  try {
    result.setVPack(VPackParser::fromJson(result.body()));
    result._body.clear();
  } catch (std::exception const& e) {
    LOG_TOPIC("d8594", ERR, Logger::AGENCYCOMM) << "Error transforming result: " << e.what();
    result.clear();
  } catch (...) {
    LOG_TOPIC("0ba23", ERR, Logger::AGENCYCOMM)
        << "Error transforming result: out of memory";
    result.clear();
  }

  auto l = TRI_VPackToV8(isolate, result.slice());

  TRI_V8_RETURN(l);
  TRI_V8_TRY_CATCH_END
}
////////////////////////////////////////////////////////////////////////////////
/// @brief returns the agency endpoints
////////////////////////////////////////////////////////////////////////////////

static void JS_EndpointsAgency(v8::FunctionCallbackInfo<v8::Value> const& args) {
  TRI_V8_TRY_CATCH_BEGIN(isolate);
  v8::HandleScope scope(isolate);
  auto context = TRI_IGETC;

  onlyInClusterOrActiveFailover();

  if (args.Length() != 0) {
    TRI_V8_THROW_EXCEPTION_USAGE("endpoints()");
  }

  auto endpoints = AsyncAgencyCommManager::INSTANCE->endpoints();
  // make the list of endpoints unique
  std::sort(endpoints.begin(), endpoints.end());
  endpoints.assign(endpoints.begin(), std::unique(endpoints.begin(), endpoints.end()));

  v8::Handle<v8::Array> l = v8::Array::New(isolate);

  for (size_t i = 0; i < endpoints.size(); ++i) {
    std::string const endpoint = endpoints[i];

    l->Set(context, (uint32_t)i, TRI_V8_STD_STRING(isolate, endpoint)).FromMaybe(false);
  }

  TRI_V8_RETURN(l);
  TRI_V8_TRY_CATCH_END
}

////////////////////////////////////////////////////////////////////////////////
/// @brief returns the agency prefix
////////////////////////////////////////////////////////////////////////////////

static void JS_PrefixAgency(v8::FunctionCallbackInfo<v8::Value> const& args) {
  TRI_V8_TRY_CATCH_BEGIN(isolate);
  v8::HandleScope scope(isolate);

  std::string const prefix = AgencyCommHelper::path();

  TRI_V8_RETURN_STD_STRING(prefix);
  TRI_V8_TRY_CATCH_END
}

////////////////////////////////////////////////////////////////////////////////
/// @brief creates a uniqid
////////////////////////////////////////////////////////////////////////////////

static void JS_UniqidAgency(v8::FunctionCallbackInfo<v8::Value> const& args) {
  TRI_V8_TRY_CATCH_BEGIN(isolate);
  v8::HandleScope scope(isolate);

  onlyInClusterOrActiveFailover();

  if (args.Length() > 2) {
    TRI_V8_THROW_EXCEPTION_USAGE("uniqid(<count>, <timeout>)");
  }

  uint64_t count = 1;
  if (args.Length() > 0) {
    count = TRI_ObjectToUInt64(isolate, args[0], true);
  }

  if (count < 1 || count > 10000000) {
    TRI_V8_THROW_EXCEPTION_PARAMETER("<count> is invalid");
  }

  double timeout = 0.0;
  if (args.Length() > 1) {
    timeout = TRI_ObjectToDouble(isolate, args[1]);
  }

  TRI_GET_GLOBALS();
  AgencyComm comm(v8g->_server);
  uint64_t result = comm.uniqid(count, timeout);

  std::string const value = StringUtils::itoa(result);

  TRI_V8_RETURN_STD_STRING(value);
  TRI_V8_TRY_CATCH_END
}

////////////////////////////////////////////////////////////////////////////////
/// @brief returns the agency version
////////////////////////////////////////////////////////////////////////////////

static void JS_VersionAgency(v8::FunctionCallbackInfo<v8::Value> const& args) {
  TRI_V8_TRY_CATCH_BEGIN(isolate);
  v8::HandleScope scope(isolate);

  onlyInClusterOrActiveFailover();

  if (args.Length() != 0) {
    TRI_V8_THROW_EXCEPTION_USAGE("version()");
  }

  TRI_GET_GLOBALS();
  AgencyComm comm(v8g->_server);
  std::string const version = comm.version();

  TRI_V8_RETURN_STD_STRING(version);
  TRI_V8_TRY_CATCH_END
}

////////////////////////////////////////////////////////////////////////////////
/// @brief whether or not a specific database exists
////////////////////////////////////////////////////////////////////////////////

static void JS_DoesDatabaseExistClusterInfo(v8::FunctionCallbackInfo<v8::Value> const& args) {
  TRI_V8_TRY_CATCH_BEGIN(isolate);
  v8::HandleScope scope(isolate);

  onlyInCluster();

  if (args.Length() != 1) {
    TRI_V8_THROW_EXCEPTION_USAGE("doesDatabaseExist(<database-id>)");
  }

  TRI_GET_GLOBALS();
  auto& ci = v8g->_server.getFeature<ClusterFeature>().clusterInfo();
  bool const result = ci.doesDatabaseExist(TRI_ObjectToString(isolate, args[0]), true);

  if (result) {
    TRI_V8_RETURN_TRUE();
  }
  TRI_V8_RETURN_FALSE();
  TRI_V8_TRY_CATCH_END
}

////////////////////////////////////////////////////////////////////////////////
/// @brief get the list of databases in the cluster
////////////////////////////////////////////////////////////////////////////////

static void JS_Databases(v8::FunctionCallbackInfo<v8::Value> const& args) {
  TRI_V8_TRY_CATCH_BEGIN(isolate);
  v8::HandleScope scope(isolate);
  auto context = TRI_IGETC;
  onlyInCluster();

  if (args.Length() != 0) {
    TRI_V8_THROW_EXCEPTION_USAGE("databases()");
  }

  TRI_GET_GLOBALS();
  auto& ci = v8g->_server.getFeature<ClusterFeature>().clusterInfo();
  std::vector<DatabaseID> res = ci.databases(false);
  v8::Handle<v8::Array> a = v8::Array::New(isolate, (int)res.size());
  std::vector<DatabaseID>::iterator it;
  int count = 0;
  for (it = res.begin(); it != res.end(); ++it) {
    a->Set(context, (uint32_t)count++, TRI_V8_STD_STRING(isolate, (*it))).FromMaybe(false);
  }
  TRI_V8_RETURN(a);
  TRI_V8_TRY_CATCH_END
}

////////////////////////////////////////////////////////////////////////////////
/// @brief flush the caches (used for testing only)
////////////////////////////////////////////////////////////////////////////////

static void JS_FlushClusterInfo(v8::FunctionCallbackInfo<v8::Value> const& args) {
  TRI_V8_TRY_CATCH_BEGIN(isolate);
  v8::HandleScope scope(isolate);

  onlyInCluster();

  if (args.Length() != 0) {
    TRI_V8_THROW_EXCEPTION_USAGE("flush()");
  }

  TRI_GET_GLOBALS();
  auto& ci = v8g->_server.getFeature<ClusterFeature>().clusterInfo();
  ci.flush();

  TRI_V8_RETURN_TRUE();
  TRI_V8_TRY_CATCH_END
}

////////////////////////////////////////////////////////////////////////////////
/// @brief get the info about a collection in Plan
////////////////////////////////////////////////////////////////////////////////

static void JS_GetCollectionInfoClusterInfo(v8::FunctionCallbackInfo<v8::Value> const& args) {
  TRI_V8_TRY_CATCH_BEGIN(isolate);
  v8::HandleScope scope(isolate);
  auto context = TRI_IGETC;

  onlyInCluster();

  if (args.Length() != 2) {
    TRI_V8_THROW_EXCEPTION_USAGE(
        "getCollectionInfo(<database-id>, <collection-id>)");
  }

  auto databaseID = TRI_ObjectToString(isolate, args[0]);
  auto collectionID = TRI_ObjectToString(isolate, args[1]);
  TRI_GET_GLOBALS();
  auto& ci = v8g->_server.getFeature<ClusterFeature>().clusterInfo();
  std::shared_ptr<LogicalCollection> col = ci.getCollectionNT(databaseID, collectionID);
  if (col == nullptr) {
    TRI_V8_THROW_EXCEPTION_MESSAGE(TRI_ERROR_ARANGO_DATA_SOURCE_NOT_FOUND,
                                   ClusterInfo::getCollectionNotFoundMsg(databaseID, collectionID));
  }

  std::unordered_set<std::string> ignoreKeys{"allowUserKeys",
                                             "avoidServers",
                                             "cid",
                                             "globallyUniqueId",
                                             "count",
                                             "distributeShardsLike",
                                             "indexBuckets",
                                             "keyOptions",
                                             "numberOfShards",
                                             "path",
                                             "planId",
                                             "version",
                                             "objectId"};
  VPackBuilder infoBuilder =
      col->toVelocyPackIgnore(ignoreKeys, LogicalDataSource::Serialization::List);
  VPackSlice info = infoBuilder.slice();

  TRI_ASSERT(info.isObject());
  v8::Handle<v8::Object> result =
      TRI_VPackToV8(isolate, info)->ToObject(TRI_IGETC).FromMaybe(v8::Local<v8::Object>());

  // Compute ShardShorts
  auto serverAliases = ci.getServerAliases();
  VPackSlice shards = info.get("shards");
  TRI_ASSERT(shards.isObject());
  v8::Handle<v8::Object> shardShorts = v8::Object::New(isolate);
  for (auto const& p : VPackObjectIterator(shards)) {
    TRI_ASSERT(p.value.isArray());
    v8::Handle<v8::Array> shorts = v8::Array::New(isolate);
    uint32_t pos = 0;
    for (VPackSlice s : VPackArrayIterator(p.value)) {
      try {
        std::string t = s.copyString();
        if (t.at(0) == '_') {
          t = t.substr(1);
        }
        shorts->Set(context, pos, TRI_V8_STD_STRING(isolate, serverAliases.at(t))).FromMaybe(false);
        pos++;
      } catch (...) {
      }
    }
    shardShorts->Set(context, TRI_V8_STD_STRING(isolate, p.key.copyString()), shorts).FromMaybe(false);
  }
  result->Set(context, TRI_V8_ASCII_STRING(isolate, "shardShorts"), shardShorts).FromMaybe(false);
  TRI_V8_RETURN(result);
  TRI_V8_TRY_CATCH_END
}

////////////////////////////////////////////////////////////////////////////////
/// @brief get the info about a collection in Current
////////////////////////////////////////////////////////////////////////////////

static void JS_GetCollectionInfoCurrentClusterInfo(v8::FunctionCallbackInfo<v8::Value> const& args) {
  TRI_V8_TRY_CATCH_BEGIN(isolate);
  v8::HandleScope scope(isolate);
  auto context = TRI_IGETC;

  onlyInCluster();

  if (args.Length() != 3) {
    TRI_V8_THROW_EXCEPTION_USAGE(
        "getCollectionInfoCurrent(<database-id>, <collection-id>, <shardID>)");
  }

  ShardID shardID = TRI_ObjectToString(isolate, args[2]);

  auto databaseID = TRI_ObjectToString(isolate, args[0]);
  auto collectionID = TRI_ObjectToString(isolate, args[1]);
  TRI_GET_GLOBALS();
  auto& ci = v8g->_server.getFeature<ClusterFeature>().clusterInfo();
  std::shared_ptr<LogicalCollection> col = ci.getCollectionNT(databaseID, collectionID);
  if (col == nullptr) {
    TRI_V8_THROW_EXCEPTION_MESSAGE(TRI_ERROR_ARANGO_DATA_SOURCE_NOT_FOUND,
                                   ClusterInfo::getCollectionNotFoundMsg(databaseID, collectionID));
  }

  v8::Handle<v8::Object> result = v8::Object::New(isolate);
  // First some stuff from Plan for which Current does not make sense:
  auto cid = std::to_string(col->id());
  std::string const& name = col->name();
  result->Set(context, TRI_V8_ASCII_STRING(isolate, "id"), TRI_V8_STD_STRING(isolate, cid)).FromMaybe(false);
  result->Set(context, TRI_V8_ASCII_STRING(isolate, "name"), TRI_V8_STD_STRING(isolate, name)).FromMaybe(false);

  std::shared_ptr<CollectionInfoCurrent> cic =
      ci.getCollectionCurrent(TRI_ObjectToString(isolate, args[0]), cid);

  result->Set(context,
              TRI_V8_ASCII_STRING(isolate, "currentVersion"),
              v8::Number::New(isolate, (double)cic->getCurrentVersion())).FromMaybe(false);
  result->Set(context,
              TRI_V8_ASCII_STRING(isolate, "type"),
              v8::Number::New(isolate, (int)col->type())).FromMaybe(false);

  VPackSlice slice = cic->getIndexes(shardID);
  v8::Handle<v8::Value> indexes = TRI_VPackToV8(isolate, slice);
  result->Set(context,
              TRI_V8_ASCII_STRING(isolate, "indexes"), indexes).FromMaybe(false);

  // Finally, report any possible error:
  bool error = cic->error(shardID);
  result->Set(context,
              TRI_V8_STD_STRING(isolate, StaticStrings::Error),
              v8::Boolean::New(isolate, error)).FromMaybe(false);
  if (error) {
    result->Set(context,
                TRI_V8_STD_STRING(isolate, StaticStrings::ErrorNum),
                v8::Number::New(isolate, cic->errorNum(shardID))).FromMaybe(false);
    std::string const errorMessage = cic->errorMessage(shardID);
    result->Set(context,
                TRI_V8_STD_STRING(isolate, StaticStrings::ErrorMessage),
                TRI_V8_STD_STRING(isolate, errorMessage)).FromMaybe(false);
  }
  auto servers = cic->servers(shardID);
  v8::Handle<v8::Array> list =
      v8::Array::New(isolate, static_cast<int>(servers.size()));
  v8::Handle<v8::Array> shorts = v8::Array::New(isolate);
  auto serverAliases = ci.getServerAliases();
  uint32_t pos = 0;
  for (auto const& s : servers) {
    try {
      shorts->Set(context,
                  pos,
                  TRI_V8_STD_STRING(isolate, serverAliases.at(s))).FromMaybe(false);
    } catch (...) {
    }
    list->Set(context, pos, TRI_V8_STD_STRING(isolate, s)).FromMaybe(false);
    pos++;
  }
  result->Set(context,
              TRI_V8_ASCII_STRING(isolate, "servers"), list).FromMaybe(false);
  result->Set(context,
              TRI_V8_ASCII_STRING(isolate, "shorts"), shorts).FromMaybe(false);

  servers = cic->failoverCandidates(shardID);
  list = v8::Array::New(isolate, static_cast<int>(servers.size()));
  pos = 0;
  for (auto const& s : servers) {
    list->Set(context, pos, TRI_V8_STD_STRING(isolate, s)).FromMaybe(false);
    pos++;
  }
  result->Set(context,
              TRI_V8_ASCII_STRING(isolate, "failoverCandidates"), list).FromMaybe(false);

  TRI_V8_RETURN(result);
  TRI_V8_TRY_CATCH_END
}

////////////////////////////////////////////////////////////////////////////////
/// @brief get the responsible server
////////////////////////////////////////////////////////////////////////////////

static void JS_GetResponsibleServerClusterInfo(v8::FunctionCallbackInfo<v8::Value> const& args) {
  TRI_V8_TRY_CATCH_BEGIN(isolate);
  v8::HandleScope scope(isolate);
  auto context = TRI_IGETC;

  onlyInCluster();

  if (args.Length() != 1) {
    TRI_V8_THROW_EXCEPTION_USAGE("getResponsibleServer(<shard-id>)");
  }

  TRI_GET_GLOBALS();
  auto& ci = v8g->_server.getFeature<ClusterFeature>().clusterInfo();
  auto result = ci.getResponsibleServer(TRI_ObjectToString(isolate, args[0]));
  v8::Handle<v8::Array> list = v8::Array::New(isolate, (int)result->size());
  uint32_t count = 0;
  for (auto const& s : *result) {
    list->Set(context, count++, TRI_V8_STD_STRING(isolate, s)).FromMaybe(true);
  }

  TRI_V8_RETURN(list);
  TRI_V8_TRY_CATCH_END
}

////////////////////////////////////////////////////////////////////////////////
/// @brief get the responsible server
////////////////////////////////////////////////////////////////////////////////

static void JS_GetResponsibleServersClusterInfo(v8::FunctionCallbackInfo<v8::Value> const& args) {
  TRI_V8_TRY_CATCH_BEGIN(isolate);
  v8::HandleScope scope(isolate);
  auto context = TRI_IGETC;

  onlyInCluster();

  if (args.Length() != 1 || !args[0]->IsArray()) {
    TRI_V8_THROW_EXCEPTION_USAGE("getResponsibleServers(<shard-ids>)");
  }

  std::unordered_set<std::string> shardIds;
  v8::Handle<v8::Array> array = v8::Handle<v8::Array>::Cast(args[0]);

  uint32_t const n = array->Length();
  for (uint32_t i = 0; i < n; ++i) {
    shardIds.emplace(TRI_ObjectToString(isolate, array->Get(context, i).FromMaybe(v8::Local<v8::Value>())));
  }

  if (shardIds.empty()) {
    TRI_V8_THROW_EXCEPTION_USAGE("getResponsibleServers(<shard-ids>)");
  }

  TRI_GET_GLOBALS();
  auto& ci = v8g->_server.getFeature<ClusterFeature>().clusterInfo();
  auto result = ci.getResponsibleServers(shardIds);

  v8::Handle<v8::Object> responsible = v8::Object::New(isolate);
  for (auto const& it : result) {
    responsible->Set(context,
                     TRI_V8_ASCII_STRING(isolate, it.first.data()),
                     TRI_V8_STD_STRING(isolate, it.second)).FromMaybe(false);
  }

  TRI_V8_RETURN(responsible);
  TRI_V8_TRY_CATCH_END
}

////////////////////////////////////////////////////////////////////////////////
/// @brief get the responsible shard
////////////////////////////////////////////////////////////////////////////////

static void JS_GetResponsibleShardClusterInfo(v8::FunctionCallbackInfo<v8::Value> const& args) {
  TRI_V8_TRY_CATCH_BEGIN(isolate);
  v8::HandleScope scope(isolate);
  auto context = TRI_IGETC;

  onlyInCluster();

  if (args.Length() < 2 || args.Length() > 3) {
    TRI_V8_THROW_EXCEPTION_USAGE(
        "getResponsibleShard(<collection-id>, <document>, "
        "<documentIsComplete>)");
  }

  if (!args[0]->IsString() && !args[0]->IsStringObject()) {
    TRI_V8_THROW_TYPE_ERROR("expecting a string for <collection-id>)");
  }

  if (!args[1]->IsObject()) {
    TRI_V8_THROW_TYPE_ERROR("expecting an object for <document>)");
  }

  bool documentIsComplete = true;
  if (args.Length() > 2) {
    documentIsComplete = TRI_ObjectToBoolean(isolate, args[2]);
  }

  VPackBuilder builder;
  int res = TRI_V8ToVPack(isolate, builder, args[1], false);

  if (res != TRI_ERROR_NO_ERROR) {
    TRI_V8_THROW_EXCEPTION(res);
  }

  ShardID shardId;
  CollectionID collectionId = TRI_ObjectToString(isolate, args[0]);
  auto& vocbase = GetContextVocBase(isolate);
  TRI_GET_GLOBALS();
  auto& ci = v8g->_server.getFeature<ClusterFeature>().clusterInfo();
  auto collInfo = ci.getCollectionNT(vocbase.name(), collectionId);
  if (collInfo == nullptr) {
    TRI_V8_THROW_EXCEPTION_MESSAGE(TRI_ERROR_ARANGO_DATA_SOURCE_NOT_FOUND,
                                   ClusterInfo::getCollectionNotFoundMsg(vocbase.name(), collectionId));
  }

  bool usesDefaultShardingAttributes;

  res = collInfo->getResponsibleShard(builder.slice(), documentIsComplete,
                                      shardId, usesDefaultShardingAttributes);

  if (res != TRI_ERROR_NO_ERROR) {
    TRI_V8_THROW_EXCEPTION(res);
  }

  v8::Handle<v8::Object> result = v8::Object::New(isolate);
  result->Set(context,
              TRI_V8_ASCII_STRING(isolate, "shardId"),
              TRI_V8_STD_STRING(isolate, shardId)).FromMaybe(true);
  result->Set(context,
              TRI_V8_ASCII_STRING(isolate, "usesDefaultShardingAttributes"),
              v8::Boolean::New(isolate, usesDefaultShardingAttributes)).FromMaybe(true);

  TRI_V8_RETURN(result);
  TRI_V8_TRY_CATCH_END
}

////////////////////////////////////////////////////////////////////////////////
/// @brief get the server endpoint for a server
////////////////////////////////////////////////////////////////////////////////

static void JS_GetServerEndpointClusterInfo(v8::FunctionCallbackInfo<v8::Value> const& args) {
  TRI_V8_TRY_CATCH_BEGIN(isolate);
  v8::HandleScope scope(isolate);

  onlyInCluster();

  if (args.Length() != 1) {
    TRI_V8_THROW_EXCEPTION_USAGE("getServerEndpoint(<server-id>)");
  }

  TRI_GET_GLOBALS();
  auto& ci = v8g->_server.getFeature<ClusterFeature>().clusterInfo();
  std::string const result = ci.getServerEndpoint(TRI_ObjectToString(isolate, args[0]));

  TRI_V8_RETURN_STD_STRING(result);
  TRI_V8_TRY_CATCH_END
}

////////////////////////////////////////////////////////////////////////////////
/// @brief get the server name for an endpoint
////////////////////////////////////////////////////////////////////////////////

static void JS_GetServerNameClusterInfo(v8::FunctionCallbackInfo<v8::Value> const& args) {
  TRI_V8_TRY_CATCH_BEGIN(isolate);
  v8::HandleScope scope(isolate);

  onlyInCluster();

  if (args.Length() != 1) {
    TRI_V8_THROW_EXCEPTION_USAGE("getServerName(<endpoint>)");
  }

  TRI_GET_GLOBALS();
  auto& ci = v8g->_server.getFeature<ClusterFeature>().clusterInfo();
  std::string const result = ci.getServerName(TRI_ObjectToString(isolate, args[0]));

  TRI_V8_RETURN_STD_STRING(result);
  TRI_V8_TRY_CATCH_END
}

////////////////////////////////////////////////////////////////////////////////
/// @brief returns the DBServers currently registered
////////////////////////////////////////////////////////////////////////////////

static void JS_GetDBServers(v8::FunctionCallbackInfo<v8::Value> const& args) {
  TRI_V8_TRY_CATCH_BEGIN(isolate);
  v8::HandleScope scope(isolate);
  auto context = TRI_IGETC;

  onlyInCluster();

  if (args.Length() != 0) {
    TRI_V8_THROW_EXCEPTION_USAGE("getDBServers()");
  }

  TRI_GET_GLOBALS();
  auto& ci = v8g->_server.getFeature<ClusterFeature>().clusterInfo();
  auto DBServers = ci.getCurrentDBServers();
  auto serverAliases = ci.getServerAliases();

  v8::Handle<v8::Array> l = v8::Array::New(isolate);

  for (size_t i = 0; i < DBServers.size(); ++i) {
    v8::Handle<v8::Object> result = v8::Object::New(isolate);
    auto id = DBServers[i];

    result->Set(context,
                TRI_V8_ASCII_STRING(isolate, "serverId"),
                TRI_V8_STD_STRING(isolate, id)).FromMaybe(false);

    auto itr = serverAliases.find(id);

    if (itr != serverAliases.end()) {
      result->Set(context,
                  TRI_V8_ASCII_STRING(isolate, "serverName"),
                  TRI_V8_STD_STRING(isolate, itr->second)).FromMaybe(false);
    } else {
      result->Set(context,
                  TRI_V8_ASCII_STRING(isolate, "serverName"),
                  TRI_V8_STD_STRING(isolate, id)).FromMaybe(false);
    }

    l->Set(context, (uint32_t)i, result).FromMaybe(false);
  }

  TRI_V8_RETURN(l);
  TRI_V8_TRY_CATCH_END
}

////////////////////////////////////////////////////////////////////////////////
/// @brief returns the coordinators currently registered
////////////////////////////////////////////////////////////////////////////////

static void JS_GetCoordinators(v8::FunctionCallbackInfo<v8::Value> const& args) {
  TRI_V8_TRY_CATCH_BEGIN(isolate);
  v8::HandleScope scope(isolate);
  auto context = TRI_IGETC;

  onlyInCluster();

  if (args.Length() != 0) {
    TRI_V8_THROW_EXCEPTION_USAGE("getCoordinators()");
  }

  TRI_GET_GLOBALS();
  auto& ci = v8g->_server.getFeature<ClusterFeature>().clusterInfo();
  std::vector<std::string> coordinators = ci.getCurrentCoordinators();

  v8::Handle<v8::Array> l = v8::Array::New(isolate);

  for (size_t i = 0; i < coordinators.size(); ++i) {
    ServerID const sid = coordinators[i];

    l->Set(context, (uint32_t)i, TRI_V8_STD_STRING(isolate, sid)).FromMaybe(false);
  }

  TRI_V8_RETURN(l);
  TRI_V8_TRY_CATCH_END
}

////////////////////////////////////////////////////////////////////////////////
/// @brief returns a unique id
////////////////////////////////////////////////////////////////////////////////

static void JS_UniqidClusterInfo(v8::FunctionCallbackInfo<v8::Value> const& args) {
  TRI_V8_TRY_CATCH_BEGIN(isolate);
  v8::HandleScope scope(isolate);

  if (args.Length() > 1) {
    TRI_V8_THROW_EXCEPTION_USAGE("uniqid(<count>)");
  }

  uint64_t count = 1;
  if (args.Length() > 0) {
    count = TRI_ObjectToUInt64(isolate, args[0], true);
  }

  if (count == 0) {
    TRI_V8_THROW_EXCEPTION_PARAMETER("<count> is invalid");
  }

  TRI_GET_GLOBALS();
  auto& ci = v8g->_server.getFeature<ClusterFeature>().clusterInfo();
  uint64_t value = ci.uniqid(count);

  if (value == 0) {
    TRI_V8_THROW_EXCEPTION_MESSAGE(TRI_ERROR_INTERNAL,
                                   "unable to generate unique id");
  }

  std::string const id = StringUtils::itoa(value);

  TRI_V8_RETURN_STD_STRING(id);
  TRI_V8_TRY_CATCH_END
}

////////////////////////////////////////////////////////////////////////////////
/// @brief return the servers address
////////////////////////////////////////////////////////////////////////////////

static void JS_AddressServerState(v8::FunctionCallbackInfo<v8::Value> const& args) {
  TRI_V8_TRY_CATCH_BEGIN(isolate);
  v8::HandleScope scope(isolate);

  if (args.Length() != 0) {
    TRI_V8_THROW_EXCEPTION_USAGE("address()");
  }

  std::string const address = ServerState::instance()->getEndpoint();
  TRI_V8_RETURN_STD_STRING(address);
  TRI_V8_TRY_CATCH_END
}

////////////////////////////////////////////////////////////////////////////////
/// @brief return the servers id
////////////////////////////////////////////////////////////////////////////////

static void JS_IdServerState(v8::FunctionCallbackInfo<v8::Value> const& args) {
  TRI_V8_TRY_CATCH_BEGIN(isolate);
  v8::HandleScope scope(isolate);

  onlyInClusterOrActiveFailover();

  if (args.Length() != 0) {
    TRI_V8_THROW_EXCEPTION_USAGE("id()");
  }

  std::string const id = ServerState::instance()->getId();
  TRI_V8_RETURN_STD_STRING(id);
  TRI_V8_TRY_CATCH_END
}

static void JS_isFoxxmaster(v8::FunctionCallbackInfo<v8::Value> const& args) {
  TRI_V8_TRY_CATCH_BEGIN(isolate);
  v8::HandleScope scope(isolate);

  if (args.Length() != 0) {
    TRI_V8_THROW_EXCEPTION_USAGE("isFoxxmaster()");
  }

  if (ServerState::instance()->isFoxxmaster()) {
    TRI_V8_RETURN_TRUE();
  } else {
    TRI_V8_RETURN_FALSE();
  }
  TRI_V8_TRY_CATCH_END
}

static void JS_getFoxxmasterQueueupdate(v8::FunctionCallbackInfo<v8::Value> const& args) {
  TRI_V8_TRY_CATCH_BEGIN(isolate);
  v8::HandleScope scope(isolate);

  if (args.Length() != 0) {
    TRI_V8_THROW_EXCEPTION_USAGE("getFoxxmasterQueueupdate()");
  }

  if (ServerState::instance()->getFoxxmasterQueueupdate()) {
    TRI_V8_RETURN_TRUE();
  } else {
    TRI_V8_RETURN_FALSE();
  }
  TRI_V8_TRY_CATCH_END
}

static void JS_setFoxxmasterQueueupdate(v8::FunctionCallbackInfo<v8::Value> const& args) {
  TRI_V8_TRY_CATCH_BEGIN(isolate);
  v8::HandleScope scope(isolate);

  if (args.Length() != 1) {
    TRI_V8_THROW_EXCEPTION_USAGE("setFoxxmasterQueueupdate(<value>)");
  }
      
  bool value = TRI_ObjectToBoolean(isolate, args[0]);

  ServerState::instance()->setFoxxmasterQueueupdate(value);

  if (AsyncAgencyCommManager::isEnabled()) {
    TRI_GET_GLOBALS();
    AgencyComm comm(v8g->_server);
    std::string key = "Current/FoxxmasterQueueupdate";
    VPackSlice val = value ? VPackSlice::trueSlice() : VPackSlice::falseSlice();
    AgencyCommResult result = comm.setValue(key, val, 0.0);
    if (result.successful()) {
      result = comm.increment("Current/Version");
    }
    if (!result.successful() && result.errorCode() != TRI_ERROR_SHUTTING_DOWN &&
        !v8g->_server.isStopping()) {
      // gracefully ignore any shutdown errors here
      THROW_AGENCY_EXCEPTION(result);
    }
  }

  TRI_V8_TRY_CATCH_END
}

static void JS_GetFoxxmasterSince(v8::FunctionCallbackInfo<v8::Value> const& args) {
  TRI_V8_TRY_CATCH_BEGIN(isolate);
  v8::HandleScope scope(isolate);

  if (args.Length() != 0) {
    TRI_V8_THROW_EXCEPTION_USAGE("getFoxxmasterSince()");
  }

  // Ticks can be up to 56 bits, but JS numbers can represent ints up to
  // 53 bits only.
  std::string const foxxmasterSince =
      std::to_string(ServerState::instance()->getFoxxmasterSince());
  TRI_V8_RETURN_STD_STRING(foxxmasterSince);

  TRI_V8_TRY_CATCH_END
}

////////////////////////////////////////////////////////////////////////////////
/// @brief return whether the cluster is initialized
////////////////////////////////////////////////////////////////////////////////

static void JS_InitializedServerState(v8::FunctionCallbackInfo<v8::Value> const& args) {
  TRI_V8_TRY_CATCH_BEGIN(isolate);
  v8::HandleScope scope(isolate);

  if (args.Length() != 0) {
    TRI_V8_THROW_EXCEPTION_USAGE("initialized()");
  }

  if (ServerState::instance()->initialized()) {
    TRI_V8_RETURN_TRUE();
  }
  TRI_V8_RETURN_FALSE();
  TRI_V8_TRY_CATCH_END
}

////////////////////////////////////////////////////////////////////////////////
/// @brief whether or not the server is a coordinator
////////////////////////////////////////////////////////////////////////////////

static void JS_IsCoordinatorServerState(v8::FunctionCallbackInfo<v8::Value> const& args) {
  TRI_V8_TRY_CATCH_BEGIN(isolate);
  v8::HandleScope scope(isolate);

  if (args.Length() != 0) {
    TRI_V8_THROW_EXCEPTION_USAGE("isCoordinator()");
  }

  if (ServerState::instance()->getRole() == ServerState::ROLE_COORDINATOR) {
    TRI_V8_RETURN_TRUE();
  }
  TRI_V8_RETURN_FALSE();
  TRI_V8_TRY_CATCH_END
}

////////////////////////////////////////////////////////////////////////////////
/// @brief returns the server role
////////////////////////////////////////////////////////////////////////////////

static void JS_RoleServerState(v8::FunctionCallbackInfo<v8::Value> const& args) {
  TRI_V8_TRY_CATCH_BEGIN(isolate);
  v8::HandleScope scope(isolate);

  if (args.Length() != 0) {
    TRI_V8_THROW_EXCEPTION_USAGE("role()");
  }

  std::string const role = ServerState::roleToString(ServerState::instance()->getRole());

  TRI_V8_RETURN_STD_STRING(role);
  TRI_V8_TRY_CATCH_END
}

////////////////////////////////////////////////////////////////////////////////
/// @brief sets the server role (used for testing)
////////////////////////////////////////////////////////////////////////////////

static void JS_SetRoleServerState(v8::FunctionCallbackInfo<v8::Value> const& args) {
  TRI_V8_TRY_CATCH_BEGIN(isolate);
  v8::HandleScope scope(isolate);

  if (args.Length() != 1) {
    TRI_V8_THROW_EXCEPTION_USAGE("setRole(<role>)");
  }

  std::string const role = TRI_ObjectToString(isolate, args[0]);
  ServerState::RoleEnum r = ServerState::stringToRole(role);

  if (r == ServerState::ROLE_UNDEFINED) {
    TRI_V8_THROW_EXCEPTION_PARAMETER("<role> is invalid");
  }

  ServerState::instance()->setRole(r);

  TRI_V8_RETURN_TRUE();
  TRI_V8_TRY_CATCH_END
}

////////////////////////////////////////////////////////////////////////////////
/// @brief returns the server state
////////////////////////////////////////////////////////////////////////////////

static void JS_StatusServerState(v8::FunctionCallbackInfo<v8::Value> const& args) {
  TRI_V8_TRY_CATCH_BEGIN(isolate);
  v8::HandleScope scope(isolate);

  if (args.Length() != 0) {
    TRI_V8_THROW_EXCEPTION_USAGE("status()");
  }

  std::string const state =
      ServerState::stateToString(ServerState::instance()->getState());

  TRI_V8_RETURN_STD_STRING(state);
  TRI_V8_TRY_CATCH_END
}

typedef TRI_voc_tid_t CoordTransactionID;
typedef TRI_voc_tid_t OperationID;
namespace {
struct AsyncRequest {
  network::Response response;
  std::string destination;
  CoordTransactionID coordTransactionID;
  OperationID operationID;
  bool done = false;
};

static std::mutex _requestMutex;
std::condition_variable _requestCV;
static std::vector<std::shared_ptr<AsyncRequest>> _requests;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief prepare to send a request
///
/// this is used for asynchronous as well as synchronous requests.
////////////////////////////////////////////////////////////////////////////////

static void PrepareClusterCommRequest(v8::FunctionCallbackInfo<v8::Value> const& args,
                                      fuerte::RestVerb& reqType,
                                      std::string& destination,
                                      std::string& dbname,
                                      std::string& path, VPackBufferUInt8& body,
                                      std::unordered_map<std::string, std::string>& headerFields,
                                      CoordTransactionID& coordTransactionID, double& timeout,
                                      double& initTimeout) {
  v8::Isolate* isolate = args.GetIsolate();
  TRI_V8_CURRENT_GLOBALS_AND_SCOPE;
  auto context = TRI_IGETC;

  onlyInClusterOrActiveFailover();

  TRI_ASSERT(args.Length() >= 4);

  reqType = fuerte::RestVerb::Get;
  if (args[0]->IsString()) {
    TRI_Utf8ValueNFC UTF8(isolate, args[0]);
    std::string methstring = *UTF8;
    StringUtils::toupperInPlace(methstring);
    reqType = fuerte::from_string(methstring);
    if (reqType == fuerte::RestVerb::Illegal) {
      reqType = fuerte::RestVerb::Get;
    }
  }

  destination = TRI_ObjectToString(isolate, args[1]);
  dbname = TRI_ObjectToString(isolate, args[2]);
  path = TRI_ObjectToString(isolate, args[3]);

  body.clear();
  if (!args[4]->IsUndefined()) {
    if (args[4]->IsObject() && V8Buffer::hasInstance(isolate, args[4])) {
      // supplied body is a Buffer object
      char const* data = V8Buffer::data(isolate, args[4].As<v8::Object>());
      size_t size = V8Buffer::length(isolate, args[4].As<v8::Object>());

      if (data == nullptr) {
        TRI_V8_THROW_EXCEPTION_MESSAGE(TRI_ERROR_BAD_PARAMETER,
                                       "invalid <body> buffer value");
      }

      body.append(reinterpret_cast<uint8_t const*>(data), size);
    } else {
      auto str = TRI_ObjectToString(isolate, args[4]);
      body.append(reinterpret_cast<uint8_t const*>(str.data()), str.length());
    }
  }

  if (args.Length() > 5 && args[5]->IsObject()) {
    v8::Handle<v8::Object> obj = args[5].As<v8::Object>();
    v8::Handle<v8::Array> props = obj->GetOwnPropertyNames(context).FromMaybe(v8::Local<v8::Array>());
    uint32_t i;
    for (i = 0; i < props->Length(); ++i) {
      v8::Handle<v8::Value> prop = props->Get(context, i).FromMaybe(v8::Handle<v8::Value>());
      v8::Handle<v8::Value> val = obj->Get(context, prop).FromMaybe(v8::Handle<v8::Value>());
      std::string propstring = TRI_ObjectToString(isolate, prop);
      std::string valstring = TRI_ObjectToString(isolate, val);
      if (propstring != "") {
        headerFields.insert(std::make_pair(propstring, valstring));
      }
    }
  }

  coordTransactionID = 0;
  timeout = 24 * 3600.0;

  if (args.Length() > 6 && args[6]->IsObject()) {
    v8::Handle<v8::Object> opt = args[6].As<v8::Object>();
    TRI_GET_GLOBAL_STRING(CoordTransactionIDKey);
    if (TRI_HasProperty(context, isolate, opt, CoordTransactionIDKey)) {
      coordTransactionID =
        TRI_ObjectToUInt64(isolate, opt->Get(context, CoordTransactionIDKey).FromMaybe(v8::Handle<v8::Value>()), true);
    }
    TRI_GET_GLOBAL_STRING(TimeoutKey);
    if (TRI_HasProperty(context, isolate, opt, TimeoutKey)) {
      timeout = TRI_ObjectToDouble(isolate, opt->Get(context, TimeoutKey).FromMaybe(v8::Handle<v8::Value>()));
    }
    TRI_GET_GLOBAL_STRING(InitTimeoutKey);
    if (TRI_HasProperty(context, isolate, opt, InitTimeoutKey)) {
      initTimeout = TRI_ObjectToDouble(isolate, opt->Get(context, InitTimeoutKey).FromMaybe(v8::Handle<v8::Value>()));
    }
  }
  if (coordTransactionID == 0) {
    coordTransactionID = TRI_NewTickServer();
  }
  if (timeout == 0) {
    timeout = 24 * 3600.0;
  }
}

////////////////////////////////////////////////////////////////////////////////
/// @brief prepare a ClusterCommResult for JavaScript
////////////////////////////////////////////////////////////////////////////////

static void Return_PrepareClusterCommResultForJS(v8::FunctionCallbackInfo<v8::Value> const& args,
                                                 ::AsyncRequest& res) {
  v8::Isolate* isolate = args.GetIsolate();
  TRI_V8_CURRENT_GLOBALS_AND_SCOPE;
  auto context = TRI_IGETC;

  v8::Handle<v8::Object> r = v8::Object::New(isolate);
    // convert the ids to strings as uint64_t might be too big for JavaScript
    // numbers
    TRI_GET_GLOBAL_STRING(CoordTransactionIDKey);
    std::string id = StringUtils::itoa(res.coordTransactionID);
    r->Set(context, CoordTransactionIDKey, TRI_V8_STD_STRING(isolate, id)).FromMaybe(false);

    id = StringUtils::itoa(res.operationID);
    TRI_GET_GLOBAL_STRING(OperationIDKey);
    r->Set(context, OperationIDKey, TRI_V8_STD_STRING(isolate, id)).FromMaybe(false);
    TRI_GET_GLOBAL_STRING(EndpointKey);

    r->Set(context, EndpointKey, TRI_V8_STD_STRING(isolate, res.destination)).FromMaybe(false);
    TRI_GET_GLOBAL_STRING(SingleRequestKey);
    r->Set(context, SingleRequestKey, v8::Boolean::New(isolate, false)).FromMaybe(false);
    TRI_GET_GLOBAL_STRING(ShardIDKey);
    r->Set(context, ShardIDKey, TRI_V8_STD_STRING(isolate, res.destination.substr(8))).FromMaybe(false);

    if (!res.done) {
      TRI_GET_GLOBAL_STRING(StatusKey);
      r->Set(context, StatusKey, TRI_V8_ASCII_STRING(isolate, "SENDING")).FromMaybe(false);

      TRI_V8_RETURN(r);
    }
    
    network::Response const& response = res.response;

    if (response.ok()) {
      {
        // The headers:
        v8::Handle<v8::Object> h = v8::Object::New(isolate);
        TRI_GET_GLOBAL_STRING(StatusKey);
        r->Set(context, StatusKey, TRI_V8_ASCII_STRING(isolate, "RECEIVED")).FromMaybe(false);

        auto headers = response.response->header.meta();
        headers[StaticStrings::ContentLength] = StringUtils::itoa(response.response->payloadSize());
        for (auto& it : headers) {
          h->Set(context, TRI_V8_STD_STRING(isolate, it.first), TRI_V8_STD_STRING(isolate, it.second)).FromMaybe(false);
        }
        r->Set(context, TRI_V8_ASCII_STRING(isolate, "headers"), h).FromMaybe(false);
      }
      TRI_GET_GLOBAL_STRING(CodeKey);
      r->Set(context, CodeKey, v8::Number::New(isolate, response.statusCode())).FromMaybe(false);
    
      std::string json;
      if (response.response->isContentTypeVPack()) {
        json = response.response->slice().toJson();
      } else if (response.response->isContentTypeJSON()) {
        auto raw = response.response->payload();
        json.append(reinterpret_cast<const char*>(raw.data()), raw.size());
      }
      if (json.size() > 0) {
        r->Set(context, TRI_V8_ASCII_STRING(isolate, "body"), TRI_V8_ASCII_PAIR_STRING(isolate, json.data(), json.size())).FromMaybe(false);
        V8Buffer* buffer = V8Buffer::New(isolate, json.data(), json.size());
        v8::Local<v8::Object> bufferObject =
            v8::Local<v8::Object>::New(isolate, buffer->_handle);
        r->Set(context, TRI_V8_ASCII_STRING(isolate, "rawBody"), bufferObject).FromMaybe(false);
      }
    } else if (response.error == fuerte::Error::Timeout) {
      TRI_GET_GLOBAL_STRING(StatusKey);
      r->Set(context, StatusKey, TRI_V8_ASCII_STRING(isolate, "TIMEOUT")).FromMaybe(false);
      TRI_GET_GLOBAL_STRING(TimeoutKey);
      r->Set(context, TimeoutKey, v8::BooleanObject::New(isolate, true)).FromMaybe(false);
    } else if (response.error == fuerte::Error::CouldNotConnect) {
      TRI_GET_GLOBAL_STRING(StatusKey);
      r->Set(context, StatusKey, TRI_V8_ASCII_STRING(isolate, "BACKEND_UNAVAILABLE")).FromMaybe(false);
      TRI_GET_GLOBAL_STRING(ErrorMessageKey);
      r->Set(context,
             ErrorMessageKey,
             TRI_V8_ASCII_STRING(isolate,
                                 "required backend was not available")).FromMaybe(false);
    } else {
      TRI_GET_GLOBAL_STRING(StatusKey);
      r->Set(context, StatusKey, TRI_V8_ASCII_STRING(isolate, "ERROR")).FromMaybe(false);
    }
  TRI_V8_RETURN(r);
}

////////////////////////////////////////////////////////////////////////////////
/// @brief send an asynchronous request
////////////////////////////////////////////////////////////////////////////////

static void JS_AsyncRequest(v8::FunctionCallbackInfo<v8::Value> const& args) {
  TRI_V8_TRY_CATCH_BEGIN(isolate);
  v8::HandleScope scope(isolate);

  onlyInClusterOrActiveFailover();

  if (args.Length() < 4 || args.Length() > 7) {
    TRI_V8_THROW_EXCEPTION_USAGE(
        "asyncRequest("
        "reqType, destination, dbname, path, body, headers, options)");
  }
  // Possible options:
  //   - coordTransactionID   (number)
  //   - timeout              (number)
  //   - initTimeout          (number)

  TRI_GET_GLOBALS();

  auto* pool = v8g->_server.getFeature<arangodb::NetworkFeature>().pool();

  if (pool == nullptr) {
    TRI_V8_THROW_EXCEPTION_MESSAGE(
        TRI_ERROR_SHUTTING_DOWN,
        "connectionpool object not found (JS_AsyncRequest)");
  }

  fuerte::RestVerb reqType;
  std::string destination;
  std::string dbname;
  std::string path;
  VPackBufferUInt8 body;
  std::unordered_map<std::string, std::string> headerFields;
  CoordTransactionID coordTransactionID = 0;
  double timeout = 0.0;
  double initTimeout = -1.0;

  PrepareClusterCommRequest(args, reqType, destination, dbname, path, body, headerFields,
                            coordTransactionID, timeout, initTimeout);
  
  network::RequestOptions reqOpts;
  reqOpts.database = dbname;
  reqOpts.retryNotFound = false;
  reqOpts.timeout = network::Timeout(timeout);
  reqOpts.skipScheduler = true;
  reqOpts.contentType = StaticStrings::MimeTypeJsonNoEncoding;
  
  OperationID opId = TRI_NewTickServer();
  auto ar = std::make_shared<::AsyncRequest>();
  ar->destination = destination;
  ar->operationID = opId;
  ar->coordTransactionID = coordTransactionID;
  
  network::sendRequest(pool, destination, reqType, path, std::move(body), reqOpts)
  .thenValue([ar] (network::Response&& r) {
    {
      std::lock_guard<std::mutex> guard(::_requestMutex);
      ar->response = std::move(r);
      ar->done = true;
    }
    ::_requestCV.notify_all();
  });
  {
    std::lock_guard<std::mutex> guard(::_requestMutex);
    ::_requests.push_back(ar);
    Return_PrepareClusterCommResultForJS(args, *ar);
  }
  ::_requestCV.notify_all();

  LOG_TOPIC("cea85", DEBUG, Logger::CLUSTER)
      << "JS_AsyncRequest: request has been submitted";

  TRI_V8_TRY_CATCH_END
}

////////////////////////////////////////////////////////////////////////////////
/// @brief enquire information about an asynchronous request
////////////////////////////////////////////////////////////////////////////////

static void JS_Enquire(v8::FunctionCallbackInfo<v8::Value> const& args) {
  TRI_V8_TRY_CATCH_BEGIN(isolate);
  TRI_V8_CURRENT_GLOBALS_AND_SCOPE;
  auto context = TRI_IGETC;
  onlyInCluster();

  if (args.Length() != 1) {
    TRI_V8_THROW_EXCEPTION_USAGE("enquire(operationID)");
  }

  OperationID operationID = TRI_ObjectToUInt64(isolate, args[0], true);

  LOG_TOPIC("77dbd", DEBUG, Logger::CLUSTER)
      << "JS_Enquire: calling ClusterComm::enquire()";
  
  {
    std::lock_guard<std::mutex> guard(::_requestMutex);
    auto it = ::_requests.begin();
    while (it != _requests.end()) {
      std::shared_ptr<::AsyncRequest> req = *it;
      if (req->operationID == operationID) {
        Return_PrepareClusterCommResultForJS(args, *req);
        return;
      }
      it++;
    }
  }

  v8::Handle<v8::Object> r = v8::Object::New(isolate);
  TRI_GET_GLOBAL_STRING(ErrorMessageKey);
  r->Set(context,
         ErrorMessageKey,
         TRI_V8_ASCII_STRING(isolate, "operation was dropped")).FromMaybe(false);
  TRI_V8_RETURN(r);
  
  TRI_V8_TRY_CATCH_END
}

////////////////////////////////////////////////////////////////////////////////
/// @brief wait for the result of an asynchronous request
////////////////////////////////////////////////////////////////////////////////

static void JS_Wait(v8::FunctionCallbackInfo<v8::Value> const& args) {
  TRI_V8_TRY_CATCH_BEGIN(isolate);
  TRI_V8_CURRENT_GLOBALS_AND_SCOPE;
  auto context = TRI_IGETC;

  onlyInClusterOrActiveFailover();

  if (args.Length() != 1) {
    TRI_V8_THROW_EXCEPTION_USAGE("wait(obj)");
  }
  // Possible options:
  //   - clientTransactionID  (string)
  //   - coordTransactionID   (number)
  //   - operationID          (number)
  //   - shardID              (string)
  //   - timeout              (number)


  CoordTransactionID mycoordTransactionID = 0;
  OperationID myoperationID = 0;
  ShardID myshardID = "";
  double mytimeout = 24 * 3600.0;

  if (args[0]->IsObject()) {
    v8::Handle<v8::Object> obj = args[0].As<v8::Object>();
    TRI_GET_GLOBAL_STRING(CoordTransactionIDKey);
    if (TRI_HasProperty(context, isolate, obj, CoordTransactionIDKey)) {
      mycoordTransactionID =
        TRI_ObjectToUInt64(isolate, obj->Get(context, CoordTransactionIDKey).FromMaybe(v8::Handle<v8::Value>()), true);
    }
    TRI_GET_GLOBAL_STRING(OperationIDKey);
    if (TRI_HasProperty(context, isolate, obj, OperationIDKey)) {
      myoperationID = TRI_ObjectToUInt64(isolate, obj->Get(context, OperationIDKey).FromMaybe(v8::Handle<v8::Value>()), true);
    }
    TRI_GET_GLOBAL_STRING(ShardIDKey);
    if (TRI_HasProperty(context, isolate, obj, ShardIDKey)) {
      myshardID = TRI_ObjectToString(isolate, obj->Get(context, ShardIDKey).FromMaybe(v8::Handle<v8::Value>()));
    }
    TRI_GET_GLOBAL_STRING(TimeoutKey);
    if (TRI_HasProperty(context, isolate, obj, TimeoutKey)) {
      mytimeout = TRI_ObjectToDouble(isolate, obj->Get(context, TimeoutKey).FromMaybe(v8::Handle<v8::Value>()));
      if (mytimeout == 0.0) {
        mytimeout = 24 * 3600.0;
      }
    }
  }
  
  auto end = std::chrono::steady_clock::now() + std::chrono::duration<double>(mytimeout);
  
  while (end > std::chrono::steady_clock::now()) {
    std::unique_lock<std::mutex> guard(::_requestMutex);
    auto it = ::_requests.begin();
    while (it != _requests.end()) {
      std::shared_ptr<::AsyncRequest> req = *it;
      if (req->coordTransactionID == mycoordTransactionID ||
          req->operationID == myoperationID) {
        if (req->done) {
          Return_PrepareClusterCommResultForJS(args, *req);
          _requests.erase(it);
          return;
        }
      }
      it++;
    }
    auto duration = (end - std::chrono::steady_clock::now());
    ::_requestCV.wait_for(guard, duration);
  }

  LOG_TOPIC("04f61", DEBUG, Logger::CLUSTER) << "JS_Wait: calling ClusterComm::wait()";

  v8::Handle<v8::Object> r = v8::Object::New(isolate);
  TRI_GET_GLOBAL_STRING(ErrorMessageKey);
  r->Set(context,
         ErrorMessageKey,
         TRI_V8_ASCII_STRING(isolate, "operation was dropped")).FromMaybe(false);
  TRI_V8_RETURN(r);
    
  TRI_V8_TRY_CATCH_END
}

////////////////////////////////////////////////////////////////////////////////
/// @brief drop the result of an asynchronous request
////////////////////////////////////////////////////////////////////////////////

static void JS_Drop(v8::FunctionCallbackInfo<v8::Value> const& args) {
  TRI_V8_TRY_CATCH_BEGIN(isolate);
  TRI_V8_CURRENT_GLOBALS_AND_SCOPE;
  auto context = TRI_IGETC;

  onlyInCluster();

  if (args.Length() != 1) {
    TRI_V8_THROW_EXCEPTION_USAGE("drop(obj)");
  }
  // Possible options:
  //   - coordTransactionID   (number)
  //   - operationID          (number)
  //   - shardID              (string)

  CoordTransactionID mycoordTransactionID = 0;
  OperationID myoperationID = 0;
  ShardID myshardID = "";

  if (args[0]->IsObject()) {
    v8::Handle<v8::Object> obj = args[0].As<v8::Object>();
    TRI_GET_GLOBAL_STRING(CoordTransactionIDKey);
    if (TRI_HasProperty(context, isolate, obj, CoordTransactionIDKey)) {
      mycoordTransactionID =
        TRI_ObjectToUInt64(isolate, obj->Get(context, CoordTransactionIDKey).FromMaybe(v8::Handle<v8::Value>()), true);
    }
    TRI_GET_GLOBAL_STRING(OperationIDKey);
    if (TRI_HasProperty(context, isolate, obj, OperationIDKey)) {
      myoperationID = TRI_ObjectToUInt64(isolate,
                                         obj->Get(context,
                                                  OperationIDKey).FromMaybe(v8::Handle<v8::Value>()),
                                         true);
    }
    TRI_GET_GLOBAL_STRING(ShardIDKey);
    if (TRI_HasProperty(context, isolate, obj, ShardIDKey)) {
      myshardID = TRI_ObjectToString(isolate,
                                     obj->Get(context,
                                              ShardIDKey).FromMaybe(v8::Handle<v8::Value>()));
    }
  }

  LOG_TOPIC("f2376", DEBUG, Logger::CLUSTER) << "JS_Drop: calling ClusterComm::drop()";
  
  {
    std::lock_guard<std::mutex> guard(::_requestMutex);
    auto it = ::_requests.begin();
    while (it != _requests.end()) {
      std::shared_ptr<::AsyncRequest>& req = *it;
      if (req->coordTransactionID == mycoordTransactionID ||
          req->operationID == myoperationID) {
        _requests.erase(it);
        break;
      }
      it++;
    }
  }


  TRI_V8_RETURN_UNDEFINED();
  TRI_V8_TRY_CATCH_END
}

////////////////////////////////////////////////////////////////////////////////
/// @brief get an ID for use with coordTransactionId
////////////////////////////////////////////////////////////////////////////////

static void JS_GetId(v8::FunctionCallbackInfo<v8::Value> const& args) {
  TRI_V8_TRY_CATCH_BEGIN(isolate);

  if (args.Length() != 0) {
    TRI_V8_THROW_EXCEPTION_USAGE("getId()");
  }

  auto id = TRI_NewTickServer();
  std::string st = StringUtils::itoa(id);
  v8::Handle<v8::String> s = TRI_V8_ASCII_STRING(isolate, st.c_str());

  TRI_V8_RETURN(s);
  TRI_V8_TRY_CATCH_END
}

static void JS_ClusterDownload(v8::FunctionCallbackInfo<v8::Value> const& args) {
  TRI_V8_TRY_CATCH_BEGIN(isolate);
  auto context = TRI_IGETC;

  AuthenticationFeature* af = AuthenticationFeature::instance();
  if (af != nullptr && af->isActive()) {
    // mop: really quick and dirty
    v8::Handle<v8::Object> options = v8::Object::New(isolate);
    v8::Handle<v8::Object> headers = v8::Object::New(isolate);
    if (args.Length() > 2) {
      if (args[2]->IsObject()) {
        options = v8::Handle<v8::Object>::Cast(args[2]);
        if (TRI_HasProperty(context, isolate, options, "headers")) {
          headers = v8::Handle<v8::Object>::Cast(
                                                 options->Get(context,
                                                              TRI_V8_ASCII_STRING(isolate, "headers")).FromMaybe(v8::Handle<v8::Value>()));
        }
      }
    }
    options->Set(context, TRI_V8_ASCII_STRING(isolate, "headers"), headers).FromMaybe(false);

    std::string authorization = "bearer " + af->tokenCache().jwtToken();
    v8::Handle<v8::String> v8Authorization = TRI_V8_STD_STRING(isolate, authorization);
    headers->Set(context, TRI_V8_ASCII_STRING(isolate, "Authorization"), v8Authorization).FromMaybe(false);

    args[2] = options;
  }
  TRI_V8_TRY_CATCH_END
  return JS_Download(args);
}

////////////////////////////////////////////////////////////////////////////////
/// @brief Collect the distribution of shards
////////////////////////////////////////////////////////////////////////////////

static void JS_GetShardDistribution(v8::FunctionCallbackInfo<v8::Value> const& args) {
  TRI_V8_TRY_CATCH_BEGIN(isolate);

  onlyInCluster();

  v8::HandleScope scope(isolate);
  auto& vocbase = GetContextVocBase(isolate);
  auto reporter = cluster::ShardDistributionReporter::instance(vocbase.server());
  VPackBuilder result;

  reporter->getDistributionForDatabase(vocbase.name(), result);

  TRI_V8_RETURN(TRI_VPackToV8(isolate, result.slice()));
  TRI_V8_TRY_CATCH_END
}

////////////////////////////////////////////////////////////////////////////////
/// @brief Collect the distribution of shards of a specific collection
////////////////////////////////////////////////////////////////////////////////

static void JS_GetCollectionShardDistribution(v8::FunctionCallbackInfo<v8::Value> const& args) {
  TRI_V8_TRY_CATCH_BEGIN(isolate);

  onlyInCluster();

  if (args.Length() != 1) {
    TRI_V8_THROW_EXCEPTION_USAGE(
        "GetCollectionShardDistribution(<collectionName>)");
  }

  std::string const colName = TRI_ObjectToString(isolate, args[0]);

  v8::HandleScope scope(isolate);
  auto& vocbase = GetContextVocBase(isolate);
  auto reporter = cluster::ShardDistributionReporter::instance(vocbase.server());
  VPackBuilder result;

  reporter->getCollectionDistributionForDatabase(vocbase.name(), colName, result);

  TRI_V8_RETURN(TRI_VPackToV8(isolate, result.slice()));
  TRI_V8_TRY_CATCH_END
}

////////////////////////////////////////////////////////////////////////////////
/// @brief returns database analyzers revision
////////////////////////////////////////////////////////////////////////////////

static void JS_GetAnalyzersRevision(v8::FunctionCallbackInfo<v8::Value> const& args) {
  TRI_V8_TRY_CATCH_BEGIN(isolate);

  onlyInCluster();

  if (args.Length() != 1) {
    TRI_V8_THROW_EXCEPTION_USAGE("getAnalyzersRevision(<databaseName>)");
  }

  auto const databaseID = TRI_ObjectToString(isolate, args[0]);

  TRI_GET_GLOBALS();
  auto& ci = v8g->_server.getFeature<ClusterFeature>().clusterInfo();
  auto const analyzerRevision = ci.getAnalyzersRevision(databaseID);

  if (!analyzerRevision) {
    TRI_V8_THROW_EXCEPTION_PARAMETER("<databaseName> is invalid");
  }

  VPackBuilder result;
  analyzerRevision->toVelocyPack(result);

  TRI_V8_RETURN(TRI_VPackToV8(isolate, result.slice()));
  TRI_V8_TRY_CATCH_END
}

////////////////////////////////////////////////////////////////////////////////
/// @brief creates a global cluster context
////////////////////////////////////////////////////////////////////////////////

void TRI_InitV8Cluster(v8::Isolate* isolate, v8::Handle<v8::Context> context) {
  TRI_V8_CURRENT_GLOBALS_AND_SCOPE;
  TRI_ASSERT(v8g != nullptr);

  v8::Handle<v8::ObjectTemplate> rt;
  v8::Handle<v8::FunctionTemplate> ft;

  // ...........................................................................
  // generate the agency template
  // ...........................................................................

  ft = v8::FunctionTemplate::New(isolate);
  ft->SetClassName(TRI_V8_ASCII_STRING(isolate, "ArangoAgency"));

  rt = ft->InstanceTemplate();
  rt->SetInternalFieldCount(2);

  TRI_AddMethodVocbase(isolate, rt, TRI_V8_ASCII_STRING(isolate, "agency"), JS_Agency);

  TRI_AddMethodVocbase(isolate, rt, TRI_V8_ASCII_STRING(isolate, "read"), JS_ReadAgency);
  TRI_AddMethodVocbase(isolate, rt, TRI_V8_ASCII_STRING(isolate, "write"), JS_WriteAgency);
  TRI_AddMethodVocbase(isolate, rt, TRI_V8_ASCII_STRING(isolate, "transact"), JS_TransactAgency);
  TRI_AddMethodVocbase(isolate, rt, TRI_V8_ASCII_STRING(isolate, "transient"), JS_TransientAgency);

  TRI_AddMethodVocbase(isolate, rt, TRI_V8_ASCII_STRING(isolate, "cas"), JS_CasAgency);
  TRI_AddMethodVocbase(isolate, rt,
                       TRI_V8_ASCII_STRING(isolate, "createDirectory"),
                       JS_CreateDirectoryAgency);
  TRI_AddMethodVocbase(isolate, rt, TRI_V8_ASCII_STRING(isolate, "get"), JS_GetAgency);
  TRI_AddMethodVocbase(isolate, rt, TRI_V8_ASCII_STRING(isolate, "isEnabled"), JS_IsEnabledAgency);
  TRI_AddMethodVocbase(isolate, rt,
                       TRI_V8_ASCII_STRING(isolate, "increaseVersion"),
                       JS_IncreaseVersionAgency);
  TRI_AddMethodVocbase(isolate, rt, TRI_V8_ASCII_STRING(isolate, "remove"), JS_RemoveAgency);
  TRI_AddMethodVocbase(isolate, rt, TRI_V8_ASCII_STRING(isolate, "set"), JS_SetAgency);
  TRI_AddMethodVocbase(isolate, rt, TRI_V8_ASCII_STRING(isolate, "endpoints"), JS_EndpointsAgency);
  TRI_AddMethodVocbase(isolate, rt, TRI_V8_ASCII_STRING(isolate, "prefix"), JS_PrefixAgency);
  TRI_AddMethodVocbase(isolate, rt, TRI_V8_ASCII_STRING(isolate, "uniqid"), JS_UniqidAgency);
  TRI_AddMethodVocbase(isolate, rt, TRI_V8_ASCII_STRING(isolate, "version"), JS_VersionAgency);

  v8g->AgencyTempl.Reset(isolate, rt);
  ft->SetClassName(TRI_V8_ASCII_STRING(isolate, "ArangoAgencyCtor"));

  TRI_AddGlobalFunctionVocbase(isolate,
                               TRI_V8_ASCII_STRING(isolate, "ArangoAgencyCtor"),
                               ft->GetFunction(TRI_IGETC).FromMaybe(v8::Local<v8::Function>()), true);

  // register the global object
  v8::Handle<v8::Object> aa = rt->NewInstance(TRI_IGETC).FromMaybe(v8::Local<v8::Object>());
  if (!aa.IsEmpty()) {
    TRI_AddGlobalVariableVocbase(isolate,
                                 TRI_V8_ASCII_STRING(isolate, "ArangoAgency"), aa);
  }

  // ...........................................................................
  // generate the cluster info template
  // ...........................................................................

  ft = v8::FunctionTemplate::New(isolate);
  ft->SetClassName(TRI_V8_ASCII_STRING(isolate, "ArangoClusterInfo"));

  rt = ft->InstanceTemplate();
  rt->SetInternalFieldCount(2);

  TRI_AddMethodVocbase(isolate, rt,
                       TRI_V8_ASCII_STRING(isolate, "doesDatabaseExist"),
                       JS_DoesDatabaseExistClusterInfo);
  TRI_AddMethodVocbase(isolate, rt, TRI_V8_ASCII_STRING(isolate, "databases"), JS_Databases);
  TRI_AddMethodVocbase(isolate, rt, TRI_V8_ASCII_STRING(isolate, "flush"),
                       JS_FlushClusterInfo, true);
  TRI_AddMethodVocbase(isolate, rt,
                       TRI_V8_ASCII_STRING(isolate, "getCollectionInfo"),
                       JS_GetCollectionInfoClusterInfo);
  TRI_AddMethodVocbase(isolate, rt,
                       TRI_V8_ASCII_STRING(isolate, "getCollectionInfoCurrent"),
                       JS_GetCollectionInfoCurrentClusterInfo);
  TRI_AddMethodVocbase(isolate, rt,
                       TRI_V8_ASCII_STRING(isolate, "getResponsibleServer"),
                       JS_GetResponsibleServerClusterInfo);
  TRI_AddMethodVocbase(isolate, rt,
                       TRI_V8_ASCII_STRING(isolate, "getResponsibleServers"),
                       JS_GetResponsibleServersClusterInfo);
  TRI_AddMethodVocbase(isolate, rt,
                       TRI_V8_ASCII_STRING(isolate, "getResponsibleShard"),
                       JS_GetResponsibleShardClusterInfo);
  TRI_AddMethodVocbase(isolate, rt,
                       TRI_V8_ASCII_STRING(isolate, "getServerEndpoint"),
                       JS_GetServerEndpointClusterInfo);
  TRI_AddMethodVocbase(isolate, rt,
                       TRI_V8_ASCII_STRING(isolate, "getServerName"),
                       JS_GetServerNameClusterInfo);
  TRI_AddMethodVocbase(isolate, rt, TRI_V8_ASCII_STRING(isolate, "getDBServers"), JS_GetDBServers);
  TRI_AddMethodVocbase(isolate, rt,
                       TRI_V8_ASCII_STRING(isolate, "getCoordinators"), JS_GetCoordinators);
  TRI_AddMethodVocbase(isolate, rt, TRI_V8_ASCII_STRING(isolate, "uniqid"), JS_UniqidClusterInfo);
  TRI_AddMethodVocbase(isolate, rt, TRI_V8_ASCII_STRING(isolate, "getAnalyzersRevision"), JS_GetAnalyzersRevision);

  v8g->ClusterInfoTempl.Reset(isolate, rt);
  TRI_AddGlobalFunctionVocbase(isolate,
                               TRI_V8_ASCII_STRING(isolate,
                                                   "ArangoClusterInfoCtor"),
                               ft->GetFunction(TRI_IGETC).FromMaybe(v8::Local<v8::Function>()), true);

  // register the global object
  v8::Handle<v8::Object> ci = rt->NewInstance(TRI_IGETC).FromMaybe(v8::Local<v8::Object>());
  if (!ci.IsEmpty()) {
    TRI_AddGlobalVariableVocbase(
        isolate, TRI_V8_ASCII_STRING(isolate, "ArangoClusterInfo"), ci);
  }

  // .............................................................................
  // generate the server state template
  // ...........................................................................

  ft = v8::FunctionTemplate::New(isolate);
  ft->SetClassName(TRI_V8_ASCII_STRING(isolate, "ArangoServerState"));

  rt = ft->InstanceTemplate();
  rt->SetInternalFieldCount(2);

  TRI_AddMethodVocbase(isolate, rt, TRI_V8_ASCII_STRING(isolate, "address"),
                       JS_AddressServerState);
  TRI_AddMethodVocbase(isolate, rt, TRI_V8_ASCII_STRING(isolate, "id"), JS_IdServerState);
  TRI_AddMethodVocbase(isolate, rt, TRI_V8_ASCII_STRING(isolate, "isFoxxmaster"), JS_isFoxxmaster);
  TRI_AddMethodVocbase(isolate, rt,
                       TRI_V8_ASCII_STRING(isolate, "getFoxxmasterQueueupdate"),
                       JS_getFoxxmasterQueueupdate);
  TRI_AddMethodVocbase(isolate, rt,
                       TRI_V8_ASCII_STRING(isolate, "setFoxxmasterQueueupdate"),
                       JS_setFoxxmasterQueueupdate);
  TRI_AddMethodVocbase(isolate, rt,
                       TRI_V8_ASCII_STRING(isolate, "getFoxxmasterSince"),
                       JS_GetFoxxmasterSince);
  TRI_AddMethodVocbase(isolate, rt, TRI_V8_ASCII_STRING(isolate, "initialized"),
                       JS_InitializedServerState);
  TRI_AddMethodVocbase(isolate, rt,
                       TRI_V8_ASCII_STRING(isolate, "isCoordinator"),
                       JS_IsCoordinatorServerState);
  TRI_AddMethodVocbase(isolate, rt, TRI_V8_ASCII_STRING(isolate, "role"), JS_RoleServerState);
  TRI_AddMethodVocbase(isolate, rt, TRI_V8_ASCII_STRING(isolate, "setRole"),
                       JS_SetRoleServerState, true);
  TRI_AddMethodVocbase(isolate, rt, TRI_V8_ASCII_STRING(isolate, "status"), JS_StatusServerState);

  v8g->ServerStateTempl.Reset(isolate, rt);
  TRI_AddGlobalFunctionVocbase(isolate,
                               TRI_V8_ASCII_STRING(isolate,
                                                   "ArangoServerStateCtor"),
                               ft->GetFunction(TRI_IGETC).FromMaybe(v8::Local<v8::Function>()), true);

  // register the global object
  v8::Handle<v8::Object> ss = rt->NewInstance(TRI_IGETC).FromMaybe(v8::Local<v8::Object>());
  if (!ss.IsEmpty()) {
    TRI_AddGlobalVariableVocbase(
        isolate, TRI_V8_ASCII_STRING(isolate, "ArangoServerState"), ss);
  }

  // ...........................................................................
  // generate the cluster comm template
  // ...........................................................................

  ft = v8::FunctionTemplate::New(isolate);
  ft->SetClassName(TRI_V8_ASCII_STRING(isolate, "ArangoClusterComm"));

  rt = ft->InstanceTemplate();
  rt->SetInternalFieldCount(2);

  TRI_AddMethodVocbase(isolate, rt, TRI_V8_ASCII_STRING(isolate, "asyncRequest"), JS_AsyncRequest);
  TRI_AddMethodVocbase(isolate, rt, TRI_V8_ASCII_STRING(isolate, "enquire"), JS_Enquire);
  TRI_AddMethodVocbase(isolate, rt, TRI_V8_ASCII_STRING(isolate, "wait"), JS_Wait);
  TRI_AddMethodVocbase(isolate, rt, TRI_V8_ASCII_STRING(isolate, "drop"), JS_Drop);
  TRI_AddMethodVocbase(isolate, rt, TRI_V8_ASCII_STRING(isolate, "getId"), JS_GetId);

  v8g->ClusterCommTempl.Reset(isolate, rt);
  TRI_AddGlobalFunctionVocbase(isolate,
                               TRI_V8_ASCII_STRING(isolate,
                                                   "ArangoClusterCommCtor"),
                               ft->GetFunction(TRI_IGETC).FromMaybe(v8::Local<v8::Function>()), true);

  // register the global object
  ss = rt->NewInstance(TRI_IGETC).FromMaybe(v8::Local<v8::Object>());
  if (!ss.IsEmpty()) {
    TRI_AddGlobalVariableVocbase(
        isolate, TRI_V8_ASCII_STRING(isolate, "ArangoClusterComm"), ss);
  }
  TRI_AddGlobalFunctionVocbase(isolate,
                               TRI_V8_ASCII_STRING(isolate,
                                                   "SYS_CLUSTER_DOWNLOAD"),
                               JS_ClusterDownload);

  TRI_AddGlobalFunctionVocbase(
      isolate, TRI_V8_ASCII_STRING(isolate, "SYS_CLUSTER_SHARD_DISTRIBUTION"),
      JS_GetShardDistribution);

  TRI_AddGlobalFunctionVocbase(
      isolate,
      TRI_V8_ASCII_STRING(isolate, "SYS_CLUSTER_COLLECTION_SHARD_DISTRIBUTION"),
      JS_GetCollectionShardDistribution);
}
