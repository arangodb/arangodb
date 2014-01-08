////////////////////////////////////////////////////////////////////////////////
/// @brief V8-cluster bridge
///
/// @file
///
/// DISCLAIMER
///
/// Copyright 2004-2013 triAGENS GmbH, Cologne, Germany
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
/// Copyright holder is triAGENS GmbH, Cologne, Germany
///
/// @author Jan Steemann
/// @author Copyright 2011-2013, triAGENS GmbH, Cologne, Germany
////////////////////////////////////////////////////////////////////////////////

#include "v8-cluster.h"

#include "BasicsC/common.h"

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
  v8::Handle<v8::String> errorMessage = v8::String::New(errorDetails.c_str(), errorDetails.size());
  v8::Handle<v8::Object> errorObject = v8::Exception::Error(errorMessage)->ToObject();

  errorObject->Set(v8::String::New("code"), v8::Number::New(result.httpCode()));
  errorObject->Set(v8::String::New("errorNum"), v8::Number::New(result.errorCode()));
  errorObject->Set(v8::String::New("errorMessage"), errorMessage);
  errorObject->Set(v8::String::New("error"), v8::True());
  
  v8::Handle<v8::Value> proto = v8g->ErrorTempl->NewInstance();
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
  const std::string oldValue = TRI_ObjectToString(argv[1]);
  const std::string newValue = TRI_ObjectToString(argv[2]);
  
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
  AgencyCommResult result = comm.casValue(key, oldValue, newValue, ttl, timeout);

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
  
  v8::Handle<v8::Object> l = v8::Object::New();

  if (withIndexes) {
    // return an object for each key
    std::map<std::string, std::string> outValues;
    std::map<std::string, std::string> outIndexes;
  
    result.flattenJson(outValues, "", false);
    result.flattenJson(outIndexes, "", true);
    
    assert(outValues.size() == outIndexes.size());

    std::map<std::string, std::string>::const_iterator it = outValues.begin(); 
    std::map<std::string, std::string>::const_iterator it2 = outIndexes.begin(); 

    while (it != outValues.end()) {
      const std::string key = (*it).first;
      const std::string value = (*it).second;
      const std::string idx = (*it2).second;

      v8::Handle<v8::Object> sub = v8::Object::New();
      
      sub->Set(v8::String::New("value"), v8::String::New(value.c_str(), value.size()));
      sub->Set(v8::String::New("index"), v8::String::New(idx.c_str(), idx.size()));

      l->Set(v8::String::New(key.c_str(), key.size()), sub);
      
      ++it;
      ++it2;
    }
  }
  else {
    // return just the value for each key
    std::map<std::string, std::string> out;
  
    result.flattenJson(out, "", false);
    std::map<std::string, std::string>::const_iterator it = out.begin(); 

    while (it != out.end()) {
      const std::string key = (*it).first;
      const std::string value = (*it).second;

      l->Set(v8::String::New(key.c_str(), key.size()), v8::String::New(value.c_str(), value.size()));
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
  std::map<std::string, bool> out;
  result.flattenJson(out, "");
  std::map<std::string, bool>::const_iterator it = out.begin(); 

  // skip first entry
  ++it;

  if (flat) {
    v8::Handle<v8::Array> l = v8::Array::New();

    uint32_t i = 0;
    while (it != out.end()) {
      const std::string key = (*it).first;

      l->Set(i++, v8::String::New(key.c_str(), key.size()));
      ++it;
    }
    return scope.Close(l);
  }

  else {
    v8::Handle<v8::Object> l = v8::Object::New();

    while (it != out.end()) {
      const std::string key = (*it).first;
      const bool isDirectory = (*it).second;

      l->Set(v8::String::New(key.c_str(), key.size()), v8::Boolean::New(isDirectory)); 
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
  const std::string value = TRI_ObjectToString(argv[1]);

  double ttl = 0.0;
  if (argv.Length() > 2) {
    ttl = TRI_ObjectToDouble(argv[2]);
  }
  
  AgencyComm comm;
  AgencyCommResult result = comm.setValue(key, value, ttl);
  
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

  std::map<std::string, std::string> out;
  result.flattenJson(out, "", false);
  std::map<std::string, std::string>::const_iterator it = out.begin(); 

  v8::Handle<v8::Object> l = v8::Object::New();

  while (it != out.end()) {
    const std::string key = (*it).first;
    const std::string value = (*it).second;

    l->Set(v8::String::New(key.c_str(), key.size()), v8::String::New(value.c_str(), value.size()));
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

  const std::vector<std::string> endpoints = AgencyComm::getEndpoints();

  v8::Handle<v8::Array> l = v8::Array::New();

  for (size_t i = 0; i < endpoints.size(); ++i) {
    const std::string endpoint = endpoints[i];

    l->Set((uint32_t) i, v8::String::New(endpoint.c_str(), endpoint.size()));
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
    return scope.Close(v8::String::New(prefix.c_str() + 1, prefix.size() - 2));
  }
  
  return scope.Close(v8::String::New(prefix.c_str(), prefix.size()));
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

  return scope.Close(v8::String::New(value.c_str(), value.size()));
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

  return scope.Close(v8::String::New(version.c_str(), version.size()));
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
 
  const bool result = ClusterInfo::instance()->doesDatabaseExist(TRI_ObjectToString(argv[0]));

  return scope.Close(v8::Boolean::New(result));
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
/// @brief get the responsible server
////////////////////////////////////////////////////////////////////////////////

static v8::Handle<v8::Value> JS_GetCollectionInfoClusterInfo (v8::Arguments const& argv) {
  v8::HandleScope scope;

  if (argv.Length() != 2) {
    TRI_V8_EXCEPTION_USAGE(scope, "getCollectionInfo(<database-id>, <collection-id>)");
  }
 
  CollectionInfo ci = ClusterInfo::instance()->getCollectionInfo(TRI_ObjectToString(argv[0]), 
                                                                 TRI_ObjectToString(argv[1]));

  v8::Handle<v8::Object> result = v8::Object::New();
  const std::string cid = triagens::basics::StringUtils::itoa(ci.cid());
  const std::string& name = ci.name();
  result->Set(v8::String::New("id"), v8::String::New(cid.c_str(), cid.size()));
  result->Set(v8::String::New("name"), v8::String::New(name.c_str(), name.size()));
  result->Set(v8::String::New("type"), v8::Number::New((int) ci.type()));
  result->Set(v8::String::New("status"), v8::Number::New((int) ci.status()));

  const std::vector<std::string>& sks = ci.shardKeys();
  v8::Handle<v8::Array> shardKeys = v8::Array::New(sks.size());
  for (uint32_t i = 0, n = sks.size(); i < n; ++i) {
    shardKeys->Set(i, v8::String::New(sks[i].c_str(), sks[i].size()));
  }
  result->Set(v8::String::New("shardKeys"), shardKeys);

  const std::map<std::string, std::string>& sis = ci.shardIds();
  v8::Handle<v8::Object> shardIds = v8::Object::New();
  std::map<std::string, std::string>::const_iterator it = sis.begin();
  while (it != sis.end()) {
    shardIds->Set(v8::String::New((*it).first.c_str(), (*it).first.size()), 
                  v8::String::New((*it).second.c_str(), (*it).second.size()));
    ++it;
  }
  result->Set(v8::String::New("shards"), shardIds);

  // TODO: fill "indexes"
  v8::Handle<v8::Array> indexes = v8::Array::New();
  result->Set(v8::String::New("indexes"), indexes);
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
 
  const std::string result = ClusterInfo::instance()->getResponsibleServer(TRI_ObjectToString(argv[0]));

  return scope.Close(v8::String::New(result.c_str(), result.size()));
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

  return scope.Close(v8::String::New(result.c_str(), result.size()));
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

  return scope.Close(v8::String::New(id.c_str(), id.size()));
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
  return scope.Close(v8::String::New(address.c_str(), address.size()));
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
  return scope.Close(v8::String::New(id.c_str(), id.size()));
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

  return scope.Close(v8::String::New(role.c_str(), role.size()));
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

  return scope.Close(v8::String::New(state.c_str(), state.size()));
}

////////////////////////////////////////////////////////////////////////////////
/// @brief prepare to send a request
///
/// this is used for asynchronous as well as synchronous requests.
////////////////////////////////////////////////////////////////////////////////

static void PrepareClusterCommRequest (
                        v8::Arguments const& argv,
                        triagens::rest::HttpRequest::HttpRequestType& reqType,
                        ShardID& shardID,
                        string& path,
                        string& body,
                        map<string, string>* headerFields,
                        ClientTransactionID& clientTransactionID,
                        CoordTransactionID& coordTransactionID,
                        double& timeout) {

  TRI_v8_global_t* v8g = (TRI_v8_global_t*) 
                         v8::Isolate::GetCurrent()->GetData();

  reqType = triagens::rest::HttpRequest::HTTP_REQUEST_GET;
  if (argv.Length() > 0 && argv[0]->IsString()) {
    TRI_Utf8ValueNFC UTF8(TRI_UNKNOWN_MEM_ZONE, argv[0]);
    string methstring = *UTF8;
    reqType = triagens::rest::HttpRequest::translateMethod(methstring);
    if (reqType == triagens::rest::HttpRequest::HTTP_REQUEST_ILLEGAL) {
      reqType = triagens::rest::HttpRequest::HTTP_REQUEST_GET;
    }
  }

  shardID.clear();
  if (argv.Length() > 1) {
    shardID = TRI_ObjectToString(argv[1]);
  }
  if (shardID == "") {
    shardID = "shardBlubb";
  }
  
  string dbname;
  if (argv.Length() > 2) {
    dbname = TRI_ObjectToString(argv[2]);
  } 
  if (dbname == "") {
    dbname = "_system";
  }
  path.clear();
  if (argv.Length() > 3) {
    path = TRI_ObjectToString(argv[3]);
  }
  if (path == "") {
    path = "/_admin/version";
  }
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
                           res->clientTransactionID.size()));

    // convert the ids to strings as uint64_t might be too big for JavaScript numbers
    std::string id = StringUtils::itoa(res->coordTransactionID);
    r->Set(v8g->CoordTransactionIDKey, v8::String::New(id.c_str(), id.size()));

    id = StringUtils::itoa(res->operationID);
    r->Set(v8g->OperationIDKey, v8::String::New(id.c_str(), id.size()));

    r->Set(v8g->ShardIDKey,
           v8::String::New(res->shardID.c_str(), res->shardID.size()));
    if (res->status == CL_COMM_SUBMITTED) {
      r->Set(v8g->StatusKey, v8::String::New("SUBMITTED"));
    }
    else if (res->status == CL_COMM_SENDING) {
      r->Set(v8g->StatusKey, v8::String::New("SENDING"));
    }
    else if (res->status == CL_COMM_SENT) {
      r->Set(v8g->StatusKey, v8::String::New("SENT"));
      // Maybe return the response of the initial request???
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
        details->Set(v8::String::New("body"), v8::String::New(res->result->getBody().str().c_str(), res->result->getBody().str().length()));

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
      assert(res->status == CL_COMM_RECEIVED);
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
                                                    res->answer->bodySize()));
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
      "reqType, shardID, dbname, path, body, headers, options)");
  }
  // Possible options:
  //   - clientTransactionID  (string)
  //   - coordTransactionID   (number)
  //   - timeout              (number)
  
  if (ServerState::instance()->getRole() != ServerState::ROLE_COORDINATOR) {
    TRI_V8_EXCEPTION_INTERNAL(scope,"request works only in coordinator role");
  }

  ClusterComm* cc = ClusterComm::instance();

  if (cc == 0) {
    TRI_V8_EXCEPTION_MESSAGE(scope, TRI_ERROR_INTERNAL, 
                             "clustercomm object not found");
  }

  triagens::rest::HttpRequest::HttpRequestType reqType;
  ShardID shardID;
  string path;
  string body;
  map<string, string>* headerFields = new map<string, string>;
  ClientTransactionID clientTransactionID;
  CoordTransactionID coordTransactionID;
  double timeout;

  PrepareClusterCommRequest(argv, reqType, shardID, path, body, headerFields,
                            clientTransactionID, coordTransactionID, timeout);

  ClusterCommResult const* res;

  res = cc->asyncRequest(clientTransactionID, coordTransactionID, shardID, 
                         reqType, path, body.c_str(), body.size(), 
                         headerFields, 0, timeout);

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
      "reqType, shardID, dbname, path, body, headers, options)");
  }
  // Possible options:
  //   - clientTransactionID  (string)
  //   - coordTransactionID   (number)
  //   - timeout              (number)
  
  if (ServerState::instance()->getRole() != ServerState::ROLE_COORDINATOR) {
    TRI_V8_EXCEPTION_INTERNAL(scope,"request works only in coordinator role");
  }

  ClusterComm* cc = ClusterComm::instance();

  if (cc == 0) {
    TRI_V8_EXCEPTION_MESSAGE(scope, TRI_ERROR_INTERNAL, 
                             "clustercomm object not found");
  }

  triagens::rest::HttpRequest::HttpRequestType reqType;
  ShardID shardID;
  string path;
  string body;
  map<string, string>* headerFields = new map<string, string>;
  ClientTransactionID clientTransactionID;
  CoordTransactionID coordTransactionID;
  double timeout;

  PrepareClusterCommRequest(argv, reqType, shardID, path, body, headerFields,
                            clientTransactionID, coordTransactionID, timeout);

  ClusterCommResult const* res;

  res = cc->syncRequest(clientTransactionID, coordTransactionID, shardID, 
                         reqType, path, body.c_str(), body.size(), 
                         *headerFields, timeout);

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
    TRI_V8_EXCEPTION_USAGE(scope, "wait(operationID)");
  }
  
  if (ServerState::instance()->getRole() != ServerState::ROLE_COORDINATOR) {
    TRI_V8_EXCEPTION_INTERNAL(scope,"request works only in coordinator role");
  }

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
  
  if (ServerState::instance()->getRole() != ServerState::ROLE_COORDINATOR) {
    TRI_V8_EXCEPTION_INTERNAL(scope,"request works only in coordinator role");
  }

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
  
  if (ServerState::instance()->getRole() != ServerState::ROLE_COORDINATOR) {
    TRI_V8_EXCEPTION_INTERNAL(scope,"request works only in coordinator role");
  }

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
  assert(v8g != 0);

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
  TRI_AddMethodVocbase(rt, "flush", JS_FlushClusterInfo, true);
  TRI_AddMethodVocbase(rt, "getCollectionInfo", JS_GetCollectionInfoClusterInfo);
  TRI_AddMethodVocbase(rt, "getResponsibleServer", JS_GetResponsibleServerClusterInfo);
  TRI_AddMethodVocbase(rt, "getServerEndpoint", JS_GetServerEndpointClusterInfo);
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
  TRI_AddMethodVocbase(rt, "initialised", JS_InitialisedServerState);
  TRI_AddMethodVocbase(rt, "isCoordinator", JS_IsCoordinatorServerState);
  TRI_AddMethodVocbase(rt, "role", JS_RoleServerState);
  TRI_AddMethodVocbase(rt, "setId", JS_SetIdServerState, true);
  TRI_AddMethodVocbase(rt, "setRole", JS_SetRoleServerState, true);
  TRI_AddMethodVocbase(rt, "status", JS_StatusServerState);

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
  TRI_AddGlobalFunctionVocbase(context, "ArangoClusterCommCtor", 
                               ft->GetFunction());

  // register the global object
  ss = v8g->ClusterCommTempl->NewInstance();
  if (! ss.IsEmpty()) {
    TRI_AddGlobalVariableVocbase(context, "ArangoClusterComm", ss);
  }
}

// Local Variables:
// mode: outline-minor
// outline-regexp: "/// @brief\\|/// {@inheritDoc}\\|/// @addtogroup\\|/// @page\\|// --SECTION--\\|/// @\\}"
// End:
