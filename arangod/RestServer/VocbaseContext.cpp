////////////////////////////////////////////////////////////////////////////////
/// @brief vocbase context
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
/// @author Dr. Frank Celler
/// @author Copyright 2014, ArangoDB GmbH, Cologne, Germany
/// @author Copyright 2011-2013, triAGENS GmbH, Cologne, Germany
////////////////////////////////////////////////////////////////////////////////

#include "VocbaseContext.h"

#include "Basics/MutexLocker.h"
#include "Basics/logging.h"
#include "Basics/tri-strings.h"
#include "Cluster/ServerState.h"
#include "Rest/ConnectionInfo.h"
#include "VocBase/auth.h"
#include "VocBase/server.h"
#include "VocBase/vocbase.h"

using namespace std;
using namespace triagens::basics;
using namespace triagens::arango;
using namespace triagens::rest;

// -----------------------------------------------------------------------------
// --SECTION--                                                 private variables
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @brief sid lock
////////////////////////////////////////////////////////////////////////////////

static Mutex SidLock;

////////////////////////////////////////////////////////////////////////////////
/// @brief sid cache
////////////////////////////////////////////////////////////////////////////////

static unordered_map<string, pair<string, uint64_t>> SidCache;

// -----------------------------------------------------------------------------
// --SECTION--                                             static public methods
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @brief defines a sid
////////////////////////////////////////////////////////////////////////////////

void VocbaseContext::createSid (const string& sid, const string& username) {
  MUTEX_LOCKER(SidLock);

  uint64_t t = static_cast<uint64_t>(TRI_microtime() * 1000.0);
  SidCache[sid] = make_pair(username, t);
}

////////////////////////////////////////////////////////////////////////////////
/// @brief clears a sid
////////////////////////////////////////////////////////////////////////////////

void VocbaseContext::clearSid (const string& sid) {
  MUTEX_LOCKER(SidLock);

  SidCache.erase(sid);
}

////////////////////////////////////////////////////////////////////////////////
/// @brief gets the last access time
////////////////////////////////////////////////////////////////////////////////

uint64_t VocbaseContext::accessSid (const string& sid) {
  MUTEX_LOCKER(SidLock);

  auto it = SidCache.find(sid);

  if (it == SidCache.end()) {
    return 0;
  }

  return it->second.second;
}

// -----------------------------------------------------------------------------
// --SECTION--                                              class VocbaseContext
// -----------------------------------------------------------------------------

// -----------------------------------------------------------------------------
// --SECTION--                                      constructors and destructors
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @brief constructor
////////////////////////////////////////////////////////////////////////////////

VocbaseContext::VocbaseContext (HttpRequest* request,
                                TRI_server_t* server,
                                TRI_vocbase_t* vocbase) :
  RequestContext(request),
  _server(server),
  _vocbase(vocbase) {

  TRI_ASSERT(_server != nullptr);
  TRI_ASSERT(_vocbase != nullptr);
}

////////////////////////////////////////////////////////////////////////////////
/// @brief destructor
////////////////////////////////////////////////////////////////////////////////

VocbaseContext::~VocbaseContext () {
  TRI_ReleaseVocBase(_vocbase);
}

// -----------------------------------------------------------------------------
// --SECTION--                                                    public methods
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @brief whether or not to use special cluster authentication
////////////////////////////////////////////////////////////////////////////////

bool VocbaseContext::useClusterAuthentication () const {
  if (ServerState::instance()->isDBServer()) {
    return true;
  }

  string s(_request->requestPath());

  if (ServerState::instance()->isCoordinator() &&
      (s == "/_api/shard-comm" || s == "/_admin/shutdown")) {
    return true;
  }

  return false;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief return authentication realm
////////////////////////////////////////////////////////////////////////////////

const char* VocbaseContext::getRealm () const {
  if (_vocbase == nullptr) {
    return nullptr;
  }

  return _vocbase->_name;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief checks the authentication
////////////////////////////////////////////////////////////////////////////////

HttpResponse::HttpResponseCode VocbaseContext::authenticate () {
  TRI_ASSERT(_vocbase != nullptr);

  if (! _vocbase->_settings.requireAuthentication) {
    // no authentication required at all
    return HttpResponse::OK;
  }

#ifdef TRI_HAVE_LINUX_SOCKETS
  // check if we need to run authentication for this type of
  // endpoint
  ConnectionInfo const& ci = _request->connectionInfo();

  if (ci.endpointType == Endpoint::DOMAIN_UNIX &&
      ! _vocbase->_settings.requireAuthenticationUnixSockets) {
    // no authentication required for unix socket domain connections
    return HttpResponse::OK;
  }
#endif

  const char* path = _request->requestPath();

  if (_vocbase->_settings.authenticateSystemOnly) {
    // authentication required, but only for /_api, /_admin etc.

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

  if (TRI_IsPrefixString(path, "/_open/") || TRI_IsPrefixString(path, "/_admin/aardvark/") || TRI_EqualString(path, "/")) {
    return HttpResponse::OK;
  }

  // .............................................................................
  // authentication required
  // .............................................................................

  bool found;
  char cn[4096];

  cn[0] = '\0';
  strncat(cn, "arango_sid_", 11);
  strncat(cn + 11, _vocbase->_name, sizeof(cn) - 12);

  // extract the sid
  const char* sid = _request->cookieValue(cn, found);

  if (found) {
    MUTEX_LOCKER(SidLock);

    string name = _vocbase->_name;
    name += "/";
    name += sid;
    
    auto it = SidCache.find(name);

    if (it != SidCache.end()) {
      _request->setUser(it->second.first);
      uint64_t t = static_cast<uint64_t>(TRI_microtime() * 1000.0);
      it->second.second = t;
      return HttpResponse::OK;
    }
  }

  const char* auth = _request->header("authorization", found);

  if (! found || ! TRI_CaseEqualString2(auth, "basic ", 6)) {
    return HttpResponse::UNAUTHORIZED;
  }

  // skip over "basic "
  auth += 6;

  while (*auth == ' ') {
    ++auth;
  }

  if (useClusterAuthentication()) {
    string const expected = ServerState::instance()->getAuthentication();

    if (expected.substr(6) != string(auth)) {
      return HttpResponse::UNAUTHORIZED;
    }

    string const up = StringUtils::decodeBase64(auth);
    std::string::size_type n = up.find(':', 0);

    if (n == std::string::npos || n == 0 || n + 1 > up.size()) {
      LOG_TRACE("invalid authentication data found, cannot extract username/password");

      return HttpResponse::BAD;
    }

    string const username = up.substr(0, n);
    _request->setUser(username);

    return HttpResponse::OK;
  }

  // look up the info in the cache first
  bool mustChange;
  char* cached = TRI_CheckCacheAuthInfo(_vocbase, auth, &mustChange);
  string username;

  // found a cached entry, access must be granted
  if (cached != 0) {
    username = string(cached);
    TRI_Free(TRI_CORE_MEM_ZONE, cached);
  }

  // no entry found in cache, decode the basic auth info and look it up
  else {
    string const up = StringUtils::decodeBase64(auth);
    std::string::size_type n = up.find(':', 0);

    if (n == std::string::npos || n == 0 || n + 1 > up.size()) {
      LOG_TRACE("invalid authentication data found, cannot extract username/password");
      return HttpResponse::BAD;
    }

    username = up.substr(0, n);

    LOG_TRACE("checking authentication for user '%s'", username.c_str());
    bool res = TRI_CheckAuthenticationAuthInfo(
                 _vocbase, auth, username.c_str(), up.substr(n + 1).c_str(), &mustChange);

    if (! res) {
      return HttpResponse::UNAUTHORIZED;
    }
  }

  // TODO: create a user object for the VocbaseContext
  _request->setUser(username);

  if (mustChange) {
    if ((_request->requestType() == HttpRequest::HTTP_REQUEST_PUT
         || _request->requestType() == HttpRequest::HTTP_REQUEST_PATCH)
        && TRI_EqualString2(_request->requestPath(), "/_api/user/", 11)) {
      return HttpResponse::OK;
    }

    return HttpResponse::FORBIDDEN;
  }

  return HttpResponse::OK;
}

// -----------------------------------------------------------------------------
// --SECTION--                                                       END-OF-FILE
// -----------------------------------------------------------------------------

// Local Variables:
// mode: outline-minor
// outline-regexp: "/// @brief\\|/// {@inheritDoc}\\|/// @page\\|// --SECTION--\\|/// @\\}"
// End:
