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
/// @author Dr. Frank Celler
////////////////////////////////////////////////////////////////////////////////

#pragma once

#include "Basics/LruCache.h"
#include "Basics/ReadWriteLock.h"
#include "Basics/Result.h"
#include "Cluster/ServerState.h"
#include "Rest/CommonDefines.h"

#include <atomic>
#include <memory>
#include <mutex>
#include <string>
#include <string_view>
#include <unordered_map>
#include <vector>

namespace arangodb::velocypack {
class Builder;
}

namespace arangodb::auth {
class UserManager;

/// @brief Caches the basic and JWT authentication tokens
class TokenCache {
 public:
  /// Construct authentication cache
  /// @param um UserManager singleton
  /// @param timeout default token expiration timeout
  explicit TokenCache(auth::UserManager* um, double timeout);
  ~TokenCache();

  struct Entry {
    friend class auth::TokenCache;

   public:
    explicit Entry(std::string username, bool a, double t)
        : _username(std::move(username)), _expiry(t), _authenticated(a) {}

    static Entry Unauthenticated() { return Entry("", false, 0); }
    static Entry Superuser() { return Entry("", true, 0); }

    std::string const& username() const noexcept { return _username; }
    bool authenticated() const noexcept { return _authenticated; }
    void authenticated(bool value) noexcept { _authenticated = value; }
    void setExpiry(double expiry) noexcept { _expiry = expiry; }
    double expiry() const noexcept { return _expiry; }
    bool expired() const noexcept;
    std::vector<std::string> const& allowedPaths() const {
      return _allowedPaths;
    }

   private:
    /// username
    std::string _username;
    // paths that are valid for this token
    std::vector<std::string> _allowedPaths;
    /// expiration time (in seconds since epoch) of this entry
    double _expiry;
    /// User exists and password was checked
    bool _authenticated;
  };

 public:
  TokenCache::Entry checkAuthentication(rest::AuthenticationMethod authType,
                                        ServerState::Mode mode,
                                        std::string const& secret);

  /// Clear the cache of username / password auth
  void invalidateBasicCache();

#ifdef USE_ENTERPRISE
  /// set new jwt secret, regenerate _jwtToken
  void setJwtSecrets(std::string active, std::vector<std::string> passive);
#else
  /// set new jwt secret, regenerate _jwtToken
  void setJwtSecret(std::string active);
#endif

  /// Get the jwt token, which should be used for communication
  std::string const& jwtToken() const noexcept;

  std::string jwtSecret() const;

 private:
  /// Check basic HTTP Authentication header
  TokenCache::Entry checkAuthenticationBasic(std::string const& secret);
  /// Check JWT token contents
  TokenCache::Entry checkAuthenticationJWT(std::string const& secret);

  bool validateJwtHeader(std::string_view headerWebBase64);
  TokenCache::Entry validateJwtBody(std::string_view bodyWebBase64);
  bool validateJwtHMAC256Signature(std::string_view message,
                                   std::string_view signatureWebBase64);

  std::shared_ptr<velocypack::Builder> parseJson(std::string_view str,
                                                 char const* hint);

  /// generate new superuser jwtToken
  void generateSuperToken();

 private:
  auth::UserManager* const _userManager;

  mutable arangodb::basics::ReadWriteLock _basicLock;
  std::unordered_map<std::string, TokenCache::Entry> _basicCache;
  std::atomic<uint64_t> _basicCacheVersion{0};

  mutable arangodb::basics::ReadWriteLock _jwtSecretLock;

#ifdef USE_ENTERPRISE
  std::vector<std::string> _jwtPassiveSecrets;
#endif
  std::string _jwtActiveSecret;
  std::string _jwtSuperToken;  /// token for internal use

  mutable std::mutex _jwtCacheMutex;
  arangodb::basics::LruCache<std::string, TokenCache::Entry> _jwtCache;

  /// Timeout in seconds
  double const _authTimeout;
};

}  // namespace arangodb::auth
