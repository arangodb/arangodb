////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2014-2021 ArangoDB GmbH, Cologne, Germany
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
#include "ApplicationFeatures/V8SecurityFeature.h"
#include "Auth/TokenCache.h"
#include "Basics/Exceptions.h"
#include "Basics/StringBuffer.h"
#include "Basics/StringUtils.h"
#include "Cluster/ClusterFeature.h"
#include "Cluster/ClusterInfo.h"
#include "Cluster/ServerState.h"
#include "GeneralServer/AuthenticationFeature.h"
#include "Network/NetworkFeature.h"
#include "Network/Methods.h"
#include "Network/Utils.h"
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

  THROW_ARANGO_EXCEPTION_MESSAGE(TRI_ERROR_NOT_IMPLEMENTED, "ArangoDB is not running in cluster mode");
}

static void onlyInClusterOrActiveFailover(v8::Isolate* isolate) {
  TRI_GET_GLOBALS();
  auto& replicationFeature = v8g->_server.getFeature<ReplicationFeature>();
  if (replicationFeature.isActiveFailoverEnabled()) {
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
                   v8::Number::New(isolate, static_cast<int>(result.httpCode()))).FromMaybe(false);
  errorObject->Set(context,
                   TRI_V8_STD_STRING(isolate, StaticStrings::ErrorNum),
                   v8::Number::New(isolate, static_cast<int>(result.errorCode()))).FromMaybe(false);
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

  onlyInClusterOrActiveFailover(isolate);
  
  TRI_GET_GLOBALS();
  V8SecurityFeature& v8security = v8g->_server.getFeature<V8SecurityFeature>();
  if (!v8security.isInternalContext(isolate) && !v8security.isAdminScriptContext(isolate)) {
    THROW_ARANGO_EXCEPTION_MESSAGE(TRI_ERROR_FORBIDDEN,
                                   "not allowed to execute this agency operation");
  }

  if (args.Length() < 3) {
    TRI_V8_THROW_EXCEPTION_USAGE(
        "cas(<key>, <oldValue>, <newValue>, <ttl>, <timeout>, <throw>)");
  }

  std::string const key = TRI_ObjectToString(isolate, args[0]);

  VPackBuilder oldBuilder;
  TRI_V8ToVPack(isolate, oldBuilder, args[1], false);

  VPackBuilder newBuilder;
  TRI_V8ToVPack(isolate, newBuilder, args[2], false);

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

  onlyInClusterOrActiveFailover(isolate);
  
  TRI_GET_GLOBALS();
  V8SecurityFeature& v8security = v8g->_server.getFeature<V8SecurityFeature>();
  if (!v8security.isInternalContext(isolate) && !v8security.isAdminScriptContext(isolate)) {
    THROW_ARANGO_EXCEPTION_MESSAGE(TRI_ERROR_FORBIDDEN,
                                   "not allowed to execute this agency operation");
  }

  if (args.Length() != 1) {
    TRI_V8_THROW_EXCEPTION_USAGE("createDirectory(<key>)");
  }

  std::string const key = TRI_ObjectToString(isolate, args[0]);

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

  onlyInClusterOrActiveFailover(isolate);
  
  TRI_GET_GLOBALS();
  V8SecurityFeature& v8security = v8g->_server.getFeature<V8SecurityFeature>();
  if (!v8security.isInternalContext(isolate) && !v8security.isAdminScriptContext(isolate)) {
    THROW_ARANGO_EXCEPTION_MESSAGE(TRI_ERROR_FORBIDDEN,
                                   "not allowed to execute this agency operation");
  }

  if (args.Length() != 1) {
    TRI_V8_THROW_EXCEPTION_USAGE("increaseVersion(<key>)");
  }

  std::string const key = TRI_ObjectToString(isolate, args[0]);

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

  onlyInClusterOrActiveFailover(isolate);
  
  TRI_GET_GLOBALS();
  V8SecurityFeature& v8security = v8g->_server.getFeature<V8SecurityFeature>();
  if (!v8security.isInternalContext(isolate) && !v8security.isAdminScriptContext(isolate)) {
    THROW_ARANGO_EXCEPTION_MESSAGE(TRI_ERROR_FORBIDDEN,
                                   "not allowed to execute this agency operation");
  }

  if (args.Length() < 1) {
    TRI_V8_THROW_EXCEPTION_USAGE("get(<key>)");
  }

  std::string const key = TRI_ObjectToString(isolate, args[0]);
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

  onlyInClusterOrActiveFailover(isolate);
  
  TRI_GET_GLOBALS();
  V8SecurityFeature& v8security = v8g->_server.getFeature<V8SecurityFeature>();
  if (!v8security.isInternalContext(isolate) && !v8security.isAdminScriptContext(isolate)) {
    THROW_ARANGO_EXCEPTION_MESSAGE(TRI_ERROR_FORBIDDEN,
                                   "not allowed to execute this agency operation");
  }

  if (args.Length() < 1) {
    TRI_V8_THROW_EXCEPTION_USAGE(std::string(envelope) + "([[...]])");
  }

  VPackBuilder builder;
  TRI_V8ToVPack(isolate, builder, args[0], false);

  AgencyComm comm(v8g->_server);
  AgencyCommResult result =
      comm.sendWithFailover(arangodb::rest::RequestType::POST,
                            AgencyCommHelper::CONNECTION_OPTIONS._requestTimeout,
                            std::string("/_api/agency/") + envelope, builder.slice());

  if (!result.successful()) {
    THROW_AGENCY_EXCEPTION(result);
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

  onlyInClusterOrActiveFailover(isolate);
  
  TRI_GET_GLOBALS();
  V8SecurityFeature& v8security = v8g->_server.getFeature<V8SecurityFeature>();
  if (!v8security.isInternalContext(isolate) && !v8security.isAdminScriptContext(isolate)) {
    THROW_ARANGO_EXCEPTION_MESSAGE(TRI_ERROR_FORBIDDEN,
                                   "not allowed to execute this agency operation");
  }

  if (args.Length() < 1) {
    TRI_V8_THROW_EXCEPTION_USAGE("remove(<key>, <recursive>)");
  }

  std::string const key = TRI_ObjectToString(isolate, args[0]);
  bool recursive = false;

  if (args.Length() > 1) {
    recursive = TRI_ObjectToBoolean(isolate, args[1]);
  }

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

  onlyInClusterOrActiveFailover(isolate);
  
  TRI_GET_GLOBALS();
  V8SecurityFeature& v8security = v8g->_server.getFeature<V8SecurityFeature>();
  if (!v8security.isInternalContext(isolate) && !v8security.isAdminScriptContext(isolate)) {
    THROW_ARANGO_EXCEPTION_MESSAGE(TRI_ERROR_FORBIDDEN,
                                   "not allowed to execute this agency operation");
  }

  if (args.Length() < 2) {
    TRI_V8_THROW_EXCEPTION_USAGE("set(<key>, <value>, <ttl>)");
  }

  std::string const key = TRI_ObjectToString(isolate, args[0]);

  VPackBuilder builder;
  TRI_V8ToVPack(isolate, builder, args[1], false);

  double ttl = 0.0;
  if (args.Length() > 2) {
    ttl = TRI_ObjectToDouble(isolate, args[2]);
  }

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

  onlyInClusterOrActiveFailover(isolate);
  
  TRI_GET_GLOBALS();
  V8SecurityFeature& v8security = v8g->_server.getFeature<V8SecurityFeature>();
  if (!v8security.isInternalContext(isolate) && !v8security.isAdminScriptContext(isolate)) {
    THROW_ARANGO_EXCEPTION_MESSAGE(TRI_ERROR_FORBIDDEN,
                                   "not allowed to execute this agency operation");
  }

  if (args.Length() > 0) {
    TRI_V8_THROW_EXCEPTION_USAGE("agency()");
  }

  VPackBuilder builder;

  AgencyComm comm(v8g->_server);
  AgencyCommResult result =
      comm.sendWithFailover(arangodb::rest::RequestType::GET,
                            AgencyCommHelper::CONNECTION_OPTIONS._requestTimeout,
                            std::string("/_api/agency/config"), builder.slice());

  if (!result.successful()) {
    THROW_AGENCY_EXCEPTION(result);
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

  onlyInClusterOrActiveFailover(isolate);
  
  TRI_GET_GLOBALS();
  V8SecurityFeature& v8security = v8g->_server.getFeature<V8SecurityFeature>();
  if (!v8security.isInternalContext(isolate) && !v8security.isAdminScriptContext(isolate)) {
    THROW_ARANGO_EXCEPTION_MESSAGE(TRI_ERROR_FORBIDDEN,
                                   "not allowed to execute this agency operation");
  }

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

  onlyInClusterOrActiveFailover(isolate);
  
  TRI_GET_GLOBALS();
  V8SecurityFeature& v8security = v8g->_server.getFeature<V8SecurityFeature>();
  if (!v8security.isInternalContext(isolate) && !v8security.isAdminScriptContext(isolate)) {
    THROW_ARANGO_EXCEPTION_MESSAGE(TRI_ERROR_FORBIDDEN,
                                   "not allowed to execute this agency operation");
  }

  if (args.Length() > 2) {
    TRI_V8_THROW_EXCEPTION_USAGE("uniqid(<count>, <timeout>)");
  }

  uint64_t count = 1;
  if (args.Length() > 0) {
    count = TRI_ObjectToUInt64(isolate, args[0], true);
  }

  if (count < 1 || count > 100000000) {
    TRI_V8_THROW_EXCEPTION_PARAMETER("<count> is invalid");
  }

  double timeout = 0.0;
  if (args.Length() > 1) {
    timeout = TRI_ObjectToDouble(isolate, args[1]);
  }

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

  onlyInClusterOrActiveFailover(isolate);
  
  TRI_GET_GLOBALS();
  V8SecurityFeature& v8security = v8g->_server.getFeature<V8SecurityFeature>();
  if (!v8security.isInternalContext(isolate) && !v8security.isAdminScriptContext(isolate)) {
    THROW_ARANGO_EXCEPTION_MESSAGE(TRI_ERROR_FORBIDDEN,
                                   "not allowed to execute this agency operation");
  }

  if (args.Length() != 0) {
    TRI_V8_THROW_EXCEPTION_USAGE("version()");
  }

  AgencyComm comm(v8g->_server);
  auto const version = comm.version();

  TRI_V8_RETURN_STD_STRING_VIEW(version);
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
  bool const result = ci.doesDatabaseExist(TRI_ObjectToString(isolate, args[0]));

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
  std::vector<DatabaseID> res = ci.databases();
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
  
  TRI_GET_GLOBALS();
  V8SecurityFeature& v8security = v8g->_server.getFeature<V8SecurityFeature>();
  if (!v8security.isInternalContext(isolate) && !v8security.isAdminScriptContext(isolate)) {
    THROW_ARANGO_EXCEPTION_MESSAGE(TRI_ERROR_FORBIDDEN,
                                   "not allowed to execute this cluster operation");
  }

  if (args.Length() != 0) {
    TRI_V8_THROW_EXCEPTION_USAGE("flush()");
  }

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
  auto cid = std::to_string(col->id().id());
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
  TRI_V8ToVPack(isolate, builder, args[1], false);

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

  auto res = collInfo->getResponsibleShard(builder.slice(), documentIsComplete,
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
  
  TRI_GET_GLOBALS();
  V8SecurityFeature& v8security = v8g->_server.getFeature<V8SecurityFeature>();
  if (!v8security.isInternalContext(isolate) && !v8security.isAdminScriptContext(isolate)) {
    THROW_ARANGO_EXCEPTION_MESSAGE(TRI_ERROR_FORBIDDEN,
                                   "not allowed to execute this cluster operation");
  }

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

  onlyInClusterOrActiveFailover(isolate);

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

// send a self-heal request to all other coordinators, if any
static void JS_PropagateSelfHeal(v8::FunctionCallbackInfo<v8::Value> const& args) {
  TRI_V8_TRY_CATCH_BEGIN(isolate);
  v8::HandleScope scope(isolate);

  if (ServerState::instance()->isCoordinator()) {
    auto& vocbase = GetContextVocBase(isolate);

    NetworkFeature const& nf = vocbase.server().getFeature<NetworkFeature>();
    network::ConnectionPool* pool = nf.pool();
    if (pool == nullptr) {
      THROW_ARANGO_EXCEPTION(TRI_ERROR_SHUTTING_DOWN);
    }

    std::vector<network::FutureRes> futures;

    network::RequestOptions options;
    options.timeout = network::Timeout(10.0);
    options.database = vocbase.name();
    
    // send an empty body
    VPackBuffer<uint8_t> buffer;
    buffer.append(VPackSlice::emptyObjectSlice().begin(), 1);

    std::string const url("/_api/foxx/_local/heal");
    
    network::Headers headers;
    auto auth = AuthenticationFeature::instance();
    if (auth != nullptr && auth->isActive()) {
      headers.try_emplace(StaticStrings::Authorization,
                          "bearer " + auth->tokenCache().jwtToken());
    }

    auto& ci = vocbase.server().getFeature<ClusterFeature>().clusterInfo();
    for (auto const& coordinator : ci.getCurrentCoordinators()) {
      if (coordinator == ServerState::instance()->getId()) {
        // ourselves
        continue;
      }
      auto f = network::sendRequest(pool, "server:" + coordinator, fuerte::RestVerb::Post,
                                    url, buffer, options, headers);
      futures.emplace_back(std::move(f));
    }

    Result res;
    
    if (!futures.empty()) {
      auto responses = futures::collectAll(futures).get();
      for (auto const& it : responses) {
        auto& resp = it.get();
        res.reset(resp.combinedResult());

        if (res.is(TRI_ERROR_ARANGO_DATABASE_NOT_FOUND)) {
          // it is expected in a multi-coordinator setup that a coordinator is not
          // aware of a database that was created very recently.
          res.reset();
        }
        if (res.fail()) {
          break;
        }
      }
    }

    if (res.fail()) {
      THROW_ARANGO_EXCEPTION(res);
    }
  }
  
  TRI_V8_RETURN_UNDEFINED();
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

  TRI_AddGlobalFunctionVocbase(
      isolate, TRI_V8_ASCII_STRING(isolate, "SYS_CLUSTER_SHARD_DISTRIBUTION"),
      JS_GetShardDistribution, true);

  TRI_AddGlobalFunctionVocbase(
      isolate,
      TRI_V8_ASCII_STRING(isolate, "SYS_CLUSTER_COLLECTION_SHARD_DISTRIBUTION"),
      JS_GetCollectionShardDistribution, true);
  
  TRI_AddGlobalFunctionVocbase(
      isolate,
      TRI_V8_ASCII_STRING(isolate, "SYS_PROPAGATE_SELF_HEAL"),
      JS_PropagateSelfHeal, true);
}
