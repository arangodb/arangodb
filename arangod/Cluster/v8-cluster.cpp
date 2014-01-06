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

#include "Cluster/AgencyComm.h"
#include "Cluster/ClusterInfo.h"
#include "Cluster/ServerState.h"
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
    TRI_V8_EXCEPTION_USAGE(scope, "list(<key>, <recursive>)");
  }

  const std::string key = TRI_ObjectToString(argv[0]);
  bool recursive = false;

  if (argv.Length() > 1) {
    recursive = TRI_ObjectToBoolean(argv[1]);
  }
  
  AgencyComm comm;
  AgencyCommResult result = comm.getValues(key, recursive);

  if (! result.successful()) {
    return scope.Close(v8::ThrowException(CreateAgencyException(result)));
  }
  
  v8::Handle<v8::Object> l = v8::Object::New();

  // return just the value for each key
  std::map<std::string, bool> out;
  result.flattenJson(out, "");
  std::map<std::string, bool>::const_iterator it = out.begin(); 

  while (it != out.end()) {
    const std::string key = (*it).first;
    const bool isDirectory = (*it).second;

    l->Set(v8::String::New(key.c_str(), key.size()), v8::Boolean::New(isDirectory)); 
    ++it;
  }

  return scope.Close(l);
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

  if (argv.Length() != 0) {
    TRI_V8_EXCEPTION_USAGE(scope, "prefix()");
  }

  const std::string prefix = AgencyComm::prefix();

  return scope.Close(v8::String::New(prefix.c_str(), prefix.size()));
}

////////////////////////////////////////////////////////////////////////////////
/// @brief creates a uniqid
////////////////////////////////////////////////////////////////////////////////

static v8::Handle<v8::Value> JS_UniqidAgency (v8::Arguments const& argv) {
  v8::HandleScope scope;

  if (argv.Length() > 3) {
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
/// @brief get the responsible server
////////////////////////////////////////////////////////////////////////////////

static v8::Handle<v8::Value> JS_GetCollectionInfoClusterInfo (v8::Arguments const& argv) {
  v8::HandleScope scope;

  if (argv.Length() != 1) {
    TRI_V8_EXCEPTION_USAGE(scope, "getCollectionInfo(<database-id>, <collection-id>)");
  }
 
  ClusterInfo::instance()->getCollectionInfo(TRI_ObjectToString(argv[0]), TRI_ObjectToString(argv[1]));

  return scope.Close(v8::True());
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

  // .............................................................................
  // generate the agency template
  // .............................................................................

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
  TRI_AddMethodVocbase(rt, "uniqid", JS_UniqidAgency);
  TRI_AddMethodVocbase(rt, "unlockRead", JS_UnlockReadAgency);
  TRI_AddMethodVocbase(rt, "unlockWrite", JS_UnlockWriteAgency);
  TRI_AddMethodVocbase(rt, "version", JS_VersionAgency);

  v8g->AgencyTempl = v8::Persistent<v8::ObjectTemplate>::New(isolate, rt);
  TRI_AddGlobalFunctionVocbase(context, "ArangoAgency", ft->GetFunction());
  
  // .............................................................................
  // generate the cluster info template
  // .............................................................................

  ft = v8::FunctionTemplate::New();
  ft->SetClassName(TRI_V8_SYMBOL("ArangoClusterInfo"));

  rt = ft->InstanceTemplate();
  rt->SetInternalFieldCount(2);

  TRI_AddMethodVocbase(rt, "doesDatabaseExist", JS_DoesDatabaseExistClusterInfo);
  TRI_AddMethodVocbase(rt, "getCollectionInfo", JS_GetCollectionInfoClusterInfo);
  TRI_AddMethodVocbase(rt, "getResponsibleServer", JS_GetResponsibleServerClusterInfo);
  TRI_AddMethodVocbase(rt, "getServerEndpoint", JS_GetServerEndpointClusterInfo);
  TRI_AddMethodVocbase(rt, "uniqid", JS_UniqidClusterInfo);

  v8g->ServerStateTempl = v8::Persistent<v8::ObjectTemplate>::New(isolate, rt);
  TRI_AddGlobalFunctionVocbase(context, "ArangoClusterInfo", ft->GetFunction());
  
  // .............................................................................
  // generate the server state template
  // .............................................................................

  ft = v8::FunctionTemplate::New();
  ft->SetClassName(TRI_V8_SYMBOL("ArangoServerState"));

  rt = ft->InstanceTemplate();
  rt->SetInternalFieldCount(2);

  TRI_AddMethodVocbase(rt, "isCoordinator", JS_IsCoordinatorServerState);
  TRI_AddMethodVocbase(rt, "role", JS_RoleServerState);
  TRI_AddMethodVocbase(rt, "status", JS_StatusServerState);

  v8g->ServerStateTempl = v8::Persistent<v8::ObjectTemplate>::New(isolate, rt);
  TRI_AddGlobalFunctionVocbase(context, "ArangoServerState", ft->GetFunction());
}

// Local Variables:
// mode: outline-minor
// outline-regexp: "/// @brief\\|/// {@inheritDoc}\\|/// @addtogroup\\|/// @page\\|// --SECTION--\\|/// @\\}"
// End:
