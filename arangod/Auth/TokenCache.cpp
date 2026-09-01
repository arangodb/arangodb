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
////////////////////////////////////////////////////////////////////////////////

#include "TokenCache.h"

#include "Agency/AgencyComm.h"
#include "Auth/UserManager.h"
#include "Basics/ReadLocker.h"
#include "Basics/StringUtils.h"
#include "Basics/WriteLocker.h"
#include "Basics/system-functions.h"
#include "Cluster/ServerState.h"
#include "GeneralServer/AuthenticationFeature.h"
#include "Logger/LogMacros.h"
#include "Ssl/SslInterface.h"
#include "Ssl/jwt.h"
#include "Ssl/JwtSignature.h"

#include <absl/strings/escaping.h>

#include <velocypack/Builder.h>
#include <velocypack/Collection.h>
#include <velocypack/Iterator.h>
#include <velocypack/Parser.h>

#include <chrono>
#include <mutex>
#include <shared_mutex>
#include <variant>

namespace arangodb::auth {

namespace {
constexpr std::string_view hs256String("HS256");
constexpr std::string_view es256String("ES256");
constexpr std::string_view jwtString("JWT");
}  // namespace

bool TokenCache::Entry::expired() const noexcept {
  return _expiry != 0 && _expiry < TRI_microtime();
}

TokenCache::TokenCache(UserManager* um, double timeout)
    : _userManager(um), _jwtCache(16384), _authTimeout(timeout) {}

TokenCache::~TokenCache() {
  // properly clear structs while using the appropriate locks
  {
    WRITE_LOCKER(readLocker, _basicLock);
    _basicCache.clear();
  }
  {
    auto guard = std::lock_guard<std::mutex>(_jwtCacheMutex);
    _jwtCache.clear();
  }
}

void TokenCache::setJwtSecrets(AuthInfo secrets) {
  {
    LOG_TOPIC("71a76", DEBUG, Logger::AUTHENTICATION)
        << "Setting active jwt secret " << authKeyInfo(secrets.activeSecret)
        << ", additionally setting (" << secrets.passiveSecrets.size()
        << ") passive secret(s)";
    _jwtSecrets.assign(std::move(secrets));
  }
  {
    std::lock_guard<std::mutex> guard(_jwtCacheMutex);
    _jwtCache.clear();
  }
  generateSuperToken();
}

std::string TokenCache::jwtToken() const noexcept {
  TRI_ASSERT(!_jwtSuperToken.empty());
  return _jwtSuperToken;
}

AuthKey TokenCache::jwtSecret() const {
  return _jwtSecrets.getLockedGuard()->activeSecret;  // intentional copy
}

// public called from {H2,Http}CommTask.cpp
// should only lock if required, otherwise we will serialize all
// requests whether we need to or not
TokenCache::Entry TokenCache::checkAuthentication(
    rest::AuthenticationMethod authType, ServerState::Mode mode,
    std::string const& secret) {
  switch (authType) {
    case rest::AuthenticationMethod::BASIC:
      if (mode == ServerState::Mode::STARTUP) {
        // during the startup phase, we have no access to the underlying
        // database data, so we cannot validate the credentials.
        return TokenCache::Entry::Unauthenticated();
      }
      return checkAuthenticationBasic(secret);

    case rest::AuthenticationMethod::JWT:
      // JWTs work fine even during the startup phase
      return checkAuthenticationJWT(secret);

    default:
      return TokenCache::Entry::Unauthenticated();
  }
}

void TokenCache::invalidateBasicCache() {
  WRITE_LOCKER(guard, _basicLock);
  _basicCache.clear();
}

// private
TokenCache::Entry TokenCache::checkAuthenticationBasic(
    std::string const& secret) {
  if (_userManager == nullptr) {  // server does not support users
    LOG_TOPIC("9900c", DEBUG, Logger::AUTHENTICATION)
        << "Basic auth not supported";
    return TokenCache::Entry::Unauthenticated();
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
      TokenCache::Entry res = it->second;
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
    LOG_TOPIC("2a529", TRACE, Logger::AUTHENTICATION)
        << "invalid authentication data found, cannot extract "
           "username/password";
    return TokenCache::Entry::Unauthenticated();
  }

  std::string username = up.substr(0, n);
  std::string password = up.substr(n + 1);
  std::string un;
  std::optional<double> tokenValidUntil;
  bool authorized =
      _userManager->checkCredentials(username, password, un, tokenValidUntil);

  if (authorized) {
    username = un;
  }

  // for personal access tokens, cache the entry only until the token's own
  // expiration time, instead of the generic authentication-timeout. for
  // password authentication (and unauthenticated attempts) tokenValidUntil
  // is left unset, so fall back to the generic timeout as before.
  double expiry;
  if (tokenValidUntil.has_value()) {
    expiry = *tokenValidUntil;
  } else {
    expiry = _authTimeout;
    if (expiry > 0) {
      expiry += TRI_microtime();
    }
  }

  TokenCache::Entry entry(std::move(username), authorized, expiry);
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

TokenCache::Entry TokenCache::checkAuthenticationJWT(std::string const& jwt) {
  // note that we need the write lock here because it is an LRU
  // cache. reading from it will move the read entry to the start of
  // the cache's linked list. so acquiring just a read-lock is
  // insufficient!!
  {
    std::lock_guard<std::mutex> guard(_jwtCacheMutex);
    // intentionally copy the entry from the cache
    TokenCache::Entry const* entry = _jwtCache.get(jwt);
    if (entry != nullptr) {
      // would have thrown if not found
      if (entry->expired()) {
        _jwtCache.remove(jwt);
        LOG_TOPIC("65e15", TRACE, Logger::AUTHENTICATION)
            << "JWT Token expired";
        return TokenCache::Entry::Unauthenticated();
      }
      return *entry;
    }
  }
  std::vector<std::string> const parts = basics::StringUtils::split(jwt, '.');
  if (parts.size() != 3) {
    LOG_TOPIC("94a73", TRACE, Logger::AUTHENTICATION)
        << "Secret contains " << parts.size() << " parts";
    return TokenCache::Entry::Unauthenticated();
  }

  std::string const& header = parts[0];
  std::string const& body = parts[1];
  std::string const& signature = parts[2];

  // TODO(COR-922): return the JwtAlgorithm from Jwt header parse
  bool isES256 = false;
  if (!validateJwtHeader(header, isES256)) {
    LOG_TOPIC("2eb8a", TRACE, Logger::AUTHENTICATION)
        << "Couldn't validate jwt header: SENSITIVE_DETAILS_HIDDEN";
    return TokenCache::Entry::Unauthenticated();
  }

  std::string const message = header + "." + body;

  // TODO(COR-923): constructor maybe?
  auto jwtSignature = JwtSignature{.algorithm = isES256 ? JwtAlgorithm::ES256
                                                        : JwtAlgorithm::HS256};
  absl::WebSafeBase64Unescape(signature, &jwtSignature.signature);

  auto signatureValid =
      validateJwtSignature(_jwtSecrets.copy(), message, jwtSignature);

  if (!signatureValid) {
    LOG_TOPIC("176c4", TRACE, Logger::AUTHENTICATION)
        << "Couldn't validate jwt signature against given secret";
    return TokenCache::Entry::Unauthenticated();
  }

  TokenCache::Entry newEntry = validateJwtBody(body);
  if (!newEntry.authenticated()) {
    LOG_TOPIC("5fcba", TRACE, Logger::AUTHENTICATION)
        << "Couldn't validate jwt body: SENSITIVE_DETAILS_HIDDEN";
    return TokenCache::Entry::Unauthenticated();
  }

  // Store the full JWT token in the entry
  newEntry._jwtToken = jwt;

  {
    std::lock_guard<std::mutex> guard(_jwtCacheMutex);
    _jwtCache.put(jwt, newEntry);
  }
  return newEntry;
}

std::shared_ptr<VPackBuilder> TokenCache::parseJson(std::string_view str,
                                                    char const* hint) {
  std::shared_ptr<VPackBuilder> result;
  VPackParser parser;
  try {
    parser.parse(str);
    result = parser.steal();
  } catch (std::bad_alloc const&) {
    LOG_TOPIC("125c4", ERR, Logger::AUTHENTICATION)
        << "Out of memory parsing " << hint << "!";
  } catch (VPackException const& ex) {
    LOG_TOPIC("cc356", DEBUG, Logger::AUTHENTICATION)
        << "Couldn't parse " << hint << ": " << ex.what();
  } catch (...) {
    LOG_TOPIC("12c5d", ERR, Logger::AUTHENTICATION)
        << "Got unknown exception trying to parse " << hint;
  }

  return result;
}

bool TokenCache::validateJwtHeader(std::string_view headerWebBase64,
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

  if (algSlice.isEqualString(es256String)) {
    isES256 = true;
  } else if (algSlice.isEqualString(hs256String)) {
    isES256 = false;
  } else {
    return false;
  }

  if (!typSlice.isEqualString(jwtString)) {
    return false;
  }

  return true;
}

TokenCache::Entry TokenCache::validateJwtBody(std::string_view bodyWebBase64) {
  std::string body;
  absl::WebSafeBase64Unescape(bodyWebBase64, &body);
  std::shared_ptr<VPackBuilder> bodyBuilder = parseJson(body, "jwt body");
  if (bodyBuilder == nullptr) {
    LOG_TOPIC("99524", TRACE, Logger::AUTHENTICATION) << "invalid JWT body";
    return TokenCache::Entry::Unauthenticated();
  }

  VPackSlice const bodySlice = bodyBuilder->slice();
  if (!bodySlice.isObject()) {
    LOG_TOPIC("7dc0f", TRACE, Logger::AUTHENTICATION) << "invalid JWT value";
    return TokenCache::Entry::Unauthenticated();
  }

  VPackSlice const issSlice = bodySlice.get("iss");
  if (!issSlice.isString()) {
    LOG_TOPIC("ce204", TRACE, Logger::AUTHENTICATION) << "missing iss value";
    return TokenCache::Entry::Unauthenticated();
  }

  if (!issSlice.isEqualString(std::string_view("arangodb"))) {
    LOG_TOPIC("2547e", TRACE, Logger::AUTHENTICATION) << "invalid iss value";
    return TokenCache::Entry::Unauthenticated();
  }

  TokenCache::Entry authResult("", false, 0);
  VPackSlice const usernameSlice = bodySlice.get("preferred_username");
  if (!usernameSlice.isNone()) {
    if (!usernameSlice.isString() || usernameSlice.getStringLength() == 0) {
      return TokenCache::Entry::Unauthenticated();
    }
    authResult._username = usernameSlice.copyString();
    if (_userManager == nullptr ||
        !_userManager->userExists(authResult._username)) {
      return TokenCache::Entry::Unauthenticated();
    }
  } else if (bodySlice.hasKey("server_id")) {
    // mop: hmm...nothing to do here :D
  } else {
    LOG_TOPIC("84c61", TRACE, Logger::AUTHENTICATION)
        << "Lacking preferred_username or server_id";
    return TokenCache::Entry::Unauthenticated();
  }

  VPackSlice const paths = bodySlice.get("allowed_paths");
  if (!paths.isNone()) {
    if (!paths.isArray()) {
      LOG_TOPIC("89898", TRACE, Logger::AUTHENTICATION)
          << "allowed_paths must be an array";
      return TokenCache::Entry::Unauthenticated();
    }
    if (paths.length() == 0) {
      LOG_TOPIC("89893", TRACE, Logger::AUTHENTICATION)
          << "allowed_paths may not be empty";
      return TokenCache::Entry::Unauthenticated();
    }
    for (auto const& path : VPackArrayIterator(paths)) {
      if (!path.isString()) {
        LOG_TOPIC("89891", TRACE, Logger::AUTHENTICATION)
            << "allowed_paths may only contain strings";
        return TokenCache::Entry::Unauthenticated();
      }
      authResult._allowedPaths.push_back(path.copyString());
    }
  }

  // Extract roles from JWT token if present
  VPackSlice const rolesSlice = bodySlice.get("roles");
  if (!rolesSlice.isNone() && !rolesSlice.isNull()) {
    if (!rolesSlice.isArray()) {
      LOG_TOPIC("89899", TRACE, Logger::AUTHENTICATION)
          << "roles must be an array";
      return TokenCache::Entry::Unauthenticated();
    }
    for (auto const& role : VPackArrayIterator(rolesSlice)) {
      if (!role.isString()) {
        LOG_TOPIC("89892", TRACE, Logger::AUTHENTICATION)
            << "roles may only contain strings";
        return TokenCache::Entry::Unauthenticated();
      }
      authResult._roles.push_back(role.copyString());
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

/// generate a JWT token for internal cluster communication
void TokenCache::generateSuperToken() {
  std::string sid = ServerState::instance()->getId();

  auto guard = _jwtSecrets.getLockedGuard();

  if (std::holds_alternative<auth::ES256PrivateKey>(guard->activeSecret)) {
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
    int result = rest::SslInterface::signES256(
        std::get<ES256PrivateKey>(guard->activeSecret)._data, fullMessage,
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
    _jwtSuperToken = rest::SslInterface::jwt::generateInternalToken(
        std::get<HS256Key>(guard->activeSecret)._data, sid);
  }
}

}  // namespace arangodb::auth
