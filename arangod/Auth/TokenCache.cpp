////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2014-2024 ArangoDB GmbH, Cologne, Germany
/// Copyright 2004-2014 triAGENS GmbH, Cologne, Germany
///
/// Licensed under the Business Source License 1.1 (the "License");
/// you may not use this file except in compliance with the License.
/// You may obtain a copy of the License at
///
///     https://github.com/arangodb/arangodb/blob/devel/LICENSE
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
#include "Auth/UserManager.h"
#include "Basics/ReadLocker.h"
#include "Basics/StringUtils.h"
#include "Basics/VelocyPackHelper.h"
#include "Basics/WriteLocker.h"
#include "Basics/debugging.h"
#include "Basics/system-functions.h"
#include "Basics/tri-strings.h"
#include "Cluster/ServerState.h"
#include "GeneralServer/AuthenticationFeature.h"
#include "Logger/LogMacros.h"
#include "Ssl/SslInterface.h"

#include <absl/strings/escaping.h>
#include <fuerte/jwt.h>

#include <velocypack/Builder.h>
#include <velocypack/Collection.h>
#include <velocypack/Iterator.h>

#include <chrono>

using namespace arangodb;
using namespace arangodb::basics;
using namespace arangodb::velocypack;
using namespace arangodb::rest;

namespace {
constexpr std::string_view hs256String("HS256");
constexpr std::string_view es256String("ES256");
constexpr std::string_view jwtString("JWT");
}  // namespace

bool auth::TokenCache::Entry::expired() const noexcept {
  return _expiry != 0 && _expiry < TRI_microtime();
}

auth::TokenCache::TokenCache(auth::UserManager* um, double timeout)
    : _userManager(um), _jwtCache(16384), _authTimeout(timeout) {}

auth::TokenCache::~TokenCache() {
  // properly clear structs while using the appropriate locks
  {
    WRITE_LOCKER(readLocker, _basicLock);
    _basicCache.clear();
  }
  {
    WRITE_LOCKER(writeLocker, _jwtSecretLock);
    _jwtCache.clear();
  }
}

void auth::TokenCache::setJwtSecrets(std::string active,
                                     std::vector<std::string> passive,
                                     bool isES256) {
  {
    WRITE_LOCKER(writeLocker, _jwtSecretLock);
    LOG_TOPIC("71a76", DEBUG, Logger::AUTHENTICATION)
        << "Setting active jwt secret of size " << active.size()
        << ", additionally setting (" << passive.size() << ") passive secret(s)"
        << " (ES256: " << (isES256 ? "yes" : "no") << ")";

    _jwtActiveSecret = std::move(active);
    _jwtPassiveSecrets = std::move(passive);
    _jwtActiveSecretIsES256 = isES256;
  }
  {
    std::lock_guard<std::mutex> guard(_jwtCacheMutex);
    _jwtCache.clear();
  }
  generateSuperToken();
}

std::string const& auth::TokenCache::jwtToken() const noexcept {
  TRI_ASSERT(!_jwtSuperToken.empty());
  return _jwtSuperToken;
}

std::string auth::TokenCache::jwtSecret() const {
  READ_LOCKER(writeLocker, _jwtSecretLock);
  return _jwtActiveSecret;  // intentional copy
}

// public called from {H2,Http}CommTask.cpp
// should only lock if required, otherwise we will serialize all
// requests whether we need to or not
auth::TokenCache::Entry auth::TokenCache::checkAuthentication(
    AuthenticationMethod authType, ServerState::Mode mode,
    std::string const& secret) {
  switch (authType) {
    case AuthenticationMethod::BASIC:
      if (mode == ServerState::Mode::STARTUP) {
        // during the startup phase, we have no access to the underlying
        // database data, so we cannot validate the credentials.
        return auth::TokenCache::Entry::Unauthenticated();
      }
      return checkAuthenticationBasic(secret);

    case AuthenticationMethod::JWT:
      // JWTs work fine even during the startup phase
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
  if (_userManager == nullptr) {  // server does not support users
    LOG_TOPIC("9900c", DEBUG, Logger::AUTHENTICATION)
        << "Basic auth not supported";
    return auth::TokenCache::Entry::Unauthenticated();
  }

  uint64_t version = _userManager->globalVersion();
  if (_basicCacheVersion.load(std::memory_order_acquire) != version) {
    WRITE_LOCKER(guard, _basicLock);
    _basicCache.clear();
    _basicCacheVersion.store(version, std::memory_order_release);
  } else {
    READ_LOCKER(guard, _basicLock);
    auto const& it = _basicCache.find(secret);
    if (it != _basicCache.end() && !it->second.expired()) {
      // copy entry under the read-lock
      auth::TokenCache::Entry res = it->second;
      // and now give up on the read-lock
      guard.unlock();
      return res;
    }
  }

  // parse Basic auth header
  std::string up;
  absl::Base64Unescape(secret, &up);
  std::string::size_type n = up.find(':', 0);
  // if password is an access token then username might be empty
  if (n == std::string::npos || /* n == 0 || */ n + 1 > up.size()) {
    LOG_TOPIC("2a529", TRACE, arangodb::Logger::AUTHENTICATION)
        << "invalid authentication data found, cannot extract "
           "username/password";
    return auth::TokenCache::Entry::Unauthenticated();
  }

  std::string username = up.substr(0, n);
  std::string password = up.substr(n + 1);
  std::string un;
  bool authorized = _userManager->checkCredentials(username, password, un);

  if (authorized) {
    username = un;
  }

  double expiry = _authTimeout;
  if (expiry > 0) {
    expiry += TRI_microtime();
  }

  auth::TokenCache::Entry entry(std::move(username), authorized, expiry);
  {
    WRITE_LOCKER(guard, _basicLock);
    if (authorized) {
      _basicCache.insert_or_assign(std::move(secret), entry);
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
    std::lock_guard<std::mutex> guard(_jwtCacheMutex);
    // intentionally copy the entry from the cache
    auth::TokenCache::Entry const* entry = _jwtCache.get(jwt);
    if (entry != nullptr) {
      // would have thrown if not found
      if (entry->expired()) {
        _jwtCache.remove(jwt);
        LOG_TOPIC("65e15", TRACE, Logger::AUTHENTICATION)
            << "JWT Token expired";
        return auth::TokenCache::Entry::Unauthenticated();
      }
      return *entry;
    }
  }
  std::vector<std::string> const parts = StringUtils::split(jwt, '.');
  if (parts.size() != 3) {
    LOG_TOPIC("94a73", TRACE, arangodb::Logger::AUTHENTICATION)
        << "Secret contains " << parts.size() << " parts";
    return auth::TokenCache::Entry::Unauthenticated();
  }

  std::string const& header = parts[0];
  std::string const& body = parts[1];
  std::string const& signature = parts[2];

  bool isES256 = false;
  if (!validateJwtHeader(header, isES256)) {
    LOG_TOPIC("2eb8a", TRACE, arangodb::Logger::AUTHENTICATION)
        << "Couldn't validate jwt header: SENSITIVE_DETAILS_HIDDEN";
    return auth::TokenCache::Entry::Unauthenticated();
  }

  std::string const message = header + "." + body;

  bool signatureValid = false;
  if (isES256) {
    signatureValid = validateJwtES256Signature(message, signature);
  } else {
    signatureValid = validateJwtHMAC256Signature(message, signature);
  }

  if (!signatureValid) {
    LOG_TOPIC("176c4", TRACE, arangodb::Logger::AUTHENTICATION)
        << "Couldn't validate jwt signature against given secret";
    return auth::TokenCache::Entry::Unauthenticated();
  }

  auth::TokenCache::Entry newEntry = validateJwtBody(body);
  if (!newEntry.authenticated()) {
    LOG_TOPIC("5fcba", TRACE, arangodb::Logger::AUTHENTICATION)
        << "Couldn't validate jwt body: SENSITIVE_DETAILS_HIDDEN";
    return auth::TokenCache::Entry::Unauthenticated();
  }

  {
    std::lock_guard<std::mutex> guard(_jwtCacheMutex);
    _jwtCache.put(jwt, newEntry);
  }
  return newEntry;
}

std::shared_ptr<VPackBuilder> auth::TokenCache::parseJson(std::string_view str,
                                                          char const* hint) {
  std::shared_ptr<VPackBuilder> result;
  VPackParser parser;
  try {
    parser.parse(str);
    result = parser.steal();
  } catch (std::bad_alloc const&) {
    LOG_TOPIC("125c4", ERR, arangodb::Logger::AUTHENTICATION)
        << "Out of memory parsing " << hint << "!";
  } catch (VPackException const& ex) {
    LOG_TOPIC("cc356", DEBUG, arangodb::Logger::AUTHENTICATION)
        << "Couldn't parse " << hint << ": " << ex.what();
  } catch (...) {
    LOG_TOPIC("12c5d", ERR, arangodb::Logger::AUTHENTICATION)
        << "Got unknown exception trying to parse " << hint;
  }

  return result;
}

bool auth::TokenCache::validateJwtHeader(std::string_view headerWebBase64,
                                         bool& isES256) {
  std::string header;
  absl::WebSafeBase64Unescape(headerWebBase64, &header);
  std::shared_ptr<VPackBuilder> headerBuilder = parseJson(header, "jwt header");
  if (headerBuilder == nullptr) {
    return false;
  }

  VPackSlice const headerSlice = headerBuilder->slice();
  if (!headerSlice.isObject()) {
    return false;
  }

  VPackSlice const algSlice = headerSlice.get("alg");
  VPackSlice const typSlice = headerSlice.get("typ");

  if (!algSlice.isString() || !typSlice.isString()) {
    return false;
  }

  if (algSlice.isEqualString(::es256String)) {
    isES256 = true;
  } else if (algSlice.isEqualString(::hs256String)) {
    isES256 = false;
  } else {
    return false;
  }

  if (!typSlice.isEqualString(::jwtString)) {
    return false;
  }

  return true;
}

auth::TokenCache::Entry auth::TokenCache::validateJwtBody(
    std::string_view bodyWebBase64) {
  std::string body;
  absl::WebSafeBase64Unescape(bodyWebBase64, &body);
  std::shared_ptr<VPackBuilder> bodyBuilder = parseJson(body, "jwt body");
  if (bodyBuilder == nullptr) {
    LOG_TOPIC("99524", TRACE, Logger::AUTHENTICATION) << "invalid JWT body";
    return auth::TokenCache::Entry::Unauthenticated();
  }

  VPackSlice const bodySlice = bodyBuilder->slice();
  if (!bodySlice.isObject()) {
    LOG_TOPIC("7dc0f", TRACE, Logger::AUTHENTICATION) << "invalid JWT value";
    return auth::TokenCache::Entry::Unauthenticated();
  }

  VPackSlice const issSlice = bodySlice.get("iss");
  if (!issSlice.isString()) {
    LOG_TOPIC("ce204", TRACE, arangodb::Logger::AUTHENTICATION)
        << "missing iss value";
    return auth::TokenCache::Entry::Unauthenticated();
  }

  if (!issSlice.isEqualString(std::string_view("arangodb"))) {
    LOG_TOPIC("2547e", TRACE, arangodb::Logger::AUTHENTICATION)
        << "invalid iss value";
    return auth::TokenCache::Entry::Unauthenticated();
  }

  auth::TokenCache::Entry authResult("", false, 0);
  VPackSlice const usernameSlice = bodySlice.get("preferred_username");
  if (!usernameSlice.isNone()) {
    if (!usernameSlice.isString() || usernameSlice.getStringLength() == 0) {
      return auth::TokenCache::Entry::Unauthenticated();
    }
    authResult._username = usernameSlice.copyString();
    if (_userManager == nullptr ||
        !_userManager->userExists(authResult._username)) {
      return auth::TokenCache::Entry::Unauthenticated();
    }
  } else if (bodySlice.hasKey("server_id")) {
    // mop: hmm...nothing to do here :D
  } else {
    LOG_TOPIC("84c61", TRACE, arangodb::Logger::AUTHENTICATION)
        << "Lacking preferred_username or server_id";
    return auth::TokenCache::Entry::Unauthenticated();
  }

  VPackSlice const paths = bodySlice.get("allowed_paths");
  if (!paths.isNone()) {
    if (!paths.isArray()) {
      LOG_TOPIC("89898", TRACE, arangodb::Logger::AUTHENTICATION)
          << "allowed_paths must be an array";
      return auth::TokenCache::Entry::Unauthenticated();
    }
    if (paths.length() == 0) {
      LOG_TOPIC("89893", TRACE, arangodb::Logger::AUTHENTICATION)
          << "allowed_paths may not be empty";
      return auth::TokenCache::Entry::Unauthenticated();
    }
    for (auto const& path : VPackArrayIterator(paths)) {
      if (!path.isString()) {
        LOG_TOPIC("89891", TRACE, arangodb::Logger::AUTHENTICATION)
            << "allowed_paths may only contain strings";
        return auth::TokenCache::Entry::Unauthenticated();
      }
      authResult._allowedPaths.push_back(path.copyString());
    }
  }

  // mop: optional exp (cluster currently uses non expiring jwts)
  VPackSlice const expSlice = bodySlice.get("exp");
  if (!expSlice.isNone()) {
    if (!expSlice.isNumber()) {
      LOG_TOPIC("74735", TRACE, Logger::AUTHENTICATION) << "invalid exp value";
      return authResult;  // unauthenticated
    }

    // in seconds since epoch
    double expiresSecs = expSlice.getNumber<double>();
    double now = TRI_microtime();
    if (now >= expiresSecs || expiresSecs == 0) {
      LOG_TOPIC("9a8b2", TRACE, Logger::AUTHENTICATION) << "expired JWT token";
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
    std::string_view message, std::string_view signatureWebBase64) {
  std::string signature;
  absl::WebSafeBase64Unescape(signatureWebBase64, &signature);

  READ_LOCKER(readLocker, _jwtSecretLock);

  bool good =
      verifyHMAC(_jwtActiveSecret.data(), _jwtActiveSecret.size(),
                 message.data(), message.size(), signature.data(),
                 signature.size(), SslInterface::Algorithm::ALGORITHM_SHA256);
  if (good) {
    return good;
  }

  // check all the passive secrets
  for (auto const& passive : _jwtPassiveSecrets) {
    good = verifyHMAC(passive.data(), passive.size(), message.data(),
                      message.size(), signature.data(), signature.size(),
                      SslInterface::Algorithm::ALGORITHM_SHA256);
    if (good) {
      return good;
    }
  }
  return false;
}

bool auth::TokenCache::validateJwtES256Signature(
    std::string_view message, std::string_view signatureWebBase64) {
  std::string signature;
  absl::WebSafeBase64Unescape(signatureWebBase64, &signature);

  READ_LOCKER(readLocker, _jwtSecretLock);

  // First try the active secret - it can be either a private key or a public
  // key. verifyES256Signature handles both cases.
  if (!_jwtActiveSecret.empty()) {
    bool good = SslInterface::verifyES256Signature(_jwtActiveSecret, message,
                                                   signature);
    if (good) {
      return true;
    }
  }

  // check all the passive certificates
  for (auto const& passive : _jwtPassiveSecrets) {
    bool good = SslInterface::verifyES256Signature(passive, message, signature);
    if (good) {
      return good;
    }
  }
  return false;
}

/// generate a JWT token for internal cluster communication
void auth::TokenCache::generateSuperToken() {
  std::string sid = ServerState::instance()->getId();

  READ_LOCKER(guard, _jwtSecretLock);

  if (_jwtActiveSecretIsES256) {
    // Generate ES256 JWT token manually
    std::chrono::seconds iss = std::chrono::duration_cast<std::chrono::seconds>(
        std::chrono::system_clock::now().time_since_epoch());

    VPackBuilder headerBuilder;
    {
      VPackObjectBuilder h(&headerBuilder);
      headerBuilder.add("alg", VPackValue("ES256"));
      headerBuilder.add("typ", VPackValue("JWT"));
    }

    VPackBuilder bodyBuilder;
    bodyBuilder.openObject();
    bodyBuilder.add("server_id", VPackValue(sid));
    bodyBuilder.add("iss", VPackValue("arangodb"));
    bodyBuilder.add("iat", VPackValue(static_cast<uint64_t>(iss.count())));
    bodyBuilder.close();

    auto header = headerBuilder.toJson();
    auto body = bodyBuilder.toJson();
    std::string headerBase64;
    std::string bodyBase64;

    absl::WebSafeBase64Escape(header, &headerBase64);
    absl::WebSafeBase64Escape(body, &bodyBase64);

    // Remove padding from base64
    auto removePadding = [](std::string& s) {
      while (!s.empty() && s.back() == '=') {
        s.pop_back();
      }
    };
    removePadding(headerBase64);
    removePadding(bodyBase64);

    std::string fullMessage = headerBase64 + "." + bodyBase64;

    std::string signature;
    std::string error;
    int result = SslInterface::signES256(_jwtActiveSecret, fullMessage,
                                         signature, error);

    if (result != 0) {
      LOG_TOPIC("71a77", ERR, Logger::AUTHENTICATION)
          << "Failed to sign JWT token with ES256: " << error;
      _jwtSuperToken.clear();
      return;
    }

    std::string signatureBase64;
    absl::WebSafeBase64Escape(signature, &signatureBase64);
    removePadding(signatureBase64);

    _jwtSuperToken = fullMessage + "." + signatureBase64;
  } else {
    // Use existing HS256 token generation
    _jwtSuperToken = fuerte::jwt::generateInternalToken(_jwtActiveSecret, sid);
  }
}
