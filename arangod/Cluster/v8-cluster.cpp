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
#include "ApplicationFeatures/ApplicationServer.h"
#include "Cluster/ClusterComm.h"
#include "Cluster/ClusterInfo.h"
#include "Cluster/ServerState.h"
#include "GeneralServer/AuthenticationFeature.h"
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

#define ONLY_IN_CLUSTER                                 \
  if (!ServerState::instance()->isRunningInCluster()) { \
    TRI_V8_THROW_EXCEPTION_INTERNAL(                    \
        "ArangoDB is not running in cluster mode");     \
  }

static void CreateAgencyException(v8::FunctionCallbackInfo<v8::Value> const& args,
                                  AgencyCommResult const& result) {
  v8::Isolate* isolate = args.GetIsolate();
  TRI_V8_CURRENT_GLOBALS_AND_SCOPE;

  std::string const errorDetails = result.errorDetails();
  v8::Handle<v8::String> errorMessage = TRI_V8_STD_STRING(isolate, errorDetails);
  if (errorMessage.IsEmpty()) {
    isolate->ThrowException(v8::Object::New(isolate));
    return;
  }
  v8::Handle<v8::Object> errorObject = v8::Exception::Error(errorMessage)->ToObject();
  if (errorObject.IsEmpty()) {
    isolate->ThrowException(v8::Object::New(isolate));
    return;
  }

  errorObject->Set(TRI_V8_STD_STRING(isolate, StaticStrings::Code),
                   v8::Number::New(isolate, result.httpCode()));
  errorObject->Set(TRI_V8_STD_STRING(isolate, StaticStrings::ErrorNum),
                   v8::Number::New(isolate, result.errorCode()));
  errorObject->Set(TRI_V8_STD_STRING(isolate, StaticStrings::ErrorMessage), errorMessage);
  errorObject->Set(TRI_V8_STD_STRING(isolate, StaticStrings::Error), v8::True(isolate));

  TRI_GET_GLOBAL(ArangoErrorTempl, v8::ObjectTemplate);
  v8::Handle<v8::Value> proto = ArangoErrorTempl->NewInstance();
  if (!proto.IsEmpty()) {
    errorObject->SetPrototype(proto);
  }

  args.GetIsolate()->ThrowException(errorObject);
}

////////////////////////////////////////////////////////////////////////////////
/// @brief compares and swaps a value in the agency
////////////////////////////////////////////////////////////////////////////////

static void JS_CasAgency(v8::FunctionCallbackInfo<v8::Value> const& args) {
  TRI_V8_TRY_CATCH_BEGIN(isolate);
  v8::HandleScope scope(isolate);

  ONLY_IN_CLUSTER;

  if (args.Length() < 3) {
    TRI_V8_THROW_EXCEPTION_USAGE(
        "cas(<key>, <oldValue>, <newValue>, <ttl>, <timeout>, <throw>)");
  }

  std::string const key = TRI_ObjectToString(args[0]);

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
    ttl = TRI_ObjectToDouble(args[3]);
  }

  double timeout = 1.0;
  if (args.Length() > 4) {
    timeout = TRI_ObjectToDouble(args[4]);
  }

  bool shouldThrow = false;
  if (args.Length() > 5) {
    shouldThrow = TRI_ObjectToBoolean(args[5]);
  }

  AgencyComm comm;
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

  ONLY_IN_CLUSTER;

  if (args.Length() != 1) {
    TRI_V8_THROW_EXCEPTION_USAGE("createDirectory(<key>)");
  }

  std::string const key = TRI_ObjectToString(args[0]);

  AgencyComm comm;
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

  if (AgencyCommManager::isEnabled()) {
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

  ONLY_IN_CLUSTER;

  if (args.Length() != 1) {
    TRI_V8_THROW_EXCEPTION_USAGE("increaseVersion(<key>)");
  }

  std::string const key = TRI_ObjectToString(args[0]);

  AgencyComm comm;
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

  ONLY_IN_CLUSTER;

  if (args.Length() < 1) {
    TRI_V8_THROW_EXCEPTION_USAGE("get(<key>)");
  }

  std::string const key = TRI_ObjectToString(args[0]);
  AgencyComm comm;
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
        l->ForceSet(TRI_V8_STD_STRING(isolate, key), TRI_VPackToV8(isolate, slice));
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

  ONLY_IN_CLUSTER;

  if (args.Length() < 1) {
    TRI_V8_THROW_EXCEPTION_USAGE(std::string(envelope) + "([[...]])");
  }

  VPackBuilder builder;
  int res = TRI_V8ToVPack(isolate, builder, args[0], false);

  if (res != TRI_ERROR_NO_ERROR) {
    TRI_V8_THROW_EXCEPTION_PARAMETER("cannot convert query to JSON");
  }

  AgencyComm comm;
  AgencyCommResult result =
      comm.sendWithFailover(arangodb::rest::RequestType::POST,
                            AgencyCommManager::CONNECTION_OPTIONS._requestTimeout,
                            std::string("/_api/agency/") + envelope, builder.slice());

  if (!result.successful()) {
    THROW_AGENCY_EXCEPTION(result);
  }

  try {
    result.setVPack(VPackParser::fromJson(result.bodyRef()));
    result._body.clear();
  } catch (std::exception const& e) {
    LOG_TOPIC(ERR, Logger::AGENCYCOMM) << "Error transforming result: " << e.what();
    result.clear();
  } catch (...) {
    LOG_TOPIC(ERR, Logger::AGENCYCOMM)
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

  ONLY_IN_CLUSTER;

  if (args.Length() < 1) {
    TRI_V8_THROW_EXCEPTION_USAGE("remove(<key>, <recursive>)");
  }

  std::string const key = TRI_ObjectToString(args[0]);
  bool recursive = false;

  if (args.Length() > 1) {
    recursive = TRI_ObjectToBoolean(args[1]);
  }

  AgencyComm comm;
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

  ONLY_IN_CLUSTER;

  if (args.Length() < 2) {
    TRI_V8_THROW_EXCEPTION_USAGE("set(<key>, <value>, <ttl>)");
  }

  std::string const key = TRI_ObjectToString(args[0]);

  VPackBuilder builder;
  int res = TRI_V8ToVPack(isolate, builder, args[1], false);

  if (res != TRI_ERROR_NO_ERROR) {
    TRI_V8_THROW_EXCEPTION_PARAMETER("cannot convert <value> to JSON");
  }

  double ttl = 0.0;
  if (args.Length() > 2) {
    ttl = TRI_ObjectToDouble(args[2]);
  }

  AgencyComm comm;
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

  ONLY_IN_CLUSTER;

  if (args.Length() > 0) {
    TRI_V8_THROW_EXCEPTION_USAGE("agency()");
  }

  VPackBuilder builder;

  AgencyComm comm;
  AgencyCommResult result =
      comm.sendWithFailover(arangodb::rest::RequestType::GET,
                            AgencyCommManager::CONNECTION_OPTIONS._requestTimeout,
                            std::string("/_api/agency/config"), builder.slice());

  if (!result.successful()) {
    THROW_AGENCY_EXCEPTION(result);
  }

  try {
    result.setVPack(VPackParser::fromJson(result.bodyRef()));
    result._body.clear();
  } catch (std::exception const& e) {
    LOG_TOPIC(ERR, Logger::AGENCYCOMM) << "Error transforming result: " << e.what();
    result.clear();
  } catch (...) {
    LOG_TOPIC(ERR, Logger::AGENCYCOMM)
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

  ONLY_IN_CLUSTER;

  if (args.Length() != 0) {
    TRI_V8_THROW_EXCEPTION_USAGE("endpoints()");
  }

  std::vector<std::string> endpoints = AgencyCommManager::MANAGER->endpoints();
  // make the list of endpoints unique
  std::sort(endpoints.begin(), endpoints.end());
  endpoints.assign(endpoints.begin(), std::unique(endpoints.begin(), endpoints.end()));

  v8::Handle<v8::Array> l = v8::Array::New(isolate);

  for (size_t i = 0; i < endpoints.size(); ++i) {
    std::string const endpoint = endpoints[i];

    l->Set((uint32_t)i, TRI_V8_STD_STRING(isolate, endpoint));
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

  std::string const prefix = AgencyCommManager::path();

  TRI_V8_RETURN_STD_STRING(prefix);
  TRI_V8_TRY_CATCH_END
}

////////////////////////////////////////////////////////////////////////////////
/// @brief creates a uniqid
////////////////////////////////////////////////////////////////////////////////

static void JS_UniqidAgency(v8::FunctionCallbackInfo<v8::Value> const& args) {
  TRI_V8_TRY_CATCH_BEGIN(isolate);
  v8::HandleScope scope(isolate);

  ONLY_IN_CLUSTER;

  if (args.Length() > 2) {
    TRI_V8_THROW_EXCEPTION_USAGE("uniqid(<count>, <timeout>)");
  }

  uint64_t count = 1;
  if (args.Length() > 0) {
    count = TRI_ObjectToUInt64(args[0], true);
  }

  if (count < 1 || count > 10000000) {
    TRI_V8_THROW_EXCEPTION_PARAMETER("<count> is invalid");
  }

  double timeout = 0.0;
  if (args.Length() > 1) {
    timeout = TRI_ObjectToDouble(args[1]);
  }

  AgencyComm comm;
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

  ONLY_IN_CLUSTER;

  if (args.Length() != 0) {
    TRI_V8_THROW_EXCEPTION_USAGE("version()");
  }

  AgencyComm comm;
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

  ONLY_IN_CLUSTER
  if (args.Length() != 1) {
    TRI_V8_THROW_EXCEPTION_USAGE("doesDatabaseExist(<database-id>)");
  }

  bool const result =
      ClusterInfo::instance()->doesDatabaseExist(TRI_ObjectToString(args[0]), true);

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

  if (args.Length() != 0) {
    TRI_V8_THROW_EXCEPTION_USAGE("databases()");
  }

  ONLY_IN_CLUSTER
  std::vector<DatabaseID> res = ClusterInfo::instance()->databases(true);
  v8::Handle<v8::Array> a = v8::Array::New(isolate, (int)res.size());
  std::vector<DatabaseID>::iterator it;
  int count = 0;
  for (it = res.begin(); it != res.end(); ++it) {
    a->Set((uint32_t)count++, TRI_V8_STD_STRING(isolate, (*it)));
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

  ONLY_IN_CLUSTER
  if (args.Length() != 0) {
    TRI_V8_THROW_EXCEPTION_USAGE("flush()");
  }

  ClusterInfo::instance()->flush();

  TRI_V8_RETURN_TRUE();
  TRI_V8_TRY_CATCH_END
}

////////////////////////////////////////////////////////////////////////////////
/// @brief get the info about a collection in Plan
////////////////////////////////////////////////////////////////////////////////

static void JS_GetCollectionInfoClusterInfo(v8::FunctionCallbackInfo<v8::Value> const& args) {
  TRI_V8_TRY_CATCH_BEGIN(isolate);
  v8::HandleScope scope(isolate);

  ONLY_IN_CLUSTER
  if (args.Length() != 2) {
    TRI_V8_THROW_EXCEPTION_USAGE(
        "getCollectionInfo(<database-id>, <collection-id>)");
  }

  auto databaseID = TRI_ObjectToString(args[0]);
  auto collectionID = TRI_ObjectToString(args[1]);
  std::shared_ptr<LogicalCollection> ci =
      ClusterInfo::instance()->getCollectionNT(databaseID, collectionID);
  if (ci == nullptr) {
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
  VPackBuilder infoBuilder = ci->toVelocyPackIgnore(ignoreKeys, false, false);
  VPackSlice info = infoBuilder.slice();

  TRI_ASSERT(info.isObject());
  v8::Handle<v8::Object> result = TRI_VPackToV8(isolate, info)->ToObject();

  // Compute ShardShorts
  auto serverAliases = ClusterInfo::instance()->getServerAliases();
  VPackSlice shards = info.get("shards");
  TRI_ASSERT(shards.isObject());
  v8::Handle<v8::Object> shardShorts = v8::Object::New(isolate);
  for (auto const& p : VPackObjectIterator(shards)) {
    TRI_ASSERT(p.value.isArray());
    v8::Handle<v8::Array> shorts = v8::Array::New(isolate);
    uint32_t pos = 0;
    for (auto const& s : VPackArrayIterator(p.value)) {
      try {
        std::string t = s.copyString();
        if (t.at(0) == '_') {
          t = t.substr(1);
        }
        shorts->Set(pos, TRI_V8_STD_STRING(isolate, serverAliases.at(t)));
        pos++;
      } catch (...) {
      }
    }
    shardShorts->Set(TRI_V8_STD_STRING(isolate, p.key.copyString()), shorts);
  }
  result->Set(TRI_V8_ASCII_STRING(isolate, "shardShorts"), shardShorts);
  TRI_V8_RETURN(result);
  TRI_V8_TRY_CATCH_END
}

////////////////////////////////////////////////////////////////////////////////
/// @brief get the info about a collection in Current
////////////////////////////////////////////////////////////////////////////////

static void JS_GetCollectionInfoCurrentClusterInfo(v8::FunctionCallbackInfo<v8::Value> const& args) {
  TRI_V8_TRY_CATCH_BEGIN(isolate);
  v8::HandleScope scope(isolate);

  ONLY_IN_CLUSTER
  if (args.Length() != 3) {
    TRI_V8_THROW_EXCEPTION_USAGE(
        "getCollectionInfoCurrent(<database-id>, <collection-id>, <shardID>)");
  }

  ShardID shardID = TRI_ObjectToString(args[2]);

  auto databaseID = TRI_ObjectToString(args[0]);
  auto collectionID = TRI_ObjectToString(args[1]);
  std::shared_ptr<LogicalCollection> ci =
      ClusterInfo::instance()->getCollectionNT(databaseID, collectionID);
  if (ci == nullptr) {
    TRI_V8_THROW_EXCEPTION_MESSAGE(TRI_ERROR_ARANGO_DATA_SOURCE_NOT_FOUND,
                                   ClusterInfo::getCollectionNotFoundMsg(databaseID, collectionID));
  }

  v8::Handle<v8::Object> result = v8::Object::New(isolate);
  // First some stuff from Plan for which Current does not make sense:
  auto cid = std::to_string(ci->id());
  std::string const& name = ci->name();
  result->Set(TRI_V8_ASCII_STRING(isolate, "id"), TRI_V8_STD_STRING(isolate, cid));
  result->Set(TRI_V8_ASCII_STRING(isolate, "name"), TRI_V8_STD_STRING(isolate, name));

  std::shared_ptr<CollectionInfoCurrent> cic =
      ClusterInfo::instance()->getCollectionCurrent(TRI_ObjectToString(args[0]), cid);

  result->Set(TRI_V8_ASCII_STRING(isolate, "currentVersion"),
              v8::Number::New(isolate, (double)cic->getCurrentVersion()));
  result->Set(TRI_V8_ASCII_STRING(isolate, "type"),
              v8::Number::New(isolate, (int)ci->type()));

  VPackSlice slice = cic->getIndexes(shardID);
  v8::Handle<v8::Value> indexes = TRI_VPackToV8(isolate, slice);
  result->Set(TRI_V8_ASCII_STRING(isolate, "indexes"), indexes);

  // Finally, report any possible error:
  bool error = cic->error(shardID);
  result->Set(TRI_V8_STD_STRING(isolate, StaticStrings::Error),
              v8::Boolean::New(isolate, error));
  if (error) {
    result->Set(TRI_V8_STD_STRING(isolate, StaticStrings::ErrorNum),
                v8::Number::New(isolate, cic->errorNum(shardID)));
    std::string const errorMessage = cic->errorMessage(shardID);
    result->Set(TRI_V8_STD_STRING(isolate, StaticStrings::ErrorMessage),
                TRI_V8_STD_STRING(isolate, errorMessage));
  }
  auto servers = cic->servers(shardID);
  v8::Handle<v8::Array> list =
      v8::Array::New(isolate, static_cast<int>(servers.size()));
  v8::Handle<v8::Array> shorts = v8::Array::New(isolate);
  auto serverAliases = ClusterInfo::instance()->getServerAliases();
  uint32_t pos = 0;
  for (auto const& s : servers) {
    try {
      shorts->Set(pos, TRI_V8_STD_STRING(isolate, serverAliases.at(s)));
    } catch (...) {
    }
    list->Set(pos, TRI_V8_STD_STRING(isolate, s));
    pos++;
  }
  result->Set(TRI_V8_ASCII_STRING(isolate, "servers"), list);
  result->Set(TRI_V8_ASCII_STRING(isolate, "shorts"), shorts);

  TRI_V8_RETURN(result);
  TRI_V8_TRY_CATCH_END
}

////////////////////////////////////////////////////////////////////////////////
/// @brief get the responsible server
////////////////////////////////////////////////////////////////////////////////

static void JS_GetResponsibleServerClusterInfo(v8::FunctionCallbackInfo<v8::Value> const& args) {
  TRI_V8_TRY_CATCH_BEGIN(isolate);
  v8::HandleScope scope(isolate);

  ONLY_IN_CLUSTER
  if (args.Length() != 1) {
    TRI_V8_THROW_EXCEPTION_USAGE("getResponsibleServer(<shard-id>)");
  }

  auto result =
      ClusterInfo::instance()->getResponsibleServer(TRI_ObjectToString(args[0]));
  v8::Handle<v8::Array> list = v8::Array::New(isolate, (int)result->size());
  uint32_t count = 0;
  for (auto const& s : *result) {
    list->Set(count++, TRI_V8_STD_STRING(isolate, s));
  }

  TRI_V8_RETURN(list);
  TRI_V8_TRY_CATCH_END
}

////////////////////////////////////////////////////////////////////////////////
/// @brief get the responsible shard
////////////////////////////////////////////////////////////////////////////////

static void JS_GetResponsibleShardClusterInfo(v8::FunctionCallbackInfo<v8::Value> const& args) {
  TRI_V8_TRY_CATCH_BEGIN(isolate);
  v8::HandleScope scope(isolate);

  ONLY_IN_CLUSTER
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
    documentIsComplete = TRI_ObjectToBoolean(args[2]);
  }

  VPackBuilder builder;
  int res = TRI_V8ToVPack(isolate, builder, args[1], false);

  if (res != TRI_ERROR_NO_ERROR) {
    TRI_V8_THROW_EXCEPTION(res);
  }

  ShardID shardId;
  CollectionID collectionId = TRI_ObjectToString(args[0]);
  auto& vocbase = GetContextVocBase(isolate);
  auto ci = ClusterInfo::instance();
  auto collInfo = ci->getCollectionNT(vocbase.name(), collectionId);
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
  result->Set(TRI_V8_ASCII_STRING(isolate, "shardId"), TRI_V8_STD_STRING(isolate, shardId));
  result->Set(TRI_V8_ASCII_STRING(isolate, "usesDefaultShardingAttributes"),
              v8::Boolean::New(isolate, usesDefaultShardingAttributes));

  TRI_V8_RETURN(result);
  TRI_V8_TRY_CATCH_END
}

////////////////////////////////////////////////////////////////////////////////
/// @brief get the server endpoint for a server
////////////////////////////////////////////////////////////////////////////////

static void JS_GetServerEndpointClusterInfo(v8::FunctionCallbackInfo<v8::Value> const& args) {
  TRI_V8_TRY_CATCH_BEGIN(isolate);
  v8::HandleScope scope(isolate);

  ONLY_IN_CLUSTER
  if (args.Length() != 1) {
    TRI_V8_THROW_EXCEPTION_USAGE("getServerEndpoint(<server-id>)");
  }

  std::string const result =
      ClusterInfo::instance()->getServerEndpoint(TRI_ObjectToString(args[0]));

  TRI_V8_RETURN_STD_STRING(result);
  TRI_V8_TRY_CATCH_END
}

////////////////////////////////////////////////////////////////////////////////
/// @brief get the server name for an endpoint
////////////////////////////////////////////////////////////////////////////////

static void JS_GetServerNameClusterInfo(v8::FunctionCallbackInfo<v8::Value> const& args) {
  TRI_V8_TRY_CATCH_BEGIN(isolate);
  v8::HandleScope scope(isolate);

  ONLY_IN_CLUSTER
  if (args.Length() != 1) {
    TRI_V8_THROW_EXCEPTION_USAGE("getServerName(<endpoint>)");
  }

  std::string const result =
      ClusterInfo::instance()->getServerName(TRI_ObjectToString(args[0]));

  TRI_V8_RETURN_STD_STRING(result);
  TRI_V8_TRY_CATCH_END
}

////////////////////////////////////////////////////////////////////////////////
/// @brief returns the DBServers currently registered
////////////////////////////////////////////////////////////////////////////////

static void JS_GetDBServers(v8::FunctionCallbackInfo<v8::Value> const& args) {
  TRI_V8_TRY_CATCH_BEGIN(isolate);
  v8::HandleScope scope(isolate);

  ONLY_IN_CLUSTER
  if (args.Length() != 0) {
    TRI_V8_THROW_EXCEPTION_USAGE("getDBServers()");
  }

  auto DBServers = ClusterInfo::instance()->getCurrentDBServers();
  auto serverAliases = ClusterInfo::instance()->getServerAliases();

  v8::Handle<v8::Array> l = v8::Array::New(isolate);

  for (size_t i = 0; i < DBServers.size(); ++i) {
    v8::Handle<v8::Object> result = v8::Object::New(isolate);
    auto id = DBServers[i];

    result->Set(TRI_V8_ASCII_STRING(isolate, "serverId"), TRI_V8_STD_STRING(isolate, id));

    auto itr = serverAliases.find(id);

    if (itr != serverAliases.end()) {
      result->Set(TRI_V8_ASCII_STRING(isolate, "serverName"),
                  TRI_V8_STD_STRING(isolate, itr->second));
    } else {
      result->Set(TRI_V8_ASCII_STRING(isolate, "serverName"),
                  TRI_V8_STD_STRING(isolate, id));
    }

    l->Set((uint32_t)i, result);
  }

  TRI_V8_RETURN(l);
  TRI_V8_TRY_CATCH_END
}

////////////////////////////////////////////////////////////////////////////////
/// @brief reloads the cache of DBServers currently registered
////////////////////////////////////////////////////////////////////////////////

static void JS_ReloadDBServers(v8::FunctionCallbackInfo<v8::Value> const& args) {
  TRI_V8_TRY_CATCH_BEGIN(isolate);
  v8::HandleScope scope(isolate);

  ONLY_IN_CLUSTER
  if (args.Length() != 0) {
    TRI_V8_THROW_EXCEPTION_USAGE("reloadDBServers()");
  }

  ClusterInfo::instance()->loadCurrentDBServers();
  TRI_V8_RETURN_UNDEFINED();
  TRI_V8_TRY_CATCH_END
}

////////////////////////////////////////////////////////////////////////////////
/// @brief returns the coordinators currently registered
////////////////////////////////////////////////////////////////////////////////

static void JS_GetCoordinators(v8::FunctionCallbackInfo<v8::Value> const& args) {
  TRI_V8_TRY_CATCH_BEGIN(isolate);
  v8::HandleScope scope(isolate);

  ONLY_IN_CLUSTER

  if (args.Length() != 0) {
    TRI_V8_THROW_EXCEPTION_USAGE("getCoordinators()");
  }

  std::vector<std::string> coordinators =
      ClusterInfo::instance()->getCurrentCoordinators();

  v8::Handle<v8::Array> l = v8::Array::New(isolate);

  for (size_t i = 0; i < coordinators.size(); ++i) {
    ServerID const sid = coordinators[i];

    l->Set((uint32_t)i, TRI_V8_STD_STRING(isolate, sid));
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
    count = TRI_ObjectToUInt64(args[0], true);
  }

  if (count == 0) {
    TRI_V8_THROW_EXCEPTION_PARAMETER("<count> is invalid");
  }

  uint64_t value = ClusterInfo::instance()->uniqid(count);

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

  ONLY_IN_CLUSTER
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
    TRI_V8_THROW_EXCEPTION_USAGE("setFoxxmasterQueueupdate(bool)");
  }

  bool queueUpdate = TRI_ObjectToBoolean(args[0]);
  ServerState::instance()->setFoxxmasterQueueupdate(queueUpdate);

  if (AgencyCommManager::isEnabled()) {
    AgencyComm comm;
    std::string key = "Current/FoxxmasterQueueupdate";
    VPackSlice val = queueUpdate ? VPackSlice::trueSlice() : VPackSlice::falseSlice();
    AgencyCommResult result = comm.setValue(key, val, 0.0);
    if (result.successful()) {
      result = comm.increment("Current/Version");
    }
    if (!result.successful() && result.errorCode() != TRI_ERROR_SHUTTING_DOWN &&
        !application_features::ApplicationServer::isStopping()) {
      // gracefully ignore any shutdown errors here
      THROW_AGENCY_EXCEPTION(result);
    }
  }

  TRI_V8_TRY_CATCH_END
}

////////////////////////////////////////////////////////////////////////////////
/// @brief return the primary servers id (only for secondaries)
////////////////////////////////////////////////////////////////////////////////

static void JS_IdOfPrimaryServerState(v8::FunctionCallbackInfo<v8::Value> const& args) {
  TRI_V8_TRY_CATCH_BEGIN(isolate);
  v8::HandleScope scope(isolate);

  ONLY_IN_CLUSTER
  if (args.Length() != 0) {
    TRI_V8_THROW_EXCEPTION_USAGE("idOfPrimary()");
  }

  TRI_V8_RETURN_STRING("");  // no more secondaries
  TRI_V8_TRY_CATCH_END
}

////////////////////////////////////////////////////////////////////////////////
/// @brief returns the javascript startup path
////////////////////////////////////////////////////////////////////////////////

static void JS_JavaScriptPathServerState(v8::FunctionCallbackInfo<v8::Value> const& args) {
  TRI_V8_TRY_CATCH_BEGIN(isolate);
  v8::HandleScope scope(isolate);

  if (args.Length() != 0) {
    TRI_V8_THROW_EXCEPTION_USAGE("javaScriptPath()");
  }

  std::string const path = ServerState::instance()->getJavaScriptPath();
  TRI_V8_RETURN_STD_STRING(path);
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

  std::string const role = TRI_ObjectToString(args[0]);
  ServerState::RoleEnum r = ServerState::stringToRole(role);

  if (r == ServerState::ROLE_UNDEFINED) {
    TRI_V8_THROW_EXCEPTION_PARAMETER("<role> is invalid");
  }

  ServerState::instance()->setRole(r);

  TRI_V8_RETURN_TRUE();
  TRI_V8_TRY_CATCH_END
}

////////////////////////////////////////////////////////////////////////////////
/// @brief redetermines the role from the agency
////////////////////////////////////////////////////////////////////////////////

static void JS_RedetermineRoleServerState(v8::FunctionCallbackInfo<v8::Value> const& args) {
  TRI_V8_TRY_CATCH_BEGIN(isolate);
  v8::HandleScope scope(isolate);

  ONLY_IN_CLUSTER
  if (args.Length() != 0) {
    TRI_V8_THROW_EXCEPTION_USAGE("redetermineRole()");
  }

  /*bool changed = ServerState::instance()->redetermineRole();
  if (changed) {
    TRI_V8_RETURN_TRUE();
  } else {

  }*/
  TRI_V8_RETURN_FALSE();
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
/// @brief prepare to send a request
///
/// this is used for asynchronous as well as synchronous requests.
////////////////////////////////////////////////////////////////////////////////

static void PrepareClusterCommRequest(v8::FunctionCallbackInfo<v8::Value> const& args,
                                      arangodb::rest::RequestType& reqType,
                                      std::string& destination,
                                      std::string& path, std::string& body,
                                      std::unordered_map<std::string, std::string>& headerFields,
                                      CoordTransactionID& coordTransactionID, double& timeout,
                                      bool& singleRequest, double& initTimeout) {
  v8::Isolate* isolate = args.GetIsolate();
  TRI_V8_CURRENT_GLOBALS_AND_SCOPE;

  ONLY_IN_CLUSTER
  TRI_ASSERT(args.Length() >= 4);

  reqType = arangodb::rest::RequestType::GET;
  if (args[0]->IsString()) {
    TRI_Utf8ValueNFC UTF8(args[0]);
    std::string methstring = *UTF8;
    reqType = arangodb::HttpRequest::translateMethod(methstring);
    if (reqType == arangodb::rest::RequestType::ILLEGAL) {
      reqType = arangodb::rest::RequestType::GET;
    }
  }

  destination = TRI_ObjectToString(args[1]);

  std::string dbname = TRI_ObjectToString(args[2]);

  path = TRI_ObjectToString(args[3]);
  path = "/_db/" + dbname + path;

  body.clear();
  if (!args[4]->IsUndefined()) {
    if (args[4]->IsObject() && V8Buffer::hasInstance(isolate, args[4])) {
      // supplied body is a Buffer object
      char const* data = V8Buffer::data(args[4].As<v8::Object>());
      size_t size = V8Buffer::length(args[4].As<v8::Object>());

      if (data == nullptr) {
        TRI_V8_THROW_EXCEPTION_MESSAGE(TRI_ERROR_BAD_PARAMETER,
                                       "invalid <body> buffer value");
      }

      body.assign(data, size);
    } else {
      body = TRI_ObjectToString(args[4]);
    }
  }

  if (args.Length() > 5 && args[5]->IsObject()) {
    v8::Handle<v8::Object> obj = args[5].As<v8::Object>();
    v8::Handle<v8::Array> props = obj->GetOwnPropertyNames();
    uint32_t i;
    for (i = 0; i < props->Length(); ++i) {
      v8::Handle<v8::Value> prop = props->Get(i);
      v8::Handle<v8::Value> val = obj->Get(prop);
      std::string propstring = TRI_ObjectToString(prop);
      std::string valstring = TRI_ObjectToString(val);
      if (propstring != "") {
        headerFields.insert(std::make_pair(propstring, valstring));
      }
    }
  }

  coordTransactionID = 0;
  timeout = 24 * 3600.0;
  singleRequest = false;

  if (args.Length() > 6 && args[6]->IsObject()) {
    v8::Handle<v8::Object> opt = args[6].As<v8::Object>();
    TRI_GET_GLOBAL_STRING(CoordTransactionIDKey);
    if (opt->Has(CoordTransactionIDKey)) {
      coordTransactionID = TRI_ObjectToUInt64(opt->Get(CoordTransactionIDKey), true);
    }
    TRI_GET_GLOBAL_STRING(TimeoutKey);
    if (opt->Has(TimeoutKey)) {
      timeout = TRI_ObjectToDouble(opt->Get(TimeoutKey));
    }
    TRI_GET_GLOBAL_STRING(SingleRequestKey);
    if (opt->Has(SingleRequestKey)) {
      singleRequest = TRI_ObjectToBoolean(opt->Get(SingleRequestKey));
    }
    TRI_GET_GLOBAL_STRING(InitTimeoutKey);
    if (opt->Has(InitTimeoutKey)) {
      initTimeout = TRI_ObjectToDouble(opt->Get(InitTimeoutKey));
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
                                                 ClusterCommResult const& res) {
  v8::Isolate* isolate = args.GetIsolate();
  TRI_V8_CURRENT_GLOBALS_AND_SCOPE;

  v8::Handle<v8::Object> r = v8::Object::New(isolate);
  if (res.dropped) {
    TRI_GET_GLOBAL_STRING(ErrorMessageKey);
    r->Set(ErrorMessageKey,
           TRI_V8_ASCII_STRING(isolate, "operation was dropped"));
  } else {
    // convert the ids to strings as uint64_t might be too big for JavaScript
    // numbers
    TRI_GET_GLOBAL_STRING(CoordTransactionIDKey);
    std::string id = StringUtils::itoa(res.coordTransactionID);
    r->Set(CoordTransactionIDKey, TRI_V8_STD_STRING(isolate, id));

    id = StringUtils::itoa(res.operationID);
    TRI_GET_GLOBAL_STRING(OperationIDKey);
    r->Set(OperationIDKey, TRI_V8_STD_STRING(isolate, id));
    TRI_GET_GLOBAL_STRING(EndpointKey);
    r->Set(EndpointKey, TRI_V8_STD_STRING(isolate, res.endpoint));
    TRI_GET_GLOBAL_STRING(SingleRequestKey);
    r->Set(SingleRequestKey, v8::Boolean::New(isolate, res.single));
    TRI_GET_GLOBAL_STRING(ShardIDKey);
    r->Set(ShardIDKey, TRI_V8_STD_STRING(isolate, res.shardID));

    if (res.status == CL_COMM_SUBMITTED) {
      TRI_GET_GLOBAL_STRING(StatusKey);
      r->Set(StatusKey, TRI_V8_ASCII_STRING(isolate, "SUBMITTED"));
    } else if (res.status == CL_COMM_SENDING) {
      TRI_GET_GLOBAL_STRING(StatusKey);
      r->Set(StatusKey, TRI_V8_ASCII_STRING(isolate, "SENDING"));
    } else if (res.status == CL_COMM_SENT) {
      TRI_GET_GLOBAL_STRING(StatusKey);
      r->Set(StatusKey, TRI_V8_ASCII_STRING(isolate, "SENT"));
      // This might be the result of a synchronous request or an asynchronous
      // request with the `singleRequest` flag true and thus contain the
      // actual response. If it is an asynchronous request which has not
      // yet been answered, the following information is probably rather
      // boring:

      // The headers:
      TRI_ASSERT(res.result != nullptr);
      v8::Handle<v8::Object> h = v8::Object::New(isolate);
      for (auto const& i : res.result->getHeaderFields()) {
        h->Set(TRI_V8_STD_STRING(isolate, i.first), TRI_V8_STD_STRING(isolate, i.second));
      }
      r->Set(TRI_V8_ASCII_STRING(isolate, "headers"), h);

      // The body:
      arangodb::basics::StringBuffer& body = res.result->getBody();
      if (body.length() != 0) {
        r->Set(TRI_V8_ASCII_STRING(isolate, "body"), TRI_V8_STD_STRING(isolate, body));
        V8Buffer* buffer = V8Buffer::New(isolate, body.c_str(), body.length());
        v8::Local<v8::Object> bufferObject =
            v8::Local<v8::Object>::New(isolate, buffer->_handle);
        r->Set(TRI_V8_ASCII_STRING(isolate, "rawBody"), bufferObject);
      }
    } else if (res.status == CL_COMM_TIMEOUT) {
      TRI_GET_GLOBAL_STRING(StatusKey);
      r->Set(StatusKey, TRI_V8_ASCII_STRING(isolate, "TIMEOUT"));
      TRI_GET_GLOBAL_STRING(TimeoutKey);
      r->Set(TimeoutKey, v8::BooleanObject::New(true));
    } else if (res.status == CL_COMM_ERROR) {
      TRI_GET_GLOBAL_STRING(StatusKey);
      r->Set(StatusKey, TRI_V8_ASCII_STRING(isolate, "ERROR"));

      if (res.result && res.result->isComplete()) {
        v8::Handle<v8::Object> details = v8::Object::New(isolate);
        details->Set(TRI_V8_ASCII_STRING(isolate, "code"),
                     v8::Number::New(isolate, res.result->getHttpReturnCode()));
        details->Set(TRI_V8_ASCII_STRING(isolate, "message"),
                     TRI_V8_STD_STRING(isolate, res.result->getHttpReturnMessage()));
        arangodb::basics::StringBuffer& body = res.result->getBody();
        details->Set(TRI_V8_ASCII_STRING(isolate, "body"), TRI_V8_STD_STRING(isolate, body));
        V8Buffer* buffer = V8Buffer::New(isolate, body.c_str(), body.length());
        v8::Local<v8::Object> bufferObject =
            v8::Local<v8::Object>::New(isolate, buffer->_handle);
        details->Set(TRI_V8_ASCII_STRING(isolate, "rawBody"), bufferObject);

        r->Set(TRI_V8_ASCII_STRING(isolate, "details"), details);
        TRI_GET_GLOBAL_STRING(ErrorMessageKey);
        r->Set(ErrorMessageKey, TRI_V8_STD_STRING(isolate, res.errorMessage));
      } else {
        TRI_GET_GLOBAL_STRING(ErrorMessageKey);
        r->Set(ErrorMessageKey, TRI_V8_STD_STRING(isolate, res.errorMessage));
      }
    } else if (res.status == CL_COMM_DROPPED) {
      TRI_GET_GLOBAL_STRING(StatusKey);
      r->Set(StatusKey, TRI_V8_ASCII_STRING(isolate, "DROPPED"));
      TRI_GET_GLOBAL_STRING(ErrorMessageKey);
      r->Set(ErrorMessageKey,
             TRI_V8_ASCII_STRING(isolate,
                                 "request dropped whilst waiting for answer"));
    } else if (res.status == CL_COMM_BACKEND_UNAVAILABLE) {
      TRI_GET_GLOBAL_STRING(StatusKey);
      r->Set(StatusKey, TRI_V8_ASCII_STRING(isolate, "BACKEND_UNAVAILABLE"));
      TRI_GET_GLOBAL_STRING(ErrorMessageKey);
      r->Set(ErrorMessageKey,
             TRI_V8_ASCII_STRING(isolate,
                                 "required backend was not available"));
    } else if (res.status == CL_COMM_RECEIVED) {  // Everything is OK
      // FIXME HANDLE VST
      auto httpRequest = std::dynamic_pointer_cast<HttpRequest>(res.answer);
      if (httpRequest == nullptr) {
        THROW_ARANGO_EXCEPTION_MESSAGE(TRI_ERROR_INTERNAL,
                                       "invalid request type");
      }

      // The headers:
      v8::Handle<v8::Object> h = v8::Object::New(isolate);
      TRI_GET_GLOBAL_STRING(StatusKey);
      r->Set(StatusKey, TRI_V8_ASCII_STRING(isolate, "RECEIVED"));
      TRI_ASSERT(res.answer != nullptr);
      std::unordered_map<std::string, std::string> headers = res.answer->headers();
      headers["content-length"] = StringUtils::itoa(httpRequest->contentLength());
      for (auto& it : headers) {
        h->Set(TRI_V8_STD_STRING(isolate, it.first), TRI_V8_STD_STRING(isolate, it.second));
      }
      r->Set(TRI_V8_ASCII_STRING(isolate, "headers"), h);

      // The body:
      std::string const& body = httpRequest->body();
      if (!body.empty()) {
        r->Set(TRI_V8_ASCII_STRING(isolate, "body"), TRI_V8_STD_STRING(isolate, body));
        V8Buffer* buffer = V8Buffer::New(isolate, body.c_str(), body.length());
        v8::Local<v8::Object> bufferObject =
            v8::Local<v8::Object>::New(isolate, buffer->_handle);
        r->Set(TRI_V8_ASCII_STRING(isolate, "rawBody"), bufferObject);
      }

    } else {
      TRI_V8_THROW_EXCEPTION_MESSAGE(TRI_ERROR_INTERNAL,
                                     "unknown ClusterComm result status");
    }
  }

  TRI_V8_RETURN(r);
}

////////////////////////////////////////////////////////////////////////////////
/// @brief send an asynchronous request
////////////////////////////////////////////////////////////////////////////////

static void JS_AsyncRequest(v8::FunctionCallbackInfo<v8::Value> const& args) {
  TRI_V8_TRY_CATCH_BEGIN(isolate);
  v8::HandleScope scope(isolate);
  ONLY_IN_CLUSTER

  if (args.Length() < 4 || args.Length() > 7) {
    TRI_V8_THROW_EXCEPTION_USAGE(
        "asyncRequest("
        "reqType, destination, dbname, path, body, headers, options)");
  }
  // Possible options:
  //   - coordTransactionID   (number)
  //   - timeout              (number)
  //   - singleRequest        (boolean) default is false
  //   - initTimeout          (number)

  auto cc = ClusterComm::instance();

  if (cc == nullptr) {
    TRI_V8_THROW_EXCEPTION_MESSAGE(
        TRI_ERROR_SHUTTING_DOWN,
        "clustercomm object not found (JS_AsyncRequest)");
  }

  arangodb::rest::RequestType reqType;
  std::string destination;
  std::string path;
  auto body = std::make_shared<std::string>();
  std::unordered_map<std::string, std::string> headerFields;
  CoordTransactionID coordTransactionID;
  double timeout;
  double initTimeout = -1.0;
  bool singleRequest = false;

  PrepareClusterCommRequest(args, reqType, destination, path, *body, headerFields,
                            coordTransactionID, timeout, singleRequest, initTimeout);

  OperationID opId = cc->asyncRequest(coordTransactionID, destination, reqType,
                                      path, body, headerFields, nullptr,
                                      timeout, singleRequest, initTimeout);
  ClusterCommResult res = cc->enquire(opId);
  if (res.status == CL_COMM_BACKEND_UNAVAILABLE) {
    TRI_V8_THROW_EXCEPTION_MESSAGE(TRI_ERROR_INTERNAL,
                                   "couldn't queue async request");
  }

  LOG_TOPIC(DEBUG, Logger::CLUSTER)
      << "JS_AsyncRequest: request has been submitted";

  Return_PrepareClusterCommResultForJS(args, res);
  TRI_V8_TRY_CATCH_END
}

////////////////////////////////////////////////////////////////////////////////
/// @brief send a synchronous request
////////////////////////////////////////////////////////////////////////////////

static void JS_SyncRequest(v8::FunctionCallbackInfo<v8::Value> const& args) {
  TRI_V8_TRY_CATCH_BEGIN(isolate);
  v8::HandleScope scope(isolate);
  ONLY_IN_CLUSTER

  if (args.Length() < 4 || args.Length() > 7) {
    TRI_V8_THROW_EXCEPTION_USAGE(
        "syncRequest("
        "reqType, destination, dbname, path, body, headers, options)");
  }
  // Possible options:
  //   - coordTransactionID   (number)
  //   - timeout              (number)

  // Disabled to allow communication originating in a DBserver:
  // 31.7.2014 Max

  // if (ServerState::instance()->getRole() != ServerState::ROLE_COORDINATOR) {
  //  TRI_V8_THROW_EXCEPTION_INTERNAL(scope,"request works only in coordinator
  //  role");
  //}

  auto cc = ClusterComm::instance();

  if (cc == nullptr) {
    TRI_V8_THROW_EXCEPTION_MESSAGE(TRI_ERROR_SHUTTING_DOWN,
                                   "clustercomm object not found");
  }

  arangodb::rest::RequestType reqType;
  std::string destination;
  std::string path;
  std::string body;
  auto headerFields = std::make_unique<std::unordered_map<std::string, std::string>>();
  CoordTransactionID coordTransactionID;
  double timeout;
  double initTimeout = -1.0;
  bool singleRequest = false;  // of no relevance here

  PrepareClusterCommRequest(args, reqType, destination, path, body, *headerFields,
                            coordTransactionID, timeout, singleRequest, initTimeout);

  std::unique_ptr<ClusterCommResult> res =
      cc->syncRequest(coordTransactionID, destination, reqType, path, body,
                      *headerFields, timeout);

  if (res.get() == nullptr) {
    TRI_V8_THROW_EXCEPTION_MESSAGE(TRI_ERROR_INTERNAL,
                                   "couldn't do sync request");
  }

  LOG_TOPIC(DEBUG, Logger::CLUSTER) << "JS_SyncRequest: request has been done";

  Return_PrepareClusterCommResultForJS(args, *res);
  TRI_V8_TRY_CATCH_END
}

////////////////////////////////////////////////////////////////////////////////
/// @brief enquire information about an asynchronous request
////////////////////////////////////////////////////////////////////////////////

static void JS_Enquire(v8::FunctionCallbackInfo<v8::Value> const& args) {
  TRI_V8_TRY_CATCH_BEGIN(isolate);
  v8::HandleScope scope(isolate);
  ONLY_IN_CLUSTER

  if (args.Length() != 1) {
    TRI_V8_THROW_EXCEPTION_USAGE("enquire(operationID)");
  }

  auto cc = ClusterComm::instance();

  if (cc == nullptr) {
    TRI_V8_THROW_EXCEPTION_MESSAGE(
        TRI_ERROR_INTERNAL, "clustercomm object not found (JS_SyncRequest)");
  }

  OperationID operationID = TRI_ObjectToUInt64(args[0], true);

  LOG_TOPIC(DEBUG, Logger::CLUSTER)
      << "JS_Enquire: calling ClusterComm::enquire()";

  ClusterCommResult const res = cc->enquire(operationID);

  Return_PrepareClusterCommResultForJS(args, res);
  TRI_V8_TRY_CATCH_END
}

////////////////////////////////////////////////////////////////////////////////
/// @brief wait for the result of an asynchronous request
////////////////////////////////////////////////////////////////////////////////

static void JS_Wait(v8::FunctionCallbackInfo<v8::Value> const& args) {
  TRI_V8_TRY_CATCH_BEGIN(isolate);
  TRI_V8_CURRENT_GLOBALS_AND_SCOPE;
  ONLY_IN_CLUSTER

  if (args.Length() != 1) {
    TRI_V8_THROW_EXCEPTION_USAGE("wait(obj)");
  }
  // Possible options:
  //   - clientTransactionID  (string)
  //   - coordTransactionID   (number)
  //   - operationID          (number)
  //   - shardID              (string)
  //   - timeout              (number)

  auto cc = ClusterComm::instance();

  if (cc == nullptr) {
    TRI_V8_THROW_EXCEPTION_MESSAGE(TRI_ERROR_SHUTTING_DOWN,
                                   "clustercomm object not found (JS_Wait)");
  }

  CoordTransactionID mycoordTransactionID = 0;
  OperationID myoperationID = 0;
  ShardID myshardID = "";
  double mytimeout = 24 * 3600.0;

  if (args[0]->IsObject()) {
    v8::Handle<v8::Object> obj = args[0].As<v8::Object>();
    TRI_GET_GLOBAL_STRING(CoordTransactionIDKey);
    if (obj->Has(CoordTransactionIDKey)) {
      mycoordTransactionID = TRI_ObjectToUInt64(obj->Get(CoordTransactionIDKey), true);
    }
    TRI_GET_GLOBAL_STRING(OperationIDKey);
    if (obj->Has(OperationIDKey)) {
      myoperationID = TRI_ObjectToUInt64(obj->Get(OperationIDKey), true);
    }
    TRI_GET_GLOBAL_STRING(ShardIDKey);
    if (obj->Has(ShardIDKey)) {
      myshardID = TRI_ObjectToString(obj->Get(ShardIDKey));
    }
    TRI_GET_GLOBAL_STRING(TimeoutKey);
    if (obj->Has(TimeoutKey)) {
      mytimeout = TRI_ObjectToDouble(obj->Get(TimeoutKey));
      if (mytimeout == 0.0) {
        mytimeout = 24 * 3600.0;
      }
    }
  }

  LOG_TOPIC(DEBUG, Logger::CLUSTER) << "JS_Wait: calling ClusterComm::wait()";

  ClusterCommResult const res =
      cc->wait(mycoordTransactionID, myoperationID, myshardID, mytimeout);

  Return_PrepareClusterCommResultForJS(args, res);
  TRI_V8_TRY_CATCH_END
}

////////////////////////////////////////////////////////////////////////////////
/// @brief drop the result of an asynchronous request
////////////////////////////////////////////////////////////////////////////////

static void JS_Drop(v8::FunctionCallbackInfo<v8::Value> const& args) {
  TRI_V8_TRY_CATCH_BEGIN(isolate);
  TRI_V8_CURRENT_GLOBALS_AND_SCOPE;
  ONLY_IN_CLUSTER

  if (args.Length() != 1) {
    TRI_V8_THROW_EXCEPTION_USAGE("drop(obj)");
  }
  // Possible options:
  //   - coordTransactionID   (number)
  //   - operationID          (number)
  //   - shardID              (string)

  auto cc = ClusterComm::instance();

  if (cc == nullptr) {
    TRI_V8_THROW_EXCEPTION_MESSAGE(TRI_ERROR_INTERNAL,
                                   "clustercomm object not found (JS_Drop)");
  }

  CoordTransactionID mycoordTransactionID = 0;
  OperationID myoperationID = 0;
  ShardID myshardID = "";

  if (args[0]->IsObject()) {
    v8::Handle<v8::Object> obj = args[0].As<v8::Object>();
    TRI_GET_GLOBAL_STRING(CoordTransactionIDKey);
    if (obj->Has(CoordTransactionIDKey)) {
      mycoordTransactionID = TRI_ObjectToUInt64(obj->Get(CoordTransactionIDKey), true);
    }
    TRI_GET_GLOBAL_STRING(OperationIDKey);
    if (obj->Has(OperationIDKey)) {
      myoperationID = TRI_ObjectToUInt64(obj->Get(OperationIDKey), true);
    }
    TRI_GET_GLOBAL_STRING(ShardIDKey);
    if (obj->Has(ShardIDKey)) {
      myshardID = TRI_ObjectToString(obj->Get(ShardIDKey));
    }
  }

  LOG_TOPIC(DEBUG, Logger::CLUSTER) << "JS_Drop: calling ClusterComm::drop()";

  cc->drop(mycoordTransactionID, myoperationID, myshardID);

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

  AuthenticationFeature* af = AuthenticationFeature::instance();
  if (af != nullptr && af->isActive()) {
    // mop: really quick and dirty
    v8::Handle<v8::Object> options = v8::Object::New(isolate);
    v8::Handle<v8::Object> headers = v8::Object::New(isolate);
    if (args.Length() > 2) {
      if (args[2]->IsObject()) {
        options = v8::Handle<v8::Object>::Cast(args[2]);
        if (options->Has(TRI_V8_ASCII_STRING(isolate, "headers"))) {
          headers = v8::Handle<v8::Object>::Cast(
              options->Get(TRI_V8_ASCII_STRING(isolate, "headers")));
        }
      }
    }
    options->Set(TRI_V8_ASCII_STRING(isolate, "headers"), headers);

    std::string authorization = "bearer " + af->tokenCache().jwtToken();
    v8::Handle<v8::String> v8Authorization = TRI_V8_STD_STRING(isolate, authorization);
    headers->Set(TRI_V8_ASCII_STRING(isolate, "Authorization"), v8Authorization);

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
  ONLY_IN_CLUSTER

  v8::HandleScope scope(isolate);
  auto& vocbase = GetContextVocBase(isolate);
  auto reporter = cluster::ShardDistributionReporter::instance();
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
  ONLY_IN_CLUSTER

  if (args.Length() != 1) {
    TRI_V8_THROW_EXCEPTION_USAGE(
        "GetCollectionShardDistribution(<collectionName>)");
  }

  std::string const colName = TRI_ObjectToString(args[0]);

  v8::HandleScope scope(isolate);
  auto& vocbase = GetContextVocBase(isolate);
  auto reporter = cluster::ShardDistributionReporter::instance();
  VPackBuilder result;

  reporter->getCollectionDistributionForDatabase(vocbase.name(), colName, result);

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
                               ft->GetFunction(), true);

  // register the global object
  v8::Handle<v8::Object> aa = rt->NewInstance();
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
                       TRI_V8_ASCII_STRING(isolate, "reloadDBServers"), JS_ReloadDBServers);
  TRI_AddMethodVocbase(isolate, rt,
                       TRI_V8_ASCII_STRING(isolate, "getCoordinators"), JS_GetCoordinators);
  TRI_AddMethodVocbase(isolate, rt, TRI_V8_ASCII_STRING(isolate, "uniqid"), JS_UniqidClusterInfo);

  v8g->ClusterInfoTempl.Reset(isolate, rt);
  TRI_AddGlobalFunctionVocbase(isolate,
                               TRI_V8_ASCII_STRING(isolate,
                                                   "ArangoClusterInfoCtor"),
                               ft->GetFunction(), true);

  // register the global object
  v8::Handle<v8::Object> ci = rt->NewInstance();
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
  TRI_AddMethodVocbase(isolate, rt, TRI_V8_ASCII_STRING(isolate, "idOfPrimary"),
                       JS_IdOfPrimaryServerState);
  TRI_AddMethodVocbase(isolate, rt,
                       TRI_V8_ASCII_STRING(isolate, "javaScriptPath"),
                       JS_JavaScriptPathServerState);
  TRI_AddMethodVocbase(isolate, rt, TRI_V8_ASCII_STRING(isolate, "initialized"),
                       JS_InitializedServerState);
  TRI_AddMethodVocbase(isolate, rt,
                       TRI_V8_ASCII_STRING(isolate, "isCoordinator"),
                       JS_IsCoordinatorServerState);
  TRI_AddMethodVocbase(isolate, rt, TRI_V8_ASCII_STRING(isolate, "role"), JS_RoleServerState);
  TRI_AddMethodVocbase(isolate, rt, TRI_V8_ASCII_STRING(isolate, "setRole"),
                       JS_SetRoleServerState, true);
  TRI_AddMethodVocbase(isolate, rt,
                       TRI_V8_ASCII_STRING(isolate, "redetermineRole"),
                       JS_RedetermineRoleServerState, true);
  TRI_AddMethodVocbase(isolate, rt, TRI_V8_ASCII_STRING(isolate, "status"), JS_StatusServerState);

  v8g->ServerStateTempl.Reset(isolate, rt);
  TRI_AddGlobalFunctionVocbase(isolate,
                               TRI_V8_ASCII_STRING(isolate,
                                                   "ArangoServerStateCtor"),
                               ft->GetFunction(), true);

  // register the global object
  v8::Handle<v8::Object> ss = rt->NewInstance();
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
  TRI_AddMethodVocbase(isolate, rt, TRI_V8_ASCII_STRING(isolate, "syncRequest"), JS_SyncRequest);
  TRI_AddMethodVocbase(isolate, rt, TRI_V8_ASCII_STRING(isolate, "enquire"), JS_Enquire);
  TRI_AddMethodVocbase(isolate, rt, TRI_V8_ASCII_STRING(isolate, "wait"), JS_Wait);
  TRI_AddMethodVocbase(isolate, rt, TRI_V8_ASCII_STRING(isolate, "drop"), JS_Drop);
  TRI_AddMethodVocbase(isolate, rt, TRI_V8_ASCII_STRING(isolate, "getId"), JS_GetId);

  v8g->ClusterCommTempl.Reset(isolate, rt);
  TRI_AddGlobalFunctionVocbase(isolate,
                               TRI_V8_ASCII_STRING(isolate,
                                                   "ArangoClusterCommCtor"),
                               ft->GetFunction(), true);

  // register the global object
  ss = rt->NewInstance();
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
