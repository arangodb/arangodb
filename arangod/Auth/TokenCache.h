////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2014-2017 ArangoDB GmbH, Cologne, Germany
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
////////////////////////////////////////////////////////////////////////////////

#ifndef ARANGOD_AUTHENTICATION_TOKEN_CACHE_H
#define ARANGOD_AUTHENTICATION_TOKEN_CACHE_H 1

#include "Basics/Common.h"
#include "Basics/LruCache.h"
#include "Basics/Mutex.h"
#include "Basics/ReadWriteLock.h"
#include "Basics/Result.h"
#include "Basics/debugging.h"
#include "Basics/system-functions.h"
#include "Rest/CommonDefines.h"

#include <velocypack/Builder.h>
#include <velocypack/Slice.h>

namespace arangodb {
namespace auth {
class UserManager;

/// @brief Caches the basic and JWT authentication tokens
class TokenCache {
 public:
  /// Construct authentication cache
  /// @param um UserManager singleton
  /// @param timeout default token expiration timeout
  explicit TokenCache(auth::UserManager* um, double timeout);
  ~TokenCache();

 public:
  struct Entry {
    friend class auth::TokenCache;

   public:
    explicit Entry(std::string const& username, bool a, double t)
        : _username(username), _expiry(t), _authenticated(a) {}

    static Entry Unauthenticated() { return Entry("", false, 0); }
    static Entry Superuser() { return Entry("", true, 0); }

    std::string const& username() const { return _username; }
    bool authenticated() const { return _authenticated; }
    void authenticated(bool value) { _authenticated = value; }
    void setExpiry(double expiry) { _expiry = expiry; }
    bool expired() const { return _expiry != 0 && _expiry < TRI_microtime(); }
    std::vector<std::string> const& allowedPaths() const { return _allowedPaths; }

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
  
  TokenCache::Entry checkAuthentication(arangodb::rest::AuthenticationMethod authType,
                                        std::string const& secret);

  /// Clear the cache of username / password auth
  void invalidateBasicCache();
  
#ifdef USE_ENTERPRISE
  /// set new jwt secret, regenerate _jwtToken
  void setJwtSecrets(std::string const& active,
                     std::vector<std::string> const& passive);
#else
  /// set new jwt secret, regenerate _jwtToken
  void setJwtSecret(std::string const& active);
#endif

  /// Get the jwt token, which should be used for communication
  std::string const& jwtToken() const noexcept {
    TRI_ASSERT(!_jwtSuperToken.empty());
    return _jwtSuperToken;
  }
  
  std::string jwtSecret() const;

 private:
  /// Check basic HTTP Authentication header
  TokenCache::Entry checkAuthenticationBasic(std::string const& secret);
  /// Check JWT token contents
  TokenCache::Entry checkAuthenticationJWT(std::string const& secret);

  bool validateJwtHeader(std::string const&);
  TokenCache::Entry validateJwtBody(std::string const&);
  bool validateJwtHMAC256Signature(std::string const&, std::string const&);

  std::shared_ptr<velocypack::Builder> parseJson(std::string const& str, char const* hint);
  
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
}  // namespace auth
}  // namespace arangodb

#endif
