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
// --SECTION--                                                            public
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @addtogroup ArangoDB
/// @{
////////////////////////////////////////////////////////////////////////////////

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
*/

////////////////////////////////////////////////////////////////////////////////
/// @brief authenticate a request
////////////////////////////////////////////////////////////////////////////////

/*        
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
