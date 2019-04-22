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
        : _username(username), _authenticated(a), _expiry(t), _allowedPaths() {}

    static Entry Unauthenticated() { return Entry("", false, 0); }

    std::string const& username() const { return _username; }
    bool authenticated() const { return _authenticated; }
    void authenticated(bool value) { _authenticated = value; }
    void setExpiry(double expiry) { _expiry = expiry; }
    bool expired() const { return _expiry != 0 && _expiry < TRI_microtime(); }

   public:
    /// username
    std::string _username;
    /// User exists and password was checked
    bool _authenticated;
    /// expiration time (in seconds since epoch) of this entry
    double _expiry;
    // paths that are valid for this token
    std::vector<std::string> _allowedPaths;
  };

 public:
  TokenCache::Entry checkAuthentication(arangodb::rest::AuthenticationMethod authType,
                                        std::string const& secret);

  /// Clear the cache of username / password auth
  void invalidateBasicCache();

  /// set new jwt secret, regenerate _jetToken
  void setJwtSecret(std::string const&);
  std::string jwtSecret() const;
  /// Get the jwt token, which should be used for communication
  std::string const& jwtToken() const noexcept {
    TRI_ASSERT(!_jwtToken.empty());
    return _jwtToken;
  }

  std::string generateRawJwt(velocypack::Slice const&) const;
  std::string generateJwt(velocypack::Slice const&) const;

 private:
  /// Check basic HTTP Authentication header
  TokenCache::Entry checkAuthenticationBasic(std::string const& secret);
  /// Check JWT token contents
  TokenCache::Entry checkAuthenticationJWT(std::string const& secret);

  bool validateJwtHeader(std::string const&);
  TokenCache::Entry validateJwtBody(std::string const&);
  bool validateJwtHMAC256Signature(std::string const&, std::string const&);

  std::shared_ptr<velocypack::Builder> parseJson(std::string const&, std::string const&);

  /// generate new _jwtToken
  void generateJwtToken();

 private:
  auth::UserManager* const _userManager;
  /// Timeout in seconds
  double const _authTimeout;

  mutable arangodb::basics::ReadWriteLock _basicLock;
  std::atomic<uint64_t> _basicCacheVersion;
  std::unordered_map<std::string, TokenCache::Entry> _basicCache;

  std::string _jwtSecret;
  std::string _jwtToken;

  mutable arangodb::basics::ReadWriteLock _jwtLock;
  arangodb::basics::LruCache<std::string, TokenCache::Entry> _jwtCache;
};
}  // namespace auth
}  // namespace arangodb

#endif
