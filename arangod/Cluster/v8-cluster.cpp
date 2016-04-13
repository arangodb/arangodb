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

#include "v8-cluster.h"
#include "Cluster/AgencyComm.h"
#include "Cluster/ClusterInfo.h"
#include "Cluster/ServerState.h"
#include "Cluster/ClusterComm.h"
#include "V8/v8-buffer.h"
#include "V8/v8-conv.h"
#include "V8/v8-globals.h"
#include "V8/v8-utils.h"
#include "V8/v8-vpack.h"
#include "VocBase/server.h"

using namespace arangodb;
using namespace arangodb::basics;

////////////////////////////////////////////////////////////////////////////////
/// @brief creates a v8 exception object
////////////////////////////////////////////////////////////////////////////////

#define THROW_AGENCY_EXCEPTION(data) \
  CreateAgencyException(args, data); \
  return;

static void CreateAgencyException(
    v8::FunctionCallbackInfo<v8::Value> const& args,
    AgencyCommResult const& result) {
  v8::Isolate* isolate = args.GetIsolate();
  TRI_V8_CURRENT_GLOBALS_AND_SCOPE;

  std::string const errorDetails = result.errorDetails();
  v8::Handle<v8::String> errorMessage = TRI_V8_STD_STRING(errorDetails);
  v8::Handle<v8::Object> errorObject =
      v8::Exception::Error(errorMessage)->ToObject();

  errorObject->Set(TRI_V8_ASCII_STRING("code"),
                   v8::Number::New(isolate, result.httpCode()));
  errorObject->Set(TRI_V8_ASCII_STRING("errorNum"),
                   v8::Number::New(isolate, result.errorCode()));
  errorObject->Set(TRI_V8_ASCII_STRING("errorMessage"), errorMessage);
  errorObject->Set(TRI_V8_ASCII_STRING("error"), v8::True(isolate));

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

static void JS_CreateDirectoryAgency(
    v8::FunctionCallbackInfo<v8::Value> const& args) {
  TRI_V8_TRY_CATCH_BEGIN(isolate);
  v8::HandleScope scope(isolate);

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

static void JS_IsEnabledAgency(
    v8::FunctionCallbackInfo<v8::Value> const& args) {
  TRI_V8_TRY_CATCH_BEGIN(isolate)
  v8::HandleScope scope(isolate);

  if (args.Length() != 0) {
    TRI_V8_THROW_EXCEPTION_USAGE("isEnabled()");
  }

  std::string const prefix = AgencyComm::prefix();

  if (!prefix.empty()) {
    TRI_V8_RETURN_TRUE();
  }
  TRI_V8_RETURN_FALSE();
  TRI_V8_TRY_CATCH_END
}

////////////////////////////////////////////////////////////////////////////////
/// @brief increase the version number
////////////////////////////////////////////////////////////////////////////////

static void JS_IncreaseVersionAgency(
    v8::FunctionCallbackInfo<v8::Value> const& args) {
  TRI_V8_TRY_CATCH_BEGIN(isolate)
  v8::HandleScope scope(isolate);

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

  if (args.Length() < 1) {
    TRI_V8_THROW_EXCEPTION_USAGE("get(<key>, <recursive>, <withIndexes>)");
  }

  std::string const key = TRI_ObjectToString(args[0]);
  bool recursive = false;
  bool withIndexes = false;

  if (args.Length() > 1) {
    recursive = TRI_ObjectToBoolean(args[1]);
  }
  if (args.Length() > 2) {
    withIndexes = TRI_ObjectToBoolean(args[2]);
  }

  AgencyComm comm;
  AgencyCommResult result = comm.getValues(key, recursive);

  if (!result.successful()) {
    THROW_AGENCY_EXCEPTION(result);
  }

  result.parse("", false);

  v8::Handle<v8::Object> l = v8::Object::New(isolate);

  if (withIndexes) {
    std::map<std::string, AgencyCommResultEntry>::const_iterator it =
        result._values.begin();

    while (it != result._values.end()) {
      std::string const key = (*it).first;
      VPackSlice const slice = it->second._vpack->slice();
      std::string const idx = StringUtils::itoa((*it).second._index);

      if (!slice.isNone()) {
        v8::Handle<v8::Object> sub = v8::Object::New(isolate);

        sub->Set(TRI_V8_ASCII_STRING("value"), TRI_VPackToV8(isolate, slice));
        sub->Set(TRI_V8_ASCII_STRING("index"), TRI_V8_STD_STRING(idx));

        l->Set(TRI_V8_STD_STRING(key), sub);
      }

      ++it;
    }
  } else {
    // return just the value for each key
    std::map<std::string, AgencyCommResultEntry>::const_iterator it =
        result._values.begin();

    while (it != result._values.end()) {
      std::string const key = (*it).first;
      VPackSlice const slice = it->second._vpack->slice();

      if (!slice.isNone()) {
        l->ForceSet(TRI_V8_STD_STRING(key), TRI_VPackToV8(isolate, slice));
      }

      ++it;
    }
  }

  TRI_V8_RETURN(l);
  TRI_V8_TRY_CATCH_END
}

////////////////////////////////////////////////////////////////////////////////
/// @brief lists a directory from the agency
////////////////////////////////////////////////////////////////////////////////

static void JS_ListAgency(v8::FunctionCallbackInfo<v8::Value> const& args) {
  TRI_V8_TRY_CATCH_BEGIN(isolate)
  v8::HandleScope scope(isolate);

  if (args.Length() < 1) {
    TRI_V8_THROW_EXCEPTION_USAGE("list(<key>, <recursive>, <flat>)");
  }

  std::string const key = TRI_ObjectToString(args[0]);
  bool recursive = false;

  if (args.Length() > 1) {
    recursive = TRI_ObjectToBoolean(args[1]);
  }

  bool flat = false;
  if (args.Length() > 2) {
    flat = TRI_ObjectToBoolean(args[2]);
  }

  AgencyComm comm;
  AgencyCommResult result = comm.getValues(key, recursive);

  if (!result.successful()) {
    THROW_AGENCY_EXCEPTION(result);
  }

  // return just the value for each key
  result.parse("", true);
  std::map<std::string, AgencyCommResultEntry>::const_iterator it =
      result._values.begin();

  // skip first entry
  if (it != result._values.end()) {
    ++it;
  }

  if (flat) {
    v8::Handle<v8::Array> l = v8::Array::New(isolate);

    uint32_t i = 0;
    while (it != result._values.end()) {
      std::string const key = (*it).first;

      l->Set(i++, TRI_V8_STD_STRING(key));
      ++it;
    }
    TRI_V8_RETURN(l);
  }

  else {
    v8::Handle<v8::Object> l = v8::Object::New(isolate);

    while (it != result._values.end()) {
      std::string const key = (*it).first;
      bool const isDirectory = (*it).second._isDir;

      l->Set(TRI_V8_STD_STRING(key), v8::Boolean::New(isolate, isDirectory));
      ++it;
    }
    TRI_V8_RETURN(l);
  }
  TRI_V8_TRY_CATCH_END
}

////////////////////////////////////////////////////////////////////////////////
/// @brief acquires a read-lock in the agency
////////////////////////////////////////////////////////////////////////////////

static void JS_LockReadAgency(v8::FunctionCallbackInfo<v8::Value> const& args) {
  TRI_V8_TRY_CATCH_BEGIN(isolate);
  v8::HandleScope scope(isolate);

  if (args.Length() < 1) {
    TRI_V8_THROW_EXCEPTION_USAGE("lockRead(<part>, <ttl>, <timeout>)");
  }

  std::string const part = TRI_ObjectToString(args[0]);

  double ttl = 0.0;
  if (args.Length() > 1) {
    ttl = TRI_ObjectToDouble(args[1]);
  }

  double timeout = 0.0;
  if (args.Length() > 2) {
    timeout = TRI_ObjectToDouble(args[2]);
  }

  AgencyComm comm;
  if (!comm.lockRead(part, ttl, timeout)) {
    TRI_V8_THROW_EXCEPTION_MESSAGE(TRI_ERROR_INTERNAL,
                                   "unable to acquire lock");
  }

  TRI_V8_RETURN_TRUE();
  TRI_V8_TRY_CATCH_END
}

////////////////////////////////////////////////////////////////////////////////
/// @brief acquires a write-lock in the agency
////////////////////////////////////////////////////////////////////////////////

static void JS_LockWriteAgency(
    v8::FunctionCallbackInfo<v8::Value> const& args) {
  TRI_V8_TRY_CATCH_BEGIN(isolate);
  v8::HandleScope scope(isolate);

  if (args.Length() < 1) {
    TRI_V8_THROW_EXCEPTION_USAGE("lockWrite(<part>, <ttl>, <timeout>)");
  }

  std::string const part = TRI_ObjectToString(args[0]);

  double ttl = 0.0;
  if (args.Length() > 1) {
    ttl = TRI_ObjectToDouble(args[1]);
  }

  double timeout = 0.0;
  if (args.Length() > 2) {
    timeout = TRI_ObjectToDouble(args[2]);
  }

  AgencyComm comm;
  if (!comm.lockWrite(part, ttl, timeout)) {
    TRI_V8_THROW_EXCEPTION_MESSAGE(TRI_ERROR_INTERNAL,
                                   "unable to acquire lock");
  }

  TRI_V8_RETURN_TRUE();
  TRI_V8_TRY_CATCH_END
}

////////////////////////////////////////////////////////////////////////////////
/// @brief releases a read-lock in the agency
////////////////////////////////////////////////////////////////////////////////

static void JS_UnlockReadAgency(
    v8::FunctionCallbackInfo<v8::Value> const& args) {
  TRI_V8_TRY_CATCH_BEGIN(isolate);
  v8::HandleScope scope(isolate);

  if (args.Length() > 2) {
    TRI_V8_THROW_EXCEPTION_USAGE("unlockRead(<part>, <timeout>)");
  }

  std::string const part = TRI_ObjectToString(args[0]);

  double timeout = 0.0;
  if (args.Length() > 1) {
    timeout = TRI_ObjectToDouble(args[1]);
  }

  AgencyComm comm;
  if (!comm.unlockRead(part, timeout)) {
    TRI_V8_THROW_EXCEPTION_MESSAGE(TRI_ERROR_INTERNAL,
                                   "unable to release lock");
  }

  TRI_V8_RETURN_TRUE();
  TRI_V8_TRY_CATCH_END
}

////////////////////////////////////////////////////////////////////////////////
/// @brief releases a write-lock in the agency
////////////////////////////////////////////////////////////////////////////////

static void JS_UnlockWriteAgency(
    v8::FunctionCallbackInfo<v8::Value> const& args) {
  TRI_V8_TRY_CATCH_BEGIN(isolate);
  v8::HandleScope scope(isolate);

  if (args.Length() > 2) {
    TRI_V8_THROW_EXCEPTION_USAGE("unlockWrite(<part>, <timeout>)");
  }

  std::string const part = TRI_ObjectToString(args[0]);

  double timeout = 0.0;
  if (args.Length() > 1) {
    timeout = TRI_ObjectToDouble(args[1]);
  }

  AgencyComm comm;
  if (!comm.unlockWrite(part, timeout)) {
    TRI_V8_THROW_EXCEPTION_MESSAGE(TRI_ERROR_INTERNAL,
                                   "unable to release lock");
  }

  TRI_V8_RETURN_TRUE();
  TRI_V8_TRY_CATCH_END
}

////////////////////////////////////////////////////////////////////////////////
/// @brief removes a value from the agency
////////////////////////////////////////////////////////////////////////////////

static void JS_RemoveAgency(v8::FunctionCallbackInfo<v8::Value> const& args) {
  TRI_V8_TRY_CATCH_BEGIN(isolate);
  v8::HandleScope scope(isolate);

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
/// @brief returns the agency endpoints
////////////////////////////////////////////////////////////////////////////////

static void JS_EndpointsAgency(
    v8::FunctionCallbackInfo<v8::Value> const& args) {
  TRI_V8_TRY_CATCH_BEGIN(isolate);
  v8::HandleScope scope(isolate);

  if (args.Length() != 0) {
    TRI_V8_THROW_EXCEPTION_USAGE("endpoints()");
  }

  std::vector<std::string> endpoints = AgencyComm::getEndpoints();
  // make the list of endpoints unique
  std::sort(endpoints.begin(), endpoints.end());
  endpoints.assign(endpoints.begin(),
                   std::unique(endpoints.begin(), endpoints.end()));

  v8::Handle<v8::Array> l = v8::Array::New(isolate);

  for (size_t i = 0; i < endpoints.size(); ++i) {
    std::string const endpoint = endpoints[i];

    l->Set((uint32_t)i, TRI_V8_STD_STRING(endpoint));
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

  if (args.Length() > 1) {
    TRI_V8_THROW_EXCEPTION_USAGE("prefix(<strip>)");
  }

  bool strip = false;
  if (args.Length() > 0) {
    strip = TRI_ObjectToBoolean(args[0]);
  }

  std::string const prefix = AgencyComm::prefix();

  if (strip && prefix.size() > 2) {
    TRI_V8_RETURN_PAIR_STRING(prefix.c_str() + 1, (int)prefix.size() - 2);
  }

  TRI_V8_RETURN_STD_STRING(prefix);
  TRI_V8_TRY_CATCH_END
}

////////////////////////////////////////////////////////////////////////////////
/// @brief sets the agency prefix
////////////////////////////////////////////////////////////////////////////////

static void JS_SetPrefixAgency(
    v8::FunctionCallbackInfo<v8::Value> const& args) {
  TRI_V8_TRY_CATCH_BEGIN(isolate);
  v8::HandleScope scope(isolate);

  if (args.Length() != 1) {
    TRI_V8_THROW_EXCEPTION_USAGE("setPrefix(<prefix>)");
  }

  bool const result = AgencyComm::setPrefix(TRI_ObjectToString(args[0]));

  if (result) {
    TRI_V8_RETURN_TRUE();
  }
  TRI_V8_RETURN_FALSE();
  TRI_V8_TRY_CATCH_END
}

////////////////////////////////////////////////////////////////////////////////
/// @brief creates a uniqid
////////////////////////////////////////////////////////////////////////////////

static void JS_UniqidAgency(v8::FunctionCallbackInfo<v8::Value> const& args) {
  TRI_V8_TRY_CATCH_BEGIN(isolate);
  v8::HandleScope scope(isolate);

  if (args.Length() < 1 || args.Length() > 3) {
    TRI_V8_THROW_EXCEPTION_USAGE("uniqid(<key>, <count>, <timeout>)");
  }

  std::string const key = TRI_ObjectToString(args[0]);

  uint64_t count = 1;
  if (args.Length() > 1) {
    count = TRI_ObjectToUInt64(args[1], true);
  }

  if (count < 1 || count > 10000000) {
    TRI_V8_THROW_EXCEPTION_PARAMETER("<count> is invalid");
  }

  double timeout = 0.0;
  if (args.Length() > 2) {
    timeout = TRI_ObjectToDouble(args[2]);
  }

  AgencyComm comm;
  AgencyCommResult result = comm.uniqid(key, count, timeout);

  if (!result.successful() || result._index == 0) {
    THROW_AGENCY_EXCEPTION(result);
  }

  std::string const value = StringUtils::itoa(result._index);

  TRI_V8_RETURN_STD_STRING(value);
  TRI_V8_TRY_CATCH_END
}

////////////////////////////////////////////////////////////////////////////////
/// @brief returns the agency version
////////////////////////////////////////////////////////////////////////////////

static void JS_VersionAgency(v8::FunctionCallbackInfo<v8::Value> const& args) {
  TRI_V8_TRY_CATCH_BEGIN(isolate);
  v8::HandleScope scope(isolate);

  if (args.Length() != 0) {
    TRI_V8_THROW_EXCEPTION_USAGE("version()");
  }

  AgencyComm comm;
  std::string const version = comm.getVersion();

  TRI_V8_RETURN_STD_STRING(version);
  TRI_V8_TRY_CATCH_END
}

////////////////////////////////////////////////////////////////////////////////
/// @brief whether or not a specific database exists
////////////////////////////////////////////////////////////////////////////////

static void JS_DoesDatabaseExistClusterInfo(
    v8::FunctionCallbackInfo<v8::Value> const& args) {
  TRI_V8_TRY_CATCH_BEGIN(isolate);
  v8::HandleScope scope(isolate);

  if (args.Length() != 1) {
    TRI_V8_THROW_EXCEPTION_USAGE("doesDatabaseExist(<database-id>)");
  }

  bool const result = ClusterInfo::instance()->doesDatabaseExist(
      TRI_ObjectToString(args[0]), true);

  if (result) {
    TRI_V8_RETURN_TRUE();
  }
  TRI_V8_RETURN_FALSE();
  TRI_V8_TRY_CATCH_END
}

////////////////////////////////////////////////////////////////////////////////
/// @brief get the list of databases in the cluster
////////////////////////////////////////////////////////////////////////////////

static void JS_ListDatabases(v8::FunctionCallbackInfo<v8::Value> const& args) {
  TRI_V8_TRY_CATCH_BEGIN(isolate);
  v8::HandleScope scope(isolate);

  if (args.Length() != 0) {
    TRI_V8_THROW_EXCEPTION_USAGE("listDatabases()");
  }

  std::vector<DatabaseID> res = ClusterInfo::instance()->listDatabases(true);
  v8::Handle<v8::Array> a = v8::Array::New(isolate, (int)res.size());
  std::vector<DatabaseID>::iterator it;
  int count = 0;
  for (it = res.begin(); it != res.end(); ++it) {
    a->Set((uint32_t)count++, TRI_V8_STD_STRING((*it)));
  }
  TRI_V8_RETURN(a);
  TRI_V8_TRY_CATCH_END
}

////////////////////////////////////////////////////////////////////////////////
/// @brief flush the caches (used for testing only)
////////////////////////////////////////////////////////////////////////////////

static void JS_FlushClusterInfo(
    v8::FunctionCallbackInfo<v8::Value> const& args) {
  TRI_V8_TRY_CATCH_BEGIN(isolate);
  v8::HandleScope scope(isolate);

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

static void JS_GetCollectionInfoClusterInfo(
    v8::FunctionCallbackInfo<v8::Value> const& args) {
  TRI_V8_TRY_CATCH_BEGIN(isolate);
  v8::HandleScope scope(isolate);

  if (args.Length() != 2) {
    TRI_V8_THROW_EXCEPTION_USAGE(
        "getCollectionInfo(<database-id>, <collection-id>)");
  }

  std::shared_ptr<CollectionInfo> ci = ClusterInfo::instance()->getCollection(
      TRI_ObjectToString(args[0]), TRI_ObjectToString(args[1]));

  v8::Handle<v8::Object> result = v8::Object::New(isolate);
  std::string const cid = arangodb::basics::StringUtils::itoa(ci->id());
  std::string const& name = ci->name();
  result->Set(TRI_V8_ASCII_STRING("id"), TRI_V8_STD_STRING(cid));
  result->Set(TRI_V8_ASCII_STRING("name"), TRI_V8_STD_STRING(name));
  result->Set(TRI_V8_ASCII_STRING("type"),
              v8::Number::New(isolate, (int)ci->type()));
  result->Set(TRI_V8_ASCII_STRING("status"),
              v8::Number::New(isolate, (int)ci->status()));

  std::string const statusString = ci->statusString();
  result->Set(TRI_V8_ASCII_STRING("statusString"),
              TRI_V8_STD_STRING(statusString));

  result->Set(TRI_V8_ASCII_STRING("deleted"),
              v8::Boolean::New(isolate, ci->deleted()));
  result->Set(TRI_V8_ASCII_STRING("doCompact"),
              v8::Boolean::New(isolate, ci->doCompact()));
  result->Set(TRI_V8_ASCII_STRING("isSystem"),
              v8::Boolean::New(isolate, ci->isSystem()));
  result->Set(TRI_V8_ASCII_STRING("isVolatile"),
              v8::Boolean::New(isolate, ci->isVolatile()));
  result->Set(TRI_V8_ASCII_STRING("waitForSync"),
              v8::Boolean::New(isolate, ci->waitForSync()));
  result->Set(TRI_V8_ASCII_STRING("journalSize"),
              v8::Number::New(isolate, ci->journalSize()));
  result->Set(TRI_V8_ASCII_STRING("replicationFactor"),
              v8::Number::New(isolate, ci->replicationFactor()));
  result->Set(TRI_V8_ASCII_STRING("replicationQuorum"),
              v8::Number::New(isolate, ci->replicationQuorum()));

  std::vector<std::string> const& sks = ci->shardKeys();
  v8::Handle<v8::Array> shardKeys = v8::Array::New(isolate, (int)sks.size());
  for (uint32_t i = 0, n = (uint32_t)sks.size(); i < n; ++i) {
    shardKeys->Set(i, TRI_V8_STD_STRING(sks[i]));
  }
  result->Set(TRI_V8_ASCII_STRING("shardKeys"), shardKeys);

  auto shardMap = ci->shardIds();
  v8::Handle<v8::Object> shardIds = v8::Object::New(isolate);
  for (auto const& p : *shardMap) {
    v8::Handle<v8::Array> list = v8::Array::New(isolate, (int)p.second.size());
    uint32_t pos = 0;
    for (auto const& s : p.second) {
      list->Set(pos++, TRI_V8_STD_STRING(s));
    }
    shardIds->Set(TRI_V8_STD_STRING(p.first), list);
  }
  result->Set(TRI_V8_ASCII_STRING("shards"), shardIds);

  v8::Handle<v8::Value> indexes = TRI_ObjectJson(isolate, ci->getIndexes());
  result->Set(TRI_V8_ASCII_STRING("indexes"), indexes);

  TRI_V8_RETURN(result);
  TRI_V8_TRY_CATCH_END
}

////////////////////////////////////////////////////////////////////////////////
/// @brief get the info about a collection in Current
////////////////////////////////////////////////////////////////////////////////

static void JS_GetCollectionInfoCurrentClusterInfo(
    v8::FunctionCallbackInfo<v8::Value> const& args) {
  TRI_V8_TRY_CATCH_BEGIN(isolate);
  v8::HandleScope scope(isolate);

  if (args.Length() != 3) {
    TRI_V8_THROW_EXCEPTION_USAGE(
        "getCollectionInfoCurrent(<database-id>, <collection-id>, <shardID>)");
  }

  ShardID shardID = TRI_ObjectToString(args[2]);

  std::shared_ptr<CollectionInfo> ci = ClusterInfo::instance()->getCollection(
      TRI_ObjectToString(args[0]), TRI_ObjectToString(args[1]));

  v8::Handle<v8::Object> result = v8::Object::New(isolate);
  // First some stuff from Plan for which Current does not make sense:
  std::string const cid = arangodb::basics::StringUtils::itoa(ci->id());
  std::string const& name = ci->name();
  result->Set(TRI_V8_ASCII_STRING("id"), TRI_V8_STD_STRING(cid));
  result->Set(TRI_V8_ASCII_STRING("name"), TRI_V8_STD_STRING(name));

  std::shared_ptr<CollectionInfoCurrent> cic =
      ClusterInfo::instance()->getCollectionCurrent(TRI_ObjectToString(args[0]),
                                                    cid);

  result->Set(TRI_V8_ASCII_STRING("type"),
              v8::Number::New(isolate, (int)ci->type()));

  TRI_json_t const* json = cic->getIndexes(shardID);
  v8::Handle<v8::Value> indexes = TRI_ObjectJson(isolate, json);
  result->Set(TRI_V8_ASCII_STRING("indexes"), indexes);

  // Finally, report any possible error:
  bool error = cic->error(shardID);
  result->Set(TRI_V8_ASCII_STRING("error"), v8::Boolean::New(isolate, error));
  if (error) {
    result->Set(TRI_V8_ASCII_STRING("errorNum"),
                v8::Number::New(isolate, cic->errorNum(shardID)));
    std::string const errorMessage = cic->errorMessage(shardID);
    result->Set(TRI_V8_ASCII_STRING("errorMessage"),
                TRI_V8_STD_STRING(errorMessage));
  }

  TRI_V8_RETURN(result);
  TRI_V8_TRY_CATCH_END
}

////////////////////////////////////////////////////////////////////////////////
/// @brief get the responsible server
////////////////////////////////////////////////////////////////////////////////

static void JS_GetResponsibleServerClusterInfo(
    v8::FunctionCallbackInfo<v8::Value> const& args) {
  TRI_V8_TRY_CATCH_BEGIN(isolate);
  v8::HandleScope scope(isolate);

  if (args.Length() != 1) {
    TRI_V8_THROW_EXCEPTION_USAGE("getResponsibleServer(<shard-id>)");
  }

  auto result = ClusterInfo::instance()->getResponsibleServer(
      TRI_ObjectToString(args[0]));
  v8::Handle<v8::Array> list = v8::Array::New(isolate, (int)result->size());
  uint32_t count = 0;
  for (auto const& s : *result) {
    list->Set(count++, TRI_V8_STD_STRING(s));
  }

  TRI_V8_RETURN(list);
  TRI_V8_TRY_CATCH_END
}

////////////////////////////////////////////////////////////////////////////////
/// @brief get the responsible shard
////////////////////////////////////////////////////////////////////////////////

static void JS_GetResponsibleShardClusterInfo(
    v8::FunctionCallbackInfo<v8::Value> const& args) {
  TRI_V8_TRY_CATCH_BEGIN(isolate);
  v8::HandleScope scope(isolate);

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

  std::unique_ptr<TRI_json_t> json(TRI_ObjectToJson(isolate, args[1]));

  if (json == nullptr) {
    TRI_V8_THROW_EXCEPTION_MEMORY();
  }

  ShardID shardId;
  CollectionID collectionId = TRI_ObjectToString(args[0]);
  bool usesDefaultShardingAttributes;
  int res = ClusterInfo::instance()->getResponsibleShard(
      collectionId, json.get(), documentIsComplete, shardId,
      usesDefaultShardingAttributes);

  if (res != TRI_ERROR_NO_ERROR) {
    TRI_V8_THROW_EXCEPTION(res);
  }

  v8::Handle<v8::Object> result = v8::Object::New(isolate);
  result->Set(TRI_V8_ASCII_STRING("shardId"), TRI_V8_STD_STRING(shardId));
  result->Set(TRI_V8_ASCII_STRING("usesDefaultShardingAttributes"),
              v8::Boolean::New(isolate, usesDefaultShardingAttributes));

  TRI_V8_RETURN(result);
  TRI_V8_TRY_CATCH_END
}

////////////////////////////////////////////////////////////////////////////////
/// @brief get the server endpoint for a server
////////////////////////////////////////////////////////////////////////////////

static void JS_GetServerEndpointClusterInfo(
    v8::FunctionCallbackInfo<v8::Value> const& args) {
  TRI_V8_TRY_CATCH_BEGIN(isolate);
  v8::HandleScope scope(isolate);

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

static void JS_GetServerNameClusterInfo(
    v8::FunctionCallbackInfo<v8::Value> const& args) {
  TRI_V8_TRY_CATCH_BEGIN(isolate);
  v8::HandleScope scope(isolate);

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

  if (args.Length() != 0) {
    TRI_V8_THROW_EXCEPTION_USAGE("getDBServers()");
  }

  std::vector<std::string> DBServers =
      ClusterInfo::instance()->getCurrentDBServers();

  v8::Handle<v8::Array> l = v8::Array::New(isolate);

  for (size_t i = 0; i < DBServers.size(); ++i) {
    ServerID const sid = DBServers[i];

    l->Set((uint32_t)i, TRI_V8_STD_STRING(sid));
  }

  TRI_V8_RETURN(l);
  TRI_V8_TRY_CATCH_END
}

////////////////////////////////////////////////////////////////////////////////
/// @brief reloads the cache of DBServers currently registered
////////////////////////////////////////////////////////////////////////////////

static void JS_ReloadDBServers(
    v8::FunctionCallbackInfo<v8::Value> const& args) {
  TRI_V8_TRY_CATCH_BEGIN(isolate);
  v8::HandleScope scope(isolate);

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

static void JS_GetCoordinators(
    v8::FunctionCallbackInfo<v8::Value> const& args) {
  TRI_V8_TRY_CATCH_BEGIN(isolate);
  v8::HandleScope scope(isolate);

  if (args.Length() != 0) {
    TRI_V8_THROW_EXCEPTION_USAGE("getCoordinators()");
  }

  std::vector<std::string> coordinators =
      ClusterInfo::instance()->getCurrentCoordinators();

  v8::Handle<v8::Array> l = v8::Array::New(isolate);

  for (size_t i = 0; i < coordinators.size(); ++i) {
    ServerID const sid = coordinators[i];

    l->Set((uint32_t)i, TRI_V8_STD_STRING(sid));
  }

  TRI_V8_RETURN(l);
  TRI_V8_TRY_CATCH_END
}

////////////////////////////////////////////////////////////////////////////////
/// @brief returns a unique id
////////////////////////////////////////////////////////////////////////////////

static void JS_UniqidClusterInfo(
    v8::FunctionCallbackInfo<v8::Value> const& args) {
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

  uint64_t value = ClusterInfo::instance()->uniqid();

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

static void JS_AddressServerState(
    v8::FunctionCallbackInfo<v8::Value> const& args) {
  TRI_V8_TRY_CATCH_BEGIN(isolate);
  v8::HandleScope scope(isolate);

  if (args.Length() != 0) {
    TRI_V8_THROW_EXCEPTION_USAGE("address()");
  }

  std::string const address = ServerState::instance()->getAddress();
  TRI_V8_RETURN_STD_STRING(address);
  TRI_V8_TRY_CATCH_END
}

////////////////////////////////////////////////////////////////////////////////
/// @brief flush the server state (used for testing only)
////////////////////////////////////////////////////////////////////////////////

static void JS_FlushServerState(
    v8::FunctionCallbackInfo<v8::Value> const& args) {
  TRI_V8_TRY_CATCH_BEGIN(isolate);
  v8::HandleScope scope(isolate);

  if (args.Length() != 0) {
    TRI_V8_THROW_EXCEPTION_USAGE("flush()");
  }

  ServerState::instance()->flush();

  TRI_V8_RETURN_TRUE();
  TRI_V8_TRY_CATCH_END
}

////////////////////////////////////////////////////////////////////////////////
/// @brief return the servers local info
////////////////////////////////////////////////////////////////////////////////

static void JS_LocalInfoServerState(
    v8::FunctionCallbackInfo<v8::Value> const& args) {
  TRI_V8_TRY_CATCH_BEGIN(isolate);
  v8::HandleScope scope(isolate);

  if (args.Length() != 0) {
    TRI_V8_THROW_EXCEPTION_USAGE("localInfo()");
  }

  std::string const li = ServerState::instance()->getLocalInfo();
  TRI_V8_RETURN_STD_STRING(li);
  TRI_V8_TRY_CATCH_END
}

////////////////////////////////////////////////////////////////////////////////
/// @brief return the servers id
////////////////////////////////////////////////////////////////////////////////

static void JS_IdServerState(v8::FunctionCallbackInfo<v8::Value> const& args) {
  TRI_V8_TRY_CATCH_BEGIN(isolate);
  v8::HandleScope scope(isolate);

  if (args.Length() != 0) {
    TRI_V8_THROW_EXCEPTION_USAGE("id()");
  }

  std::string const id = ServerState::instance()->getId();
  TRI_V8_RETURN_STD_STRING(id);
  TRI_V8_TRY_CATCH_END
}

////////////////////////////////////////////////////////////////////////////////
/// @brief return the primary servers id (only for secondaries)
////////////////////////////////////////////////////////////////////////////////

static void JS_IdOfPrimaryServerState(
    v8::FunctionCallbackInfo<v8::Value> const& args) {
  TRI_V8_TRY_CATCH_BEGIN(isolate);
  v8::HandleScope scope(isolate);

  if (args.Length() != 0) {
    TRI_V8_THROW_EXCEPTION_USAGE("idOfPrimary()");
  }

  std::string const id = ServerState::instance()->getPrimaryId();
  TRI_V8_RETURN_STD_STRING(id);
  TRI_V8_TRY_CATCH_END
}

////////////////////////////////////////////////////////////////////////////////
/// @brief return the servers description
////////////////////////////////////////////////////////////////////////////////

static void JS_DescriptionServerState(
    v8::FunctionCallbackInfo<v8::Value> const& args) {
  TRI_V8_TRY_CATCH_BEGIN(isolate);
  v8::HandleScope scope(isolate);

  if (args.Length() != 0) {
    TRI_V8_THROW_EXCEPTION_USAGE("description()");
  }

  std::string const description = ServerState::instance()->getDescription();
  TRI_V8_RETURN_STD_STRING(description);
  TRI_V8_TRY_CATCH_END
}

////////////////////////////////////////////////////////////////////////////////
/// @brief returns the data path
////////////////////////////////////////////////////////////////////////////////

static void JS_DataPathServerState(
    v8::FunctionCallbackInfo<v8::Value> const& args) {
  TRI_V8_TRY_CATCH_BEGIN(isolate);
  v8::HandleScope scope(isolate);

  if (args.Length() != 0) {
    TRI_V8_THROW_EXCEPTION_USAGE("dataPath()");
  }

  std::string const path = ServerState::instance()->getDataPath();
  TRI_V8_RETURN_STD_STRING(path);
  TRI_V8_TRY_CATCH_END
}

////////////////////////////////////////////////////////////////////////////////
/// @brief returns the log path
////////////////////////////////////////////////////////////////////////////////

static void JS_LogPathServerState(
    v8::FunctionCallbackInfo<v8::Value> const& args) {
  TRI_V8_TRY_CATCH_BEGIN(isolate);
  v8::HandleScope scope(isolate);

  if (args.Length() != 0) {
    TRI_V8_THROW_EXCEPTION_USAGE("logPath()");
  }

  std::string const path = ServerState::instance()->getLogPath();
  TRI_V8_RETURN_STD_STRING(path);
  TRI_V8_TRY_CATCH_END
}

////////////////////////////////////////////////////////////////////////////////
/// @brief returns the arangod path
////////////////////////////////////////////////////////////////////////////////

static void JS_ArangodPathServerState(
    v8::FunctionCallbackInfo<v8::Value> const& args) {
  TRI_V8_TRY_CATCH_BEGIN(isolate);
  v8::HandleScope scope(isolate);

  if (args.Length() != 0) {
    TRI_V8_THROW_EXCEPTION_USAGE("arangodPath()");
  }

  std::string const path = ServerState::instance()->getArangodPath();
  TRI_V8_RETURN_STD_STRING(path);
  TRI_V8_TRY_CATCH_END
}

////////////////////////////////////////////////////////////////////////////////
/// @brief returns the javascript startup path
////////////////////////////////////////////////////////////////////////////////

static void JS_JavaScriptPathServerState(
    v8::FunctionCallbackInfo<v8::Value> const& args) {
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
/// @brief returns the DBserver config
////////////////////////////////////////////////////////////////////////////////

static void JS_DBserverConfigServerState(
    v8::FunctionCallbackInfo<v8::Value> const& args) {
  TRI_V8_TRY_CATCH_BEGIN(isolate);
  v8::HandleScope scope(isolate);

  if (args.Length() != 0) {
    TRI_V8_THROW_EXCEPTION_USAGE("dbserverConfig()");
  }

  std::string const path = ServerState::instance()->getDBserverConfig();
  TRI_V8_RETURN_STD_STRING(path);
  TRI_V8_TRY_CATCH_END
}

////////////////////////////////////////////////////////////////////////////////
/// @brief returns the coordinator config
////////////////////////////////////////////////////////////////////////////////

static void JS_CoordinatorConfigServerState(
    v8::FunctionCallbackInfo<v8::Value> const& args) {
  TRI_V8_TRY_CATCH_BEGIN(isolate);
  v8::HandleScope scope(isolate);

  if (args.Length() != 0) {
    TRI_V8_THROW_EXCEPTION_USAGE("coordinatorConfig()");
  }

  std::string const path = ServerState::instance()->getCoordinatorConfig();
  TRI_V8_RETURN_STD_STRING(path);
  TRI_V8_TRY_CATCH_END
}

////////////////////////////////////////////////////////////////////////////////
/// @brief return whether the cluster is initialized
////////////////////////////////////////////////////////////////////////////////

static void JS_InitializedServerState(
    v8::FunctionCallbackInfo<v8::Value> const& args) {
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

static void JS_IsCoordinatorServerState(
    v8::FunctionCallbackInfo<v8::Value> const& args) {
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

static void JS_RoleServerState(
    v8::FunctionCallbackInfo<v8::Value> const& args) {
  TRI_V8_TRY_CATCH_BEGIN(isolate);
  v8::HandleScope scope(isolate);

  if (args.Length() != 0) {
    TRI_V8_THROW_EXCEPTION_USAGE("role()");
  }

  std::string const role =
      ServerState::roleToString(ServerState::instance()->getRole());

  TRI_V8_RETURN_STD_STRING(role);
  TRI_V8_TRY_CATCH_END
}

////////////////////////////////////////////////////////////////////////////////
/// @brief sets the server local info (used for testing)
////////////////////////////////////////////////////////////////////////////////

static void JS_SetLocalInfoServerState(
    v8::FunctionCallbackInfo<v8::Value> const& args) {
  TRI_V8_TRY_CATCH_BEGIN(isolate);
  v8::HandleScope scope(isolate);

  if (args.Length() != 1) {
    TRI_V8_THROW_EXCEPTION_USAGE("setLocalInfo(<info>)");
  }

  std::string const li = TRI_ObjectToString(args[0]);
  ServerState::instance()->setLocalInfo(li);

  TRI_V8_RETURN_TRUE();
  TRI_V8_TRY_CATCH_END
}

////////////////////////////////////////////////////////////////////////////////
/// @brief sets the server id (used for testing)
////////////////////////////////////////////////////////////////////////////////

static void JS_SetIdServerState(
    v8::FunctionCallbackInfo<v8::Value> const& args) {
  TRI_V8_TRY_CATCH_BEGIN(isolate);
  v8::HandleScope scope(isolate);

  if (args.Length() != 1) {
    TRI_V8_THROW_EXCEPTION_USAGE("setId(<id>)");
  }

  std::string const id = TRI_ObjectToString(args[0]);
  ServerState::instance()->setId(id);

  TRI_V8_RETURN_TRUE();
  TRI_V8_TRY_CATCH_END
}

////////////////////////////////////////////////////////////////////////////////
/// @brief sets the server role (used for testing)
////////////////////////////////////////////////////////////////////////////////

static void JS_SetRoleServerState(
    v8::FunctionCallbackInfo<v8::Value> const& args) {
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

static void JS_RedetermineRoleServerState(
    v8::FunctionCallbackInfo<v8::Value> const& args) {
  TRI_V8_TRY_CATCH_BEGIN(isolate);
  v8::HandleScope scope(isolate);

  if (args.Length() != 0) {
    TRI_V8_THROW_EXCEPTION_USAGE("redetermineRole()");
  }

  bool changed = ServerState::instance()->redetermineRole();
  if (changed) {
    TRI_V8_RETURN_TRUE();
  } else {
    TRI_V8_RETURN_FALSE();
  }
  TRI_V8_TRY_CATCH_END
}

////////////////////////////////////////////////////////////////////////////////
/// @brief returns the server state
////////////////////////////////////////////////////////////////////////////////

static void JS_StatusServerState(
    v8::FunctionCallbackInfo<v8::Value> const& args) {
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

static void PrepareClusterCommRequest(
    v8::FunctionCallbackInfo<v8::Value> const& args,
    arangodb::GeneralRequest::RequestType& reqType, std::string& destination,
    std::string& path, std::string& body,
    std::map<std::string, std::string>& headerFields,
    ClientTransactionID& clientTransactionID,
    CoordTransactionID& coordTransactionID, double& timeout,
    bool& singleRequest) {
  v8::Isolate* isolate = args.GetIsolate();
  TRI_V8_CURRENT_GLOBALS_AND_SCOPE;

  TRI_ASSERT(args.Length() >= 4);

  reqType = arangodb::GeneralRequest::RequestType::GET;
  if (args[0]->IsString()) {
    TRI_Utf8ValueNFC UTF8(TRI_UNKNOWN_MEM_ZONE, args[0]);
    std::string methstring = *UTF8;
    reqType = arangodb::HttpRequest::translateMethod(methstring);
    if (reqType == arangodb::GeneralRequest::RequestType::ILLEGAL) {
      reqType = arangodb::GeneralRequest::RequestType::GET;
    }
  }

  destination = TRI_ObjectToString(args[1]);

  std::string dbname = TRI_ObjectToString(args[2]);

  path = TRI_ObjectToString(args[3]);
  path = "/_db/" + dbname + path;

  body.clear();
  if (args.Length() > 4) {
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

  clientTransactionID = "";
  coordTransactionID = 0;
  timeout = 24 * 3600.0;
  singleRequest = false;

  if (args.Length() > 6 && args[6]->IsObject()) {
    v8::Handle<v8::Object> opt = args[6].As<v8::Object>();
    TRI_GET_GLOBAL_STRING(ClientTransactionIDKey);
    if (opt->Has(ClientTransactionIDKey)) {
      clientTransactionID =
          TRI_ObjectToString(opt->Get(ClientTransactionIDKey));
    }
    TRI_GET_GLOBAL_STRING(CoordTransactionIDKey);
    if (opt->Has(CoordTransactionIDKey)) {
      coordTransactionID =
          TRI_ObjectToUInt64(opt->Get(CoordTransactionIDKey), true);
    }
    TRI_GET_GLOBAL_STRING(TimeoutKey);
    if (opt->Has(TimeoutKey)) {
      timeout = TRI_ObjectToDouble(opt->Get(TimeoutKey));
    }
    TRI_GET_GLOBAL_STRING(SingleRequestKey);
    if (opt->Has(SingleRequestKey)) {
      singleRequest = TRI_ObjectToBoolean(opt->Get(SingleRequestKey));
    }
  }
  if (clientTransactionID == "") {
    clientTransactionID = StringUtils::itoa(TRI_NewTickServer());
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

static void Return_PrepareClusterCommResultForJS(
    v8::FunctionCallbackInfo<v8::Value> const& args,
    ClusterCommResult const& res) {
  v8::Isolate* isolate = args.GetIsolate();
  TRI_V8_CURRENT_GLOBALS_AND_SCOPE;

  v8::Handle<v8::Object> r = v8::Object::New(isolate);
  if (res.dropped) {
    TRI_GET_GLOBAL_STRING(ErrorMessageKey);
    r->Set(ErrorMessageKey, TRI_V8_ASCII_STRING("operation was dropped"));
  } else {
    TRI_GET_GLOBAL_STRING(ClientTransactionIDKey);
    r->Set(ClientTransactionIDKey, TRI_V8_STD_STRING(res.clientTransactionID));

    // convert the ids to strings as uint64_t might be too big for JavaScript
    // numbers
    TRI_GET_GLOBAL_STRING(CoordTransactionIDKey);
    std::string id = StringUtils::itoa(res.coordTransactionID);
    r->Set(CoordTransactionIDKey, TRI_V8_STD_STRING(id));

    id = StringUtils::itoa(res.operationID);
    TRI_GET_GLOBAL_STRING(OperationIDKey);
    r->Set(OperationIDKey, TRI_V8_STD_STRING(id));
    TRI_GET_GLOBAL_STRING(EndpointKey);
    r->Set(EndpointKey, TRI_V8_STD_STRING(res.endpoint));
    TRI_GET_GLOBAL_STRING(SingleRequestKey);
    r->Set(SingleRequestKey, v8::Boolean::New(isolate, res.single));
    TRI_GET_GLOBAL_STRING(ShardIDKey);
    r->Set(ShardIDKey, TRI_V8_STD_STRING(res.shardID));

    if (res.status == CL_COMM_SUBMITTED) {
      TRI_GET_GLOBAL_STRING(StatusKey);
      r->Set(StatusKey, TRI_V8_ASCII_STRING("SUBMITTED"));
    } else if (res.status == CL_COMM_SENDING) {
      TRI_GET_GLOBAL_STRING(StatusKey);
      r->Set(StatusKey, TRI_V8_ASCII_STRING("SENDING"));
    } else if (res.status == CL_COMM_SENT) {
      TRI_GET_GLOBAL_STRING(StatusKey);
      r->Set(StatusKey, TRI_V8_ASCII_STRING("SENT"));
      // This might be the result of a synchronous request or an asynchronous
      // request with the `singleRequest` flag true and thus contain the
      // actual response. If it is an asynchronous request which has not
      // yet been answered, the following information is probably rather
      // boring:

      // The headers:
      TRI_ASSERT(res.result != nullptr);
      v8::Handle<v8::Object> h = v8::Object::New(isolate);
      for (auto const& i : res.result->getHeaderFields()) {
        h->Set(TRI_V8_STD_STRING(i.first), TRI_V8_STD_STRING(i.second));
      }
      r->Set(TRI_V8_ASCII_STRING("headers"), h);

      // The body:
      arangodb::basics::StringBuffer& body = res.result->getBody();
      if (body.length() != 0) {
        r->Set(TRI_V8_ASCII_STRING("body"), TRI_V8_STD_STRING(body));
      }
    } else if (res.status == CL_COMM_TIMEOUT) {
      TRI_GET_GLOBAL_STRING(StatusKey);
      r->Set(StatusKey, TRI_V8_ASCII_STRING("TIMEOUT"));
      TRI_GET_GLOBAL_STRING(TimeoutKey);
      r->Set(TimeoutKey, v8::BooleanObject::New(true));
    } else if (res.status == CL_COMM_ERROR) {
      TRI_GET_GLOBAL_STRING(StatusKey);
      r->Set(StatusKey, TRI_V8_ASCII_STRING("ERROR"));

      if (res.result && res.result->isComplete()) {
        v8::Handle<v8::Object> details = v8::Object::New(isolate);
        details->Set(TRI_V8_ASCII_STRING("code"),
                     v8::Number::New(isolate, res.result->getHttpReturnCode()));
        details->Set(TRI_V8_ASCII_STRING("message"),
                     TRI_V8_STD_STRING(res.result->getHttpReturnMessage()));
        details->Set(TRI_V8_ASCII_STRING("body"),
                     TRI_V8_STD_STRING(res.result->getBody()));

        r->Set(TRI_V8_ASCII_STRING("details"), details);
        TRI_GET_GLOBAL_STRING(ErrorMessageKey);
        r->Set(ErrorMessageKey, TRI_V8_STD_STRING(res.errorMessage));
      } else {
        TRI_GET_GLOBAL_STRING(ErrorMessageKey);
        r->Set(ErrorMessageKey, TRI_V8_STD_STRING(res.errorMessage));
      }
    } else if (res.status == CL_COMM_DROPPED) {
      TRI_GET_GLOBAL_STRING(StatusKey);
      r->Set(StatusKey, TRI_V8_ASCII_STRING("DROPPED"));
      TRI_GET_GLOBAL_STRING(ErrorMessageKey);
      r->Set(ErrorMessageKey,
             TRI_V8_ASCII_STRING("request dropped whilst waiting for answer"));
    } else if (res.status == CL_COMM_BACKEND_UNAVAILABLE) {
      TRI_GET_GLOBAL_STRING(StatusKey);
      r->Set(StatusKey, TRI_V8_ASCII_STRING("BACKEND_UNAVAILABLE"));
      TRI_GET_GLOBAL_STRING(ErrorMessageKey);
      r->Set(ErrorMessageKey,
             TRI_V8_ASCII_STRING("required backend was not available"));
    } else if (res.status == CL_COMM_RECEIVED) {  // Everything is OK
      // The headers:
      v8::Handle<v8::Object> h = v8::Object::New(isolate);
      TRI_GET_GLOBAL_STRING(StatusKey);
      r->Set(StatusKey, TRI_V8_ASCII_STRING("RECEIVED"));
      TRI_ASSERT(res.answer != nullptr);
      std::map<std::string, std::string> headers = res.answer->headers();
      headers["content-length"] =
          StringUtils::itoa(res.answer->contentLength());
      std::map<std::string, std::string>::iterator i;
      for (i = headers.begin(); i != headers.end(); ++i) {
        h->Set(TRI_V8_STD_STRING(i->first), TRI_V8_STD_STRING(i->second));
      }
      r->Set(TRI_V8_ASCII_STRING("headers"), h);

      // The body:
      std::string const& body = res.answer->body();

      if (!body.empty()) {
        r->Set(TRI_V8_ASCII_STRING("body"), TRI_V8_STD_STRING(body));
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

  if (args.Length() < 4 || args.Length() > 7) {
    TRI_V8_THROW_EXCEPTION_USAGE(
        "asyncRequest("
        "reqType, destination, dbname, path, body, headers, options)");
  }
  // Possible options:
  //   - clientTransactionID  (string)
  //   - coordTransactionID   (number)
  //   - timeout              (number)
  //   - singleRequest        (boolean) default is false

  ClusterComm* cc = ClusterComm::instance();

  if (cc == nullptr) {
    TRI_V8_THROW_EXCEPTION_MESSAGE(TRI_ERROR_INTERNAL,
                                   "clustercomm object not found");
  }

  arangodb::GeneralRequest::RequestType reqType;
  std::string destination;
  std::string path;
  auto body = std::make_shared<std::string>();
  std::unique_ptr<std::map<std::string, std::string>> headerFields(
      new std::map<std::string, std::string>());
  ClientTransactionID clientTransactionID;
  CoordTransactionID coordTransactionID;
  double timeout;
  bool singleRequest = false;

  PrepareClusterCommRequest(args, reqType, destination, path, *body,
                            *headerFields, clientTransactionID,
                            coordTransactionID, timeout, singleRequest);

  ClusterCommResult const res = cc->asyncRequest(
      clientTransactionID, coordTransactionID, destination, reqType, path, body,
      headerFields, 0, timeout, singleRequest);

  if (res.status == CL_COMM_ERROR) {
    TRI_V8_THROW_EXCEPTION_MESSAGE(TRI_ERROR_INTERNAL,
                                   "couldn't queue async request");
  }

  LOG(DEBUG) << "JS_AsyncRequest: request has been submitted";

  Return_PrepareClusterCommResultForJS(args, res);
  TRI_V8_TRY_CATCH_END
}

////////////////////////////////////////////////////////////////////////////////
/// @brief send a synchronous request
////////////////////////////////////////////////////////////////////////////////

static void JS_SyncRequest(v8::FunctionCallbackInfo<v8::Value> const& args) {
  TRI_V8_TRY_CATCH_BEGIN(isolate);
  v8::HandleScope scope(isolate);

  if (args.Length() < 4 || args.Length() > 7) {
    TRI_V8_THROW_EXCEPTION_USAGE(
        "syncRequest("
        "reqType, destination, dbname, path, body, headers, options)");
  }
  // Possible options:
  //   - clientTransactionID  (string)
  //   - coordTransactionID   (number)
  //   - timeout              (number)

  // Disabled to allow communication originating in a DBserver:
  // 31.7.2014 Max

  // if (ServerState::instance()->getRole() != ServerState::ROLE_COORDINATOR) {
  //  TRI_V8_THROW_EXCEPTION_INTERNAL(scope,"request works only in coordinator
  //  role");
  //}

  ClusterComm* cc = ClusterComm::instance();

  if (cc == nullptr) {
    TRI_V8_THROW_EXCEPTION_MESSAGE(TRI_ERROR_INTERNAL,
                                   "clustercomm object not found");
  }

  arangodb::GeneralRequest::RequestType reqType;
  std::string destination;
  std::string path;
  std::string body;
  std::unique_ptr<std::map<std::string, std::string>> headerFields(
      new std::map<std::string, std::string>());
  ClientTransactionID clientTransactionID;
  CoordTransactionID coordTransactionID;
  double timeout;
  bool singleRequest = false;  // of no relevance here

  PrepareClusterCommRequest(args, reqType, destination, path, body,
                            *headerFields, clientTransactionID,
                            coordTransactionID, timeout, singleRequest);

  std::unique_ptr<ClusterCommResult> res =
      cc->syncRequest(clientTransactionID, coordTransactionID, destination,
                      reqType, path, body, *headerFields, timeout);

  if (res.get() == nullptr) {
    TRI_V8_THROW_EXCEPTION_MESSAGE(TRI_ERROR_INTERNAL,
                                   "couldn't do sync request");
  }

  LOG(DEBUG) << "JS_SyncRequest: request has been done";

  Return_PrepareClusterCommResultForJS(args, *res);
  TRI_V8_TRY_CATCH_END
}

////////////////////////////////////////////////////////////////////////////////
/// @brief enquire information about an asynchronous request
////////////////////////////////////////////////////////////////////////////////

static void JS_Enquire(v8::FunctionCallbackInfo<v8::Value> const& args) {
  TRI_V8_TRY_CATCH_BEGIN(isolate);
  v8::HandleScope scope(isolate);

  if (args.Length() != 1) {
    TRI_V8_THROW_EXCEPTION_USAGE("enquire(operationID)");
  }

  ClusterComm* cc = ClusterComm::instance();

  if (cc == nullptr) {
    TRI_V8_THROW_EXCEPTION_MESSAGE(TRI_ERROR_INTERNAL,
                                   "clustercomm object not found");
  }

  OperationID operationID = TRI_ObjectToUInt64(args[0], true);

  LOG(DEBUG) << "JS_Enquire: calling ClusterComm::enquire()";

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

  if (args.Length() != 1) {
    TRI_V8_THROW_EXCEPTION_USAGE("wait(obj)");
  }
  // Possible options:
  //   - clientTransactionID  (string)
  //   - coordTransactionID   (number)
  //   - operationID          (number)
  //   - shardID              (string)
  //   - timeout              (number)

  ClusterComm* cc = ClusterComm::instance();

  if (cc == nullptr) {
    TRI_V8_THROW_EXCEPTION_MESSAGE(TRI_ERROR_INTERNAL,
                                   "clustercomm object not found");
  }

  ClientTransactionID myclientTransactionID = "";
  CoordTransactionID mycoordTransactionID = 0;
  OperationID myoperationID = 0;
  ShardID myshardID = "";
  double mytimeout = 24 * 3600.0;

  if (args[0]->IsObject()) {
    v8::Handle<v8::Object> obj = args[0].As<v8::Object>();
    TRI_GET_GLOBAL_STRING(ClientTransactionIDKey);
    if (obj->Has(ClientTransactionIDKey)) {
      myclientTransactionID =
          TRI_ObjectToString(obj->Get(ClientTransactionIDKey));
    }
    TRI_GET_GLOBAL_STRING(CoordTransactionIDKey);
    if (obj->Has(CoordTransactionIDKey)) {
      mycoordTransactionID =
          TRI_ObjectToUInt64(obj->Get(CoordTransactionIDKey), true);
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

  LOG(DEBUG) << "JS_Wait: calling ClusterComm::wait()";

  ClusterCommResult const res =
      cc->wait(myclientTransactionID, mycoordTransactionID, myoperationID,
               myshardID, mytimeout);

  Return_PrepareClusterCommResultForJS(args, res);
  TRI_V8_TRY_CATCH_END
}

////////////////////////////////////////////////////////////////////////////////
/// @brief drop the result of an asynchronous request
////////////////////////////////////////////////////////////////////////////////

static void JS_Drop(v8::FunctionCallbackInfo<v8::Value> const& args) {
  TRI_V8_TRY_CATCH_BEGIN(isolate);
  TRI_V8_CURRENT_GLOBALS_AND_SCOPE;

  if (args.Length() != 1) {
    TRI_V8_THROW_EXCEPTION_USAGE("wait(obj)");
  }
  // Possible options:
  //   - clientTransactionID  (string)
  //   - coordTransactionID   (number)
  //   - operationID          (number)
  //   - shardID              (string)

  // Disabled to allow communication originating in a DBserver:
  // 31.7.2014 Max

  // if (ServerState::instance()->getRole() != ServerState::ROLE_COORDINATOR) {
  //   TRI_V8_THROW_EXCEPTION_INTERNAL(scope,"request works only in coordinator
  //   role");
  // }

  ClusterComm* cc = ClusterComm::instance();

  if (cc == nullptr) {
    TRI_V8_THROW_EXCEPTION_MESSAGE(TRI_ERROR_INTERNAL,
                                   "clustercomm object not found");
  }

  ClientTransactionID myclientTransactionID = "";
  CoordTransactionID mycoordTransactionID = 0;
  OperationID myoperationID = 0;
  ShardID myshardID = "";

  if (args[0]->IsObject()) {
    v8::Handle<v8::Object> obj = args[0].As<v8::Object>();
    TRI_GET_GLOBAL_STRING(ClientTransactionIDKey);
    if (obj->Has(ClientTransactionIDKey)) {
      myclientTransactionID =
          TRI_ObjectToString(obj->Get(ClientTransactionIDKey));
    }
    TRI_GET_GLOBAL_STRING(CoordTransactionIDKey);
    if (obj->Has(CoordTransactionIDKey)) {
      mycoordTransactionID =
          TRI_ObjectToUInt64(obj->Get(CoordTransactionIDKey), true);
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

  LOG(DEBUG) << "JS_Drop: calling ClusterComm::drop()";

  cc->drop(myclientTransactionID, mycoordTransactionID, myoperationID,
           myshardID);

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
  ft->SetClassName(TRI_V8_ASCII_STRING("ArangoAgency"));

  rt = ft->InstanceTemplate();
  rt->SetInternalFieldCount(2);

  TRI_AddMethodVocbase(isolate, rt, TRI_V8_ASCII_STRING("cas"), JS_CasAgency);
  TRI_AddMethodVocbase(isolate, rt, TRI_V8_ASCII_STRING("createDirectory"),
                       JS_CreateDirectoryAgency);
  TRI_AddMethodVocbase(isolate, rt, TRI_V8_ASCII_STRING("get"), JS_GetAgency);
  TRI_AddMethodVocbase(isolate, rt, TRI_V8_ASCII_STRING("isEnabled"),
                       JS_IsEnabledAgency);
  TRI_AddMethodVocbase(isolate, rt, TRI_V8_ASCII_STRING("increaseVersion"),
                       JS_IncreaseVersionAgency);
  TRI_AddMethodVocbase(isolate, rt, TRI_V8_ASCII_STRING("list"), JS_ListAgency);
  TRI_AddMethodVocbase(isolate, rt, TRI_V8_ASCII_STRING("lockRead"),
                       JS_LockReadAgency);
  TRI_AddMethodVocbase(isolate, rt, TRI_V8_ASCII_STRING("lockWrite"),
                       JS_LockWriteAgency);
  TRI_AddMethodVocbase(isolate, rt, TRI_V8_ASCII_STRING("remove"),
                       JS_RemoveAgency);
  TRI_AddMethodVocbase(isolate, rt, TRI_V8_ASCII_STRING("set"), JS_SetAgency);
  TRI_AddMethodVocbase(isolate, rt, TRI_V8_ASCII_STRING("endpoints"),
                       JS_EndpointsAgency);
  TRI_AddMethodVocbase(isolate, rt, TRI_V8_ASCII_STRING("prefix"),
                       JS_PrefixAgency);
  TRI_AddMethodVocbase(isolate, rt, TRI_V8_ASCII_STRING("setPrefix"),
                       JS_SetPrefixAgency, true);
  TRI_AddMethodVocbase(isolate, rt, TRI_V8_ASCII_STRING("uniqid"),
                       JS_UniqidAgency);
  TRI_AddMethodVocbase(isolate, rt, TRI_V8_ASCII_STRING("unlockRead"),
                       JS_UnlockReadAgency);
  TRI_AddMethodVocbase(isolate, rt, TRI_V8_ASCII_STRING("unlockWrite"),
                       JS_UnlockWriteAgency);
  TRI_AddMethodVocbase(isolate, rt, TRI_V8_ASCII_STRING("version"),
                       JS_VersionAgency);

  v8g->AgencyTempl.Reset(isolate, rt);
  ft->SetClassName(TRI_V8_ASCII_STRING("ArangoAgencyCtor"));

  TRI_AddGlobalFunctionVocbase(isolate, context,
                               TRI_V8_ASCII_STRING("ArangoAgencyCtor"),
                               ft->GetFunction(), true);

  // register the global object
  v8::Handle<v8::Object> aa = rt->NewInstance();
  if (!aa.IsEmpty()) {
    TRI_AddGlobalVariableVocbase(isolate, context,
                                 TRI_V8_ASCII_STRING("ArangoAgency"), aa);
  }

  // .............................................................................
  // generate the cluster info template
  // .............................................................................

  ft = v8::FunctionTemplate::New(isolate);
  ft->SetClassName(TRI_V8_ASCII_STRING("ArangoClusterInfo"));

  rt = ft->InstanceTemplate();
  rt->SetInternalFieldCount(2);

  TRI_AddMethodVocbase(isolate, rt, TRI_V8_ASCII_STRING("doesDatabaseExist"),
                       JS_DoesDatabaseExistClusterInfo);
  TRI_AddMethodVocbase(isolate, rt, TRI_V8_ASCII_STRING("listDatabases"),
                       JS_ListDatabases);
  TRI_AddMethodVocbase(isolate, rt, TRI_V8_ASCII_STRING("flush"),
                       JS_FlushClusterInfo, true);
  TRI_AddMethodVocbase(isolate, rt, TRI_V8_ASCII_STRING("getCollectionInfo"),
                       JS_GetCollectionInfoClusterInfo);
  TRI_AddMethodVocbase(isolate, rt,
                       TRI_V8_ASCII_STRING("getCollectionInfoCurrent"),
                       JS_GetCollectionInfoCurrentClusterInfo);
  TRI_AddMethodVocbase(isolate, rt, TRI_V8_ASCII_STRING("getResponsibleServer"),
                       JS_GetResponsibleServerClusterInfo);
  TRI_AddMethodVocbase(isolate, rt, TRI_V8_ASCII_STRING("getResponsibleShard"),
                       JS_GetResponsibleShardClusterInfo);
  TRI_AddMethodVocbase(isolate, rt, TRI_V8_ASCII_STRING("getServerEndpoint"),
                       JS_GetServerEndpointClusterInfo);
  TRI_AddMethodVocbase(isolate, rt, TRI_V8_ASCII_STRING("getServerName"),
                       JS_GetServerNameClusterInfo);
  TRI_AddMethodVocbase(isolate, rt, TRI_V8_ASCII_STRING("getDBServers"),
                       JS_GetDBServers);
  TRI_AddMethodVocbase(isolate, rt, TRI_V8_ASCII_STRING("reloadDBServers"),
                       JS_ReloadDBServers);
  TRI_AddMethodVocbase(isolate, rt, TRI_V8_ASCII_STRING("getCoordinators"),
                       JS_GetCoordinators);
  TRI_AddMethodVocbase(isolate, rt, TRI_V8_ASCII_STRING("uniqid"),
                       JS_UniqidClusterInfo);

  v8g->ClusterInfoTempl.Reset(isolate, rt);
  TRI_AddGlobalFunctionVocbase(isolate, context,
                               TRI_V8_ASCII_STRING("ArangoClusterInfoCtor"),
                               ft->GetFunction(), true);

  // register the global object
  v8::Handle<v8::Object> ci = rt->NewInstance();
  if (!ci.IsEmpty()) {
    TRI_AddGlobalVariableVocbase(isolate, context,
                                 TRI_V8_ASCII_STRING("ArangoClusterInfo"), ci);
  }

  // .............................................................................
  // generate the server state template
  // ...........................................................................

  ft = v8::FunctionTemplate::New(isolate);
  ft->SetClassName(TRI_V8_ASCII_STRING("ArangoServerState"));

  rt = ft->InstanceTemplate();
  rt->SetInternalFieldCount(2);

  TRI_AddMethodVocbase(isolate, rt, TRI_V8_ASCII_STRING("address"),
                       JS_AddressServerState);
  TRI_AddMethodVocbase(isolate, rt, TRI_V8_ASCII_STRING("flush"),
                       JS_FlushServerState, true);
  TRI_AddMethodVocbase(isolate, rt, TRI_V8_ASCII_STRING("localInfo"),
                       JS_LocalInfoServerState);
  TRI_AddMethodVocbase(isolate, rt, TRI_V8_ASCII_STRING("id"),
                       JS_IdServerState);
  TRI_AddMethodVocbase(isolate, rt, TRI_V8_ASCII_STRING("idOfPrimary"),
                       JS_IdOfPrimaryServerState);
  TRI_AddMethodVocbase(isolate, rt, TRI_V8_ASCII_STRING("description"),
                       JS_DescriptionServerState);
  TRI_AddMethodVocbase(isolate, rt, TRI_V8_ASCII_STRING("dataPath"),
                       JS_DataPathServerState);
  TRI_AddMethodVocbase(isolate, rt, TRI_V8_ASCII_STRING("logPath"),
                       JS_LogPathServerState);
  TRI_AddMethodVocbase(isolate, rt, TRI_V8_ASCII_STRING("arangodPath"),
                       JS_ArangodPathServerState);
  TRI_AddMethodVocbase(isolate, rt, TRI_V8_ASCII_STRING("javaScriptPath"),
                       JS_JavaScriptPathServerState);
  TRI_AddMethodVocbase(isolate, rt, TRI_V8_ASCII_STRING("dbserverConfig"),
                       JS_DBserverConfigServerState);
  TRI_AddMethodVocbase(isolate, rt, TRI_V8_ASCII_STRING("coordinatorConfig"),
                       JS_CoordinatorConfigServerState);
  TRI_AddMethodVocbase(isolate, rt, TRI_V8_ASCII_STRING("initialized"),
                       JS_InitializedServerState);
  TRI_AddMethodVocbase(isolate, rt, TRI_V8_ASCII_STRING("isCoordinator"),
                       JS_IsCoordinatorServerState);
  TRI_AddMethodVocbase(isolate, rt, TRI_V8_ASCII_STRING("role"),
                       JS_RoleServerState);
  TRI_AddMethodVocbase(isolate, rt, TRI_V8_ASCII_STRING("setLocalInfo"),
                       JS_SetLocalInfoServerState, true);
  TRI_AddMethodVocbase(isolate, rt, TRI_V8_ASCII_STRING("setId"),
                       JS_SetIdServerState, true);
  TRI_AddMethodVocbase(isolate, rt, TRI_V8_ASCII_STRING("setRole"),
                       JS_SetRoleServerState, true);
  TRI_AddMethodVocbase(isolate, rt, TRI_V8_ASCII_STRING("redetermineRole"),
                       JS_RedetermineRoleServerState, true);
  TRI_AddMethodVocbase(isolate, rt, TRI_V8_ASCII_STRING("status"),
                       JS_StatusServerState);

  v8g->ServerStateTempl.Reset(isolate, rt);
  TRI_AddGlobalFunctionVocbase(isolate, context,
                               TRI_V8_ASCII_STRING("ArangoServerStateCtor"),
                               ft->GetFunction(), true);

  // register the global object
  v8::Handle<v8::Object> ss = rt->NewInstance();
  if (!ss.IsEmpty()) {
    TRI_AddGlobalVariableVocbase(isolate, context,
                                 TRI_V8_ASCII_STRING("ArangoServerState"), ss);
  }

  // ...........................................................................
  // generate the cluster comm template
  // ...........................................................................

  ft = v8::FunctionTemplate::New(isolate);
  ft->SetClassName(TRI_V8_ASCII_STRING("ArangoClusterComm"));

  rt = ft->InstanceTemplate();
  rt->SetInternalFieldCount(2);

  TRI_AddMethodVocbase(isolate, rt, TRI_V8_ASCII_STRING("asyncRequest"),
                       JS_AsyncRequest);
  TRI_AddMethodVocbase(isolate, rt, TRI_V8_ASCII_STRING("syncRequest"),
                       JS_SyncRequest);
  TRI_AddMethodVocbase(isolate, rt, TRI_V8_ASCII_STRING("enquire"), JS_Enquire);
  TRI_AddMethodVocbase(isolate, rt, TRI_V8_ASCII_STRING("wait"), JS_Wait);
  TRI_AddMethodVocbase(isolate, rt, TRI_V8_ASCII_STRING("drop"), JS_Drop);

  v8g->ClusterCommTempl.Reset(isolate, rt);
  TRI_AddGlobalFunctionVocbase(isolate, context,
                               TRI_V8_ASCII_STRING("ArangoClusterCommCtor"),
                               ft->GetFunction(), true);

  // register the global object
  ss = rt->NewInstance();
  if (!ss.IsEmpty()) {
    TRI_AddGlobalVariableVocbase(isolate, context,
                                 TRI_V8_ASCII_STRING("ArangoClusterComm"), ss);
  }
}
