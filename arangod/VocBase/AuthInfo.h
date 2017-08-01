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

#ifndef ARANGOD_VOC_BASE_AUTH_INFO_H
#define ARANGOD_VOC_BASE_AUTH_INFO_H 1

#include "ApplicationFeatures/ApplicationFeature.h"
#include "Aql/QueryRegistry.h"
#include "Basics/Common.h"
#include "Basics/LruCache.h"
#include "Basics/Mutex.h"
#include "Basics/ReadWriteLock.h"
#include "Basics/Result.h"
#include "Rest/CommonDefines.h"
#include "Utils/Authentication.h"
#include "Utils/ExecContext.h"
#include "VocBase/AuthUserEntry.h"

#include <velocypack/Builder.h>
#include <velocypack/Slice.h>
#include <chrono>

namespace arangodb {

class AuthResult {
 public:
  AuthResult() : _authorized(false) {}

  explicit AuthResult(std::string const& username)
      : _username(username), _authorized(false) {}

  std::string _username;
  bool _authorized;
};

class AuthJwtResult : public AuthResult {
 public:
  AuthJwtResult() : AuthResult(), _expires(false) {}
  bool _expires;
  std::chrono::system_clock::time_point _expireTime;
};

class AuthenticationHandler;

class AuthInfo {
 public:
  AuthInfo(std::unique_ptr<AuthenticationHandler>&&);
  ~AuthInfo();

 public:
  void setQueryRegistry(aql::QueryRegistry* registry) {
    TRI_ASSERT(registry != nullptr);
    _queryRegistry = registry;
  }

  /// Tells coordinator to reload his data. Only call in HearBeat thread
  void outdate() { _outdated = true; }

  /// Trigger eventual reload, user facing API call
  void reloadAllUsers();

  VPackBuilder allUsers();
  /// Add user from arangodb, do not use for LDAP  users
  Result storeUser(bool replace, std::string const& user,
                   std::string const& pass, bool active);
  Result enumerateUsers(std::function<void(AuthUserEntry&)> const& func);
  Result updateUser(std::string const& username,
                    std::function<void(AuthUserEntry&)> const&);
  Result accessUser(std::string const& username,
                  std::function<void(AuthUserEntry const&)> const&);
  velocypack::Builder serializeUser(std::string const& user);
  Result removeUser(std::string const& user);
  Result removeAllUsers();

  velocypack::Builder getConfigData(std::string const& user);
  Result setConfigData(std::string const& user, velocypack::Slice const& data);
  velocypack::Builder getUserData(std::string const& user);
  Result setUserData(std::string const& user, velocypack::Slice const& data);

  AuthResult checkPassword(std::string const& username,
                           std::string const& password);

  AuthResult checkAuthentication(arangodb::rest::AuthenticationMethod authType,
                                 std::string const& secret);

  AuthLevel canUseDatabase(std::string const& username,
                           std::string const& dbname);
  AuthLevel canUseCollection(std::string const& username,
                             std::string const& dbname,
                             std::string const& coll);

  void setJwtSecret(std::string const&);
  std::string jwtSecret();
  std::string generateJwt(VPackBuilder const&);
  std::string generateRawJwt(VPackBuilder const&);
  /*
    std::shared_ptr<AuthContext> getAuthContext(std::string const& username,
                                                std::string const& database);*/

 private:
  void loadFromDB();
  bool parseUsers(velocypack::Slice const& slice);
  void insertInitial();
  Result storeUserInternal(AuthUserEntry const& user, bool replace);

  AuthResult checkAuthenticationBasic(std::string const& secret);
  AuthResult checkAuthenticationJWT(std::string const& secret);
  bool validateJwtHeader(std::string const&);
  AuthJwtResult validateJwtBody(std::string const&);
  bool validateJwtHMAC256Signature(std::string const&, std::string const&);
  std::shared_ptr<VPackBuilder> parseJson(std::string const&,
                                          std::string const&);

 private:
  basics::ReadWriteLock _authInfoLock;
  basics::ReadWriteLock _authJwtLock;
  Mutex _loadFromDBLock;
  std::atomic<bool> _outdated;

  std::unordered_map<std::string, AuthUserEntry> _authInfo;
  std::unordered_map<std::string, arangodb::AuthResult> _authBasicCache;
  arangodb::basics::LruCache<std::string, arangodb::AuthJwtResult>
      _authJwtCache;
  std::string _jwtSecret;
  aql::QueryRegistry* _queryRegistry;
  std::unique_ptr<AuthenticationHandler> _authenticationHandler;
};
}

#endif
