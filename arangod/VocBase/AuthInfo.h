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

namespace arangodb {
namespace velocypack {
class Slice;
}

enum class AuthLevel {
  NONE, RO, RW
};

enum class AuthSource {
  COLLECTION, LDAP
};

class HexHashResult : public arangodb::Result {
  public:
    explicit HexHashResult(int errorNumber) : Result(errorNumber) {}
    explicit HexHashResult(std::string const& hexHash) : Result(0),  _hexHash(hexHash) {}
    std::string const& hexHash() { return _hexHash; }

  protected:
    std::string const _hexHash;
};

class AuthContext;

class AuthEntry {
 public:
  AuthEntry() 
      : _active(false), 
        _mustChange(false), 
        _created(TRI_microtime()), 
        _source(AuthSource::COLLECTION), 
        _allDatabases(AuthLevel::NONE) {}

  AuthEntry(std::string&& username, std::string&& passwordMethod,
            std::string&& passwordSalt, std::string&& passwordHash,
            std::unordered_map<std::string, AuthLevel>&& databases, AuthLevel allDatabases,
            bool active, bool mustChange, AuthSource source, std::unordered_map<std::string, std::shared_ptr<AuthContext>>&& authContexts)
      : _username(std::move(username)),
        _passwordMethod(std::move(passwordMethod)),
        _passwordSalt(std::move(passwordSalt)),
        _passwordHash(std::move(passwordHash)),
        _active(active),
        _mustChange(mustChange),
        _created(TRI_microtime()),
        _source(source),
        _databases(std::move(databases)),
        _authContexts(std::move(authContexts)),
        _allDatabases(allDatabases) {}
  
  AuthEntry(AuthEntry const& other) = delete;

  AuthEntry(AuthEntry&& other) noexcept
      : _username(std::move(other._username)),
        _passwordMethod(std::move(other._passwordMethod)),
        _passwordSalt(std::move(other._passwordSalt)),
        _passwordHash(std::move(other._passwordHash)),
        _active(other._active),
        _mustChange(other._mustChange),
        _created(other._created),
        _source(other._source),
        _databases(std::move(other._databases)),
        _allDatabases(other._allDatabases) {}

 public:
  std::string const& username() const { return _username; }
  std::string const& passwordMethod() const { return _passwordMethod; }
  std::string const& passwordSalt() const { return _passwordSalt; }
  std::string const& passwordHash() const { return _passwordHash; }
  bool isActive() const { return _active; }
  bool mustChange() const { return _mustChange; }
  double created() const { return _created; }
  AuthSource source() const { return _source; }

  bool checkPasswordHash(std::string const& hash) const {
    return _passwordHash == hash;
  }

  AuthLevel canUseDatabase(std::string const& dbname) const;

 private:
  std::string const _username;
  std::string const _passwordMethod;
  std::string const _passwordSalt;
  std::string const _passwordHash;
  bool const _active;
  bool _mustChange;
  double _created;
  AuthSource _source;
  std::unordered_map<std::string, AuthLevel> const _databases;
  std::unordered_map<std::string, std::shared_ptr<AuthContext>> _authContexts;
  AuthLevel const _allDatabases;
};

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

  AuthResult checkPassword(std::string const& username,
                           std::string const& password);

  AuthResult checkAuthentication(AuthType authType,
                                std::string const& secret);

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
  bool populate(velocypack::Slice const& slice);

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

  std::unordered_map<std::string, arangodb::AuthEntry> _authInfo;
  std::unordered_map<std::string, arangodb::AuthResult> _authBasicCache;
  arangodb::basics::LruCache<std::string, arangodb::AuthJwtResult> _authJwtCache;
  std::string _jwtSecret;
  aql::QueryRegistry* _queryRegistry;
  std::unique_ptr<AuthenticationHandler> _authenticationHandler;
};
}

#endif
