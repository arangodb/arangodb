////////////////////////////////////////////////////////////////////////////////
/// @brief vocbase context
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

#include "VocbaseContext.h"

#include "BasicsC/common.h"
#include "BasicsC/logging.h"
#include "BasicsC/tri-strings.h"
#include "VocBase/auth.h"
#include "VocBase/server.h"
#include "VocBase/vocbase.h"

using namespace std;
using namespace triagens::basics;
using namespace triagens::arango;
using namespace triagens::rest;


// -----------------------------------------------------------------------------
// --SECTION--                                                class ArangoServer
// -----------------------------------------------------------------------------

// -----------------------------------------------------------------------------
// --SECTION--                                      constructors and destructors
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @addtogroup ArangoDB
/// @{
////////////////////////////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////////////////////////
/// @brief constructor
////////////////////////////////////////////////////////////////////////////////

VocbaseContext::VocbaseContext (HttpRequest* request,
                                TRI_server_t* server, 
                                TRI_vocbase_t* vocbase,
                                bool releaseDatabase) :
  RequestContext(request),
  _server(server),
  _vocbase(vocbase),
  _releaseDatabase(releaseDatabase) {

  assert(_server != 0);
  assert(_vocbase != 0);
}

////////////////////////////////////////////////////////////////////////////////
/// @brief destructor
////////////////////////////////////////////////////////////////////////////////

VocbaseContext::~VocbaseContext () {
  if (_releaseDatabase) {
    TRI_ReleaseVocBase(_vocbase);
  }
}

////////////////////////////////////////////////////////////////////////////////
/// @}
////////////////////////////////////////////////////////////////////////////////

// -----------------------------------------------------------------------------
// --SECTION--                                                    public methods
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @addtogroup ArangoDB
/// @{
////////////////////////////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////////////////////////
/// @brief checks the authentication
////////////////////////////////////////////////////////////////////////////////
        
HttpResponse::HttpResponseCode VocbaseContext::authenticate () {
  assert(_vocbase != 0);

  if (! _vocbase->_settings.requireAuthentication) {
    // no authentication required at all
    return HttpResponse::OK;
  }

  if (_vocbase->_settings.authenticateSystemOnly) {
    // authentication required, but only for /_api, /_admin etc.
    const char* path = _request->requestPath();

    if (path != 0) {
      // check if path starts with /_
      if (*path != '/') {
        return HttpResponse::OK;
      }
      if (*path != '\0' && *(path + 1) != '_') {
        return HttpResponse::OK;
      }
    }
  }
  
  // authentication required
  
  
  bool found;
  char const* auth = _request->header("authorization", found);
  
  if (! found || ! TRI_CaseEqualString2(auth, "basic ", 6)) {
    return HttpResponse::UNAUTHORIZED;
  }

  // skip over "basic "
  auth += 6;

  while (*auth == ' ') {
    ++auth;
  }

  string up = StringUtils::decodeBase64(auth);    

  std::string::size_type n = up.find(':', 0);
  if (n == std::string::npos || n == 0 || n + 1 > up.size()) {
    LOG_TRACE("invalid authentication data found, cannot extract username/password");

    return HttpResponse::BAD;
  }
   
  const string username = up.substr(0, n);
    
  LOG_TRACE("checking authentication for user '%s'", username.c_str()); 

  // TODO FIXME: re-add caching

  bool res = TRI_CheckAuthenticationAuthInfo(_vocbase, username.c_str(), up.substr(n + 1).c_str());
    
  if (! res) {
    return HttpResponse::UNAUTHORIZED;
  }

  // TODO: create a user object for the VocbaseContext
  _request->setUser(username);

  return HttpResponse::OK;
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
