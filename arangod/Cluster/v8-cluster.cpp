////////////////////////////////////////////////////////////////////////////////
/// @brief V8-cluster bridge
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
/// @author Jan Steemann
/// @author Copyright 2014, ArangoDB GmbH, Cologne, Germany
/// @author Copyright 2011-2014, triAGENS GmbH, Cologne, Germany
////////////////////////////////////////////////////////////////////////////////

#include "v8-cluster.h"

#include "VocBase/server.h"

#include "Cluster/AgencyComm.h"
#include "Cluster/ClusterInfo.h"
#include "Cluster/ServerState.h"
#include "Cluster/ClusterComm.h"
#include "V8/v8-conv.h"
#include "V8/v8-globals.h"
#include "V8/v8-utils.h"

using namespace std;
using namespace triagens::basics;
using namespace triagens::arango;

// -----------------------------------------------------------------------------
// --SECTION--                                                  agency functions
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @brief creates a v8 exception object
////////////////////////////////////////////////////////////////////////////////

#define THROW_AGENCY_EXCEPTION(data)                                    \
  CreateAgencyException(args, data);                                    \
  return;

static void CreateAgencyException (const v8::FunctionCallbackInfo<v8::Value>& args,
                                   AgencyCommResult const& result) {
  v8::Isolate* isolate = args.GetIsolate();
  TRI_V8_CURRENT_GLOBALS_AND_SCOPE;

  const std::string errorDetails = result.errorDetails();
  v8::Handle<v8::String> errorMessage = TRI_V8_STD_STRING(errorDetails);
  v8::Handle<v8::Object> errorObject = v8::Exception::Error(errorMessage)->ToObject();

  errorObject->Set(TRI_V8_ASCII_STRING("code"), v8::Number::New(isolate, result.httpCode()));
  errorObject->Set(TRI_V8_ASCII_STRING("errorNum"), v8::Number::New(isolate, result.errorCode()));
  errorObject->Set(TRI_V8_ASCII_STRING("errorMessage"), errorMessage);
  errorObject->Set(TRI_V8_ASCII_STRING("error"), v8::True(isolate));

  TRI_GET_GLOBAL(ArangoErrorTempl, v8::ObjectTemplate);
  v8::Handle<v8::Value> proto = ArangoErrorTempl->NewInstance();
  if (! proto.IsEmpty()) {
    errorObject->SetPrototype(proto);
  }

  args.GetIsolate()->ThrowException(errorObject);
}

////////////////////////////////////////////////////////////////////////////////
/// @brief compares and swaps a value in the agency
////////////////////////////////////////////////////////////////////////////////

static void JS_CasAgency (const v8::FunctionCallbackInfo<v8::Value>& args) {
  v8::Isolate* isolate = args.GetIsolate();
  v8::HandleScope scope(isolate);

  if (args.Length() < 3) {
    TRI_V8_THROW_EXCEPTION_USAGE("cas(<key>, <oldValue>, <newValue>, <ttl>, <timeout>, <throw>)");
  }

  const std::string key = TRI_ObjectToString(args[0]);

  TRI_json_t* oldJson = TRI_ObjectToJson(isolate, args[1]);

  if (oldJson == 0) {
    TRI_V8_THROW_EXCEPTION_PARAMETER("cannot convert <oldValue> to JSON");
  }

  TRI_json_t* newJson = TRI_ObjectToJson(isolate, args[2]);
  if (newJson == 0) {
    TRI_FreeJson(TRI_UNKNOWN_MEM_ZONE, oldJson);
    TRI_V8_THROW_EXCEPTION_PARAMETER("cannot convert <newValue> to JSON");
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
  AgencyCommResult result = comm.casValue(key, oldJson, newJson, ttl, timeout);

  TRI_FreeJson(TRI_UNKNOWN_MEM_ZONE, newJson);
  TRI_FreeJson(TRI_UNKNOWN_MEM_ZONE, oldJson);

  if (! result.successful()) {
    if (! shouldThrow) {
      TRI_V8_RETURN_FALSE();
    }

    THROW_AGENCY_EXCEPTION(result);
  }

  TRI_V8_RETURN_TRUE();
}

////////////////////////////////////////////////////////////////////////////////
/// @brief creates a directory in the agency
////////////////////////////////////////////////////////////////////////////////

static void JS_CreateDirectoryAgency (const v8::FunctionCallbackInfo<v8::Value>& args) {
  v8::Isolate* isolate = args.GetIsolate();
  v8::HandleScope scope(isolate);


  if (args.Length() != 1) {
    TRI_V8_THROW_EXCEPTION_USAGE("createDirectory(<key>)");
  }

  const std::string key = TRI_ObjectToString(args[0]);

  AgencyComm comm;
  AgencyCommResult result = comm.createDirectory(key);

  if (! result.successful()) {
    THROW_AGENCY_EXCEPTION(result);
  }

  TRI_V8_RETURN_TRUE();
}

////////////////////////////////////////////////////////////////////////////////
/// @brief whether or not the agency is enabled
////////////////////////////////////////////////////////////////////////////////

static void JS_IsEnabledAgency (const v8::FunctionCallbackInfo<v8::Value>& args) {
  v8::Isolate* isolate = args.GetIsolate();
  v8::HandleScope scope(isolate);

  if (args.Length() != 0) {
    TRI_V8_THROW_EXCEPTION_USAGE("isEnabled()");
  }

  const std::string prefix = AgencyComm::prefix();

  if (! prefix.empty()) {
    TRI_V8_RETURN_TRUE();
  }
  else {
    TRI_V8_RETURN_FALSE();
  }
  
}

////////////////////////////////////////////////////////////////////////////////
/// @brief increase the version number
////////////////////////////////////////////////////////////////////////////////

static void JS_IncreaseVersionAgency (const v8::FunctionCallbackInfo<v8::Value>& args) {
  v8::Isolate* isolate = args.GetIsolate();
  v8::HandleScope scope(isolate);

  if (args.Length() != 1) {
    TRI_V8_THROW_EXCEPTION_USAGE("increaseVersion(<key>)");
  }

  const std::string key = TRI_ObjectToString(args[0]);

  AgencyComm comm;
  if (! comm.increaseVersion(key)) {
    TRI_V8_THROW_EXCEPTION_MESSAGE(TRI_ERROR_INTERNAL, "unable to increase version");
  }

  TRI_V8_RETURN_TRUE();
}

////////////////////////////////////////////////////////////////////////////////
/// @brief gets a value from the agency
////////////////////////////////////////////////////////////////////////////////

static void JS_GetAgency (const v8::FunctionCallbackInfo<v8::Value>& args) {
  v8::Isolate* isolate = args.GetIsolate();
  v8::HandleScope scope(isolate);

  if (args.Length() < 1) {
    TRI_V8_THROW_EXCEPTION_USAGE("get(<key>, <recursive>, <withIndexes>)");
  }

  const std::string key = TRI_ObjectToString(args[0]);
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

  if (! result.successful()) {
    THROW_AGENCY_EXCEPTION(result);
  }

  result.parse("", false);

  v8::Handle<v8::Object> l = v8::Object::New(isolate);

  if (withIndexes) {
    std::map<std::string, AgencyCommResultEntry>::const_iterator it = result._values.begin();

    while (it != result._values.end()) {
      const std::string key = (*it).first;
      TRI_json_t const* json = (*it).second._json;
      const std::string idx = StringUtils::itoa((*it).second._index);

      if (json != 0) {
        v8::Handle<v8::Object> sub = v8::Object::New(isolate);

        sub->Set(TRI_V8_ASCII_STRING("value"), TRI_ObjectJson(isolate, json));
        sub->Set(TRI_V8_ASCII_STRING("index"), TRI_V8_STD_STRING(idx));

        l->Set(TRI_V8_STD_STRING(key), sub);
      }

      ++it;
    }
  }
  else {
    // return just the value for each key
    std::map<std::string, AgencyCommResultEntry>::const_iterator it = result._values.begin();

    while (it != result._values.end()) {
      const std::string key = (*it).first;
      TRI_json_t const* json = (*it).second._json;

      if (json != 0) {
        l->ForceSet(TRI_V8_STD_STRING(key), TRI_ObjectJson(isolate, json));
      }

      ++it;
    }
  }

  TRI_V8_RETURN(l);
}

////////////////////////////////////////////////////////////////////////////////
/// @brief lists a directory from the agency
////////////////////////////////////////////////////////////////////////////////

static void JS_ListAgency (const v8::FunctionCallbackInfo<v8::Value>& args) {
  v8::Isolate* isolate = args.GetIsolate();
  v8::HandleScope scope(isolate);

  if (args.Length() < 1) {
    TRI_V8_THROW_EXCEPTION_USAGE("list(<key>, <recursive>, <flat>)");
  }

  const std::string key = TRI_ObjectToString(args[0]);
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

  if (! result.successful()) {
    THROW_AGENCY_EXCEPTION(result);
  }

  // return just the value for each key
  result.parse("", true);
  std::map<std::string, AgencyCommResultEntry>::const_iterator it = result._values.begin();

  // skip first entry
  if (it != result._values.end()) {
    ++it;
  }

  if (flat) {
    v8::Handle<v8::Array> l = v8::Array::New(isolate);

    uint32_t i = 0;
    while (it != result._values.end()) {
      const std::string key = (*it).first;

      l->Set(i++, TRI_V8_STD_STRING(key));
      ++it;
    }
    TRI_V8_RETURN(l);
  }

  else {
    v8::Handle<v8::Object> l = v8::Object::New(isolate);

    while (it != result._values.end()) {
      const std::string key = (*it).first;
      const bool isDirectory = (*it).second._isDir;

      l->Set(TRI_V8_STD_STRING(key), v8::Boolean::New(isolate, isDirectory));
      ++it;
    }
    TRI_V8_RETURN(l);
  }
}

////////////////////////////////////////////////////////////////////////////////
/// @brief acquires a read-lock in the agency
////////////////////////////////////////////////////////////////////////////////

static void JS_LockReadAgency (const v8::FunctionCallbackInfo<v8::Value>& args) {
  v8::Isolate* isolate = args.GetIsolate();
  v8::HandleScope scope(isolate);

  if (args.Length() < 1) {
    TRI_V8_THROW_EXCEPTION_USAGE("lockRead(<part>, <ttl>, <timeout>)");
  }

  const std::string part = TRI_ObjectToString(args[0]);

  double ttl = 0.0;
  if (args.Length() > 1) {
    ttl = TRI_ObjectToDouble(args[1]);
  }

  double timeout = 0.0;
  if (args.Length() > 2) {
    timeout = TRI_ObjectToDouble(args[2]);
  }

  AgencyComm comm;
  if (! comm.lockRead(part, ttl, timeout)) {
    TRI_V8_THROW_EXCEPTION_MESSAGE(TRI_ERROR_INTERNAL, "unable to acquire lock");
  }

  TRI_V8_RETURN_TRUE();
}

////////////////////////////////////////////////////////////////////////////////
/// @brief acquires a write-lock in the agency
////////////////////////////////////////////////////////////////////////////////

static void JS_LockWriteAgency (const v8::FunctionCallbackInfo<v8::Value>& args) {
  v8::Isolate* isolate = args.GetIsolate();
  v8::HandleScope scope(isolate);

  if (args.Length() < 1) {
    TRI_V8_THROW_EXCEPTION_USAGE("lockWrite(<part>, <ttl>, <timeout>)");
  }

  const std::string part = TRI_ObjectToString(args[0]);

  double ttl = 0.0;
  if (args.Length() > 1) {
    ttl = TRI_ObjectToDouble(args[1]);
  }

  double timeout = 0.0;
  if (args.Length() > 2) {
    timeout = TRI_ObjectToDouble(args[2]);
  }

  AgencyComm comm;
  if (! comm.lockWrite(part, ttl, timeout)) {
    TRI_V8_THROW_EXCEPTION_MESSAGE(TRI_ERROR_INTERNAL, "unable to acquire lock");
  }

  TRI_V8_RETURN_TRUE();
}

////////////////////////////////////////////////////////////////////////////////
/// @brief releases a read-lock in the agency
////////////////////////////////////////////////////////////////////////////////

static void JS_UnlockReadAgency (const v8::FunctionCallbackInfo<v8::Value>& args) {
  v8::Isolate* isolate = args.GetIsolate();
  v8::HandleScope scope(isolate);

  if (args.Length() > 2) {
    TRI_V8_THROW_EXCEPTION_USAGE("unlockRead(<part>, <timeout>)");
  }

  const std::string part = TRI_ObjectToString(args[0]);

  double timeout = 0.0;
  if (args.Length() > 1) {
    timeout = TRI_ObjectToDouble(args[1]);
  }

  AgencyComm comm;
  if (! comm.unlockRead(part, timeout)) {
    TRI_V8_THROW_EXCEPTION_MESSAGE(TRI_ERROR_INTERNAL, "unable to release lock");
  }

  TRI_V8_RETURN_TRUE();
}

////////////////////////////////////////////////////////////////////////////////
/// @brief releases a write-lock in the agency
////////////////////////////////////////////////////////////////////////////////

static void JS_UnlockWriteAgency (const v8::FunctionCallbackInfo<v8::Value>& args) {
  v8::Isolate* isolate = args.GetIsolate();
  v8::HandleScope scope(isolate);

  if (args.Length() > 2) {
    TRI_V8_THROW_EXCEPTION_USAGE("unlockWrite(<part>, <timeout>)");
  }

  const std::string part = TRI_ObjectToString(args[0]);

  double timeout = 0.0;
  if (args.Length() > 1) {
    timeout = TRI_ObjectToDouble(args[1]);
  }

  AgencyComm comm;
  if (! comm.unlockWrite(part, timeout)) {
    TRI_V8_THROW_EXCEPTION_MESSAGE(TRI_ERROR_INTERNAL, "unable to release lock");
  }

  TRI_V8_RETURN_TRUE();
}

////////////////////////////////////////////////////////////////////////////////
/// @brief removes a value from the agency
////////////////////////////////////////////////////////////////////////////////

static void JS_RemoveAgency (const v8::FunctionCallbackInfo<v8::Value>& args) {
  v8::Isolate* isolate = args.GetIsolate();
  v8::HandleScope scope(isolate);

  if (args.Length() < 1) {
    TRI_V8_THROW_EXCEPTION_USAGE("remove(<key>, <recursive>)");
  }

  const std::string key = TRI_ObjectToString(args[0]);
  bool recursive = false;

  if (args.Length() > 1) {
    recursive = TRI_ObjectToBoolean(args[1]);
  }

  AgencyComm comm;
  AgencyCommResult result = comm.removeValues(key, recursive);

  if (! result.successful()) {
    THROW_AGENCY_EXCEPTION(result);
  }

  TRI_V8_RETURN_TRUE();
}

////////////////////////////////////////////////////////////////////////////////
/// @brief sets a value in the agency
////////////////////////////////////////////////////////////////////////////////

static void JS_SetAgency (const v8::FunctionCallbackInfo<v8::Value>& args) {
  v8::Isolate* isolate = args.GetIsolate();
  v8::HandleScope scope(isolate);

  if (args.Length() < 2) {
    TRI_V8_THROW_EXCEPTION_USAGE("set(<key>, <value>, <ttl>)");
  }

  const std::string key = TRI_ObjectToString(args[0]);

  TRI_json_t* json = TRI_ObjectToJson(isolate, args[1]);

  if (json == 0) {
    TRI_V8_THROW_EXCEPTION_PARAMETER("cannot convert <value> to JSON");
  }

  double ttl = 0.0;
  if (args.Length() > 2) {
    ttl = TRI_ObjectToDouble(args[2]);
  }

  AgencyComm comm;
  AgencyCommResult result = comm.setValue(key, json, ttl);

  TRI_FreeJson(TRI_UNKNOWN_MEM_ZONE, json);

  if (! result.successful()) {
    THROW_AGENCY_EXCEPTION(result);
  }

  TRI_V8_RETURN_TRUE();
}

////////////////////////////////////////////////////////////////////////////////
/// @brief watches a value in the agency
////////////////////////////////////////////////////////////////////////////////

static void JS_WatchAgency (const v8::FunctionCallbackInfo<v8::Value>& args) {
  v8::Isolate* isolate = args.GetIsolate();
  v8::HandleScope scope(isolate);

  if (args.Length() < 1) {
    TRI_V8_THROW_EXCEPTION_USAGE("watch(<key>, <waitIndex>, <timeout>, <recursive>)");
  }

  const std::string key = TRI_ObjectToString(args[0]);
  double timeout = 1.0;
  uint64_t waitIndex = 0;
  bool recursive = false;

  if (args.Length() > 1) {
    waitIndex = TRI_ObjectToUInt64(args[1], true);
  }
  if (args.Length() > 2) {
    timeout = TRI_ObjectToDouble(args[2]);
  }
  if (args.Length() > 3) {
    recursive = TRI_ObjectToBoolean(args[3]);
  }

  AgencyComm comm;
  AgencyCommResult result = comm.watchValue(key, waitIndex, timeout, recursive);

  if (result._statusCode == 0) {
    // watch timed out
    TRI_V8_RETURN_FALSE();
  }

  if (! result.successful()) {
    THROW_AGENCY_EXCEPTION(result);
  }

  result.parse("", false);
  std::map<std::string, AgencyCommResultEntry>::const_iterator it = result._values.begin();

  v8::Handle<v8::Object> l = v8::Object::New(isolate);

  while (it != result._values.end()) {
    const std::string key = (*it).first;
    TRI_json_t* json = (*it).second._json;

    if (json != 0) {
      l->Set(TRI_V8_STD_STRING(key), TRI_ObjectJson(isolate, json));
    }

    ++it;
  }

  TRI_V8_RETURN(l);
}

////////////////////////////////////////////////////////////////////////////////
/// @brief returns the agency endpoints
////////////////////////////////////////////////////////////////////////////////

static void JS_EndpointsAgency (const v8::FunctionCallbackInfo<v8::Value>& args) {
  v8::Isolate* isolate = args.GetIsolate();
  v8::HandleScope scope(isolate);

  if (args.Length() != 0) {
    TRI_V8_THROW_EXCEPTION_USAGE("endpoints()");
  }

  std::vector<std::string> endpoints = AgencyComm::getEndpoints();
  // make the list of endpoints unique
  std::sort(endpoints.begin(), endpoints.end());
  endpoints.assign(endpoints.begin(), std::unique(endpoints.begin(), endpoints.end()));

  v8::Handle<v8::Array> l = v8::Array::New(isolate);

  for (size_t i = 0; i < endpoints.size(); ++i) {
    const std::string endpoint = endpoints[i];

    l->Set((uint32_t) i, TRI_V8_STD_STRING(endpoint));
  }

  TRI_V8_RETURN(l);
}

////////////////////////////////////////////////////////////////////////////////
/// @brief returns the agency prefix
////////////////////////////////////////////////////////////////////////////////

static void JS_PrefixAgency (const v8::FunctionCallbackInfo<v8::Value>& args) {
  v8::Isolate* isolate = args.GetIsolate();
  v8::HandleScope scope(isolate);

  if (args.Length() > 1) {
    TRI_V8_THROW_EXCEPTION_USAGE("prefix(<strip>)");
  }

  bool strip = false;
  if (args.Length() > 0) {
    strip = TRI_ObjectToBoolean(args[0]);
  }

  const std::string prefix = AgencyComm::prefix();

  if (strip && prefix.size() > 2) {
    TRI_V8_RETURN_PAIR_STRING(prefix.c_str() + 1, (int) prefix.size() - 2);
  }

  TRI_V8_RETURN_STD_STRING(prefix);
}

////////////////////////////////////////////////////////////////////////////////
/// @brief sets the agency prefix
////////////////////////////////////////////////////////////////////////////////

static void JS_SetPrefixAgency (const v8::FunctionCallbackInfo<v8::Value>& args) {
  v8::Isolate* isolate = args.GetIsolate();
  v8::HandleScope scope(isolate);

  if (args.Length() != 1) {
    TRI_V8_THROW_EXCEPTION_USAGE("setPrefix(<prefix>)");
  }

  const bool result = AgencyComm::setPrefix(TRI_ObjectToString(args[0]));

  if (result) {
    TRI_V8_RETURN_TRUE();
  }
  else {
    TRI_V8_RETURN_FALSE();
  }
}

////////////////////////////////////////////////////////////////////////////////
/// @brief creates a uniqid
////////////////////////////////////////////////////////////////////////////////

static void JS_UniqidAgency (const v8::FunctionCallbackInfo<v8::Value>& args) {
  v8::Isolate* isolate = args.GetIsolate();
  v8::HandleScope scope(isolate);

  if (args.Length() < 1 || args.Length() > 3) {
    TRI_V8_THROW_EXCEPTION_USAGE("uniqid(<key>, <count>, <timeout>)");
  }

  const std::string key = TRI_ObjectToString(args[0]);

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

  if (! result.successful() || result._index == 0) {
    THROW_AGENCY_EXCEPTION(result);
  }

  const std::string value = StringUtils::itoa(result._index);

  TRI_V8_RETURN_STD_STRING(value);
}

////////////////////////////////////////////////////////////////////////////////
/// @brief returns the agency version
////////////////////////////////////////////////////////////////////////////////

static void JS_VersionAgency (const v8::FunctionCallbackInfo<v8::Value>& args) {
  v8::Isolate* isolate = args.GetIsolate();
  v8::HandleScope scope(isolate);

  if (args.Length() != 0) {
    TRI_V8_THROW_EXCEPTION_USAGE("version()");
  }

  AgencyComm comm;
  const std::string version = comm.getVersion();

  TRI_V8_RETURN_STD_STRING(version);
}

// -----------------------------------------------------------------------------
// --SECTION--                                            cluster info functions
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @brief whether or not a specific database exists
////////////////////////////////////////////////////////////////////////////////

static void JS_DoesDatabaseExistClusterInfo (const v8::FunctionCallbackInfo<v8::Value>& args) {
  v8::Isolate* isolate = args.GetIsolate();
  v8::HandleScope scope(isolate);

  if (args.Length() != 1) {
    TRI_V8_THROW_EXCEPTION_USAGE("doesDatabaseExist(<database-id>)");
  }

  const bool result = ClusterInfo::instance()->doesDatabaseExist(TRI_ObjectToString(args[0]), true);

  if (result) {
    TRI_V8_RETURN_TRUE();
  }
  else {
    TRI_V8_RETURN_FALSE();
  }
}

////////////////////////////////////////////////////////////////////////////////
/// @brief get the list of databases in the cluster
////////////////////////////////////////////////////////////////////////////////

static void JS_ListDatabases (const v8::FunctionCallbackInfo<v8::Value>& args) {
  v8::Isolate* isolate = args.GetIsolate();
  v8::HandleScope scope(isolate);

  if (args.Length() != 0) {
    TRI_V8_THROW_EXCEPTION_USAGE("listDatabases()");
  }

  vector<DatabaseID> res = ClusterInfo::instance()->listDatabases(true);
  v8::Handle<v8::Array> a = v8::Array::New(isolate, (int) res.size());
  vector<DatabaseID>::iterator it;
  int count = 0;
  for (it = res.begin(); it != res.end(); ++it) {
    a->Set((uint32_t) count++, TRI_V8_STD_STRING((*it)));
  }
  TRI_V8_RETURN(a);
}

////////////////////////////////////////////////////////////////////////////////
/// @brief flush the caches (used for testing only)
////////////////////////////////////////////////////////////////////////////////

static void JS_FlushClusterInfo (const v8::FunctionCallbackInfo<v8::Value>& args) {
  v8::Isolate* isolate = args.GetIsolate();
  v8::HandleScope scope(isolate);

  if (args.Length() != 0) {
    TRI_V8_THROW_EXCEPTION_USAGE("flush()");
  }

  ClusterInfo::instance()->flush();

  TRI_V8_RETURN_TRUE();
}

////////////////////////////////////////////////////////////////////////////////
/// @brief get the info about a collection in Plan
////////////////////////////////////////////////////////////////////////////////

static void JS_GetCollectionInfoClusterInfo (const v8::FunctionCallbackInfo<v8::Value>& args) {
  v8::Isolate* isolate = args.GetIsolate();
  v8::HandleScope scope(isolate);

  if (args.Length() != 2) {
    TRI_V8_THROW_EXCEPTION_USAGE("getCollectionInfo(<database-id>, <collection-id>)");
  }

  shared_ptr<CollectionInfo> ci
        = ClusterInfo::instance()->getCollection(TRI_ObjectToString(args[0]),
                                                 TRI_ObjectToString(args[1]));

  v8::Handle<v8::Object> result = v8::Object::New(isolate);
  const std::string cid = triagens::basics::StringUtils::itoa(ci->id());
  const std::string& name = ci->name();
  result->Set(TRI_V8_ASCII_STRING("id"),          TRI_V8_STD_STRING(cid));
  result->Set(TRI_V8_ASCII_STRING("name"),        TRI_V8_STD_STRING(name));
  result->Set(TRI_V8_ASCII_STRING("type"),        v8::Number::New(isolate, (int) ci->type()));
  result->Set(TRI_V8_ASCII_STRING("status"),      v8::Number::New(isolate, (int) ci->status()));

  const string statusString = ci->statusString();
  result->Set(TRI_V8_ASCII_STRING("statusString"), TRI_V8_STD_STRING(statusString));

  result->Set(TRI_V8_ASCII_STRING("deleted"),     v8::Boolean::New(isolate, ci->deleted()));
  result->Set(TRI_V8_ASCII_STRING("doCompact"),   v8::Boolean::New(isolate, ci->doCompact()));
  result->Set(TRI_V8_ASCII_STRING("isSystem"),    v8::Boolean::New(isolate, ci->isSystem()));
  result->Set(TRI_V8_ASCII_STRING("isVolatile"),  v8::Boolean::New(isolate, ci->isVolatile()));
  result->Set(TRI_V8_ASCII_STRING("waitForSync"), v8::Boolean::New(isolate, ci->waitForSync()));
  result->Set(TRI_V8_ASCII_STRING("journalSize"), v8::Number::New(isolate, ci->journalSize()));

  const std::vector<std::string>& sks = ci->shardKeys();
  v8::Handle<v8::Array> shardKeys = v8::Array::New(isolate, (int) sks.size());
  for (uint32_t i = 0, n = (uint32_t) sks.size(); i < n; ++i) {
    shardKeys->Set(i, TRI_V8_STD_STRING(sks[i]));
  }
  result->Set(TRI_V8_ASCII_STRING("shardKeys"), shardKeys);

  const std::map<std::string, std::string>& sis = ci->shardIds();
  v8::Handle<v8::Object> shardIds = v8::Object::New(isolate);
  std::map<std::string, std::string>::const_iterator it = sis.begin();
  while (it != sis.end()) {
    shardIds->Set(TRI_V8_STD_STRING((*it).first),
                  TRI_V8_STD_STRING((*it).second));
    ++it;
  }
  result->Set(TRI_V8_ASCII_STRING("shards"), shardIds);

  // TODO: fill "indexes"
  v8::Handle<v8::Array> indexes = v8::Array::New(isolate);
  result->Set(TRI_V8_ASCII_STRING("indexes"), indexes);
  TRI_V8_RETURN(result);
}

////////////////////////////////////////////////////////////////////////////////
/// @brief get the info about a collection in Current
////////////////////////////////////////////////////////////////////////////////

static void JS_GetCollectionInfoCurrentClusterInfo (const v8::FunctionCallbackInfo<v8::Value>& args) {
  v8::Isolate* isolate = args.GetIsolate();
  v8::HandleScope scope(isolate);

  if (args.Length() != 3) {
    TRI_V8_THROW_EXCEPTION_USAGE("getCollectionInfoCurrent(<database-id>, <collection-id>, <shardID>)");
  }

  ShardID shardID = TRI_ObjectToString(args[2]);

  shared_ptr<CollectionInfo> ci = ClusterInfo::instance()->getCollection(
                                           TRI_ObjectToString(args[0]),
                                           TRI_ObjectToString(args[1]));

  v8::Handle<v8::Object> result = v8::Object::New(isolate);
  // First some stuff from Plan for which Current does not make sense:
  const std::string cid = triagens::basics::StringUtils::itoa(ci->id());
  const std::string& name = ci->name();
  result->Set(TRI_V8_ASCII_STRING("id"), TRI_V8_STD_STRING(cid));
  result->Set(TRI_V8_ASCII_STRING("name"), TRI_V8_STD_STRING(name));

  shared_ptr<CollectionInfoCurrent> cic
          = ClusterInfo::instance()->getCollectionCurrent(
                                     TRI_ObjectToString(args[0]), cid);

  result->Set(TRI_V8_ASCII_STRING("type"), v8::Number::New(isolate, (int) ci->type()));
  // Now the Current information, if we actually got it:
  TRI_vocbase_col_status_e s = cic->status(shardID);
  result->Set(TRI_V8_ASCII_STRING("status"), v8::Number::New(isolate, (int) cic->status(shardID)));
  if (s == TRI_VOC_COL_STATUS_CORRUPTED) {
    TRI_V8_RETURN(result);
  }
  const string statusString = TRI_GetStatusStringCollectionVocBase(s);
  result->Set(TRI_V8_ASCII_STRING("statusString"),TRI_V8_STD_STRING(statusString));
  result->Set(TRI_V8_ASCII_STRING("deleted"),     v8::Boolean::New(isolate, cic->deleted(shardID)));
  result->Set(TRI_V8_ASCII_STRING("doCompact"),   v8::Boolean::New(isolate, cic->doCompact(shardID)));
  result->Set(TRI_V8_ASCII_STRING("isSystem"),    v8::Boolean::New(isolate, cic->isSystem(shardID)));
  result->Set(TRI_V8_ASCII_STRING("isVolatile"),  v8::Boolean::New(isolate, cic->isVolatile(shardID)));
  result->Set(TRI_V8_ASCII_STRING("waitForSync"), v8::Boolean::New(isolate, cic->waitForSync(shardID)));
  result->Set(TRI_V8_ASCII_STRING("journalSize"), v8::Number::New (isolate, cic->journalSize(shardID)));
  const std::string serverID = cic->responsibleServer(shardID);
  result->Set(TRI_V8_ASCII_STRING("DBServer"),    TRI_V8_STD_STRING(serverID));

  // TODO: fill "indexes"
  v8::Handle<v8::Array> indexes = v8::Array::New(isolate);
  result->Set(TRI_V8_ASCII_STRING("indexes"), indexes);

  // Finally, report any possible error:
  bool error = cic->error(shardID);
  result->Set(TRI_V8_ASCII_STRING("error"), v8::Boolean::New(isolate, error));
  if (error) {
    result->Set(TRI_V8_ASCII_STRING("errorNum"), v8::Number::New(isolate, cic->errorNum(shardID)));
    const string errorMessage = cic->errorMessage(shardID);
    result->Set(TRI_V8_ASCII_STRING("errorMessage"), TRI_V8_STD_STRING(errorMessage));
  }

  TRI_V8_RETURN(result);
}

////////////////////////////////////////////////////////////////////////////////
/// @brief get the responsible server
////////////////////////////////////////////////////////////////////////////////

static void JS_GetResponsibleServerClusterInfo (const v8::FunctionCallbackInfo<v8::Value>& args) {
  v8::Isolate* isolate = args.GetIsolate();
  v8::HandleScope scope(isolate);

  if (args.Length() != 1) {
    TRI_V8_THROW_EXCEPTION_USAGE("getResponsibleServer(<shard-id>)");
  }

  std::string const result = ClusterInfo::instance()->getResponsibleServer(TRI_ObjectToString(args[0]));

  TRI_V8_RETURN_STD_STRING(result);
}

////////////////////////////////////////////////////////////////////////////////
/// @brief get the responsible shard
////////////////////////////////////////////////////////////////////////////////

static void JS_GetResponsibleShardClusterInfo (const v8::FunctionCallbackInfo<v8::Value>& args) {
  v8::Isolate* isolate = args.GetIsolate();
  v8::HandleScope scope(isolate);

  if (args.Length() < 2 || args.Length() > 3) {
    TRI_V8_THROW_EXCEPTION_USAGE("getResponsibleShard(<collection-id>, <document>, <documentIsComplete>)");
  }
  
  if (! args[0]->IsString() && ! args[0]->IsStringObject()) {
    TRI_V8_THROW_TYPE_ERROR("expecting a string for <collection-id>)");
  }

  if (! args[1]->IsObject()) {
    TRI_V8_THROW_TYPE_ERROR("expecting an object for <document>)");
  }

  bool documentIsComplete = true;
  if (args.Length() > 2) {
    documentIsComplete = TRI_ObjectToBoolean(args[2]);
  }

  TRI_json_t* json = TRI_ObjectToJson(isolate, args[1]);

  if (json == nullptr) {
    TRI_V8_THROW_EXCEPTION_MEMORY();
  }

  ShardID shardId;
  CollectionID collectionId = TRI_ObjectToString(args[0]);
  bool usesDefaultShardingAttributes;
  int res = ClusterInfo::instance()->getResponsibleShard(collectionId, json, documentIsComplete, shardId, usesDefaultShardingAttributes);

  TRI_FreeJson(TRI_UNKNOWN_MEM_ZONE, json);

  if (res != TRI_ERROR_NO_ERROR) {
    TRI_V8_THROW_EXCEPTION(res);
  }

  v8::Handle<v8::Object> result = v8::Object::New(isolate);
  result->Set(TRI_V8_ASCII_STRING("shardId"), TRI_V8_STD_STRING(shardId));
  result->Set(TRI_V8_ASCII_STRING("usesDefaultShardingAttributes"), v8::Boolean::New(isolate, usesDefaultShardingAttributes));

  TRI_V8_RETURN(result);
}

////////////////////////////////////////////////////////////////////////////////
/// @brief get the server endpoint for a server
////////////////////////////////////////////////////////////////////////////////

static void JS_GetServerEndpointClusterInfo (const v8::FunctionCallbackInfo<v8::Value>& args) {
  v8::Isolate* isolate = args.GetIsolate();
  v8::HandleScope scope(isolate);

  if (args.Length() != 1) {
    TRI_V8_THROW_EXCEPTION_USAGE("getServerEndpoint(<server-id>)");
  }

  const std::string result = ClusterInfo::instance()->getServerEndpoint(TRI_ObjectToString(args[0]));

  TRI_V8_RETURN_STD_STRING(result);
}

////////////////////////////////////////////////////////////////////////////////
/// @brief returns the DBServers currently registered
////////////////////////////////////////////////////////////////////////////////

static void JS_GetDBServers (const v8::FunctionCallbackInfo<v8::Value>& args) {
  v8::Isolate* isolate = args.GetIsolate();
  v8::HandleScope scope(isolate);

  if (args.Length() != 0) {
    TRI_V8_THROW_EXCEPTION_USAGE("DBServers()");
  }

  std::vector<std::string> DBServers = ClusterInfo::instance()->getCurrentDBServers();

  v8::Handle<v8::Array> l = v8::Array::New(isolate);

  for (size_t i = 0; i < DBServers.size(); ++i) {
    ServerID const sid = DBServers[i];

    l->Set((uint32_t) i, TRI_V8_STD_STRING(sid));
  }

  TRI_V8_RETURN(l);
}

////////////////////////////////////////////////////////////////////////////////
/// @brief reloads the cache of DBServers currently registered
////////////////////////////////////////////////////////////////////////////////

static void JS_ReloadDBServers (const v8::FunctionCallbackInfo<v8::Value>& args) {
  v8::Isolate* isolate = args.GetIsolate();
  v8::HandleScope scope(isolate);

  if (args.Length() != 0) {
    TRI_V8_THROW_EXCEPTION_USAGE("reloadDBServers()");
  }

  ClusterInfo::instance()->loadCurrentDBServers();
  TRI_V8_RETURN_UNDEFINED();
}

////////////////////////////////////////////////////////////////////////////////
/// @brief returns a unique id
////////////////////////////////////////////////////////////////////////////////

static void JS_UniqidClusterInfo (const v8::FunctionCallbackInfo<v8::Value>& args) {
  v8::Isolate* isolate = args.GetIsolate();
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
    TRI_V8_THROW_EXCEPTION_MESSAGE(TRI_ERROR_INTERNAL, "unable to generate unique id");
  }

  const std::string id = StringUtils::itoa(value);

  TRI_V8_RETURN_STD_STRING(id);
}

// -----------------------------------------------------------------------------
// --SECTION--                                            server state functions
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @brief return the servers address
////////////////////////////////////////////////////////////////////////////////

static void JS_AddressServerState (const v8::FunctionCallbackInfo<v8::Value>& args) {
  v8::Isolate* isolate = args.GetIsolate();
  v8::HandleScope scope(isolate);

  if (args.Length() != 0) {
    TRI_V8_THROW_EXCEPTION_USAGE("address()");
  }

  const std::string address = ServerState::instance()->getAddress();
  TRI_V8_RETURN_STD_STRING(address);
}

////////////////////////////////////////////////////////////////////////////////
/// @brief flush the server state (used for testing only)
////////////////////////////////////////////////////////////////////////////////

static void JS_FlushServerState (const v8::FunctionCallbackInfo<v8::Value>& args) {
  v8::Isolate* isolate = args.GetIsolate();
  v8::HandleScope scope(isolate);

  if (args.Length() != 0) {
    TRI_V8_THROW_EXCEPTION_USAGE("flush()");
  }

  ServerState::instance()->flush();

  TRI_V8_RETURN_TRUE();
}

////////////////////////////////////////////////////////////////////////////////
/// @brief return the servers id
////////////////////////////////////////////////////////////////////////////////

static void JS_IdServerState (const v8::FunctionCallbackInfo<v8::Value>& args) {
  v8::Isolate* isolate = args.GetIsolate();
  v8::HandleScope scope(isolate);

  if (args.Length() != 0) {
    TRI_V8_THROW_EXCEPTION_USAGE("id()");
  }

  const std::string id = ServerState::instance()->getId();
  TRI_V8_RETURN_STD_STRING(id);
}

////////////////////////////////////////////////////////////////////////////////
/// @brief returns the data path
////////////////////////////////////////////////////////////////////////////////

static void JS_DataPathServerState (const v8::FunctionCallbackInfo<v8::Value>& args) {
  v8::Isolate* isolate = args.GetIsolate();
  v8::HandleScope scope(isolate);

  if (args.Length() != 0) {
    TRI_V8_THROW_EXCEPTION_USAGE("dataPath()");
  }

  const std::string path = ServerState::instance()->getDataPath();
  TRI_V8_RETURN_STD_STRING(path);
}

////////////////////////////////////////////////////////////////////////////////
/// @brief returns the log path
////////////////////////////////////////////////////////////////////////////////

static void JS_LogPathServerState (const v8::FunctionCallbackInfo<v8::Value>& args) {
  v8::Isolate* isolate = args.GetIsolate();
  v8::HandleScope scope(isolate);

  if (args.Length() != 0) {
    TRI_V8_THROW_EXCEPTION_USAGE("logPath()");
  }

  const std::string path = ServerState::instance()->getLogPath();
  TRI_V8_RETURN_STD_STRING(path);
}

////////////////////////////////////////////////////////////////////////////////
/// @brief returns the agent path
////////////////////////////////////////////////////////////////////////////////

static void JS_AgentPathServerState (const v8::FunctionCallbackInfo<v8::Value>& args) {
  v8::Isolate* isolate = args.GetIsolate();
  v8::HandleScope scope(isolate);

  if (args.Length() != 0) {
    TRI_V8_THROW_EXCEPTION_USAGE("agentPath()");
  }

  const std::string path = ServerState::instance()->getAgentPath();
  TRI_V8_RETURN_STD_STRING(path);
}

////////////////////////////////////////////////////////////////////////////////
/// @brief returns the arangod path
////////////////////////////////////////////////////////////////////////////////

static void JS_ArangodPathServerState (const v8::FunctionCallbackInfo<v8::Value>& args) {
  v8::Isolate* isolate = args.GetIsolate();
  v8::HandleScope scope(isolate);

  if (args.Length() != 0) {
    TRI_V8_THROW_EXCEPTION_USAGE("arangodPath()");
  }

  const std::string path = ServerState::instance()->getArangodPath();
  TRI_V8_RETURN_STD_STRING(path);
}

////////////////////////////////////////////////////////////////////////////////
/// @brief returns the javascript startup path
////////////////////////////////////////////////////////////////////////////////

static void JS_JavaScriptPathServerState (const v8::FunctionCallbackInfo<v8::Value>& args) {
  v8::Isolate* isolate = args.GetIsolate();
  v8::HandleScope scope(isolate);

  if (args.Length() != 0) {
    TRI_V8_THROW_EXCEPTION_USAGE("javaScriptPath()");
  }

  const std::string path = ServerState::instance()->getJavaScriptPath();
  TRI_V8_RETURN_STD_STRING(path);
}

////////////////////////////////////////////////////////////////////////////////
/// @brief returns the DBserver config
////////////////////////////////////////////////////////////////////////////////

static void JS_DBserverConfigServerState (const v8::FunctionCallbackInfo<v8::Value>& args) {
  v8::Isolate* isolate = args.GetIsolate();
  v8::HandleScope scope(isolate);

  if (args.Length() != 0) {
    TRI_V8_THROW_EXCEPTION_USAGE("dbserverConfig()");
  }

  const std::string path = ServerState::instance()->getDBserverConfig();
  TRI_V8_RETURN_STD_STRING(path);
}

////////////////////////////////////////////////////////////////////////////////
/// @brief returns the coordinator config
////////////////////////////////////////////////////////////////////////////////

static void JS_CoordinatorConfigServerState (const v8::FunctionCallbackInfo<v8::Value>& args) {
  v8::Isolate* isolate = args.GetIsolate();
  v8::HandleScope scope(isolate);

  if (args.Length() != 0) {
    TRI_V8_THROW_EXCEPTION_USAGE("coordinatorConfig()");
  }

  const std::string path = ServerState::instance()->getCoordinatorConfig();
  TRI_V8_RETURN_STD_STRING(path);
}

////////////////////////////////////////////////////////////////////////////////
/// @brief returns true, if the dispatcher frontend should be disabled
////////////////////////////////////////////////////////////////////////////////

static void JS_DisableDipatcherFrontendServerState (const v8::FunctionCallbackInfo<v8::Value>& args) {
  v8::Isolate* isolate = args.GetIsolate();
  v8::HandleScope scope(isolate);

  if (args.Length() != 0) {
    TRI_V8_THROW_EXCEPTION_USAGE("disableDispatcherFrontend()");
  }

  if (ServerState::instance()->getDisableDispatcherFrontend()) {
    TRI_V8_RETURN_TRUE();
  }
  else {
    TRI_V8_RETURN_FALSE();
  }
}

////////////////////////////////////////////////////////////////////////////////
/// @brief returns true, if the dispatcher kickstarter should be disabled
////////////////////////////////////////////////////////////////////////////////

static void JS_DisableDipatcherKickstarterServerState (const v8::FunctionCallbackInfo<v8::Value>& args) {
  v8::Isolate* isolate = args.GetIsolate();
  v8::HandleScope scope(isolate);

  if (args.Length() != 0) {
    TRI_V8_THROW_EXCEPTION_USAGE("disableDispatcherKickstarter()");
  }

  if (ServerState::instance()->getDisableDispatcherKickstarter()) {
    TRI_V8_RETURN_TRUE();
  }
  else {
    TRI_V8_RETURN_FALSE();
  }
}

////////////////////////////////////////////////////////////////////////////////
/// @brief return whether the cluster is initialised
////////////////////////////////////////////////////////////////////////////////

static void JS_InitialisedServerState (const v8::FunctionCallbackInfo<v8::Value>& args) {
  v8::Isolate* isolate = args.GetIsolate();
  v8::HandleScope scope(isolate);

  if (args.Length() != 0) {
    TRI_V8_THROW_EXCEPTION_USAGE("initialised()");
  }

  if (ServerState::instance()->initialised()) {
    TRI_V8_RETURN_TRUE();
  }
  else {
    TRI_V8_RETURN_FALSE();
  }
}

////////////////////////////////////////////////////////////////////////////////
/// @brief whether or not the server is a coordinator
////////////////////////////////////////////////////////////////////////////////

static void JS_IsCoordinatorServerState (const v8::FunctionCallbackInfo<v8::Value>& args) {
  v8::Isolate* isolate = args.GetIsolate();
  v8::HandleScope scope(isolate);

  if (args.Length() != 0) {
    TRI_V8_THROW_EXCEPTION_USAGE("isCoordinator()");
  }

  if (ServerState::instance()->getRole() == ServerState::ROLE_COORDINATOR) {
    TRI_V8_RETURN_TRUE();
  }
  else {
    TRI_V8_RETURN_FALSE();
  }
}

////////////////////////////////////////////////////////////////////////////////
/// @brief returns the server role
////////////////////////////////////////////////////////////////////////////////

static void JS_RoleServerState (const v8::FunctionCallbackInfo<v8::Value>& args) {
  v8::Isolate* isolate = args.GetIsolate();
  v8::HandleScope scope(isolate);

  if (args.Length() != 0) {
    TRI_V8_THROW_EXCEPTION_USAGE("role()");
  }

  const std::string role = ServerState::roleToString(ServerState::instance()->getRole());

  TRI_V8_RETURN_STD_STRING(role);
}

////////////////////////////////////////////////////////////////////////////////
/// @brief sets the server id (used for testing)
////////////////////////////////////////////////////////////////////////////////

static void JS_SetIdServerState (const v8::FunctionCallbackInfo<v8::Value>& args) {
  v8::Isolate* isolate = args.GetIsolate();
  v8::HandleScope scope(isolate);



  if (args.Length() != 1) {
    TRI_V8_THROW_EXCEPTION_USAGE("setId(<id>)");
  }

  const std::string id = TRI_ObjectToString(args[0]);
  ServerState::instance()->setId(id);

  TRI_V8_RETURN_TRUE();
}

////////////////////////////////////////////////////////////////////////////////
/// @brief sets the server role (used for testing)
////////////////////////////////////////////////////////////////////////////////

static void JS_SetRoleServerState (const v8::FunctionCallbackInfo<v8::Value>& args) {
  v8::Isolate* isolate = args.GetIsolate();
  v8::HandleScope scope(isolate);

  if (args.Length() != 1) {
    TRI_V8_THROW_EXCEPTION_USAGE("setRole(<role>)");
  }

  const std::string role = TRI_ObjectToString(args[0]);
  ServerState::RoleEnum r = ServerState::stringToRole(role);

  if (r == ServerState::ROLE_UNDEFINED) {
    TRI_V8_THROW_EXCEPTION_PARAMETER("<role> is invalid");
  }

  ServerState::instance()->setRole(r);

  TRI_V8_RETURN_TRUE();
}

////////////////////////////////////////////////////////////////////////////////
/// @brief returns the server state
////////////////////////////////////////////////////////////////////////////////

static void JS_StatusServerState (const v8::FunctionCallbackInfo<v8::Value>& args) {
  v8::Isolate* isolate = args.GetIsolate();
  v8::HandleScope scope(isolate);

  if (args.Length() != 0) {
    TRI_V8_THROW_EXCEPTION_USAGE("status()");
  }

  const std::string state = ServerState::stateToString(ServerState::instance()->getState());

  TRI_V8_RETURN_STD_STRING(state);
}

////////////////////////////////////////////////////////////////////////////////
/// @brief returns the server state
////////////////////////////////////////////////////////////////////////////////

static void JS_GetClusterAuthentication (const v8::FunctionCallbackInfo<v8::Value>& args) {
  v8::Isolate* isolate = args.GetIsolate();
  v8::HandleScope scope(isolate);

  if (args.Length() != 0) {
    TRI_V8_THROW_EXCEPTION_USAGE("getClusterAuthentication()");
  }

  std::string auth;
  if (ServerState::instance()->getRole() == ServerState::ROLE_UNDEFINED) {
    // Only on dispatchers, otherwise this would be a security risk!
    auth = ServerState::instance()->getAuthentication();
  }

  TRI_V8_RETURN_STD_STRING(auth);
}

////////////////////////////////////////////////////////////////////////////////
/// @brief prepare to send a request
///
/// this is used for asynchronous as well as synchronous requests.
////////////////////////////////////////////////////////////////////////////////

static void PrepareClusterCommRequest (
                        const v8::FunctionCallbackInfo<v8::Value>& args,
                        triagens::rest::HttpRequest::HttpRequestType& reqType,
                        string& destination,
                        string& path,
                        string& body,
                        map<string, string>* headerFields,
                        ClientTransactionID& clientTransactionID,
                        CoordTransactionID& coordTransactionID,
                        double& timeout) {
  v8::Isolate* isolate = args.GetIsolate();
  TRI_V8_CURRENT_GLOBALS_AND_SCOPE;

  TRI_ASSERT(args.Length() >= 4);

  reqType = triagens::rest::HttpRequest::HTTP_REQUEST_GET;
  if (args[0]->IsString()) {
    TRI_Utf8ValueNFC UTF8(TRI_UNKNOWN_MEM_ZONE, args[0]);
    string methstring = *UTF8;
    reqType = triagens::rest::HttpRequest::translateMethod(methstring);
    if (reqType == triagens::rest::HttpRequest::HTTP_REQUEST_ILLEGAL) {
      reqType = triagens::rest::HttpRequest::HTTP_REQUEST_GET;
    }
  }

  destination = TRI_ObjectToString(args[1]);

  string dbname = TRI_ObjectToString(args[2]);

  path = TRI_ObjectToString(args[3]);
  path = "/_db/" + dbname + path;

  body.clear();
  if (args.Length() > 4) {
    body = TRI_ObjectToString(args[4]);
  }

  if (args.Length() > 5 && args[5]->IsObject()) {
    v8::Handle<v8::Object> obj = args[5].As<v8::Object>();
    v8::Handle<v8::Array> props = obj->GetOwnPropertyNames();
    uint32_t i;
    for (i = 0; i < props->Length(); ++i) {
      v8::Handle<v8::Value> prop = props->Get(i);
      v8::Handle<v8::Value> val = obj->Get(prop);
      string propstring = TRI_ObjectToString(prop);
      string valstring = TRI_ObjectToString(val);
      if (propstring != "") {
        headerFields->insert(pair<string,string>(propstring, valstring));
      }
    }
  }

  clientTransactionID = "";
  coordTransactionID = 0;
  timeout = 24*3600.0;

  if (args.Length() > 6 && args[6]->IsObject()) {
    v8::Handle<v8::Object> opt = args[6].As<v8::Object>();
    TRI_GET_GLOBAL_STRING(ClientTransactionIDKey);
    if (opt->Has(ClientTransactionIDKey)) {
      clientTransactionID
        = TRI_ObjectToString(opt->Get(ClientTransactionIDKey));
    }
    TRI_GET_GLOBAL_STRING(CoordTransactionIDKey);
    if (opt->Has(CoordTransactionIDKey)) {
      coordTransactionID
        = TRI_ObjectToUInt64(opt->Get(CoordTransactionIDKey), true);
    }
    TRI_GET_GLOBAL_STRING(TimeoutKey);
    if (opt->Has(TimeoutKey)) {
      timeout
        = TRI_ObjectToDouble(opt->Get(TimeoutKey));
    }
  }
  if (clientTransactionID == "") {
    clientTransactionID = StringUtils::itoa(TRI_NewTickServer());
  }
  if (coordTransactionID == 0) {
    coordTransactionID = TRI_NewTickServer();
  }
  if (timeout == 0) {
    timeout = 24*3600.0;
  }
}

////////////////////////////////////////////////////////////////////////////////
/// @brief prepare a ClusterCommResult for JavaScript
////////////////////////////////////////////////////////////////////////////////

void Return_PrepareClusterCommResultForJS(const v8::FunctionCallbackInfo<v8::Value>& args,
                                          ClusterCommResult const* res) {
  v8::Isolate* isolate = args.GetIsolate();
  TRI_V8_CURRENT_GLOBALS_AND_SCOPE;

  v8::Handle<v8::Object> r = v8::Object::New(isolate);
  if (0 == res) {
    TRI_GET_GLOBAL_STRING(ErrorMessageKey);
    r->Set(ErrorMessageKey, TRI_V8_ASCII_STRING("out of memory"));
  } else if (res->dropped) {
    TRI_GET_GLOBAL_STRING(ErrorMessageKey);
    r->Set(ErrorMessageKey, TRI_V8_ASCII_STRING("operation was dropped"));
  } else {
    TRI_GET_GLOBAL_STRING(ClientTransactionIDKey);
    r->Set(ClientTransactionIDKey,
           TRI_V8_STD_STRING(res->clientTransactionID));

    // convert the ids to strings as uint64_t might be too big for JavaScript numbers
    TRI_GET_GLOBAL_STRING(CoordTransactionIDKey);
    std::string id = StringUtils::itoa(res->coordTransactionID);
    r->Set(CoordTransactionIDKey, TRI_V8_STD_STRING(id));

    id = StringUtils::itoa(res->operationID);
    TRI_GET_GLOBAL_STRING(OperationIDKey);
    r->Set(OperationIDKey, TRI_V8_STD_STRING(id));

    TRI_GET_GLOBAL_STRING(ShardIDKey);
    r->Set(ShardIDKey, TRI_V8_STD_STRING(res->shardID));
    if (res->status == CL_COMM_SUBMITTED) {
      TRI_GET_GLOBAL_STRING(StatusKey);
      r->Set(StatusKey, TRI_V8_ASCII_STRING("SUBMITTED"));
    }
    else if (res->status == CL_COMM_SENDING) {
      TRI_GET_GLOBAL_STRING(StatusKey);
      r->Set(StatusKey, TRI_V8_ASCII_STRING("SENDING"));
    }
    else if (res->status == CL_COMM_SENT) {
      TRI_GET_GLOBAL_STRING(StatusKey);
      r->Set(StatusKey, TRI_V8_ASCII_STRING("SENT"));
      // This might be the result of a synchronous request and thus
      // contain the actual response. If it is an asynchronous request
      // which has not yet been answered, the following information is
      // probably rather boring:

      // The headers:
      v8::Handle<v8::Object> h = v8::Object::New(isolate);
      for (auto const& i : res->result->getHeaderFields()) {
        h->Set(TRI_V8_STD_STRING(i.first),
               TRI_V8_STD_STRING(i.second));
      }
      r->Set(TRI_V8_ASCII_STRING("headers"), h);

      // The body:
      triagens::basics::StringBuffer& body = res->result->getBody();
      if (body.length() != 0) {
        r->Set(TRI_V8_ASCII_STRING("body"), TRI_V8_STD_STRING(body));
      }
    }
    else if (res->status == CL_COMM_TIMEOUT) {
      TRI_GET_GLOBAL_STRING(StatusKey);
      r->Set(StatusKey, TRI_V8_ASCII_STRING("TIMEOUT"));
      TRI_GET_GLOBAL_STRING(TimeoutKey);
      r->Set(TimeoutKey,v8::BooleanObject::New(true));
    }
    else if (res->status == CL_COMM_ERROR) {
      TRI_GET_GLOBAL_STRING(StatusKey);
      r->Set(StatusKey, TRI_V8_ASCII_STRING("ERROR"));

      if (res->result && res->result->isComplete()) {
        v8::Handle<v8::Object> details = v8::Object::New(isolate);
        details->Set(TRI_V8_ASCII_STRING("code"), v8::Number::New(isolate, res->result->getHttpReturnCode()));
        details->Set(TRI_V8_ASCII_STRING("message"), TRI_V8_STD_STRING(res->result->getHttpReturnMessage()));
        details->Set(TRI_V8_ASCII_STRING("body"), TRI_V8_STD_STRING(res->result->getBody()));

        r->Set(TRI_V8_ASCII_STRING("details"), details);
        TRI_GET_GLOBAL_STRING(ErrorMessageKey);
        r->Set(ErrorMessageKey, TRI_V8_ASCII_STRING("got bad HTTP response"));
      }
      else {
        TRI_GET_GLOBAL_STRING(ErrorMessageKey);
        r->Set(ErrorMessageKey, TRI_V8_ASCII_STRING("got no HTTP response, DBserver seems gone"));
      }
    }
    else if (res->status == CL_COMM_DROPPED) {
      TRI_GET_GLOBAL_STRING(StatusKey);
      r->Set(StatusKey, TRI_V8_ASCII_STRING("DROPPED"));
      TRI_GET_GLOBAL_STRING(ErrorMessageKey);
      r->Set(ErrorMessageKey, TRI_V8_ASCII_STRING("request dropped whilst waiting for answer"));
    }
    else {   // Everything is OK
      TRI_ASSERT(res->status == CL_COMM_RECEIVED);
      // The headers:
      v8::Handle<v8::Object> h = v8::Object::New(isolate);
      TRI_GET_GLOBAL_STRING(StatusKey);
      r->Set(StatusKey, TRI_V8_ASCII_STRING("RECEIVED"));
      map<string,string> headers = res->answer->headers();
      map<string,string>::iterator i;
      for (i = headers.begin(); i != headers.end(); ++i) {
        h->Set(TRI_V8_STD_STRING(i->first),
               TRI_V8_STD_STRING(i->second));
      }
      r->Set(TRI_V8_ASCII_STRING("headers"), h);

      // The body:
      if (0 != res->answer->body()) {
        r->Set(TRI_V8_ASCII_STRING("body"),
               TRI_V8_PAIR_STRING(res->answer->body(), (int) res->answer->bodySize()));
      }
    }
  }
  TRI_V8_RETURN(r);
}

////////////////////////////////////////////////////////////////////////////////
/// @brief send an asynchronous request
////////////////////////////////////////////////////////////////////////////////

static void JS_AsyncRequest (const v8::FunctionCallbackInfo<v8::Value>& args) {
  v8::Isolate* isolate = args.GetIsolate();
  v8::HandleScope scope(isolate);



  if (args.Length() < 4 || args.Length() > 7) {
    TRI_V8_THROW_EXCEPTION_USAGE("asyncRequest("
      "reqType, destination, dbname, path, body, headers, options)");
  }
  // Possible options:
  //   - clientTransactionID  (string)
  //   - coordTransactionID   (number)
  //   - timeout              (number)

  // Disabled to allow communication originating in a DBserver:
  // 31.7.2014 Max

  // if (ServerState::instance()->getRole() != ServerState::ROLE_COORDINATOR) {
  //  TRI_V8_THROW_EXCEPTION_INTERNAL(scope,"request works only in coordinator role");
  //}

  ClusterComm* cc = ClusterComm::instance();

  if (cc == 0) {
    TRI_V8_THROW_EXCEPTION_MESSAGE(TRI_ERROR_INTERNAL,
                             "clustercomm object not found");
  }

  triagens::rest::HttpRequest::HttpRequestType reqType;
  string destination;
  string path;
  string *body = new string();
  map<string, string>* headerFields = new map<string, string>;
  ClientTransactionID clientTransactionID;
  CoordTransactionID coordTransactionID;
  double timeout;

  PrepareClusterCommRequest(args, reqType, destination, path,*body,headerFields,
                            clientTransactionID, coordTransactionID, timeout);

  ClusterCommResult const* res;

  res = cc->asyncRequest(clientTransactionID, coordTransactionID, destination,
                         reqType, path, body, true, headerFields, 0, timeout);

  if (res == 0) {
    TRI_V8_THROW_EXCEPTION_MESSAGE(TRI_ERROR_INTERNAL,
                             "couldn't queue async request");
  }

  LOG_DEBUG("JS_AsyncRequest: request has been submitted");

  Return_PrepareClusterCommResultForJS(args, res);
  delete res;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief send a synchronous request
////////////////////////////////////////////////////////////////////////////////

static void JS_SyncRequest (const v8::FunctionCallbackInfo<v8::Value>& args) {
  v8::Isolate* isolate = args.GetIsolate();
  v8::HandleScope scope(isolate);



  if (args.Length() < 4 || args.Length() > 7) {
    TRI_V8_THROW_EXCEPTION_USAGE("syncRequest("
      "reqType, destination, dbname, path, body, headers, options)");
  }
  // Possible options:
  //   - clientTransactionID  (string)
  //   - coordTransactionID   (number)
  //   - timeout              (number)

  // Disabled to allow communication originating in a DBserver:
  // 31.7.2014 Max

  //if (ServerState::instance()->getRole() != ServerState::ROLE_COORDINATOR) {
  //  TRI_V8_THROW_EXCEPTION_INTERNAL(scope,"request works only in coordinator role");
  //}

  ClusterComm* cc = ClusterComm::instance();

  if (cc == 0) {
    TRI_V8_THROW_EXCEPTION_MESSAGE(TRI_ERROR_INTERNAL,
                             "clustercomm object not found");
  }

  triagens::rest::HttpRequest::HttpRequestType reqType;
  string destination;
  string path;
  string body;
  map<string, string>* headerFields = new map<string, string>;
  ClientTransactionID clientTransactionID;
  CoordTransactionID coordTransactionID;
  double timeout;

  PrepareClusterCommRequest(args, reqType, destination, path, body, headerFields,
                            clientTransactionID, coordTransactionID, timeout);

  ClusterCommResult const* res;

  res = cc->syncRequest(clientTransactionID, coordTransactionID, destination,
                         reqType, path, body, *headerFields, timeout);

  delete headerFields;

  if (res == 0) {
    TRI_V8_THROW_EXCEPTION_MESSAGE(TRI_ERROR_INTERNAL,
                             "couldn't do sync request");
  }

  LOG_DEBUG("JS_SyncRequest: request has been done");

  Return_PrepareClusterCommResultForJS(args, res);
  delete res;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief enquire information about an asynchronous request
////////////////////////////////////////////////////////////////////////////////

static void JS_Enquire (const v8::FunctionCallbackInfo<v8::Value>& args) {
  v8::Isolate* isolate = args.GetIsolate();
  v8::HandleScope scope(isolate);

  if (args.Length() != 1) {
    TRI_V8_THROW_EXCEPTION_USAGE("enquire(operationID)");
  }

  // Disabled to allow communication originating in a DBserver:
  // 31.7.2014 Max
  
  // if (ServerState::instance()->getRole() != ServerState::ROLE_COORDINATOR) {
  //   TRI_V8_THROW_EXCEPTION_INTERNAL(scope,"request works only in coordinator role");
  // }

  ClusterComm* cc = ClusterComm::instance();

  if (cc == 0) {
    TRI_V8_THROW_EXCEPTION_MESSAGE(TRI_ERROR_INTERNAL,
                             "clustercomm object not found");
  }

  OperationID operationID = TRI_ObjectToUInt64(args[0], true);

  ClusterCommResult const* res;

  LOG_DEBUG("JS_Enquire: calling ClusterComm::enquire()");

  res = cc->enquire(operationID);

  Return_PrepareClusterCommResultForJS(args, res);
  delete res;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief wait for the result of an asynchronous request
////////////////////////////////////////////////////////////////////////////////

static void JS_Wait (const v8::FunctionCallbackInfo<v8::Value>& args) {
  v8::Isolate* isolate = args.GetIsolate();
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

  // Disabled to allow communication originating in a DBserver:
  // 31.7.2014 Max

  // if (ServerState::instance()->getRole() != ServerState::ROLE_COORDINATOR) {
  //   TRI_V8_THROW_EXCEPTION_INTERNAL(scope,"request works only in coordinator role");
  // }

  ClusterComm* cc = ClusterComm::instance();

  if (cc == 0) {
    TRI_V8_THROW_EXCEPTION_MESSAGE(TRI_ERROR_INTERNAL,
                             "clustercomm object not found");
  }

  ClientTransactionID myclientTransactionID = "";
  CoordTransactionID mycoordTransactionID = 0;
  OperationID myoperationID = 0;
  ShardID myshardID = "";
  double mytimeout = 24*3600.0;

  if (args[0]->IsObject()) {
    v8::Handle<v8::Object> obj = args[0].As<v8::Object>();
    TRI_GET_GLOBAL_STRING(ClientTransactionIDKey);
    if (obj->Has(ClientTransactionIDKey)) {
      myclientTransactionID
        = TRI_ObjectToString(obj->Get(ClientTransactionIDKey));
    }
    TRI_GET_GLOBAL_STRING(CoordTransactionIDKey);
    if (obj->Has(CoordTransactionIDKey)) {
      mycoordTransactionID
        = TRI_ObjectToUInt64(obj->Get(CoordTransactionIDKey), true);
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
        mytimeout = 24*3600.0;
      }
    }
  }

  ClusterCommResult const* res;

  LOG_DEBUG("JS_Wait: calling ClusterComm::wait()");

  res = cc->wait(myclientTransactionID,
                 mycoordTransactionID,
                 myoperationID,
                 myshardID,
                 mytimeout);

  Return_PrepareClusterCommResultForJS(args, res);
  delete res;
}


////////////////////////////////////////////////////////////////////////////////
/// @brief drop the result of an asynchronous request
////////////////////////////////////////////////////////////////////////////////

static void JS_Drop (const v8::FunctionCallbackInfo<v8::Value>& args) {
  v8::Isolate* isolate = args.GetIsolate();
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
  //   TRI_V8_THROW_EXCEPTION_INTERNAL(scope,"request works only in coordinator role");
  // }

  ClusterComm* cc = ClusterComm::instance();

  if (cc == 0) {
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
      myclientTransactionID
        = TRI_ObjectToString(obj->Get(ClientTransactionIDKey));
    }
    TRI_GET_GLOBAL_STRING(CoordTransactionIDKey);
    if (obj->Has(CoordTransactionIDKey)) {
      mycoordTransactionID
        = TRI_ObjectToUInt64(obj->Get(CoordTransactionIDKey), true);
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

  LOG_DEBUG("JS_Drop: calling ClusterComm::drop()");

  cc->drop(myclientTransactionID, mycoordTransactionID, myoperationID, myshardID);

  TRI_V8_RETURN_UNDEFINED();
}


// -----------------------------------------------------------------------------
// --SECTION--                                                  public functions
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @brief creates a global cluster context
////////////////////////////////////////////////////////////////////////////////

void TRI_InitV8Cluster (v8::Isolate* isolate, v8::Handle<v8::Context> context) {
  TRI_V8_CURRENT_GLOBALS_AND_SCOPE;
  TRI_ASSERT(v8g != 0);

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
  TRI_AddMethodVocbase(isolate, rt, TRI_V8_ASCII_STRING("createDirectory"), JS_CreateDirectoryAgency);
  TRI_AddMethodVocbase(isolate, rt, TRI_V8_ASCII_STRING("get"), JS_GetAgency);
  TRI_AddMethodVocbase(isolate, rt, TRI_V8_ASCII_STRING("isEnabled"), JS_IsEnabledAgency);
  TRI_AddMethodVocbase(isolate, rt, TRI_V8_ASCII_STRING("increaseVersion"), JS_IncreaseVersionAgency);
  TRI_AddMethodVocbase(isolate, rt, TRI_V8_ASCII_STRING("list"), JS_ListAgency);
  TRI_AddMethodVocbase(isolate, rt, TRI_V8_ASCII_STRING("lockRead"), JS_LockReadAgency);
  TRI_AddMethodVocbase(isolate, rt, TRI_V8_ASCII_STRING("lockWrite"), JS_LockWriteAgency);
  TRI_AddMethodVocbase(isolate, rt, TRI_V8_ASCII_STRING("remove"), JS_RemoveAgency);
  TRI_AddMethodVocbase(isolate, rt, TRI_V8_ASCII_STRING("set"), JS_SetAgency);
  TRI_AddMethodVocbase(isolate, rt, TRI_V8_ASCII_STRING("watch"), JS_WatchAgency);
  TRI_AddMethodVocbase(isolate, rt, TRI_V8_ASCII_STRING("endpoints"), JS_EndpointsAgency);
  TRI_AddMethodVocbase(isolate, rt, TRI_V8_ASCII_STRING("prefix"), JS_PrefixAgency);
  TRI_AddMethodVocbase(isolate, rt, TRI_V8_ASCII_STRING("setPrefix"), JS_SetPrefixAgency, true);
  TRI_AddMethodVocbase(isolate, rt, TRI_V8_ASCII_STRING("uniqid"), JS_UniqidAgency);
  TRI_AddMethodVocbase(isolate, rt, TRI_V8_ASCII_STRING("unlockRead"), JS_UnlockReadAgency);
  TRI_AddMethodVocbase(isolate, rt, TRI_V8_ASCII_STRING("unlockWrite"), JS_UnlockWriteAgency);
  TRI_AddMethodVocbase(isolate, rt, TRI_V8_ASCII_STRING("version"), JS_VersionAgency);

  v8g->AgencyTempl.Reset(isolate, rt);
  ft->SetClassName(TRI_V8_ASCII_STRING("ArangoAgencyCtor"));

  TRI_AddGlobalFunctionVocbase(isolate, context, TRI_V8_ASCII_STRING("ArangoAgencyCtor"), ft->GetFunction(), true);

  // register the global object
  v8::Handle<v8::Object> aa = rt->NewInstance();
  if (! aa.IsEmpty()) {
    TRI_AddGlobalVariableVocbase(isolate, context, TRI_V8_ASCII_STRING("ArangoAgency"), aa);
  }

  // .............................................................................
  // generate the cluster info template
  // .............................................................................

  ft = v8::FunctionTemplate::New(isolate);
  ft->SetClassName(TRI_V8_ASCII_STRING("ArangoClusterInfo"));

  rt = ft->InstanceTemplate();
  rt->SetInternalFieldCount(2);

  TRI_AddMethodVocbase(isolate, rt, TRI_V8_ASCII_STRING("doesDatabaseExist"), JS_DoesDatabaseExistClusterInfo);
  TRI_AddMethodVocbase(isolate, rt, TRI_V8_ASCII_STRING("listDatabases"), JS_ListDatabases);
  TRI_AddMethodVocbase(isolate, rt, TRI_V8_ASCII_STRING("flush"), JS_FlushClusterInfo, true);
  TRI_AddMethodVocbase(isolate, rt, TRI_V8_ASCII_STRING("getCollectionInfo"), JS_GetCollectionInfoClusterInfo);
  TRI_AddMethodVocbase(isolate, rt, TRI_V8_ASCII_STRING("getCollectionInfoCurrent"), JS_GetCollectionInfoCurrentClusterInfo);
  TRI_AddMethodVocbase(isolate, rt, TRI_V8_ASCII_STRING("getResponsibleServer"), JS_GetResponsibleServerClusterInfo);
  TRI_AddMethodVocbase(isolate, rt, TRI_V8_ASCII_STRING("getResponsibleShard"), JS_GetResponsibleShardClusterInfo);
  TRI_AddMethodVocbase(isolate, rt, TRI_V8_ASCII_STRING("getServerEndpoint"), JS_GetServerEndpointClusterInfo);
  TRI_AddMethodVocbase(isolate, rt, TRI_V8_ASCII_STRING("getDBServers"), JS_GetDBServers);
  TRI_AddMethodVocbase(isolate, rt, TRI_V8_ASCII_STRING("reloadDBServers"), JS_ReloadDBServers);
  TRI_AddMethodVocbase(isolate, rt, TRI_V8_ASCII_STRING("uniqid"), JS_UniqidClusterInfo);

  v8g->ClusterInfoTempl.Reset(isolate, rt);
  TRI_AddGlobalFunctionVocbase(isolate, context, TRI_V8_ASCII_STRING("ArangoClusterInfoCtor"), ft->GetFunction(), true);

  // register the global object
  v8::Handle<v8::Object> ci = rt->NewInstance();
  if (! ci.IsEmpty()) {
    TRI_AddGlobalVariableVocbase(isolate, context, TRI_V8_ASCII_STRING("ArangoClusterInfo"), ci);
  }

  // .............................................................................
  // generate the server state template
  // ...........................................................................

  ft = v8::FunctionTemplate::New(isolate);
  ft->SetClassName(TRI_V8_ASCII_STRING("ArangoServerState"));

  rt = ft->InstanceTemplate();
  rt->SetInternalFieldCount(2);

  TRI_AddMethodVocbase(isolate, rt, TRI_V8_ASCII_STRING("address"), JS_AddressServerState);
  TRI_AddMethodVocbase(isolate, rt, TRI_V8_ASCII_STRING("flush"), JS_FlushServerState, true);
  TRI_AddMethodVocbase(isolate, rt, TRI_V8_ASCII_STRING("id"), JS_IdServerState);
  TRI_AddMethodVocbase(isolate, rt, TRI_V8_ASCII_STRING("dataPath"), JS_DataPathServerState);
  TRI_AddMethodVocbase(isolate, rt, TRI_V8_ASCII_STRING("logPath"), JS_LogPathServerState);
  TRI_AddMethodVocbase(isolate, rt, TRI_V8_ASCII_STRING("agentPath"), JS_AgentPathServerState);
  TRI_AddMethodVocbase(isolate, rt, TRI_V8_ASCII_STRING("arangodPath"), JS_ArangodPathServerState);
  TRI_AddMethodVocbase(isolate, rt, TRI_V8_ASCII_STRING("javaScriptPath"), JS_JavaScriptPathServerState);
  TRI_AddMethodVocbase(isolate, rt, TRI_V8_ASCII_STRING("dbserverConfig"), JS_DBserverConfigServerState);
  TRI_AddMethodVocbase(isolate, rt, TRI_V8_ASCII_STRING("coordinatorConfig"), JS_CoordinatorConfigServerState);
  TRI_AddMethodVocbase(isolate, rt, TRI_V8_ASCII_STRING("disableDispatcherFrontend"), JS_DisableDipatcherFrontendServerState);
  TRI_AddMethodVocbase(isolate, rt, TRI_V8_ASCII_STRING("disableDispatcherKickstarter"), JS_DisableDipatcherKickstarterServerState);
  TRI_AddMethodVocbase(isolate, rt, TRI_V8_ASCII_STRING("initialised"), JS_InitialisedServerState);
  TRI_AddMethodVocbase(isolate, rt, TRI_V8_ASCII_STRING("isCoordinator"), JS_IsCoordinatorServerState);
  TRI_AddMethodVocbase(isolate, rt, TRI_V8_ASCII_STRING("role"), JS_RoleServerState);
  TRI_AddMethodVocbase(isolate, rt, TRI_V8_ASCII_STRING("setId"), JS_SetIdServerState, true);
  TRI_AddMethodVocbase(isolate, rt, TRI_V8_ASCII_STRING("setRole"), JS_SetRoleServerState, true);
  TRI_AddMethodVocbase(isolate, rt, TRI_V8_ASCII_STRING("status"), JS_StatusServerState);
  TRI_AddMethodVocbase(isolate, rt, TRI_V8_ASCII_STRING("getClusterAuthentication"), JS_GetClusterAuthentication);

  v8g->ServerStateTempl.Reset(isolate, rt);
  TRI_AddGlobalFunctionVocbase(isolate, context, TRI_V8_ASCII_STRING("ArangoServerStateCtor"), ft->GetFunction(), true);

  // register the global object
  v8::Handle<v8::Object> ss = rt->NewInstance();
  if (! ss.IsEmpty()) {
    TRI_AddGlobalVariableVocbase(isolate, context, TRI_V8_ASCII_STRING("ArangoServerState"), ss);
  }

  // ...........................................................................
  // generate the cluster comm template
  // ...........................................................................

  ft = v8::FunctionTemplate::New(isolate);
  ft->SetClassName(TRI_V8_ASCII_STRING("ArangoClusterComm"));

  rt = ft->InstanceTemplate();
  rt->SetInternalFieldCount(2);

  TRI_AddMethodVocbase(isolate, rt, TRI_V8_ASCII_STRING("asyncRequest"), JS_AsyncRequest);
  TRI_AddMethodVocbase(isolate, rt, TRI_V8_ASCII_STRING("syncRequest"), JS_SyncRequest);
  TRI_AddMethodVocbase(isolate, rt, TRI_V8_ASCII_STRING("enquire"), JS_Enquire);
  TRI_AddMethodVocbase(isolate, rt, TRI_V8_ASCII_STRING("wait"), JS_Wait);
  TRI_AddMethodVocbase(isolate, rt, TRI_V8_ASCII_STRING("drop"), JS_Drop);

  v8g->ClusterCommTempl.Reset(isolate, rt);
  TRI_AddGlobalFunctionVocbase(isolate, context, TRI_V8_ASCII_STRING("ArangoClusterCommCtor"), ft->GetFunction(), true);

  // register the global object
  ss = rt->NewInstance();
  if (! ss.IsEmpty()) {
    TRI_AddGlobalVariableVocbase(isolate, context, TRI_V8_ASCII_STRING("ArangoClusterComm"), ss);
  }
}

// -----------------------------------------------------------------------------
// --SECTION--                                                       END-OF-FILE
// -----------------------------------------------------------------------------

// Local Variables:
// mode: outline-minor
// outline-regexp: "/// @brief\\|/// {@inheritDoc}\\|/// @page\\|// --SECTION--\\|/// @\\}"
// End:
