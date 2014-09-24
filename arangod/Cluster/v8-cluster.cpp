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

static v8::Handle<v8::Value> CreateAgencyException (AgencyCommResult const& result) {
  v8::HandleScope scope;

  TRI_v8_global_t* v8g = (TRI_v8_global_t*) v8::Isolate::GetCurrent()->GetData();

  const std::string errorDetails = result.errorDetails();
  v8::Handle<v8::String> errorMessage = v8::String::New(errorDetails.c_str(), (int) errorDetails.size());
  v8::Handle<v8::Object> errorObject = v8::Exception::Error(errorMessage)->ToObject();

  errorObject->Set(v8::String::New("code"), v8::Number::New(result.httpCode()));
  errorObject->Set(v8::String::New("errorNum"), v8::Number::New(result.errorCode()));
  errorObject->Set(v8::String::New("errorMessage"), errorMessage);
  errorObject->Set(v8::String::New("error"), v8::True());

  v8::Handle<v8::Value> proto = v8g->ArangoErrorTempl->NewInstance();
  if (! proto.IsEmpty()) {
    errorObject->SetPrototype(proto);
  }

  return scope.Close(errorObject);
}

////////////////////////////////////////////////////////////////////////////////
/// @brief compares and swaps a value in the agency
////////////////////////////////////////////////////////////////////////////////

static v8::Handle<v8::Value> JS_CasAgency (v8::Arguments const& argv) {
  v8::HandleScope scope;

  if (argv.Length() < 3) {
    TRI_V8_EXCEPTION_USAGE(scope, "cas(<key>, <oldValue>, <newValue>, <ttl>, <timeout>, <throw>)");
  }

  const std::string key = TRI_ObjectToString(argv[0]);

  TRI_json_t* oldJson = TRI_ObjectToJson(argv[1]);

  if (oldJson == 0) {
    TRI_V8_EXCEPTION_PARAMETER(scope, "cannot convert <oldValue> to JSON");
  }

  TRI_json_t* newJson = TRI_ObjectToJson(argv[2]);
  if (newJson == 0) {
    TRI_FreeJson(TRI_UNKNOWN_MEM_ZONE, oldJson);
    TRI_V8_EXCEPTION_PARAMETER(scope, "cannot convert <newValue> to JSON");
  }

  double ttl = 0.0;
  if (argv.Length() > 3) {
    ttl = TRI_ObjectToDouble(argv[3]);
  }

  double timeout = 1.0;
  if (argv.Length() > 4) {
    timeout = TRI_ObjectToDouble(argv[4]);
  }

  bool shouldThrow = false;
  if (argv.Length() > 5) {
    shouldThrow = TRI_ObjectToBoolean(argv[5]);
  }

  AgencyComm comm;
  AgencyCommResult result = comm.casValue(key, oldJson, newJson, ttl, timeout);

  TRI_FreeJson(TRI_UNKNOWN_MEM_ZONE, newJson);
  TRI_FreeJson(TRI_UNKNOWN_MEM_ZONE, oldJson);

  if (! result.successful()) {
    if (! shouldThrow) {
      return scope.Close(v8::False());
    }

    return scope.Close(v8::ThrowException(CreateAgencyException(result)));
  }

  return scope.Close(v8::True());
}

////////////////////////////////////////////////////////////////////////////////
/// @brief creates a directory in the agency
////////////////////////////////////////////////////////////////////////////////

static v8::Handle<v8::Value> JS_CreateDirectoryAgency (v8::Arguments const& argv) {
  v8::HandleScope scope;

  if (argv.Length() != 1) {
    TRI_V8_EXCEPTION_USAGE(scope, "createDirectory(<key>)");
  }

  const std::string key = TRI_ObjectToString(argv[0]);

  AgencyComm comm;
  AgencyCommResult result = comm.createDirectory(key);

  if (! result.successful()) {
    return scope.Close(v8::ThrowException(CreateAgencyException(result)));
  }

  return scope.Close(v8::True());
}

////////////////////////////////////////////////////////////////////////////////
/// @brief whether or not the agency is enabled
////////////////////////////////////////////////////////////////////////////////

static v8::Handle<v8::Value> JS_IsEnabledAgency (v8::Arguments const& argv) {
  v8::HandleScope scope;

  if (argv.Length() != 0) {
    TRI_V8_EXCEPTION_USAGE(scope, "isEnabled()");
  }

  const std::string prefix = AgencyComm::prefix();

  return scope.Close(v8::Boolean::New(! prefix.empty()));
}

////////////////////////////////////////////////////////////////////////////////
/// @brief increase the version number
////////////////////////////////////////////////////////////////////////////////

static v8::Handle<v8::Value> JS_IncreaseVersionAgency (v8::Arguments const& argv) {
  v8::HandleScope scope;

  if (argv.Length() != 1) {
    TRI_V8_EXCEPTION_USAGE(scope, "increaseVersion(<key>)");
  }

  const std::string key = TRI_ObjectToString(argv[0]);

  AgencyComm comm;
  if (! comm.increaseVersion(key)) {
    TRI_V8_EXCEPTION_MESSAGE(scope, TRI_ERROR_INTERNAL, "unable to increase version");
  }

  return scope.Close(v8::Boolean::New(true));
}

////////////////////////////////////////////////////////////////////////////////
/// @brief gets a value from the agency
////////////////////////////////////////////////////////////////////////////////

static v8::Handle<v8::Value> JS_GetAgency (v8::Arguments const& argv) {
  v8::HandleScope scope;

  if (argv.Length() < 1) {
    TRI_V8_EXCEPTION_USAGE(scope, "get(<key>, <recursive>, <withIndexes>)");
  }

  const std::string key = TRI_ObjectToString(argv[0]);
  bool recursive = false;
  bool withIndexes = false;

  if (argv.Length() > 1) {
    recursive = TRI_ObjectToBoolean(argv[1]);
  }
  if (argv.Length() > 2) {
    withIndexes = TRI_ObjectToBoolean(argv[2]);
  }

  AgencyComm comm;
  AgencyCommResult result = comm.getValues(key, recursive);

  if (! result.successful()) {
    return scope.Close(v8::ThrowException(CreateAgencyException(result)));
  }

  result.parse("", false);

  v8::Handle<v8::Object> l = v8::Object::New();

  if (withIndexes) {
    std::map<std::string, AgencyCommResultEntry>::const_iterator it = result._values.begin();

    while (it != result._values.end()) {
      const std::string key = (*it).first;
      TRI_json_t const* json = (*it).second._json;
      const std::string idx = StringUtils::itoa((*it).second._index);

      if (json != 0) {
        v8::Handle<v8::Object> sub = v8::Object::New();

        sub->Set(v8::String::New("value"), TRI_ObjectJson(json));
        sub->Set(v8::String::New("index"), v8::String::New(idx.c_str(), (int) idx.size()));

        l->Set(v8::String::New(key.c_str(), (int) key.size()), sub);
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
        l->Set(v8::String::New(key.c_str(), (int) key.size()), TRI_ObjectJson(json));
      }

      ++it;
    }
  }

  return scope.Close(l);
}

////////////////////////////////////////////////////////////////////////////////
/// @brief lists a directory from the agency
////////////////////////////////////////////////////////////////////////////////

static v8::Handle<v8::Value> JS_ListAgency (v8::Arguments const& argv) {
  v8::HandleScope scope;

  if (argv.Length() < 1) {
    TRI_V8_EXCEPTION_USAGE(scope, "list(<key>, <recursive>, <flat>)");
  }

  const std::string key = TRI_ObjectToString(argv[0]);
  bool recursive = false;

  if (argv.Length() > 1) {
    recursive = TRI_ObjectToBoolean(argv[1]);
  }

  bool flat = false;
  if (argv.Length() > 2) {
    flat = TRI_ObjectToBoolean(argv[2]);
  }

  AgencyComm comm;
  AgencyCommResult result = comm.getValues(key, recursive);

  if (! result.successful()) {
    return scope.Close(v8::ThrowException(CreateAgencyException(result)));
  }

  // return just the value for each key
  result.parse("", true);
  std::map<std::string, AgencyCommResultEntry>::const_iterator it = result._values.begin();

  // skip first entry
  if (it != result._values.end()) {
    ++it;
  }

  if (flat) {
    v8::Handle<v8::Array> l = v8::Array::New();

    uint32_t i = 0;
    while (it != result._values.end()) {
      const std::string key = (*it).first;

      l->Set(i++, v8::String::New(key.c_str(), (int) key.size()));
      ++it;
    }
    return scope.Close(l);
  }

  else {
    v8::Handle<v8::Object> l = v8::Object::New();

    while (it != result._values.end()) {
      const std::string key = (*it).first;
      const bool isDirectory = (*it).second._isDir;

      l->Set(v8::String::New(key.c_str(), (int) key.size()), v8::Boolean::New(isDirectory));
      ++it;
    }
    return scope.Close(l);
  }
}

////////////////////////////////////////////////////////////////////////////////
/// @brief acquires a read-lock in the agency
////////////////////////////////////////////////////////////////////////////////

static v8::Handle<v8::Value> JS_LockReadAgency (v8::Arguments const& argv) {
  v8::HandleScope scope;

  if (argv.Length() < 1) {
    TRI_V8_EXCEPTION_USAGE(scope, "lockRead(<part>, <ttl>, <timeout>)");
  }

  const std::string part = TRI_ObjectToString(argv[0]);

  double ttl = 0.0;
  if (argv.Length() > 1) {
    ttl = TRI_ObjectToDouble(argv[1]);
  }

  double timeout = 0.0;
  if (argv.Length() > 2) {
    timeout = TRI_ObjectToDouble(argv[2]);
  }

  AgencyComm comm;
  if (! comm.lockRead(part, ttl, timeout)) {
    TRI_V8_EXCEPTION_MESSAGE(scope, TRI_ERROR_INTERNAL, "unable to acquire lock");
  }

  return scope.Close(v8::True());
}

////////////////////////////////////////////////////////////////////////////////
/// @brief acquires a write-lock in the agency
////////////////////////////////////////////////////////////////////////////////

static v8::Handle<v8::Value> JS_LockWriteAgency (v8::Arguments const& argv) {
  v8::HandleScope scope;

  if (argv.Length() < 1) {
    TRI_V8_EXCEPTION_USAGE(scope, "lockWrite(<part>, <ttl>, <timeout>)");
  }

  const std::string part = TRI_ObjectToString(argv[0]);

  double ttl = 0.0;
  if (argv.Length() > 1) {
    ttl = TRI_ObjectToDouble(argv[1]);
  }

  double timeout = 0.0;
  if (argv.Length() > 2) {
    timeout = TRI_ObjectToDouble(argv[2]);
  }

  AgencyComm comm;
  if (! comm.lockWrite(part, ttl, timeout)) {
    TRI_V8_EXCEPTION_MESSAGE(scope, TRI_ERROR_INTERNAL, "unable to acquire lock");
  }

  return scope.Close(v8::True());
}

////////////////////////////////////////////////////////////////////////////////
/// @brief releases a read-lock in the agency
////////////////////////////////////////////////////////////////////////////////

static v8::Handle<v8::Value> JS_UnlockReadAgency (v8::Arguments const& argv) {
  v8::HandleScope scope;

  if (argv.Length() > 2) {
    TRI_V8_EXCEPTION_USAGE(scope, "unlockRead(<part>, <timeout>)");
  }

  const std::string part = TRI_ObjectToString(argv[0]);

  double timeout = 0.0;
  if (argv.Length() > 1) {
    timeout = TRI_ObjectToDouble(argv[1]);
  }

  AgencyComm comm;
  if (! comm.unlockRead(part, timeout)) {
    TRI_V8_EXCEPTION_MESSAGE(scope, TRI_ERROR_INTERNAL, "unable to release lock");
  }

  return scope.Close(v8::True());
}

////////////////////////////////////////////////////////////////////////////////
/// @brief releases a write-lock in the agency
////////////////////////////////////////////////////////////////////////////////

static v8::Handle<v8::Value> JS_UnlockWriteAgency (v8::Arguments const& argv) {
  v8::HandleScope scope;

  if (argv.Length() > 2) {
    TRI_V8_EXCEPTION_USAGE(scope, "unlockWrite(<part>, <timeout>)");
  }

  const std::string part = TRI_ObjectToString(argv[0]);

  double timeout = 0.0;
  if (argv.Length() > 1) {
    timeout = TRI_ObjectToDouble(argv[1]);
  }

  AgencyComm comm;
  if (! comm.unlockWrite(part, timeout)) {
    TRI_V8_EXCEPTION_MESSAGE(scope, TRI_ERROR_INTERNAL, "unable to release lock");
  }

  return scope.Close(v8::True());
}

////////////////////////////////////////////////////////////////////////////////
/// @brief removes a value from the agency
////////////////////////////////////////////////////////////////////////////////

static v8::Handle<v8::Value> JS_RemoveAgency (v8::Arguments const& argv) {
  v8::HandleScope scope;

  if (argv.Length() < 1) {
    TRI_V8_EXCEPTION_USAGE(scope, "remove(<key>, <recursive>)");
  }

  const std::string key = TRI_ObjectToString(argv[0]);
  bool recursive = false;

  if (argv.Length() > 1) {
    recursive = TRI_ObjectToBoolean(argv[1]);
  }

  AgencyComm comm;
  AgencyCommResult result = comm.removeValues(key, recursive);

  if (! result.successful()) {
    return scope.Close(v8::ThrowException(CreateAgencyException(result)));
  }

  return scope.Close(v8::True());
}

////////////////////////////////////////////////////////////////////////////////
/// @brief sets a value in the agency
////////////////////////////////////////////////////////////////////////////////

static v8::Handle<v8::Value> JS_SetAgency (v8::Arguments const& argv) {
  v8::HandleScope scope;

  if (argv.Length() < 2) {
    TRI_V8_EXCEPTION_USAGE(scope, "set(<key>, <value>, <ttl>)");
  }

  const std::string key = TRI_ObjectToString(argv[0]);

  TRI_json_t* json = TRI_ObjectToJson(argv[1]);

  if (json == 0) {
    TRI_V8_EXCEPTION_PARAMETER(scope, "cannot convert <value> to JSON");
  }

  double ttl = 0.0;
  if (argv.Length() > 2) {
    ttl = TRI_ObjectToDouble(argv[2]);
  }

  AgencyComm comm;
  AgencyCommResult result = comm.setValue(key, json, ttl);

  TRI_FreeJson(TRI_UNKNOWN_MEM_ZONE, json);

  if (! result.successful()) {
    return scope.Close(v8::ThrowException(CreateAgencyException(result)));
  }

  return scope.Close(v8::True());
}

////////////////////////////////////////////////////////////////////////////////
/// @brief watches a value in the agency
////////////////////////////////////////////////////////////////////////////////

static v8::Handle<v8::Value> JS_WatchAgency (v8::Arguments const& argv) {
  v8::HandleScope scope;

  if (argv.Length() < 1) {
    TRI_V8_EXCEPTION_USAGE(scope, "watch(<key>, <waitIndex>, <timeout>, <recursive>)");
  }

  const std::string key = TRI_ObjectToString(argv[0]);
  double timeout = 1.0;
  uint64_t waitIndex = 0;
  bool recursive = false;

  if (argv.Length() > 1) {
    waitIndex = TRI_ObjectToUInt64(argv[1], true);
  }
  if (argv.Length() > 2) {
    timeout = TRI_ObjectToDouble(argv[2]);
  }
  if (argv.Length() > 3) {
    recursive = TRI_ObjectToBoolean(argv[3]);
  }

  AgencyComm comm;
  AgencyCommResult result = comm.watchValue(key, waitIndex, timeout, recursive);

  if (result._statusCode == 0) {
    // watch timed out
    return scope.Close(v8::False());
  }

  if (! result.successful()) {
    return scope.Close(v8::ThrowException(CreateAgencyException(result)));
  }

  result.parse("", false);
  std::map<std::string, AgencyCommResultEntry>::const_iterator it = result._values.begin();

  v8::Handle<v8::Object> l = v8::Object::New();

  while (it != result._values.end()) {
    const std::string key = (*it).first;
    TRI_json_t* json = (*it).second._json;

    if (json != 0) {
      l->Set(v8::String::New(key.c_str(), (int) key.size()), TRI_ObjectJson(json));
    }

    ++it;
  }

  return scope.Close(l);
}

////////////////////////////////////////////////////////////////////////////////
/// @brief returns the agency endpoints
////////////////////////////////////////////////////////////////////////////////

static v8::Handle<v8::Value> JS_EndpointsAgency (v8::Arguments const& argv) {
  v8::HandleScope scope;

  if (argv.Length() != 0) {
    TRI_V8_EXCEPTION_USAGE(scope, "endpoints()");
  }

  std::vector<std::string> endpoints = AgencyComm::getEndpoints();
  // make the list of endpoints unique
  std::sort(endpoints.begin(), endpoints.end());
  endpoints.assign(endpoints.begin(), std::unique(endpoints.begin(), endpoints.end()));

  v8::Handle<v8::Array> l = v8::Array::New();

  for (size_t i = 0; i < endpoints.size(); ++i) {
    const std::string endpoint = endpoints[i];

    l->Set((uint32_t) i, v8::String::New(endpoint.c_str(), (int) endpoint.size()));
  }

  return scope.Close(l);
}

////////////////////////////////////////////////////////////////////////////////
/// @brief returns the agency prefix
////////////////////////////////////////////////////////////////////////////////

static v8::Handle<v8::Value> JS_PrefixAgency (v8::Arguments const& argv) {
  v8::HandleScope scope;

  if (argv.Length() > 1) {
    TRI_V8_EXCEPTION_USAGE(scope, "prefix(<strip>)");
  }

  bool strip = false;
  if (argv.Length() > 0) {
    strip = TRI_ObjectToBoolean(argv[0]);
  }

  const std::string prefix = AgencyComm::prefix();

  if (strip && prefix.size() > 2) {
    return scope.Close(v8::String::New(prefix.c_str() + 1, (int) prefix.size() - 2));
  }

  return scope.Close(v8::String::New(prefix.c_str(), (int) prefix.size()));
}

////////////////////////////////////////////////////////////////////////////////
/// @brief sets the agency prefix
////////////////////////////////////////////////////////////////////////////////

static v8::Handle<v8::Value> JS_SetPrefixAgency (v8::Arguments const& argv) {
  v8::HandleScope scope;

  if (argv.Length() != 1) {
    TRI_V8_EXCEPTION_USAGE(scope, "setPrefix(<prefix>)");
  }

  const bool result = AgencyComm::setPrefix(TRI_ObjectToString(argv[0]));

  return scope.Close(v8::Boolean::New(result));
}

////////////////////////////////////////////////////////////////////////////////
/// @brief creates a uniqid
////////////////////////////////////////////////////////////////////////////////

static v8::Handle<v8::Value> JS_UniqidAgency (v8::Arguments const& argv) {
  v8::HandleScope scope;

  if (argv.Length() < 1 || argv.Length() > 3) {
    TRI_V8_EXCEPTION_USAGE(scope, "uniqid(<key>, <count>, <timeout>)");
  }

  const std::string key = TRI_ObjectToString(argv[0]);

  uint64_t count = 1;
  if (argv.Length() > 1) {
    count = TRI_ObjectToUInt64(argv[1], true);
  }

  if (count < 1 || count > 10000000) {
    TRI_V8_EXCEPTION_PARAMETER(scope, "<count> is invalid");
  }

  double timeout = 0.0;
  if (argv.Length() > 2) {
    timeout = TRI_ObjectToDouble(argv[2]);
  }

  AgencyComm comm;
  AgencyCommResult result = comm.uniqid(key, count, timeout);

  if (! result.successful() || result._index == 0) {
    return scope.Close(v8::ThrowException(CreateAgencyException(result)));
  }

  const std::string value = StringUtils::itoa(result._index);

  return scope.Close(v8::String::New(value.c_str(), (int) value.size()));
}

////////////////////////////////////////////////////////////////////////////////
/// @brief returns the agency version
////////////////////////////////////////////////////////////////////////////////

static v8::Handle<v8::Value> JS_VersionAgency (v8::Arguments const& argv) {
  v8::HandleScope scope;

  if (argv.Length() != 0) {
    TRI_V8_EXCEPTION_USAGE(scope, "version()");
  }

  AgencyComm comm;
  const std::string version = comm.getVersion();

  return scope.Close(v8::String::New(version.c_str(), (int) version.size()));
}

// -----------------------------------------------------------------------------
// --SECTION--                                            cluster info functions
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @brief whether or not a specific database exists
////////////////////////////////////////////////////////////////////////////////

static v8::Handle<v8::Value> JS_DoesDatabaseExistClusterInfo (v8::Arguments const& argv) {
  v8::HandleScope scope;

  if (argv.Length() != 1) {
    TRI_V8_EXCEPTION_USAGE(scope, "doesDatabaseExist(<database-id>)");
  }

  const bool result = ClusterInfo::instance()->doesDatabaseExist(TRI_ObjectToString(argv[0]), true);

  return scope.Close(v8::Boolean::New(result));
}

////////////////////////////////////////////////////////////////////////////////
/// @brief get the list of databases in the cluster
////////////////////////////////////////////////////////////////////////////////

static v8::Handle<v8::Value> JS_ListDatabases (v8::Arguments const& argv) {
  v8::HandleScope scope;

  if (argv.Length() != 0) {
    TRI_V8_EXCEPTION_USAGE(scope, "listDatabases()");
  }

  vector<DatabaseID> res = ClusterInfo::instance()->listDatabases(true);
  v8::Handle<v8::Array> a = v8::Array::New((int) res.size());
  vector<DatabaseID>::iterator it;
  int count = 0;
  for (it = res.begin(); it != res.end(); ++it) {
    a->Set(count++, v8::String::New(it->c_str(), (int) it->size()));
  }
  return scope.Close(a);
}

////////////////////////////////////////////////////////////////////////////////
/// @brief flush the caches (used for testing only)
////////////////////////////////////////////////////////////////////////////////

static v8::Handle<v8::Value> JS_FlushClusterInfo (v8::Arguments const& argv) {
  v8::HandleScope scope;

  if (argv.Length() != 0) {
    TRI_V8_EXCEPTION_USAGE(scope, "flush()");
  }

  ClusterInfo::instance()->flush();

  return scope.Close(v8::True());
}

////////////////////////////////////////////////////////////////////////////////
/// @brief get the info about a collection in Plan
////////////////////////////////////////////////////////////////////////////////

static v8::Handle<v8::Value> JS_GetCollectionInfoClusterInfo (v8::Arguments const& argv) {
  v8::HandleScope scope;

  if (argv.Length() != 2) {
    TRI_V8_EXCEPTION_USAGE(scope, "getCollectionInfo(<database-id>, <collection-id>)");
  }

  shared_ptr<CollectionInfo> ci
        = ClusterInfo::instance()->getCollection(TRI_ObjectToString(argv[0]),
                                                 TRI_ObjectToString(argv[1]));

  v8::Handle<v8::Object> result = v8::Object::New();
  const std::string cid = triagens::basics::StringUtils::itoa(ci->id());
  const std::string& name = ci->name();
  result->Set(v8::String::New("id"), v8::String::New(cid.c_str(), (int) cid.size()));
  result->Set(v8::String::New("name"), v8::String::New(name.c_str(), (int) name.size()));
  result->Set(v8::String::New("type"), v8::Number::New((int) ci->type()));
  result->Set(v8::String::New("status"), v8::Number::New((int) ci->status()));

  const string statusString = ci->statusString();
  result->Set(v8::String::New("statusString"),
              v8::String::New(statusString.c_str(), (int) statusString.size()));

  result->Set(v8::String::New("deleted"), v8::Boolean::New(ci->deleted()));
  result->Set(v8::String::New("doCompact"), v8::Boolean::New(ci->doCompact()));
  result->Set(v8::String::New("isSystem"), v8::Boolean::New(ci->isSystem()));
  result->Set(v8::String::New("isVolatile"), v8::Boolean::New(ci->isVolatile()));
  result->Set(v8::String::New("waitForSync"), v8::Boolean::New(ci->waitForSync()));
  result->Set(v8::String::New("journalSize"), v8::Number::New(ci->journalSize()));

  const std::vector<std::string>& sks = ci->shardKeys();
  v8::Handle<v8::Array> shardKeys = v8::Array::New((int) sks.size());
  for (uint32_t i = 0, n = (uint32_t) sks.size(); i < n; ++i) {
    shardKeys->Set(i, v8::String::New(sks[i].c_str(), (int) sks[i].size()));
  }
  result->Set(v8::String::New("shardKeys"), shardKeys);

  const std::map<std::string, std::string>& sis = ci->shardIds();
  v8::Handle<v8::Object> shardIds = v8::Object::New();
  std::map<std::string, std::string>::const_iterator it = sis.begin();
  while (it != sis.end()) {
    shardIds->Set(v8::String::New((*it).first.c_str(), (int) (*it).first.size()),
                  v8::String::New((*it).second.c_str(), (int) (*it).second.size()));
    ++it;
  }
  result->Set(v8::String::New("shards"), shardIds);

  // TODO: fill "indexes"
  v8::Handle<v8::Array> indexes = v8::Array::New();
  result->Set(v8::String::New("indexes"), indexes);
  return scope.Close(result);
}

////////////////////////////////////////////////////////////////////////////////
/// @brief get the info about a collection in Current
////////////////////////////////////////////////////////////////////////////////

static v8::Handle<v8::Value> JS_GetCollectionInfoCurrentClusterInfo (v8::Arguments const& argv) {
  v8::HandleScope scope;

  if (argv.Length() != 3) {
    TRI_V8_EXCEPTION_USAGE(scope, "getCollectionInfoCurrent(<database-id>, <collection-id>, <shardID>)");
  }

  ShardID shardID = TRI_ObjectToString(argv[2]);

  shared_ptr<CollectionInfo> ci = ClusterInfo::instance()->getCollection(
                                           TRI_ObjectToString(argv[0]),
                                           TRI_ObjectToString(argv[1]));

  v8::Handle<v8::Object> result = v8::Object::New();
  // First some stuff from Plan for which Current does not make sense:
  const std::string cid = triagens::basics::StringUtils::itoa(ci->id());
  const std::string& name = ci->name();
  result->Set(v8::String::New("id"), v8::String::New(cid.c_str(), (int) cid.size()));
  result->Set(v8::String::New("name"), v8::String::New(name.c_str(), (int) name.size()));

  shared_ptr<CollectionInfoCurrent> cic
          = ClusterInfo::instance()->getCollectionCurrent(
                                     TRI_ObjectToString(argv[0]), cid);

  result->Set(v8::String::New("type"), v8::Number::New((int) ci->type()));
  // Now the Current information, if we actually got it:
  TRI_vocbase_col_status_e s = cic->status(shardID);
  result->Set(v8::String::New("status"), v8::Number::New((int) cic->status(shardID)));
  if (s == TRI_VOC_COL_STATUS_CORRUPTED) {
    return scope.Close(result);
  }
  const string statusString = TRI_GetStatusStringCollectionVocBase(s);
  result->Set(v8::String::New("statusString"),
              v8::String::New(statusString.c_str(), (int) statusString.size()));

  result->Set(v8::String::New("deleted"), v8::Boolean::New(cic->deleted(shardID)));
  result->Set(v8::String::New("doCompact"), v8::Boolean::New(cic->doCompact(shardID)));
  result->Set(v8::String::New("isSystem"), v8::Boolean::New(cic->isSystem(shardID)));
  result->Set(v8::String::New("isVolatile"), v8::Boolean::New(cic->isVolatile(shardID)));
  result->Set(v8::String::New("waitForSync"), v8::Boolean::New(cic->waitForSync(shardID)));
  result->Set(v8::String::New("journalSize"), v8::Number::New(cic->journalSize(shardID)));
  const std::string serverID = cic->responsibleServer(shardID);
  result->Set(v8::String::New("DBServer"), v8::String::New(serverID.c_str(), (int) serverID.size()));

  // TODO: fill "indexes"
  v8::Handle<v8::Array> indexes = v8::Array::New();
  result->Set(v8::String::New("indexes"), indexes);

  // Finally, report any possible error:
  bool error = cic->error(shardID);
  result->Set(v8::String::New("error"), v8::Boolean::New(error));
  if (error) {
    result->Set(v8::String::New("errorNum"), v8::Number::New(cic->errorNum(shardID)));
    const string errorMessage = cic->errorMessage(shardID);
    result->Set(v8::String::New("errorMessage"),
                v8::String::New(errorMessage.c_str(), (int) errorMessage.size()));
  }

  return scope.Close(result);
}

////////////////////////////////////////////////////////////////////////////////
/// @brief get the responsible server
////////////////////////////////////////////////////////////////////////////////

static v8::Handle<v8::Value> JS_GetResponsibleServerClusterInfo (v8::Arguments const& argv) {
  v8::HandleScope scope;

  if (argv.Length() != 1) {
    TRI_V8_EXCEPTION_USAGE(scope, "getResponsibleServer(<shard-id>)");
  }

  std::string const result = ClusterInfo::instance()->getResponsibleServer(TRI_ObjectToString(argv[0]));

  return scope.Close(v8::String::New(result.c_str(), (int) result.size()));
}

////////////////////////////////////////////////////////////////////////////////
/// @brief get the responsible shard
////////////////////////////////////////////////////////////////////////////////

static v8::Handle<v8::Value> JS_GetResponsibleShardClusterInfo (v8::Arguments const& argv) {
  v8::HandleScope scope;

  if (argv.Length() < 2 || argv.Length() > 3) {
    TRI_V8_EXCEPTION_USAGE(scope, "getResponsibleShard(<collection-id>, <document>, <documentIsComplete>)");
  }
  
  if (! argv[0]->IsString() && ! argv[0]->IsStringObject()) {
    TRI_V8_TYPE_ERROR(scope, "expecting a string for <collection-id>)");
  }

  if (! argv[1]->IsObject()) {
    TRI_V8_TYPE_ERROR(scope, "expecting an object for <document>)");
  }

  bool documentIsComplete = true;
  if (argv.Length() > 2) {
    documentIsComplete = TRI_ObjectToBoolean(argv[2]);
  }

  TRI_json_t* json = TRI_ObjectToJson(argv[1]);

  if (json == nullptr) {
    TRI_V8_EXCEPTION_MEMORY(scope);
  }

  ShardID shardId;
  CollectionID collectionId = TRI_ObjectToString(argv[0]);
  bool usesDefaultShardingAttributes;
  int res = ClusterInfo::instance()->getResponsibleShard(collectionId, json, documentIsComplete, shardId, usesDefaultShardingAttributes);

  TRI_FreeJson(TRI_UNKNOWN_MEM_ZONE, json);

  if (res != TRI_ERROR_NO_ERROR) {
    TRI_V8_EXCEPTION(scope, res);
  }

  v8::Handle<v8::Object> result = v8::Object::New();
  result->Set(TRI_V8_STRING("shardId"), v8::String::New(shardId.c_str(), (int) shardId.size()));
  result->Set(TRI_V8_STRING("usesDefaultShardingAttributes"), v8::Boolean::New(usesDefaultShardingAttributes));

  return scope.Close(result);
}

////////////////////////////////////////////////////////////////////////////////
/// @brief get the server endpoint for a server
////////////////////////////////////////////////////////////////////////////////

static v8::Handle<v8::Value> JS_GetServerEndpointClusterInfo (v8::Arguments const& argv) {
  v8::HandleScope scope;

  if (argv.Length() != 1) {
    TRI_V8_EXCEPTION_USAGE(scope, "getServerEndpoint(<server-id>)");
  }

  const std::string result = ClusterInfo::instance()->getServerEndpoint(TRI_ObjectToString(argv[0]));

  return scope.Close(v8::String::New(result.c_str(), (int) result.size()));
}

////////////////////////////////////////////////////////////////////////////////
/// @brief returns the DBServers currently registered
////////////////////////////////////////////////////////////////////////////////

static v8::Handle<v8::Value> JS_GetDBServers (v8::Arguments const& argv) {
  v8::HandleScope scope;

  if (argv.Length() != 0) {
    TRI_V8_EXCEPTION_USAGE(scope, "DBServers()");
  }

  std::vector<std::string> DBServers = ClusterInfo::instance()->getCurrentDBServers();

  v8::Handle<v8::Array> l = v8::Array::New();

  for (size_t i = 0; i < DBServers.size(); ++i) {
    ServerID const sid = DBServers[i];

    l->Set((uint32_t) i, v8::String::New(sid.c_str(), (int) sid.size()));
  }

  return scope.Close(l);
}

////////////////////////////////////////////////////////////////////////////////
/// @brief reloads the cache of DBServers currently registered
////////////////////////////////////////////////////////////////////////////////

static v8::Handle<v8::Value> JS_ReloadDBServers (v8::Arguments const& argv) {
  v8::HandleScope scope;

  if (argv.Length() != 0) {
    TRI_V8_EXCEPTION_USAGE(scope, "reloadDBServers()");
  }

  ClusterInfo::instance()->loadCurrentDBServers();
  return scope.Close(v8::Undefined());
}

////////////////////////////////////////////////////////////////////////////////
/// @brief returns a unique id
////////////////////////////////////////////////////////////////////////////////

static v8::Handle<v8::Value> JS_UniqidClusterInfo (v8::Arguments const& argv) {
  v8::HandleScope scope;

  if (argv.Length() > 1) {
    TRI_V8_EXCEPTION_USAGE(scope, "uniqid(<count>)");
  }

  uint64_t count = 1;
  if (argv.Length() > 0) {
    count = TRI_ObjectToUInt64(argv[0], true);
  }

  if (count == 0) {
    TRI_V8_EXCEPTION_PARAMETER(scope, "<count> is invalid");
  }

  uint64_t value = ClusterInfo::instance()->uniqid();

  if (value == 0) {
    TRI_V8_EXCEPTION_MESSAGE(scope, TRI_ERROR_INTERNAL, "unable to generate unique id");
  }

  const std::string id = StringUtils::itoa(value);

  return scope.Close(v8::String::New(id.c_str(), (int) id.size()));
}

// -----------------------------------------------------------------------------
// --SECTION--                                            server state functions
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @brief return the servers address
////////////////////////////////////////////////////////////////////////////////

static v8::Handle<v8::Value> JS_AddressServerState (v8::Arguments const& argv) {
  v8::HandleScope scope;

  if (argv.Length() != 0) {
    TRI_V8_EXCEPTION_USAGE(scope, "address()");
  }

  const std::string address = ServerState::instance()->getAddress();
  return scope.Close(v8::String::New(address.c_str(), (int) address.size()));
}

////////////////////////////////////////////////////////////////////////////////
/// @brief flush the server state (used for testing only)
////////////////////////////////////////////////////////////////////////////////

static v8::Handle<v8::Value> JS_FlushServerState (v8::Arguments const& argv) {
  v8::HandleScope scope;

  if (argv.Length() != 0) {
    TRI_V8_EXCEPTION_USAGE(scope, "flush()");
  }

  ServerState::instance()->flush();

  return scope.Close(v8::True());
}

////////////////////////////////////////////////////////////////////////////////
/// @brief return the servers id
////////////////////////////////////////////////////////////////////////////////

static v8::Handle<v8::Value> JS_IdServerState (v8::Arguments const& argv) {
  v8::HandleScope scope;

  if (argv.Length() != 0) {
    TRI_V8_EXCEPTION_USAGE(scope, "id()");
  }

  const std::string id = ServerState::instance()->getId();
  return scope.Close(v8::String::New(id.c_str(), (int) id.size()));
}

////////////////////////////////////////////////////////////////////////////////
/// @brief returns the data path
////////////////////////////////////////////////////////////////////////////////

static v8::Handle<v8::Value> JS_DataPathServerState (v8::Arguments const& argv) {
  v8::HandleScope scope;

  if (argv.Length() != 0) {
    TRI_V8_EXCEPTION_USAGE(scope, "dataPath()");
  }

  const std::string path = ServerState::instance()->getDataPath();
  return scope.Close(v8::String::New(path.c_str(), (int) path.size()));
}

////////////////////////////////////////////////////////////////////////////////
/// @brief returns the log path
////////////////////////////////////////////////////////////////////////////////

static v8::Handle<v8::Value> JS_LogPathServerState (v8::Arguments const& argv) {
  v8::HandleScope scope;

  if (argv.Length() != 0) {
    TRI_V8_EXCEPTION_USAGE(scope, "logPath()");
  }

  const std::string path = ServerState::instance()->getLogPath();
  return scope.Close(v8::String::New(path.c_str(), (int) path.size()));
}

////////////////////////////////////////////////////////////////////////////////
/// @brief returns the agent path
////////////////////////////////////////////////////////////////////////////////

static v8::Handle<v8::Value> JS_AgentPathServerState (v8::Arguments const& argv) {
  v8::HandleScope scope;

  if (argv.Length() != 0) {
    TRI_V8_EXCEPTION_USAGE(scope, "agentPath()");
  }

  const std::string path = ServerState::instance()->getAgentPath();
  return scope.Close(v8::String::New(path.c_str(), (int) path.size()));
}

////////////////////////////////////////////////////////////////////////////////
/// @brief returns the arangod path
////////////////////////////////////////////////////////////////////////////////

static v8::Handle<v8::Value> JS_ArangodPathServerState (v8::Arguments const& argv) {
  v8::HandleScope scope;

  if (argv.Length() != 0) {
    TRI_V8_EXCEPTION_USAGE(scope, "arangodPath()");
  }

  const std::string path = ServerState::instance()->getArangodPath();
  return scope.Close(v8::String::New(path.c_str(), (int) path.size()));
}

////////////////////////////////////////////////////////////////////////////////
/// @brief returns the javascript startup path
////////////////////////////////////////////////////////////////////////////////

static v8::Handle<v8::Value> JS_JavaScriptPathServerState (v8::Arguments const& argv) {
  v8::HandleScope scope;

  if (argv.Length() != 0) {
    TRI_V8_EXCEPTION_USAGE(scope, "javaScriptPath()");
  }

  const std::string path = ServerState::instance()->getJavaScriptPath();
  return scope.Close(v8::String::New(path.c_str(), (int) path.size()));
}

////////////////////////////////////////////////////////////////////////////////
/// @brief returns the DBserver config
////////////////////////////////////////////////////////////////////////////////

static v8::Handle<v8::Value> JS_DBserverConfigServerState (v8::Arguments const& argv) {
  v8::HandleScope scope;

  if (argv.Length() != 0) {
    TRI_V8_EXCEPTION_USAGE(scope, "dbserverConfig()");
  }

  const std::string path = ServerState::instance()->getDBserverConfig();
  return scope.Close(v8::String::New(path.c_str(), (int) path.size()));
}

////////////////////////////////////////////////////////////////////////////////
/// @brief returns the coordinator config
////////////////////////////////////////////////////////////////////////////////

static v8::Handle<v8::Value> JS_CoordinatorConfigServerState (v8::Arguments const& argv) {
  v8::HandleScope scope;

  if (argv.Length() != 0) {
    TRI_V8_EXCEPTION_USAGE(scope, "coordinatorConfig()");
  }

  const std::string path = ServerState::instance()->getCoordinatorConfig();
  return scope.Close(v8::String::New(path.c_str(), (int) path.size()));
}

////////////////////////////////////////////////////////////////////////////////
/// @brief returns true, if the dispatcher frontend should be disabled
////////////////////////////////////////////////////////////////////////////////

static v8::Handle<v8::Value> JS_DisableDipatcherFrontendServerState (v8::Arguments const& argv) {
  v8::HandleScope scope;

  if (argv.Length() != 0) {
    TRI_V8_EXCEPTION_USAGE(scope, "disableDispatcherFrontend()");
  }

  return scope.Close(v8::Boolean::New(ServerState::instance()->getDisableDispatcherFrontend()));
}

////////////////////////////////////////////////////////////////////////////////
/// @brief returns true, if the dispatcher kickstarter should be disabled
////////////////////////////////////////////////////////////////////////////////

static v8::Handle<v8::Value> JS_DisableDipatcherKickstarterServerState (v8::Arguments const& argv) {
  v8::HandleScope scope;

  if (argv.Length() != 0) {
    TRI_V8_EXCEPTION_USAGE(scope, "disableDispatcherKickstarter()");
  }

  return scope.Close(v8::Boolean::New(ServerState::instance()->getDisableDispatcherKickstarter()));
}

////////////////////////////////////////////////////////////////////////////////
/// @brief return whether the cluster is initialised
////////////////////////////////////////////////////////////////////////////////

static v8::Handle<v8::Value> JS_InitialisedServerState (v8::Arguments const& argv) {
  v8::HandleScope scope;

  if (argv.Length() != 0) {
    TRI_V8_EXCEPTION_USAGE(scope, "initialised()");
  }

  return scope.Close(v8::Boolean::New(ServerState::instance()->initialised()));
}

////////////////////////////////////////////////////////////////////////////////
/// @brief whether or not the server is a coordinator
////////////////////////////////////////////////////////////////////////////////

static v8::Handle<v8::Value> JS_IsCoordinatorServerState (v8::Arguments const& argv) {
  v8::HandleScope scope;

  if (argv.Length() != 0) {
    TRI_V8_EXCEPTION_USAGE(scope, "isCoordinator()");
  }

  return scope.Close(v8::Boolean::New(ServerState::instance()->getRole() == ServerState::ROLE_COORDINATOR));
}

////////////////////////////////////////////////////////////////////////////////
/// @brief returns the server role
////////////////////////////////////////////////////////////////////////////////

static v8::Handle<v8::Value> JS_RoleServerState (v8::Arguments const& argv) {
  v8::HandleScope scope;

  if (argv.Length() != 0) {
    TRI_V8_EXCEPTION_USAGE(scope, "role()");
  }

  const std::string role = ServerState::roleToString(ServerState::instance()->getRole());

  return scope.Close(v8::String::New(role.c_str(), (int) role.size()));
}

////////////////////////////////////////////////////////////////////////////////
/// @brief sets the server id (used for testing)
////////////////////////////////////////////////////////////////////////////////

static v8::Handle<v8::Value> JS_SetIdServerState (v8::Arguments const& argv) {
  v8::HandleScope scope;

  if (argv.Length() != 1) {
    TRI_V8_EXCEPTION_USAGE(scope, "setId(<id>)");
  }

  const std::string id = TRI_ObjectToString(argv[0]);
  ServerState::instance()->setId(id);

  return scope.Close(v8::True());
}

////////////////////////////////////////////////////////////////////////////////
/// @brief sets the server role (used for testing)
////////////////////////////////////////////////////////////////////////////////

static v8::Handle<v8::Value> JS_SetRoleServerState (v8::Arguments const& argv) {
  v8::HandleScope scope;

  if (argv.Length() != 1) {
    TRI_V8_EXCEPTION_USAGE(scope, "setRole(<role>)");
  }

  const std::string role = TRI_ObjectToString(argv[0]);
  ServerState::RoleEnum r = ServerState::stringToRole(role);

  if (r == ServerState::ROLE_UNDEFINED) {
    TRI_V8_EXCEPTION_PARAMETER(scope, "<role> is invalid");
  }

  ServerState::instance()->setRole(r);

  return scope.Close(v8::True());
}

////////////////////////////////////////////////////////////////////////////////
/// @brief returns the server state
////////////////////////////////////////////////////////////////////////////////

static v8::Handle<v8::Value> JS_StatusServerState (v8::Arguments const& argv) {
  v8::HandleScope scope;

  if (argv.Length() != 0) {
    TRI_V8_EXCEPTION_USAGE(scope, "status()");
  }

  const std::string state = ServerState::stateToString(ServerState::instance()->getState());

  return scope.Close(v8::String::New(state.c_str(), (int) state.size()));
}

////////////////////////////////////////////////////////////////////////////////
/// @brief returns the server state
////////////////////////////////////////////////////////////////////////////////

static v8::Handle<v8::Value> JS_GetClusterAuthentication (v8::Arguments const& argv) {
  v8::HandleScope scope;

  if (argv.Length() != 0) {
    TRI_V8_EXCEPTION_USAGE(scope, "getClusterAuthentication()");
  }

  std::string auth;
  if (ServerState::instance()->getRole() == ServerState::ROLE_UNDEFINED) {
    // Only on dispatchers, otherwise this would be a security risk!
    auth = ServerState::instance()->getAuthentication();
  }

  return scope.Close(v8::String::New(auth.c_str(), (int) auth.size()));
}

////////////////////////////////////////////////////////////////////////////////
/// @brief prepare to send a request
///
/// this is used for asynchronous as well as synchronous requests.
////////////////////////////////////////////////////////////////////////////////

static void PrepareClusterCommRequest (
                        v8::Arguments const& argv,
                        triagens::rest::HttpRequest::HttpRequestType& reqType,
                        string& destination,
                        string& path,
                        string& body,
                        map<string, string>* headerFields,
                        ClientTransactionID& clientTransactionID,
                        CoordTransactionID& coordTransactionID,
                        double& timeout) {

  TRI_v8_global_t* v8g = (TRI_v8_global_t*)
                         v8::Isolate::GetCurrent()->GetData();

  TRI_ASSERT(argv.Length() >= 4);

  reqType = triagens::rest::HttpRequest::HTTP_REQUEST_GET;
  if (argv[0]->IsString()) {
    TRI_Utf8ValueNFC UTF8(TRI_UNKNOWN_MEM_ZONE, argv[0]);
    string methstring = *UTF8;
    reqType = triagens::rest::HttpRequest::translateMethod(methstring);
    if (reqType == triagens::rest::HttpRequest::HTTP_REQUEST_ILLEGAL) {
      reqType = triagens::rest::HttpRequest::HTTP_REQUEST_GET;
    }
  }

  destination = TRI_ObjectToString(argv[1]);

  string dbname = TRI_ObjectToString(argv[2]);

  path = TRI_ObjectToString(argv[3]);
  path = "/_db/" + dbname + path;

  body.clear();
  if (argv.Length() > 4) {
    body = TRI_ObjectToString(argv[4]);
  }

  if (argv.Length() > 5 && argv[5]->IsObject()) {
    v8::Handle<v8::Object> obj = argv[5].As<v8::Object>();
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

  if (argv.Length() > 6 && argv[6]->IsObject()) {
    v8::Handle<v8::Object> opt = argv[6].As<v8::Object>();
    if (opt->Has(v8g->ClientTransactionIDKey)) {
      clientTransactionID
        = TRI_ObjectToString(opt->Get(v8g->ClientTransactionIDKey));
    }
    if (opt->Has(v8g->CoordTransactionIDKey)) {
      coordTransactionID
        = TRI_ObjectToUInt64(opt->Get(v8g->CoordTransactionIDKey), true);
    }
    if (opt->Has(v8g->TimeoutKey)) {
      timeout
        = TRI_ObjectToDouble(opt->Get(v8g->TimeoutKey));
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

v8::Handle<v8::Object> PrepareClusterCommResultForJS(
                                  ClusterCommResult const* res) {
  v8::HandleScope scope;
  TRI_v8_global_t* v8g = (TRI_v8_global_t*)
                         v8::Isolate::GetCurrent()->GetData();

  v8::Handle<v8::Object> r = v8::Object::New();
  if (0 == res) {
    r->Set(v8g->ErrorMessageKey, v8::String::New("out of memory"));
  } else if (res->dropped) {
    r->Set(v8g->ErrorMessageKey, v8::String::New("operation was dropped"));
  } else {
    r->Set(v8g->ClientTransactionIDKey,
           v8::String::New(res->clientTransactionID.c_str(),
                           (int) res->clientTransactionID.size()));

    // convert the ids to strings as uint64_t might be too big for JavaScript numbers
    std::string id = StringUtils::itoa(res->coordTransactionID);
    r->Set(v8g->CoordTransactionIDKey, v8::String::New(id.c_str(), (int) id.size()));

    id = StringUtils::itoa(res->operationID);
    r->Set(v8g->OperationIDKey, v8::String::New(id.c_str(), (int) id.size()));

    r->Set(v8g->ShardIDKey,
           v8::String::New(res->shardID.c_str(), (int) res->shardID.size()));
    if (res->status == CL_COMM_SUBMITTED) {
      r->Set(v8g->StatusKey, v8::String::New("SUBMITTED"));
    }
    else if (res->status == CL_COMM_SENDING) {
      r->Set(v8g->StatusKey, v8::String::New("SENDING"));
    }
    else if (res->status == CL_COMM_SENT) {
      r->Set(v8g->StatusKey, v8::String::New("SENT"));
      // This might be the result of a synchronous request and thus
      // contain the actual response. If it is an asynchronous request
      // which has not yet been answered, the following information is
      // probably rather boring:

      // The headers:
      v8::Handle<v8::Object> h = v8::Object::New();
      for (auto const& i : res->result->getHeaderFields()) {
        h->Set(v8::String::New(i.first.c_str(), (int) i.first.size()),
               v8::String::New(i.second.c_str(), (int) i.second.size()));
      }
      r->Set(v8::String::New("headers"), h);

      // The body:
      triagens::basics::StringBuffer& body = res->result->getBody();
      if (body.length() != 0) {
        r->Set(v8::String::New("body"), v8::String::New(body.c_str(),
                                                    (int) body.length()));
      }
    }
    else if (res->status == CL_COMM_TIMEOUT) {
      r->Set(v8g->StatusKey, v8::String::New("TIMEOUT"));
      r->Set(v8g->TimeoutKey,v8::BooleanObject::New(true));
    }
    else if (res->status == CL_COMM_ERROR) {
      r->Set(v8g->StatusKey, v8::String::New("ERROR"));

      if (res->result && res->result->isComplete()) {
        v8::Handle<v8::Object> details = v8::Object::New();
        details->Set(v8::String::New("code"), v8::Number::New(res->result->getHttpReturnCode()));
        details->Set(v8::String::New("message"), v8::String::New(res->result->getHttpReturnMessage().c_str()));
        details->Set(v8::String::New("body"),
                     v8::String::New(res->result->getBody().c_str(),
                                     (int) res->result->getBody().length()));

        r->Set(v8::String::New("details"), details);
        r->Set(v8g->ErrorMessageKey,
               v8::String::New("got bad HTTP response"));
      }
      else {
        r->Set(v8g->ErrorMessageKey,
               v8::String::New("got no HTTP response, DBserver seems gone"));
      }
    }
    else if (res->status == CL_COMM_DROPPED) {
      r->Set(v8g->StatusKey, v8::String::New("DROPPED"));
      r->Set(v8g->ErrorMessageKey,
             v8::String::New("request dropped whilst waiting for answer"));
    }
    else {   // Everything is OK
      TRI_ASSERT(res->status == CL_COMM_RECEIVED);
      // The headers:
      v8::Handle<v8::Object> h = v8::Object::New();
      r->Set(v8g->StatusKey, v8::String::New("RECEIVED"));
      map<string,string> headers = res->answer->headers();
      map<string,string>::iterator i;
      for (i = headers.begin(); i != headers.end(); ++i) {
        h->Set(v8::String::New(i->first.c_str()),
               v8::String::New(i->second.c_str()));
      }
      r->Set(v8::String::New("headers"), h);

      // The body:
      if (0 != res->answer->body()) {
        r->Set(v8::String::New("body"), v8::String::New(res->answer->body(),
                                                    (int) res->answer->bodySize()));
      }
    }
  }
  return scope.Close(r);
}

////////////////////////////////////////////////////////////////////////////////
/// @brief send an asynchronous request
////////////////////////////////////////////////////////////////////////////////

static v8::Handle<v8::Value> JS_AsyncRequest (v8::Arguments const& argv) {
  v8::HandleScope scope;

  if (argv.Length() < 4 || argv.Length() > 7) {
    TRI_V8_EXCEPTION_USAGE(scope, "asyncRequest("
      "reqType, destination, dbname, path, body, headers, options)");
  }
  // Possible options:
  //   - clientTransactionID  (string)
  //   - coordTransactionID   (number)
  //   - timeout              (number)

  // Disabled to allow communication originating in a DBserver:
  // 31.7.2014 Max

  // if (ServerState::instance()->getRole() != ServerState::ROLE_COORDINATOR) {
  //  TRI_V8_EXCEPTION_INTERNAL(scope,"request works only in coordinator role");
  //}

  ClusterComm* cc = ClusterComm::instance();

  if (cc == 0) {
    TRI_V8_EXCEPTION_MESSAGE(scope, TRI_ERROR_INTERNAL,
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

  PrepareClusterCommRequest(argv, reqType, destination, path,*body,headerFields,
                            clientTransactionID, coordTransactionID, timeout);

  ClusterCommResult const* res;

  res = cc->asyncRequest(clientTransactionID, coordTransactionID, destination,
                         reqType, path, body, true, headerFields, 0, timeout);

  if (res == 0) {
    TRI_V8_EXCEPTION_MESSAGE(scope, TRI_ERROR_INTERNAL,
                             "couldn't queue async request");
  }

  LOG_DEBUG("JS_AsyncRequest: request has been submitted");

  v8::Handle<v8::Object> result = PrepareClusterCommResultForJS(res);
  delete res;

  return scope.Close(result);
}

////////////////////////////////////////////////////////////////////////////////
/// @brief send a synchronous request
////////////////////////////////////////////////////////////////////////////////

static v8::Handle<v8::Value> JS_SyncRequest (v8::Arguments const& argv) {
  v8::HandleScope scope;

  if (argv.Length() < 4 || argv.Length() > 7) {
    TRI_V8_EXCEPTION_USAGE(scope, "syncRequest("
      "reqType, destination, dbname, path, body, headers, options)");
  }
  // Possible options:
  //   - clientTransactionID  (string)
  //   - coordTransactionID   (number)
  //   - timeout              (number)

  // Disabled to allow communication originating in a DBserver:
  // 31.7.2014 Max

  //if (ServerState::instance()->getRole() != ServerState::ROLE_COORDINATOR) {
  //  TRI_V8_EXCEPTION_INTERNAL(scope,"request works only in coordinator role");
  //}

  ClusterComm* cc = ClusterComm::instance();

  if (cc == 0) {
    TRI_V8_EXCEPTION_MESSAGE(scope, TRI_ERROR_INTERNAL,
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

  PrepareClusterCommRequest(argv, reqType, destination, path, body, headerFields,
                            clientTransactionID, coordTransactionID, timeout);

  ClusterCommResult const* res;

  res = cc->syncRequest(clientTransactionID, coordTransactionID, destination,
                         reqType, path, body, *headerFields, timeout);

  delete headerFields;

  if (res == 0) {
    TRI_V8_EXCEPTION_MESSAGE(scope, TRI_ERROR_INTERNAL,
                             "couldn't do sync request");
  }

  LOG_DEBUG("JS_SyncRequest: request has been done");

  v8::Handle<v8::Object> result = PrepareClusterCommResultForJS(res);
  delete res;

  return scope.Close(result);
}

////////////////////////////////////////////////////////////////////////////////
/// @brief enquire information about an asynchronous request
////////////////////////////////////////////////////////////////////////////////

static v8::Handle<v8::Value> JS_Enquire (v8::Arguments const& argv) {
  v8::HandleScope scope;

  if (argv.Length() != 1) {
    TRI_V8_EXCEPTION_USAGE(scope, "enquire(operationID)");
  }

  // Disabled to allow communication originating in a DBserver:
  // 31.7.2014 Max
  
  // if (ServerState::instance()->getRole() != ServerState::ROLE_COORDINATOR) {
  //   TRI_V8_EXCEPTION_INTERNAL(scope,"request works only in coordinator role");
  // }

  ClusterComm* cc = ClusterComm::instance();

  if (cc == 0) {
    TRI_V8_EXCEPTION_MESSAGE(scope, TRI_ERROR_INTERNAL,
                             "clustercomm object not found");
  }

  OperationID operationID = TRI_ObjectToUInt64(argv[0], true);

  ClusterCommResult const* res;

  LOG_DEBUG("JS_Enquire: calling ClusterComm::enquire()");

  res = cc->enquire(operationID);

  v8::Handle<v8::Object> result = PrepareClusterCommResultForJS(res);
  delete res;

  return scope.Close(result);
}

////////////////////////////////////////////////////////////////////////////////
/// @brief wait for the result of an asynchronous request
////////////////////////////////////////////////////////////////////////////////

static v8::Handle<v8::Value> JS_Wait (v8::Arguments const& argv) {
  v8::HandleScope scope;

  if (argv.Length() != 1) {
    TRI_V8_EXCEPTION_USAGE(scope, "wait(obj)");
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
  //   TRI_V8_EXCEPTION_INTERNAL(scope,"request works only in coordinator role");
  // }

  ClusterComm* cc = ClusterComm::instance();

  if (cc == 0) {
    TRI_V8_EXCEPTION_MESSAGE(scope, TRI_ERROR_INTERNAL,
                             "clustercomm object not found");
  }

  ClientTransactionID clientTransactionID = "";
  CoordTransactionID coordTransactionID = 0;
  OperationID operationID = 0;
  ShardID shardID = "";
  double timeout = 24*3600.0;

  TRI_v8_global_t* v8g = (TRI_v8_global_t*)
                         v8::Isolate::GetCurrent()->GetData();

  if (argv[0]->IsObject()) {
    v8::Handle<v8::Object> obj = argv[0].As<v8::Object>();
    if (obj->Has(v8g->ClientTransactionIDKey)) {
      clientTransactionID
        = TRI_ObjectToString(obj->Get(v8g->ClientTransactionIDKey));
    }
    if (obj->Has(v8g->CoordTransactionIDKey)) {
      coordTransactionID
        = TRI_ObjectToUInt64(obj->Get(v8g->CoordTransactionIDKey), true);
    }
    if (obj->Has(v8g->OperationIDKey)) {
      operationID = TRI_ObjectToUInt64(obj->Get(v8g->OperationIDKey), true);
    }
    if (obj->Has(v8g->ShardIDKey)) {
      shardID = TRI_ObjectToString(obj->Get(v8g->ShardIDKey));
    }
    if (obj->Has(v8g->TimeoutKey)) {
      timeout = TRI_ObjectToDouble(obj->Get(v8g->TimeoutKey));
      if (timeout == 0.0) {
        timeout = 24*3600.0;
      }
    }
  }

  ClusterCommResult const* res;

  LOG_DEBUG("JS_Wait: calling ClusterComm::wait()");

  res = cc->wait(clientTransactionID, coordTransactionID, operationID,
                 shardID, timeout);

  v8::Handle<v8::Object> result = PrepareClusterCommResultForJS(res);
  delete res;

  return scope.Close(result);
}


////////////////////////////////////////////////////////////////////////////////
/// @brief drop the result of an asynchronous request
////////////////////////////////////////////////////////////////////////////////

static v8::Handle<v8::Value> JS_Drop (v8::Arguments const& argv) {
  v8::HandleScope scope;

  if (argv.Length() != 1) {
    TRI_V8_EXCEPTION_USAGE(scope, "wait(obj)");
  }
  // Possible options:
  //   - clientTransactionID  (string)
  //   - coordTransactionID   (number)
  //   - operationID          (number)
  //   - shardID              (string)

  // Disabled to allow communication originating in a DBserver:
  // 31.7.2014 Max

  // if (ServerState::instance()->getRole() != ServerState::ROLE_COORDINATOR) {
  //   TRI_V8_EXCEPTION_INTERNAL(scope,"request works only in coordinator role");
  // }

  ClusterComm* cc = ClusterComm::instance();

  if (cc == 0) {
    TRI_V8_EXCEPTION_MESSAGE(scope, TRI_ERROR_INTERNAL,
                             "clustercomm object not found");
  }

  ClientTransactionID clientTransactionID = "";
  CoordTransactionID coordTransactionID = 0;
  OperationID operationID = 0;
  ShardID shardID = "";

  TRI_v8_global_t* v8g = (TRI_v8_global_t*)
                         v8::Isolate::GetCurrent()->GetData();

  if (argv[0]->IsObject()) {
    v8::Handle<v8::Object> obj = argv[0].As<v8::Object>();
    if (obj->Has(v8g->ClientTransactionIDKey)) {
      clientTransactionID
        = TRI_ObjectToString(obj->Get(v8g->ClientTransactionIDKey));
    }
    if (obj->Has(v8g->CoordTransactionIDKey)) {
      coordTransactionID
        = TRI_ObjectToUInt64(obj->Get(v8g->CoordTransactionIDKey), true);
    }
    if (obj->Has(v8g->OperationIDKey)) {
      operationID = TRI_ObjectToUInt64(obj->Get(v8g->OperationIDKey), true);
    }
    if (obj->Has(v8g->ShardIDKey)) {
      shardID = TRI_ObjectToString(obj->Get(v8g->ShardIDKey));
    }
  }

  LOG_DEBUG("JS_Drop: calling ClusterComm::drop()");

  cc->drop(clientTransactionID, coordTransactionID, operationID, shardID);

  return scope.Close(v8::Undefined());
}


// -----------------------------------------------------------------------------
// --SECTION--                                                  public functions
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @brief creates a global cluster context
////////////////////////////////////////////////////////////////////////////////

void TRI_InitV8Cluster (v8::Handle<v8::Context> context) {
  v8::HandleScope scope;

  // check the isolate
  v8::Isolate* isolate = v8::Isolate::GetCurrent();
  TRI_v8_global_t* v8g = (TRI_v8_global_t*) v8::Isolate::GetCurrent()->GetData();
  TRI_ASSERT(v8g != 0);

  v8::Handle<v8::ObjectTemplate> rt;
  v8::Handle<v8::FunctionTemplate> ft;

  // ...........................................................................
  // generate the agency template
  // ...........................................................................

  ft = v8::FunctionTemplate::New();
  ft->SetClassName(TRI_V8_SYMBOL("ArangoAgency"));

  rt = ft->InstanceTemplate();
  rt->SetInternalFieldCount(2);

  TRI_AddMethodVocbase(rt, "cas", JS_CasAgency);
  TRI_AddMethodVocbase(rt, "createDirectory", JS_CreateDirectoryAgency);
  TRI_AddMethodVocbase(rt, "get", JS_GetAgency);
  TRI_AddMethodVocbase(rt, "isEnabled", JS_IsEnabledAgency);
  TRI_AddMethodVocbase(rt, "increaseVersion", JS_IncreaseVersionAgency);
  TRI_AddMethodVocbase(rt, "list", JS_ListAgency);
  TRI_AddMethodVocbase(rt, "lockRead", JS_LockReadAgency);
  TRI_AddMethodVocbase(rt, "lockWrite", JS_LockWriteAgency);
  TRI_AddMethodVocbase(rt, "remove", JS_RemoveAgency);
  TRI_AddMethodVocbase(rt, "set", JS_SetAgency);
  TRI_AddMethodVocbase(rt, "watch", JS_WatchAgency);
  TRI_AddMethodVocbase(rt, "endpoints", JS_EndpointsAgency);
  TRI_AddMethodVocbase(rt, "prefix", JS_PrefixAgency);
  TRI_AddMethodVocbase(rt, "setPrefix", JS_SetPrefixAgency, true);
  TRI_AddMethodVocbase(rt, "uniqid", JS_UniqidAgency);
  TRI_AddMethodVocbase(rt, "unlockRead", JS_UnlockReadAgency);
  TRI_AddMethodVocbase(rt, "unlockWrite", JS_UnlockWriteAgency);
  TRI_AddMethodVocbase(rt, "version", JS_VersionAgency);

  v8g->AgencyTempl = v8::Persistent<v8::ObjectTemplate>::New(isolate, rt);
  TRI_AddGlobalFunctionVocbase(context, "ArangoAgencyCtor", ft->GetFunction(), true);

  // register the global object
  v8::Handle<v8::Object> aa = v8g->AgencyTempl->NewInstance();
  if (! aa.IsEmpty()) {
    TRI_AddGlobalVariableVocbase(context, "ArangoAgency", aa);
  }

  // .............................................................................
  // generate the cluster info template
  // .............................................................................

  ft = v8::FunctionTemplate::New();
  ft->SetClassName(TRI_V8_SYMBOL("ArangoClusterInfo"));

  rt = ft->InstanceTemplate();
  rt->SetInternalFieldCount(2);

  TRI_AddMethodVocbase(rt, "doesDatabaseExist", JS_DoesDatabaseExistClusterInfo);
  TRI_AddMethodVocbase(rt, "listDatabases", JS_ListDatabases);
  TRI_AddMethodVocbase(rt, "flush", JS_FlushClusterInfo, true);
  TRI_AddMethodVocbase(rt, "getCollectionInfo", JS_GetCollectionInfoClusterInfo);
  TRI_AddMethodVocbase(rt, "getCollectionInfoCurrent", JS_GetCollectionInfoCurrentClusterInfo);
  TRI_AddMethodVocbase(rt, "getResponsibleServer", JS_GetResponsibleServerClusterInfo);
  TRI_AddMethodVocbase(rt, "getResponsibleShard", JS_GetResponsibleShardClusterInfo);
  TRI_AddMethodVocbase(rt, "getServerEndpoint", JS_GetServerEndpointClusterInfo);
  TRI_AddMethodVocbase(rt, "getDBServers", JS_GetDBServers);
  TRI_AddMethodVocbase(rt, "reloadDBServers", JS_ReloadDBServers);
  TRI_AddMethodVocbase(rt, "uniqid", JS_UniqidClusterInfo);

  v8g->ClusterInfoTempl = v8::Persistent<v8::ObjectTemplate>::New(isolate, rt);
  TRI_AddGlobalFunctionVocbase(context, "ArangoClusterInfoCtor", ft->GetFunction(), true);

  // register the global object
  v8::Handle<v8::Object> ci = v8g->ClusterInfoTempl->NewInstance();
  if (! ci.IsEmpty()) {
    TRI_AddGlobalVariableVocbase(context, "ArangoClusterInfo", ci);
  }

  // .............................................................................
  // generate the server state template
  // ...........................................................................

  ft = v8::FunctionTemplate::New();
  ft->SetClassName(TRI_V8_SYMBOL("ArangoServerState"));

  rt = ft->InstanceTemplate();
  rt->SetInternalFieldCount(2);

  TRI_AddMethodVocbase(rt, "address", JS_AddressServerState);
  TRI_AddMethodVocbase(rt, "flush", JS_FlushServerState, true);
  TRI_AddMethodVocbase(rt, "id", JS_IdServerState);
  TRI_AddMethodVocbase(rt, "dataPath", JS_DataPathServerState);
  TRI_AddMethodVocbase(rt, "logPath", JS_LogPathServerState);
  TRI_AddMethodVocbase(rt, "agentPath", JS_AgentPathServerState);
  TRI_AddMethodVocbase(rt, "arangodPath", JS_ArangodPathServerState);
  TRI_AddMethodVocbase(rt, "javaScriptPath", JS_JavaScriptPathServerState);
  TRI_AddMethodVocbase(rt, "dbserverConfig", JS_DBserverConfigServerState);
  TRI_AddMethodVocbase(rt, "coordinatorConfig", JS_CoordinatorConfigServerState);
  TRI_AddMethodVocbase(rt, "disableDispatcherFrontend", JS_DisableDipatcherFrontendServerState);
  TRI_AddMethodVocbase(rt, "disableDispatcherKickstarter", JS_DisableDipatcherKickstarterServerState);
  TRI_AddMethodVocbase(rt, "initialised", JS_InitialisedServerState);
  TRI_AddMethodVocbase(rt, "isCoordinator", JS_IsCoordinatorServerState);
  TRI_AddMethodVocbase(rt, "role", JS_RoleServerState);
  TRI_AddMethodVocbase(rt, "setId", JS_SetIdServerState, true);
  TRI_AddMethodVocbase(rt, "setRole", JS_SetRoleServerState, true);
  TRI_AddMethodVocbase(rt, "status", JS_StatusServerState);
  TRI_AddMethodVocbase(rt, "getClusterAuthentication", JS_GetClusterAuthentication);

  v8g->ServerStateTempl = v8::Persistent<v8::ObjectTemplate>::New(isolate, rt);
  TRI_AddGlobalFunctionVocbase(context, "ArangoServerStateCtor", ft->GetFunction(), true);

  // register the global object
  v8::Handle<v8::Object> ss = v8g->ServerStateTempl->NewInstance();
  if (! ss.IsEmpty()) {
    TRI_AddGlobalVariableVocbase(context, "ArangoServerState", ss);
  }

  // ...........................................................................
  // generate the cluster comm template
  // ...........................................................................

  ft = v8::FunctionTemplate::New();
  ft->SetClassName(TRI_V8_SYMBOL("ArangoClusterComm"));

  rt = ft->InstanceTemplate();
  rt->SetInternalFieldCount(2);

  TRI_AddMethodVocbase(rt, "asyncRequest", JS_AsyncRequest);
  TRI_AddMethodVocbase(rt, "syncRequest", JS_SyncRequest);
  TRI_AddMethodVocbase(rt, "enquire", JS_Enquire);
  TRI_AddMethodVocbase(rt, "wait", JS_Wait);
  TRI_AddMethodVocbase(rt, "drop", JS_Drop);

  v8g->ClusterCommTempl = v8::Persistent<v8::ObjectTemplate>::New(isolate, rt);
  TRI_AddGlobalFunctionVocbase(context, "ArangoClusterCommCtor", ft->GetFunction(), true);

  // register the global object
  ss = v8g->ClusterCommTempl->NewInstance();
  if (! ss.IsEmpty()) {
    TRI_AddGlobalVariableVocbase(context, "ArangoClusterComm", ss);
  }
}

// -----------------------------------------------------------------------------
// --SECTION--                                                       END-OF-FILE
// -----------------------------------------------------------------------------

// Local Variables:
// mode: outline-minor
// outline-regexp: "/// @brief\\|/// {@inheritDoc}\\|/// @page\\|// --SECTION--\\|/// @\\}"
// End:
