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

#include <velocypack/Builder.h>
#include <velocypack/velocypack-aliases.h>

#include "Basics/ReadWriteLock.h"

namespace arangodb {
namespace velocypack {
class Slice;
}

enum class AuthLevel {
  NONE, RO, RW
};
  
class AuthEntry {
 public:
  AuthEntry() : _active(false), _mustChange(false), _allDatabases(AuthLevel::NONE) {}

  AuthEntry(std::string const& username, std::string const& passwordMethod,
            std::string const& passwordSalt, std::string const& passwordHash,
            std::unordered_map<std::string, AuthLevel> const& databases, AuthLevel allDatabases,
            bool active, bool mustChange)
      : _username(username),
        _passwordMethod(passwordMethod),
        _passwordSalt(passwordSalt),
        _passwordHash(passwordHash),
        _active(active),
        _mustChange(mustChange),
        _databases(databases),
        _allDatabases(allDatabases) {}

 public:
  std::string const& username() const { return _username; }
  std::string const& passwordMethod() const { return _passwordMethod; }
  std::string const& passwordSalt() const { return _passwordSalt; }
  std::string const& passwordHash() const { return _passwordHash; }
  bool isActive() const { return _active; }
  bool mustChange() const { return _mustChange; }

  bool checkPasswordHash(std::string const& hash) const {
    return _passwordHash == hash;
  }

  AuthLevel canUseDatabase(std::string const& dbname) const;

 private:
  std::string _username;
  std::string _passwordMethod;
  std::string _passwordSalt;
  std::string _passwordHash;
  bool _active;
  bool _mustChange;
  std::unordered_map<std::string, AuthLevel> _databases;
  AuthLevel _allDatabases;
};

class AuthResult {
 public:
  AuthResult() : _authorized(false), _mustChange(false) {}
  std::string _username;
  bool _authorized;
  bool _mustChange;
};

class AuthInfo {
 public:
  enum class AuthType {
    BASIC, JWT
  };

 public:
  AuthInfo() : _outdated(true) {}
  
 public:
  void outdate() { _outdated = true; }

  AuthResult checkPassword(std::string const& username,
                           std::string const& password);

  AuthResult checkAuthentication(AuthType authType,
                                std::string const& secret);

  AuthLevel canUseDatabase(std::string const& username,
                           std::string const& dbname);

 private:
  void reload();
  void clear();
  void insertInitial();
  bool populate(velocypack::Slice const& slice);

  AuthResult checkAuthenticationBasic(std::string const& secret);
  AuthResult checkAuthenticationJWT(std::string const& secret);
  bool validateJwtHeader(std::string const&);
  bool validateJwtBody(std::string const&, std::string*);
  bool validateJwtHMAC256Signature(std::string const&, std::string const&);
  std::shared_ptr<VPackBuilder> parseJson(std::string const&, std::string const&);

 private:
  basics::ReadWriteLock _authInfoLock;
  std::atomic<bool> _outdated;

  std::unordered_map<std::string, arangodb::AuthEntry> _authInfo;
  std::unordered_map<std::string, arangodb::AuthResult> _authBasicCache;
};
}

#endif
