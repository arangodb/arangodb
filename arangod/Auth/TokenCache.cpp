////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2014-2018 ArangoDB GmbH, Cologne, Germany
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
/// @author Simon Grätzer
/// @author Dr. Frank Celler
////////////////////////////////////////////////////////////////////////////////

#include "TokenCache.h"

#include "Agency/AgencyComm.h"
#include "Auth/Handler.h"
#include "Basics/ReadLocker.h"
#include "Basics/ReadUnlocker.h"
#include "Basics/VelocyPackHelper.h"
#include "Basics/WriteLocker.h"
#include "Basics/tri-strings.h"
#include "Cluster/ServerState.h"
#include "GeneralServer/AuthenticationFeature.h"
#include "Logger/Logger.h"
#include "Ssl/SslInterface.h"

#include <velocypack/Builder.h>
#include <velocypack/Collection.h>
#include <velocypack/Iterator.h>
#include <velocypack/velocypack-aliases.h>

using namespace arangodb;
using namespace arangodb::basics;
using namespace arangodb::velocypack;
using namespace arangodb::rest;

auth::TokenCache::TokenCache(auth::UserManager* um, double timeout)
    : _userManager(um),
      _authTimeout(timeout),
      _basicCacheVersion(0),
      _jwtSecret(""),
      _jwtCache(16384) {}

auth::TokenCache::~TokenCache() {
  // properly clear structs while using the appropriate locks
  {
    WRITE_LOCKER(readLocker, _basicLock);
    _basicCache.clear();
  }
  {
    WRITE_LOCKER(writeLocker, _jwtLock);
    _jwtCache.clear();
  }
}

void auth::TokenCache::setJwtSecret(std::string const& jwtSecret) {
  WRITE_LOCKER(writeLocker, _jwtLock);
  LOG_TOPIC(DEBUG, Logger::AUTHENTICATION)
      << "Setting jwt secret " << Logger::BINARY(jwtSecret.data(), jwtSecret.size());
  _jwtSecret = jwtSecret;
  _jwtCache.clear();
  generateJwtToken();
}

std::string auth::TokenCache::jwtSecret() const {
  READ_LOCKER(writeLocker, _jwtLock);
  return _jwtSecret; // intentional copy
}

// public called from HttpCommTask.cpp and VstCommTask.cpp
// should only lock if required, otherwise we will serialize all
// requests whether we need to or not
auth::TokenCache::Entry auth::TokenCache::checkAuthentication(
    AuthenticationMethod authType, std::string const& secret) {
  switch (authType) {
    case AuthenticationMethod::BASIC:
      return checkAuthenticationBasic(secret);

    case AuthenticationMethod::JWT:
      return checkAuthenticationJWT(secret);

    default:
      return auth::TokenCache::Entry::Unauthenticated();
  }
}

void auth::TokenCache::invalidateBasicCache() {
  WRITE_LOCKER(guard, _basicLock);
  _basicCache.clear();
}

// private
auth::TokenCache::Entry auth::TokenCache::checkAuthenticationBasic(
    std::string const& secret) {
  if (_userManager == nullptr) { // server does not support users
    LOG_TOPIC(DEBUG, Logger::AUTHENTICATION) << "Basic auth not supported";
    return auth::TokenCache::Entry::Unauthenticated();
  }

  uint64_t version = _userManager->globalVersion();
  if (_basicCacheVersion.load(std::memory_order_acquire) != version) {
     WRITE_LOCKER(guard, _basicLock);
    _basicCache.clear();
    _basicCacheVersion.store(version, std::memory_order_release);
  }

  {
    READ_LOCKER(guard, _basicLock);
    auto const& it = _basicCache.find(secret);
    if (it != _basicCache.end() && !it->second.expired()) {
      // copy entry under the read-lock
      auth::TokenCache::Entry res = it->second;
      // and now give up on the read-lock
      guard.unlock();

      // LDAP rights might need to be refreshed
      if (!_userManager->refreshUser(res.username())) {
        return res;
      }
      // fallthrough intentional here
    }
  }

  // parse Basic auth header
  std::string const up = StringUtils::decodeBase64(secret);
  std::string::size_type n = up.find(':', 0);
  if (n == std::string::npos || n == 0 || n + 1 > up.size()) {
    LOG_TOPIC(TRACE, arangodb::Logger::AUTHENTICATION)
        << "invalid authentication data found, cannot extract "
           "username/password";
    return auth::TokenCache::Entry::Unauthenticated();
  }

  std::string username = up.substr(0, n);
  std::string password = up.substr(n + 1);

  bool authorized = _userManager->checkPassword(username, password);
  double expiry = _authTimeout;
  if (expiry > 0) {
    expiry += TRI_microtime();
  }

  auth::TokenCache::Entry entry(username, authorized, expiry);
  {
    WRITE_LOCKER(guard, _basicLock);
    if (authorized) {
      if (!_basicCache.emplace(secret, entry).second) {
        // insertion did not work - probably another thread did insert the
        // same data right now
        // erase it and re-insert our version
        _basicCache.erase(secret);
        _basicCache.emplace(secret, entry);
      }
    } else {
      _basicCache.erase(secret);
    }
  }

  return entry;
}

auth::TokenCache::Entry auth::TokenCache::checkAuthenticationJWT(
    std::string const& jwt) {
  // note that we need the write lock here because it is an LRU
  // cache. reading from it will move the read entry to the start of
  // the cache's linked list. so acquiring just a read-lock is
  // insufficient!!
  {
    WRITE_LOCKER(writeLocker, _jwtLock);
    // intentionally copy the entry from the cache
    auth::TokenCache::Entry const* entry = _jwtCache.get(jwt);
    if (entry != nullptr) {
      // would have thrown if not found
      if (entry->expired()) {
        _jwtCache.remove(jwt);
        LOG_TOPIC(TRACE, Logger::AUTHENTICATION) << "JWT Token expired";
        return auth::TokenCache::Entry::Unauthenticated();
      }
      if (_userManager != nullptr) {
        // LDAP rights might need to be refreshed
        _userManager->refreshUser(entry->username());
      }
      return *entry;
    }
  }
  std::vector<std::string> const parts = StringUtils::split(jwt, '.');
  if (parts.size() != 3) {
    LOG_TOPIC(TRACE, arangodb::Logger::AUTHENTICATION)
        << "Secret contains " << parts.size() << " parts";
    return auth::TokenCache::Entry::Unauthenticated();
  }

  std::string const& header = parts[0];
  std::string const& body = parts[1];
  std::string const& signature = parts[2];

  if (!validateJwtHeader(header)) {
    LOG_TOPIC(TRACE, arangodb::Logger::AUTHENTICATION)
        << "Couldn't validate jwt header " << header;
    return auth::TokenCache::Entry::Unauthenticated();
  }

  std::string const message = header + "." + body;
  if (!validateJwtHMAC256Signature(message, signature)) {
    LOG_TOPIC(TRACE, arangodb::Logger::AUTHENTICATION)
        << "Couldn't validate jwt signature " << signature << " against "
        << _jwtSecret;
    return auth::TokenCache::Entry::Unauthenticated();
  }
  
  auth::TokenCache::Entry newEntry = validateJwtBody(body);
  if (!newEntry._authenticated) {
    LOG_TOPIC(TRACE, arangodb::Logger::AUTHENTICATION)
    << "Couldn't validate jwt body " << body;
    return auth::TokenCache::Entry::Unauthenticated();
  }

  WRITE_LOCKER(writeLocker, _jwtLock);
  _jwtCache.put(jwt, newEntry);
  return newEntry;
}

std::shared_ptr<VPackBuilder> auth::TokenCache::parseJson(
    std::string const& str, std::string const& hint) {
  std::shared_ptr<VPackBuilder> result;
  VPackParser parser;
  try {
    parser.parse(str);
    result = parser.steal();
  } catch (std::bad_alloc const&) {
    LOG_TOPIC(ERR, arangodb::Logger::AUTHENTICATION)
        << "Out of memory parsing " << hint << "!";
  } catch (VPackException const& ex) {
    LOG_TOPIC(DEBUG, arangodb::Logger::AUTHENTICATION)
        << "Couldn't parse " << hint << ": " << ex.what();
  } catch (...) {
    LOG_TOPIC(ERR, arangodb::Logger::AUTHENTICATION)
        << "Got unknown exception trying to parse " << hint;
  }

  return result;
}

bool auth::TokenCache::validateJwtHeader(std::string const& header) {
  std::shared_ptr<VPackBuilder> headerBuilder =
      parseJson(StringUtils::decodeBase64(header), "jwt header");
  if (headerBuilder.get() == nullptr) {
    return false;
  }

  VPackSlice const headerSlice = headerBuilder->slice();
  if (!headerSlice.isObject()) {
    return false;
  }

  VPackSlice const algSlice = headerSlice.get("alg");
  VPackSlice const typSlice = headerSlice.get("typ");

  if (!algSlice.isString()) {
    return false;
  }

  if (!typSlice.isString()) {
    return false;
  }

  if (algSlice.copyString() != "HS256") {
    return false;
  }

  std::string typ = typSlice.copyString();
  if (typ != "JWT") {
    return false;
  }

  return true;
}

auth::TokenCache::Entry auth::TokenCache::validateJwtBody(
    std::string const& body) {
  std::shared_ptr<VPackBuilder> bodyBuilder =
      parseJson(StringUtils::decodeBase64(body), "jwt body");
  if (bodyBuilder.get() == nullptr) {
    LOG_TOPIC(TRACE, Logger::AUTHENTICATION) << "invalid JWT body";
    return auth::TokenCache::Entry::Unauthenticated();
  }

  VPackSlice const bodySlice = bodyBuilder->slice();
  if (!bodySlice.isObject()) {
    LOG_TOPIC(TRACE, Logger::AUTHENTICATION) << "invalid JWT value";
    return auth::TokenCache::Entry::Unauthenticated();
  }

  VPackSlice const issSlice = bodySlice.get("iss");
  if (!issSlice.isString()) {
    LOG_TOPIC(TRACE, arangodb::Logger::AUTHENTICATION) << "missing iss value";
    return auth::TokenCache::Entry::Unauthenticated();
  }

  if (issSlice.copyString() != "arangodb") {
    LOG_TOPIC(TRACE, arangodb::Logger::AUTHENTICATION) << "invalid iss value";
    return auth::TokenCache::Entry::Unauthenticated();
  }

  auth::TokenCache::Entry authResult("", false, 0);
  if (bodySlice.hasKey("preferred_username")) {
    VPackSlice const usernameSlice = bodySlice.get("preferred_username");
    if (!usernameSlice.isString() || usernameSlice.getStringLength() == 0) {
      return auth::TokenCache::Entry::Unauthenticated();
    }
    authResult._username = usernameSlice.copyString();
    if (_userManager == nullptr || !_userManager->userExists(authResult._username)) {
      return auth::TokenCache::Entry::Unauthenticated();
    }
  } else if (bodySlice.hasKey("server_id")) {
    // mop: hmm...nothing to do here :D
  } else {
    LOG_TOPIC(TRACE, arangodb::Logger::AUTHENTICATION)
        << "Lacking preferred_username or server_id";
    return auth::TokenCache::Entry::Unauthenticated();
  }

  // mop: optional exp (cluster currently uses non expiring jwts)
  if (bodySlice.hasKey("exp")) {
    VPackSlice const expSlice = bodySlice.get("exp");
    if (!expSlice.isNumber()) {
      LOG_TOPIC(TRACE, Logger::AUTHENTICATION) << "invalid exp value";
      return authResult;  // unauthenticated
    }

    // in seconds since epoch
    double expiresSecs = expSlice.getNumber<double>();
    double now = TRI_microtime();
    if (now >= expiresSecs || expiresSecs == 0) {
      LOG_TOPIC(TRACE, Logger::AUTHENTICATION) << "expired JWT token";
      return authResult;  // unauthenticated
    }
    authResult._expiry = expiresSecs;
  } else {
    authResult._expiry = 0;
  }

  authResult._authenticated = true;
  return authResult;
}

bool auth::TokenCache::validateJwtHMAC256Signature(
    std::string const& message, std::string const& signature) {
  std::string decodedSignature = StringUtils::decodeBase64U(signature);

  return verifyHMAC(_jwtSecret.c_str(), _jwtSecret.length(), message.c_str(),
                    message.length(), decodedSignature.c_str(),
                    decodedSignature.length(),
                    SslInterface::Algorithm::ALGORITHM_SHA256);
}

std::string auth::TokenCache::generateRawJwt(VPackSlice const& body) const {
  VPackBuilder headerBuilder;
  {
    VPackObjectBuilder h(&headerBuilder);
    headerBuilder.add("alg", VPackValue("HS256"));
    headerBuilder.add("typ", VPackValue("JWT"));
  }

  std::string fullMessage(StringUtils::encodeBase64(headerBuilder.toJson()) +
                          "." + StringUtils::encodeBase64(body.toJson()));
  if (_jwtSecret.empty()) {
    LOG_TOPIC(INFO, Logger::AUTHENTICATION)
        << "Using cluster without JWT Token";
  }

  std::string signature =
      sslHMAC(_jwtSecret.c_str(), _jwtSecret.length(), fullMessage.c_str(),
              fullMessage.length(), SslInterface::Algorithm::ALGORITHM_SHA256);

  return fullMessage + "." + StringUtils::encodeBase64U(signature);
}

std::string auth::TokenCache::generateJwt(VPackSlice const& payload) const {
  if (!payload.isObject()) {
    std::string error = "Need an object to generate a JWT. Got: ";
    error += payload.typeName();
    throw std::runtime_error(error);
  }
  bool hasIss = payload.hasKey("iss");
  bool hasIat = payload.hasKey("iat");
  if (hasIss && hasIat) {
    return generateRawJwt(payload);
  } else {
    VPackBuilder bodyBuilder;
    {
      VPackObjectBuilder p(&bodyBuilder);
      if (!hasIss) {
        bodyBuilder.add("iss", VPackValue("arangodb"));
      }
      if (!hasIat) {
        bodyBuilder.add("iat", VPackValue(TRI_microtime() / 1000));
      }
      for (auto const& obj : VPackObjectIterator(payload)) {
        bodyBuilder.add(obj.key.copyString(), obj.value);
      }
    }
    return generateRawJwt(bodyBuilder.slice());
  }
}

/// generate a JWT token for internal cluster communication
void auth::TokenCache::generateJwtToken() {
  VPackBuilder body;
  body.openObject();
  body.add("server_id", VPackValue(ServerState::instance()->getId()));
  body.close();
  _jwtToken = generateJwt(body.slice());
}
