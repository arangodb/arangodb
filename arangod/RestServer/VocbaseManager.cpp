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
/// @brief add the context to a request
////////////////////////////////////////////////////////////////////////////////
/*
bool VocbaseManager::setRequestContext(triagens::rest::HttpRequest* request) {
  return true;

  // TODO
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
  */

// -----------------------------------------------------------------------------
// --SECTION--                                                            public
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @addtogroup ArangoDB
/// @{
////////////////////////////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////////////////////////
/// @brief grab an exclusive lock for creation
////////////////////////////////////////////////////////////////////////////////
/*
void VocbaseManager::lockCreation () {
  _creationLock.lock();
}

////////////////////////////////////////////////////////////////////////////////
/// @brief release the exclusive lock for creation
////////////////////////////////////////////////////////////////////////////////

void VocbaseManager::unlockCreation () {
  _creationLock.unlock();
}
*/
/*
////////////////////////////////////////////////////////////////////////////////
/// @brief add user vocbase
////////////////////////////////////////////////////////////////////////////////

void VocbaseManager::addUserVocbase (TRI_vocbase_t* vocbase) {
  {
    WRITE_LOCKER(_rwLock);
    _databases[vocbase->_name] = vocbase;  
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
  
  std::map<std::string, TRI_vocbase_t*>::iterator i = _databases.begin();  
  for (; i != _databases.end(); ++i) {
    TRI_vocbase_t* vocbase = (*i).second;

    TRI_DestroyVocBase(vocbase, 0);
    TRI_Free(TRI_UNKNOWN_MEM_ZONE, vocbase);
  }
  
  _databases.clear();  
}
*/

////////////////////////////////////////////////////////////////////////////////
/// @brief look up vocbase by name
////////////////////////////////////////////////////////////////////////////////

  /*
TRI_vocbase_t* VocbaseManager::lookupVocbaseByName (string const& name) {
  if (name == TRI_VOC_SYSTEM_DATABASE) {
    return _vocbase;
  }
  
  READ_LOCKER(_rwLock);
  map<string, TRI_vocbase_s*>::iterator find = _databases.find(name);
  if (find != _databases.end()) {
    return find->second;
  } 
  
  return 0;  
}
  */

////////////////////////////////////////////////////////////////////////////////
/// @brief check if name and path are not used
////////////////////////////////////////////////////////////////////////////////

  /*
int VocbaseManager::canAddVocbase (std::string const& name, 
                                   std::string const& path,
                                   bool checkPath) {
  return false;
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
  std::map<std::string, TRI_vocbase_t*>::iterator i = _databases.begin();
  for (; i != _databases.end(); ++i) {
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
  */

////////////////////////////////////////////////////////////////////////////////
/// @brief check if a collection name is valid
////////////////////////////////////////////////////////////////////////////////
/*
bool VocbaseManager::isValidName (std::string const& name) const {
  return TRI_IsAllowedDatabaseName(false, name.c_str());
}
*/
////////////////////////////////////////////////////////////////////////////////
/// @brief remove a vocbase by name
////////////////////////////////////////////////////////////////////////////////

  /*
int VocbaseManager::deleteVocbase (string const& name) {
  if (name == TRI_VOC_SYSTEM_DATABASE) {
    // cannot delete _system database
    return TRI_ERROR_FORBIDDEN; 
  }
 
  TRI_vocbase_t* vocbase = 0;

  { 
    WRITE_LOCKER(_rwLock);

    map<string, TRI_vocbase_s*>::iterator find = _databases.find(name);
    if (find == _databases.end()) {
      // database does not exist
      return TRI_ERROR_ARANGO_DOCUMENT_NOT_FOUND;
    }
  
    // ok, we found it
    vocbase = (*find).second;

    // remove from the list
    _databases.erase(name);
  }  

  assert(vocbase != 0);
  
  const string path = string(vocbase->_path);
  LOGGER_INFO("removing database '" << name << "', path '" << path << "'");

  // destroy the vocbase, but not its collections 
  // there might be JavaScript objects referencing the collections somehow,
  // so we must not free them yet
  // putting the collections in this vector will allow us freeing them later
  TRI_DestroyVocBase(vocbase, _freeCollections);
  // same for the vocbase
  _freeVocbases.push_back(vocbase);

  // ok, now delete the data in the file system
  int res = TRI_RemoveDirectory(path.c_str());

  return res;
  return TRI_ERROR_NO_ERROR;
}
  */

/*
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
  
  READ_LOCKER(_rwLock);

  std::map<std::string, TRI_vocbase_t*>::iterator i = _databases.begin();  
  
  for (; i != _databases.end(); ++i) {
    result.push_back(i->second);
  }
  
  return result;
}
*/        
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
