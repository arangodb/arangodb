////////////////////////////////////////////////////////////////////////////////
/// @brief vocbase manager
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
/// @author Dr. Frank Celler
/// @author Copyright 2011-2013, triAGENS GmbH, Cologne, Germany
////////////////////////////////////////////////////////////////////////////////

#include "RestServer/VocbaseManager.h"

#include "BasicsC/common.h"

#include "Logger/Logger.h"
#include "Rest/ConnectionInfo.h"
#include "Basics/StringUtils.h"
#include "BasicsC/files.h"
#include "BasicsC/tri-strings.h"
#include "RestServer/VocbaseContext.h"
#include "VocBase/auth.h"
#include "Actions/actions.h"

#include "HttpServer/ApplicationEndpointServer.h"
#include "V8/JSLoader.h"
#include "V8/v8-conv.h"
#include "V8/v8-globals.h"
#include "V8/v8-utils.h"

using namespace std;
using namespace triagens::basics;
using namespace triagens::arango;
using namespace triagens::rest;

// -----------------------------------------------------------------------------
// --SECTION--                                                            static
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @brief the global manager
////////////////////////////////////////////////////////////////////////////////

VocbaseManager VocbaseManager::manager;

////////////////////////////////////////////////////////////////////////////////
/// @brief add the context to a request
////////////////////////////////////////////////////////////////////////////////

bool VocbaseManager::setRequestContext(triagens::rest::HttpRequest* request) {
  TRI_vocbase_t* vocbase = VocbaseManager::manager.lookupVocbaseByHttpRequest(request);

  if (vocbase == 0) {
    // invalid database name specified, database not found etc.
    return false;
  }

  triagens::arango::VocbaseContext* ctx = new triagens::arango::VocbaseContext(request, &VocbaseManager::manager);
  ctx->setVocbase(vocbase);
  request->addRequestContext(ctx);
  
  return true;
}

// -----------------------------------------------------------------------------
// --SECTION--                                                            public
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @addtogroup ArangoDB
/// @{
////////////////////////////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////////////////////////
/// @brief add system vocbase
////////////////////////////////////////////////////////////////////////////////

void VocbaseManager::addSystemVocbase (TRI_vocbase_t* vocbase) {
  WRITE_LOCKER(_rwLock);
  _vocbase = vocbase;
  std::map<std::string, std::string> m;
  _authCache[vocbase] = m;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief add user vocbase
////////////////////////////////////////////////////////////////////////////////

void VocbaseManager::addUserVocbase (TRI_vocbase_t* vocbase) {
  {
    WRITE_LOCKER(_rwLock);
    _vocbases[vocbase->_name] = vocbase;  
    std::map<std::string, std::string> m;
    _authCache[vocbase] = m;
  }
  
  TRI_ReloadAuthInfo(vocbase);
}

////////////////////////////////////////////////////////////////////////////////
/// @brief close user vocbases
////////////////////////////////////////////////////////////////////////////////

void VocbaseManager::closeUserVocbases () {
  WRITE_LOCKER(_rwLock);
  
  std::map<std::string, TRI_vocbase_t*>::iterator i = _vocbases.begin();  
  for (; i != _vocbases.end(); ++i) {
    TRI_vocbase_t* vocbase = i->second;
    TRI_DestroyVocBase(vocbase);
    TRI_Free(TRI_UNKNOWN_MEM_ZONE, vocbase);
  }
  
  _vocbases.clear();  
}

////////////////////////////////////////////////////////////////////////////////
/// @brief look vocbase by name
////////////////////////////////////////////////////////////////////////////////

TRI_vocbase_t* VocbaseManager::lookupVocbaseByName (string const& name) {
  if (name == TRI_VOC_SYSTEM_DATABASE) {
    return _vocbase;
  }
  
  READ_LOCKER(_rwLock);
  map<string, TRI_vocbase_s*>::iterator find = _vocbases.find(name);
  if (find != _vocbases.end()) {
    return find->second;
  } 
  
  return 0;  
}

////////////////////////////////////////////////////////////////////////////////
/// @brief check if name and path is not used
////////////////////////////////////////////////////////////////////////////////

int VocbaseManager::canAddVocbase (std::string const& name, 
                                   std::string const& path,
                                   bool checkPath) {
  if (! isValidName(name)) {
    return TRI_ERROR_ARANGO_DATABASE_NAME_INVALID;
  }

  if (path.empty()) {
    return TRI_ERROR_ARANGO_DATABASE_PATH_INVALID;
  }

  // loop over all vocbases and check name and path
  READ_LOCKER(_rwLock);
  
  // system vocbase
  if (name == string(_vocbase->_name)) {
    return TRI_ERROR_ARANGO_DATABASE_NAME_USED;
  }
  if (path == string(_vocbase->_path)) {
    return TRI_ERROR_ARANGO_DATABASE_PATH_USED;
  }
  
  // user vocbases
  std::map<std::string, TRI_vocbase_t*>::iterator i = _vocbases.begin();
  for (; i != _vocbases.end(); ++i) {
    TRI_vocbase_t* vocbase = i->second;

    if (name == string(vocbase->_name)) {
      return TRI_ERROR_ARANGO_DATABASE_NAME_USED;
    }
    if (path == string(vocbase->_path)) {
      return TRI_ERROR_ARANGO_DATABASE_PATH_USED;
    }
  }

  // check if the path already exists
  if (checkPath && TRI_ExistsFile(path.c_str())) {
    return TRI_ERROR_ARANGO_DATABASE_PATH_USED;
  }
  
  return TRI_ERROR_NO_ERROR;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief check if a collection name is valid
////////////////////////////////////////////////////////////////////////////////

bool VocbaseManager::isValidName (std::string const& name) const {
  return TRI_IsAllowedCollectionName(false, name.c_str());
}

////////////////////////////////////////////////////////////////////////////////
/// @brief run version check
/// @return bool             returns false if the version check fails
////////////////////////////////////////////////////////////////////////////////

bool VocbaseManager::runVersionCheck (TRI_vocbase_t* vocbase, 
                                      v8::Handle<v8::Context> context) {
  if (! _startupLoader) {
    LOGGER_ERROR("Javascript start up loader not found.");
    return false;
  }
  
  v8::HandleScope scope;
  TRI_v8_global_t* v8g = (TRI_v8_global_t*) v8::Isolate::GetCurrent()->GetData();
  TRI_vocbase_t* orig = (TRI_vocbase_t*) v8g->_vocbase;
  v8g->_vocbase = vocbase;      
      
  v8::Handle<v8::Value> result = _startupLoader->executeGlobalScript(context, "server/version-check.js");
 
  v8g->_vocbase = orig;
  
  return TRI_ObjectToBoolean(result);
}

////////////////////////////////////////////////////////////////////////////////
/// @brief initialize foxx
////////////////////////////////////////////////////////////////////////////////

void VocbaseManager::initializeFoxx (TRI_vocbase_t* vocbase, 
                                     v8::Handle<v8::Context> context) {
  TRI_vocbase_t* orig = 0;
  {
    v8::HandleScope scope;      
    TRI_v8_global_t* v8g = (TRI_v8_global_t*) v8::Isolate::GetCurrent()->GetData();  
    orig = (TRI_vocbase_t*) v8g->_vocbase;
    v8g->_vocbase = vocbase;      
  }
    
  v8::HandleScope scope;      
  TRI_ExecuteJavaScriptString(context,
                              v8::String::New("require(\"internal\").initializeFoxx()"),
                              v8::String::New("initialize foxx"),
                              false);
  {
    v8::HandleScope scope;
    TRI_v8_global_t* v8g = (TRI_v8_global_t*) v8::Isolate::GetCurrent()->GetData();  
    v8g->_vocbase = orig;
  }  
}

////////////////////////////////////////////////////////////////////////////////
/// @brief add an endpoint
////////////////////////////////////////////////////////////////////////////////

bool VocbaseManager::addEndpoint (std::string const& name,
                                  std::vector<std::string> const& databaseNames) {
  
  if (_endpointServer) {
    {
      WRITE_LOCKER(_rwLock);
      _endpoints[name] = databaseNames;
    }

    return _endpointServer->addEndpoint(name);
  }
  
  return false;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief look up vocbase by http request
////////////////////////////////////////////////////////////////////////////////

TRI_vocbase_t* VocbaseManager::lookupVocbaseByHttpRequest (triagens::rest::HttpRequest* request) {
  TRI_vocbase_t* vocbase = 0;

  // get database name from request
  string requestedName = request->databaseName();
 
  if (requestedName.empty()) {
    // no name set in request, use system database name as a fallback
    requestedName = TRI_VOC_SYSTEM_DATABASE;
    vocbase = _vocbase;
  }
  else if (requestedName == TRI_VOC_SYSTEM_DATABASE) {
    vocbase = _vocbase;
  }
 
  READ_LOCKER(_rwLock);
    
  // check if we have a database with the requested name  
  // this only needs to be done for non-system databases
  if (vocbase == 0) {
    map<string, TRI_vocbase_t*>::iterator it = _vocbases.find(requestedName);

    if (it == _vocbases.end()) {
      // requested database does not exist
      return 0;
    }

    vocbase = (*it).second;
  }

  assert(vocbase != 0);

  // check if we have an endpoint
  ConnectionInfo ci = request->connectionInfo();
 
  const string& endpoint = ci.endpoint;

  map<string, vector<string> >::const_iterator it2 = _endpoints.find(endpoint);

  if (it2 == _endpoints.end()) {
    // no user mapping entered for the endpoint. return the requested database
    return vocbase;  
  }

  // we have a user-defined mapping for the endpoint
  const vector<string>& databaseNames = (*it2).second;

  if (databaseNames.size() == 0) {
    // list of database names is specified but empty. this means no-one will get access
    return 0;
  }
    
  // finally check if the requested database is in the list of allowed databases for the endpoint
  vector<string>::const_iterator it3;

  for (it3 = databaseNames.begin(); it3 != databaseNames.end(); ++it3) {
    if (requestedName == *it3) {
      return vocbase;
    }
  }

  // requested database not available for the endpoint
  return 0;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief authenticate a request
////////////////////////////////////////////////////////////////////////////////
        
HttpResponse::HttpResponseCode VocbaseManager::authenticate (TRI_vocbase_t* vocbase, 
                                                             triagens::rest::HttpRequest* request) {
 
  assert(vocbase != 0); 

  std::map<TRI_vocbase_t*, std::map<std::string, std::string> >::iterator mapIter;
  
  bool found;
  char const* auth = request->header("authorization", found);
  
  if (! found || ! TRI_CaseEqualString2(auth, "basic ", 6)) {
    return HttpResponse::UNAUTHORIZED;
  }

  // skip over "basic "
  auth += 6;

  while (*auth == ' ') {
    ++auth;
  }

  {
    READ_LOCKER(_rwLock);

    mapIter = _authCache.find(vocbase);
    if (mapIter == _authCache.end()) {
      // unknown vocbase
      return HttpResponse::NOT_FOUND;
    }      

    map<string, string>::iterator i = mapIter->second.find(auth);

    if (i != mapIter->second.end()) {
      request->setUser(i->second);
      return HttpResponse::OK;
    }
  }

  string up = StringUtils::decodeBase64(auth);    

  std::string::size_type n = up.find(':', 0);
  if (n == std::string::npos || n == 0 || n + 1 > up.size()) {
    LOGGER_TRACE("invalid authentication data found, cannot extract username/password");

    return HttpResponse::BAD;
  }
   
  const string username = up.substr(0, n);
    
  LOGGER_TRACE("checking authentication for user '" << username << "'"); 

  bool res = TRI_CheckAuthenticationAuthInfo2(vocbase, username.c_str(), up.substr(n + 1).c_str());
    
  if (! res) {
    return HttpResponse::UNAUTHORIZED;
  }

  WRITE_LOCKER(_rwLock);

  mapIter = _authCache.find(vocbase);
  if (mapIter == _authCache.end()) {
    // unknown vocbase
    return HttpResponse::UNAUTHORIZED;
  }      

  mapIter->second[auth] = username;
  request->setUser(username);

  // TODO: create a user object for the VocbaseContext
  return HttpResponse::OK;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief reload auth info
////////////////////////////////////////////////////////////////////////////////

bool VocbaseManager::reloadAuthInfo (TRI_vocbase_t* vocbase) {
  std::map<TRI_vocbase_t*, std::map<std::string, std::string> >::iterator mapIter;
  
  {
    WRITE_LOCKER(_rwLock);

    mapIter = _authCache.find(vocbase);
    if (mapIter != _authCache.end()) {
      mapIter->second.clear();
    }

  }
  
  return TRI_ReloadAuthInfo(vocbase);
}

////////////////////////////////////////////////////////////////////////////////
/// @brief get list of database names
////////////////////////////////////////////////////////////////////////////////

vector<TRI_vocbase_t*> VocbaseManager::vocbases () {
  vector<TRI_vocbase_t*> result;
  result.push_back(_vocbase);
  
  std::map<std::string, TRI_vocbase_t*>::iterator i = _vocbases.begin();  
  
  for (; i != _vocbases.end(); ++i) {
    result.push_back(i->second);
  }
  
  return result;
}
        
////////////////////////////////////////////////////////////////////////////////
/// @}
////////////////////////////////////////////////////////////////////////////////

// -----------------------------------------------------------------------------
// --SECTION--                                                       END-OF-FILE
// -----------------------------------------------------------------------------

// Local Variables:
// mode: outline-minor
// outline-regexp: "/// @brief\\|/// {@inheritDoc}\\|/// @addtogroup\\|/// @page\\|// --SECTION--\\|/// @\\}"
// End:
