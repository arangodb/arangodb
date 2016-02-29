GeneralResponse::VstreamResponseCode VocbaseContext::authenticateVstream() {
  TRI_ASSERT(_vocbase != nullptr);

  if (!_vocbase->_settings.requireAuthentication) {
    // no authentication required at all
    return GeneralResponse::VSTREAM_OK;
  }

#ifdef TRI_HAVE_LINUX_SOCKETS
  // check if we need to run authentication for this type of
  // endpoint
  ConnectionInfo const& ci = _request->connectionInfo();

  if (ci.endpointType == Endpoint::DOMAIN_UNIX &&
      !_vocbase->_settings.requireAuthenticationUnixSockets) {
    // no authentication required for unix socket domain connections
    return GeneralResponse::VSTREAM_OK;
  }
#endif

  char const* path = _request->requestPath();

  if (_vocbase->_settings.authenticateSystemOnly) {
    // authentication required, but only for /_api, /_admin etc.

    if (path != nullptr) {
      // check if path starts with /_
      if (*path != '/') {
        return GeneralResponse::VSTREAM_OK;
      }
      if (*path != '\0' && *(path + 1) != '_') {
        return GeneralResponse::VSTREAM_OK;
      }
    }
  }

  if (TRI_IsPrefixString(path, "/_open/") ||
      TRI_IsPrefixString(path, "/_admin/aardvark/") ||
      TRI_EqualString(path, "/")) {
    return GeneralResponse::VSTREAM_OK;
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
  char const* sid = _request->cookieValue(cn, found);

  if (found) {
    MUTEX_LOCKER(SidLock);

    auto it = SidCache.find(_vocbase->_name);

    if (it != SidCache.end()) {
      auto& sids = (*it).second;
      auto it2 = sids.find(sid);

      if (it2 != sids.end()) {
        _request->setUser((*it2).second.first);
        double const now = TRI_microtime() * 1000.0;
        // fetch last access date of session
        double const lastAccess = (*it2).second.second;

        // check if session has expired
        if (lastAccess + (ServerSessionTtl * 1000.0) < now) {
          // session has expired
          sids.erase(sid);
          return GeneralResponse::VSTREAM_UNAUTHORIZED;
        }

        (*it2).second.second = now;
        return GeneralResponse::VSTREAM_OK;
      }
    }

    // no cookie found. fall-through to regular HTTP authentication
  }

  char const* auth = _request->header("authorization", found);

  if (!found || !TRI_CaseEqualString(auth, "basic ", 6)) {
    return GeneralResponse::VSTREAM_UNAUTHORIZED;
  }

  // skip over "basic "
  auth += 6;

  while (*auth == ' ') {
    ++auth;
  }

  if (useClusterAuthentication()) {
    std::string const expected = ServerState::instance()->getAuthentication();

    if (expected.substr(6) != std::string(auth)) {
      return GeneralResponse::VSTREAM_UNAUTHORIZED;
    }

    std::string const up = StringUtils::decodeBase64(auth);
    std::string::size_type n = up.find(':', 0);

    if (n == std::string::npos || n == 0 || n + 1 > up.size()) {
      LOG_TRACE(
          "invalid authentication data found, cannot extract "
          "username/password");

      return GeneralResponse::VSTREAM_BAD;
    }

    std::string const username = up.substr(0, n);
    _request->setUser(username);

    return GeneralResponse::VSTREAM_OK;
  }

  // look up the info in the cache first
  bool mustChange;
  char* cached = TRI_CheckCacheAuthInfo(_vocbase, auth, &mustChange);
  std::string username;

  // found a cached entry, access must be granted
  if (cached != 0) {
    username = std::string(cached);
    TRI_Free(TRI_CORE_MEM_ZONE, cached);
  }

  // no entry found in cache, decode the basic auth info and look it up
  else {
    std::string const up = StringUtils::decodeBase64(auth);
    std::string::size_type n = up.find(':', 0);

    if (n == std::string::npos || n == 0 || n + 1 > up.size()) {
      LOG_TRACE(
          "invalid authentication data found, cannot extract "
          "username/password");
      return GeneralResponse::VSTREAM_BAD;
    }

    username = up.substr(0, n);

    LOG_TRACE("checking authentication for user '%s'", username.c_str());
    bool res =
        TRI_CheckAuthenticationAuthInfo(_vocbase, auth, username.c_str(),
                                        up.substr(n + 1).c_str(), &mustChange);

    if (!res) {
      return GeneralResponse::VSTREAM_UNAUTHORIZED;
    }
  }

  // TODO: create a user object for the VocbaseContext
  _request->setUser(username);

  if (mustChange) {
    if ((_request->requestType() == GeneralRequest::HTTP_REQUEST_PUT ||
         _request->requestType() == GeneralRequest::HTTP_REQUEST_PATCH || 
         _request->requestType() == GeneralRequest::VSTREAM_REQUEST_PUT ||
         _request->requestType() == GeneralRequest::VSTREAM_REQUEST_PATCH ) &&
        TRI_EqualString(_request->requestPath(), "/_api/user/", 11)) {
      return GeneralResponse::VSTREAM_OK;
    }

    return GeneralResponse::VSTREAM_FORBIDDEN;
  }

  return GeneralResponse::VSTREAM_OK;
}

GeneralResponse::VstreamResponseCode VocbaseContext::authenticateVstream() {
  TRI_ASSERT(_vocbase != nullptr);

  if (!_vocbase->_settings.requireAuthentication) {
    // no authentication required at all
    return GeneralResponse::VSTREAM_OK;
  }

#ifdef TRI_HAVE_LINUX_SOCKETS
  // check if we need to run authentication for this type of
  // endpoint
  ConnectionInfo const& ci = _request->connectionInfo();

  if (ci.endpointType == Endpoint::DOMAIN_UNIX &&
      !_vocbase->_settings.requireAuthenticationUnixSockets) {
    // no authentication required for unix socket domain connections
    return GeneralResponse::VSTREAM_OK;
  }
#endif

  char const* path = _request->requestPath();

  if (_vocbase->_settings.authenticateSystemOnly) {
    // authentication required, but only for /_api, /_admin etc.

    if (path != nullptr) {
      // check if path starts with /_
      if (*path != '/') {
        return GeneralResponse::VSTREAM_OK;
      }
      if (*path != '\0' && *(path + 1) != '_') {
        return GeneralResponse::VSTREAM_OK;
      }
    }
  }

  if (TRI_IsPrefixString(path, "/_open/") ||
      TRI_IsPrefixString(path, "/_admin/aardvark/") ||
      TRI_EqualString(path, "/")) {
    return GeneralResponse::VSTREAM_OK;
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
  char const* sid = _request->cookieValue(cn, found);

  if (found) {
    MUTEX_LOCKER(SidLock);

    auto it = SidCache.find(_vocbase->_name);

    if (it != SidCache.end()) {
      auto& sids = (*it).second;
      auto it2 = sids.find(sid);

      if (it2 != sids.end()) {
        _request->setUser((*it2).second.first);
        double const now = TRI_microtime() * 1000.0;
        // fetch last access date of session
        double const lastAccess = (*it2).second.second;

        // check if session has expired
        if (lastAccess + (ServerSessionTtl * 1000.0) < now) {
          // session has expired
          sids.erase(sid);
          return GeneralResponse::VSTREAM_UNAUTHORIZED;
        }

        (*it2).second.second = now;
        return GeneralResponse::VSTREAM_OK;
      }
    }

    // no cookie found. fall-through to regular VelocyStream authentication
  }

  char const* auth = _request->header("authorization", found);

  if (!found || !TRI_CaseEqualString(auth, "basic ", 6)) {
    return GeneralResponse::VSTREAM_UNAUTHORIZED;
  }

  // skip over "basic "
  auth += 6;

  while (*auth == ' ') {
    ++auth;
  }

  if (useClusterAuthentication()) {
    std::string const expected = ServerState::instance()->getAuthentication();

    if (expected.substr(6) != std::string(auth)) {
      return GeneralResponse::VSTREAM_UNAUTHORIZED;
    }

    std::string const up = StringUtils::decodeBase64(auth);
    std::string::size_type n = up.find(':', 0);

    if (n == std::string::npos || n == 0 || n + 1 > up.size()) {
      LOG_TRACE(
          "invalid authentication data found, cannot extract "
          "username/password");

      return GeneralResponse::VSTREAM_BAD;
    }

    std::string const username = up.substr(0, n);
    _request->setUser(username);

    return GeneralResponse::VSTREAM_OK;
  }

  // look up the info in the cache first
  bool mustChange;
  char* cached = TRI_CheckCacheAuthInfo(_vocbase, auth, &mustChange);
  std::string username;

  // found a cached entry, access must be granted
  if (cached != 0) {
    username = std::string(cached);
    TRI_Free(TRI_CORE_MEM_ZONE, cached);
  }

  // no entry found in cache, decode the basic auth info and look it up
  else {
    std::string const up = StringUtils::decodeBase64(auth);
    std::string::size_type n = up.find(':', 0);

    if (n == std::string::npos || n == 0 || n + 1 > up.size()) {
      LOG_TRACE(
          "invalid authentication data found, cannot extract "
          "username/password");
      return GeneralResponse::VSTREAM_BAD;
    }

    username = up.substr(0, n);

    LOG_TRACE("checking authentication for user '%s'", username.c_str());
    bool res =
        TRI_CheckAuthenticationAuthInfo(_vocbase, auth, username.c_str(),
                                        up.substr(n + 1).c_str(), &mustChange);

    if (!res) {
      return GeneralResponse::VSTREAM_UNAUTHORIZED;
    }
  }

  // TODO: create a user object for the VocbaseContext
  _request->setUser(username);

  if (mustChange) {
    if ((_request->requestType() == GeneralRequest::VSTREAM_REQUEST_PUT ||
         _request->requestType() == GeneralRequest::VSTREAM_REQUEST_PATCH ) &&
        TRI_EqualString(_request->requestPath(), "/_api/user/", 11)) {
      return GeneralResponse::VSTREAM_OK;
    }

    return GeneralResponse::VSTREAM_FORBIDDEN;
  }

  return GeneralResponse::VSTREAM_OK;
}

GeneralResponse::ResponseCode VocbaseContext::authenticate() {
  TRI_ASSERT(_vocbase != nullptr);

  if (!_vocbase->_settings.requireAuthentication) {
    // no authentication required at all
    return GeneralResponse::VSTREAM_OK;
  }

#ifdef TRI_HAVE_LINUX_SOCKETS
  // check if we need to run authentication for this type of
  // endpoint
  ConnectionInfo const& ci = _request->connectionInfo();

  if (ci.endpointType == Endpoint::DOMAIN_UNIX &&
      !_vocbase->_settings.requireAuthenticationUnixSockets) {
    // no authentication required for unix socket domain connections
    return GeneralResponse::VSTREAM_OK;
  }
#endif

  char const* path = _request->requestPath();

  if (_vocbase->_settings.authenticateSystemOnly) {
    // authentication required, but only for /_api, /_admin etc.

    if (path != nullptr) {
      // check if path starts with /_
      if (*path != '/') {
        return GeneralResponse::VSTREAM_OK;
      }
      if (*path != '\0' && *(path + 1) != '_') {
        return GeneralResponse::VSTREAM_OK;
      }
    }
  }

  if (TRI_IsPrefixString(path, "/_open/") ||
      TRI_IsPrefixString(path, "/_admin/aardvark/") ||
      TRI_EqualString(path, "/")) {
    return GeneralResponse::VSTREAM_OK;
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
  char const* sid = _request->cookieValue(cn, found);

  if (found) {
    MUTEX_LOCKER(SidLock);

    auto it = SidCache.find(_vocbase->_name);

    if (it != SidCache.end()) {
      auto& sids = (*it).second;
      auto it2 = sids.find(sid);

      if (it2 != sids.end()) {
        _request->setUser((*it2).second.first);
        double const now = TRI_microtime() * 1000.0;
        // fetch last access date of session
        double const lastAccess = (*it2).second.second;

        // check if session has expired
        if (lastAccess + (ServerSessionTtl * 1000.0) < now) {
          // session has expired
          sids.erase(sid);
          return GeneralResponse::VSTREAM_UNAUTHORIZED;
        }

        (*it2).second.second = now;
        return GeneralResponse::VSTREAM_OK;
      }
    }

    // no cookie found. fall-through to regular VelocyStream authentication
  }

  char const* auth = _request->header("authorization", found);

  if (!found || !TRI_CaseEqualString(auth, "basic ", 6)) {
    return GeneralResponse::VSTREAM_UNAUTHORIZED;
  }

  // skip over "basic "
  auth += 6;

  while (*auth == ' ') {
    ++auth;
  }

  if (useClusterAuthentication()) {
    std::string const expected = ServerState::instance()->getAuthentication();

    if (expected.substr(6) != std::string(auth)) {
      return GeneralResponse::VSTREAM_UNAUTHORIZED;
    }

    std::string const up = StringUtils::decodeBase64(auth);
    std::string::size_type n = up.find(':', 0);

    if (n == std::string::npos || n == 0 || n + 1 > up.size()) {
      LOG_TRACE(
          "invalid authentication data found, cannot extract "
          "username/password");

      return GeneralResponse::VSTREAM_BAD;
    }

    std::string const username = up.substr(0, n);
    _request->setUser(username);

    return GeneralResponse::VSTREAM_OK;
  }

  // look up the info in the cache first
  bool mustChange;
  char* cached = TRI_CheckCacheAuthInfo(_vocbase, auth, &mustChange);
  std::string username;

  // found a cached entry, access must be granted
  if (cached != 0) {
    username = std::string(cached);
    TRI_Free(TRI_CORE_MEM_ZONE, cached);
  }

  // no entry found in cache, decode the basic auth info and look it up
  else {
    std::string const up = StringUtils::decodeBase64(auth);
    std::string::size_type n = up.find(':', 0);

    if (n == std::string::npos || n == 0 || n + 1 > up.size()) {
      LOG_TRACE(
          "invalid authentication data found, cannot extract "
          "username/password");
      return GeneralResponse::VSTREAM_BAD;
    }

    username = up.substr(0, n);

    LOG_TRACE("checking authentication for user '%s'", username.c_str());
    bool res =
        TRI_CheckAuthenticationAuthInfo(_vocbase, auth, username.c_str(),
                                        up.substr(n + 1).c_str(), &mustChange);

    if (!res) {
      return GeneralResponse::VSTREAM_UNAUTHORIZED;
    }
  }

  // TODO: create a user object for the VocbaseContext
  _request->setUser(username);

  if (mustChange) {
    if ((_request->requestType() == GeneralRequest::VSTREAM_REQUEST_PUT ||
         _request->requestType() == GeneralRequest::VSTREAM_REQUEST_PATCH ) &&
        TRI_EqualString(_request->requestPath(), "/_api/user/", 11)) {
      return GeneralResponse::VSTREAM_OK;
    }

    return GeneralResponse::VSTREAM_FORBIDDEN;
  }

  return GeneralResponse::VSTREAM_OK;
}