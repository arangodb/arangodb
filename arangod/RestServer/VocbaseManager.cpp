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

#include "build.h"

#include "Logger/Logger.h"
#include "Rest/ConnectionInfo.h"
#include "Basics/StringUtils.h"
#include "RestServer/VocbaseContext.h"
#include "BasicsC/tri-strings.h"
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
  TRI_vocbase_t* vb = VocbaseManager::manager.lookupVocbaseByHttpRequest(request);
  
  triagens::arango::VocbaseContext* vc = new triagens::arango::VocbaseContext(request, &VocbaseManager::manager);
  
  vc->setVocbase(vb);
  
  request->addRequestContext(vc);
  
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
  
  if (true) { //(vocbase->_needAuth) {
    TRI_ReloadAuthInfo(vocbase);
  }
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

bool VocbaseManager::canAddVocbase (std::string const& name, std::string const& path) {
  // loop over all vocbases and check name and path
  
  READ_LOCKER(_rwLock);
  
  // system vocbase
  if (name == string(_vocbase->_name)) {
    return false;
  }
  if (path == string(_vocbase->_path)) {
    return false;
  }
  
  // user vocbases
  std::map<std::string, TRI_vocbase_t*>::iterator i = _vocbases.begin();
  for (; i != _vocbases.end(); ++i) {
    TRI_vocbase_t* vocbase = i->second;
    if (name == string(vocbase->_name)) {
      return false;
    }
    if (path == string(vocbase->_path)) {
      return false;
    }
  }
  
  return true;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief run version check
/// @return bool             returns false if the version check fails
////////////////////////////////////////////////////////////////////////////////

bool VocbaseManager::runVersionCheck (TRI_vocbase_t* vocbase, v8::Handle<v8::Context> context) {
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

void VocbaseManager::initializeFoxx (TRI_vocbase_t* vocbase, v8::Handle<v8::Context> context) {
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

bool VocbaseManager::addEndpoint (std::string const& name) {
  
  if (_endpointServer) {
    return _endpointServer->addEndpoint(name);
  }
  
  return false;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief look vocbase by http request
////////////////////////////////////////////////////////////////////////////////

TRI_vocbase_t* VocbaseManager::lookupVocbaseByHttpRequest (
                                        triagens::rest::HttpRequest* request) {
  ConnectionInfo ci = request->connectionInfo();

  string prefix = (ci.serverPort > 0) 
          ? "tcp://" + triagens::basics::StringUtils::tolower(ci.serverAddress) 
          + ":" + basics::StringUtils::itoa(ci.serverPort)
          : "unix:///localhost";

  return lookupVocbaseByPrefix(prefix);
}

////////////////////////////////////////////////////////////////////////////////
/// @brief look vocbase by prefix
////////////////////////////////////////////////////////////////////////////////

TRI_vocbase_t* VocbaseManager::lookupVocbaseByPrefix (
                                                    std::string const& prefix) {
  READ_LOCKER(_rwLock);
  map<string, TRI_vocbase_s*>::iterator find = _prefix2Vocbases.find(prefix);
  if (find != _prefix2Vocbases.end()) {
    return find->second;
  }
  
  // return default
  return _vocbase;  
}

////////////////////////////////////////////////////////////////////////////////
/// @brief add prefix to database mapping
////////////////////////////////////////////////////////////////////////////////

void VocbaseManager::addPrefixMapping (std::string const& prefix, 
                                       std::string const& name) {  
  TRI_vocbase_t* vocbase = lookupVocbaseByName(name);
  
  if (vocbase) {
    LOGGER_INFO("added prefix mapping '" << prefix << "' -> '" << name << "'");    
    
    WRITE_LOCKER(_rwLock);
    _prefix2Vocbases[triagens::basics::StringUtils::tolower(prefix)] = vocbase;    
  }  
  
}

////////////////////////////////////////////////////////////////////////////////
/// @brief authenticate a request
////////////////////////////////////////////////////////////////////////////////
        
bool VocbaseManager::authenticate (TRI_vocbase_t* vocbase, 
                                   triagens::rest::HttpRequest* request) {
  
  if (!vocbase) {
    // unknown vocbase
     return false;
  }
  
  std::map<TRI_vocbase_t*, std::map<std::string, std::string> >::iterator mapIter;
  
  bool found;
  char const* auth = request->header("authorization", found);

  if (found) {
    if (! TRI_CaseEqualString2(auth, "basic ", 6)) {
      return false;
    }

    auth += 6;

    while (*auth == ' ') {
      ++auth;
    }

    {
      READ_LOCKER(_rwLock);
      
      mapIter = _authCache.find(vocbase);
      if (mapIter == _authCache.end()) {
        // unknown vocbase
        return false;
      }      

      map<string,string>::iterator i = mapIter->second.find(auth);

      if (i != mapIter->second.end()) {
        request->setUser(i->second);
        return true;
      }
    }

    string up = StringUtils::decodeBase64(auth);    
//    vector<string> split = StringUtils::split(up, ":");
//
//    if (split.size() != 2) {
//      return false;
//    }
//
//    bool res = TRI_CheckAuthenticationAuthInfo2(vocbase, split[0].c_str(), split[1].c_str());

    std::string::size_type n = up.find(':', 0);
    if (n == std::string::npos || n == 0 || n+1 > up.size()) {
      return false;
    }
    
    bool res = TRI_CheckAuthenticationAuthInfo2(vocbase, up.substr(0, n).c_str(), up.substr(n+1).c_str());
    
    if (res) {
      WRITE_LOCKER(_rwLock);

      mapIter = _authCache.find(vocbase);
      if (mapIter == _authCache.end()) {
        // unknown vocbase
        return false;
      }      
      
      mapIter->second[auth] = up.substr(0, n);
      request->setUser(up.substr(0, n));
      
      // TODO: create a user object for the VocbaseContext
    }

    return res;
  }

  return false;  
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
