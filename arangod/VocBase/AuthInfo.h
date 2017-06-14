////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2014-2016 ArangoDB GmbH, Cologne, Germany
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

#ifndef ARANGOD_VOC_BASE_AUTH_H
#define ARANGOD_VOC_BASE_AUTH_H 1

#include "Basics/Common.h"

#include <chrono>

#include <velocypack/Builder.h>
#include <velocypack/velocypack-aliases.h>

#include "ApplicationFeatures/ApplicationFeature.h"
#include "Aql/QueryRegistry.h"
#include "Basics/Mutex.h"
#include "Basics/LruCache.h"
#include "Basics/ReadWriteLock.h"
#include "Basics/Result.h"
#include "GeneralServer/AuthenticationHandler.h"
#include "Utils/ExecContext.h"
#include "VocBase/AuthUserEntry.h"

namespace arangodb {
namespace velocypack {
class Slice;
}

class HexHashResult : public arangodb::Result {
  public:
    explicit HexHashResult(int errorNumber) : Result(errorNumber) {}
    explicit HexHashResult(std::string const& hexHash) : Result(0),  _hexHash(hexHash) {}
    std::string const& hexHash() { return _hexHash; }

  protected:
    std::string const _hexHash;
};

class AuthContext;

class AuthResult {
 public:
  AuthResult() 
      : _authorized(false), _mustChange(false) {}
  
  explicit AuthResult(std::string const& username) 
      : _username(username), _authorized(false), _mustChange(false) {} 

  std::string _username;
  bool _authorized;
  bool _mustChange;
};

class AuthJwtResult: public AuthResult {
 public:
  AuthJwtResult() : AuthResult(), _expires(false) {}
  bool _expires;
  std::chrono::system_clock::time_point _expireTime;
};


class AuthenticationHandler;

class AuthInfo {
 public:
  enum class AuthType {
    BASIC, JWT
  };

 public:
  AuthInfo();

 public:
  void setQueryRegistry(aql::QueryRegistry* registry) {
    TRI_ASSERT(registry != nullptr);
    _queryRegistry = registry;
  }

  void outdate() { _outdated = true; }
  
  void addUser();

  AuthResult checkPassword(std::string const& username,
                           std::string const& password);

  AuthResult checkAuthentication(AuthType authType,
                                std::string const& secret);
  
  void grantDatabase(std::string const& username,
                     std::string const& dbname,
                     AuthLevel level);
  
  void grantCollection(std::string const& username,
                       std::string const& dbname,
                       std::string const& collection,
                       AuthLevel level);

  AuthLevel canUseDatabase(std::string const& username,
                           std::string const& dbname);
  
  void setJwtSecret(std::string const&);
  std::string jwtSecret();
  std::string generateJwt(VPackBuilder const&);
  std::string generateRawJwt(VPackBuilder const&);

  std::shared_ptr<AuthContext> getAuthContext(std::string const& username, std::string const& database);
 
 private:
  void reload();
  void insertInitial();
  bool parseUsers(velocypack::Slice const& slice);

  AuthResult checkAuthenticationBasic(std::string const& secret);
  AuthResult checkAuthenticationJWT(std::string const& secret);
  bool validateJwtHeader(std::string const&);
  AuthJwtResult validateJwtBody(std::string const&);
  bool validateJwtHMAC256Signature(std::string const&, std::string const&);
  std::shared_ptr<VPackBuilder> parseJson(std::string const&, std::string const&);

  HexHashResult hexHashFromData(std::string const& hashMethod, char const* data, size_t len);


 private:
  basics::ReadWriteLock _authInfoLock;
  basics::ReadWriteLock _authJwtLock;
  Mutex _queryLock;
  std::atomic<bool> _outdated;

  std::unordered_map<std::string, AuthUserEntry> _authInfo;
  std::shared_ptr<AuthContext> _noneAuthContext;
  std::unordered_map<std::string, arangodb::AuthResult> _authBasicCache;
  arangodb::basics::LruCache<std::string, arangodb::AuthJwtResult> _authJwtCache;
  std::string _jwtSecret;
  aql::QueryRegistry* _queryRegistry;
  std::unique_ptr<AuthenticationHandler> _authenticationHandler;
};
}

#endif
