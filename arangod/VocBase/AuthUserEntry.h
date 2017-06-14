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

#ifndef ARANGOD_VOC_BASE_AUTH_H
#define ARANGOD_VOC_BASE_AUTH_H 1

#include <unordered_map>
#include <velocypack/Builder.h>

namespace arangodb {

enum class AuthLevel {
  NONE, RO, RW
};

enum class AuthSource {
  COLLECTION, LDAP
};

class AuthContext;

class AuthUserEntry {
 public:
  AuthUserEntry()
      : _active(false), 
        _mustChange(false), 
        _created(TRI_microtime()), 
        _source(AuthSource::COLLECTION) {}

  AuthUserEntry(std::string&& username, std::string&& passwordMethod,
            std::string&& passwordSalt, std::string&& passwordHash,
            bool active, bool mustChange, AuthSource source,
            std::unordered_map<std::string, std::shared_ptr<AuthContext>>&& authContexts)
      : _username(std::move(username)),
        _passwordMethod(std::move(passwordMethod)),
        _passwordSalt(std::move(passwordSalt)),
        _passwordHash(std::move(passwordHash)),
        _active(active),
        _mustChange(mustChange),
        _created(TRI_microtime()),
        _source(source),
        _authContexts(std::move(authContexts)) {}
  
  AuthUserEntry(AuthEntry const& other) = delete;

  AuthUserEntry(AuthEntry&& other) noexcept
      : _username(std::move(other._username)),
        _passwordMethod(std::move(other._passwordMethod)),
        _passwordSalt(std::move(other._passwordSalt)),
        _passwordHash(std::move(other._passwordHash)),
        _active(other._active),
        _mustChange(other._mustChange),
        _created(other._created),
        _source(other._source),
        _authContexts(std::move(other._authContexts)) {}

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
  
  std::shared_ptr<AuthContext> getAuthContext(std::string const& database) const;
  //velocypack::Builder toVPackBuilder() const;

 private:
  std::string const _username;
  std::string const _passwordMethod;
  std::string const _passwordSalt;
  std::string const _passwordHash;
  bool const _active;
  bool _mustChange;
  double _created;
  AuthSource _source;
  std::unordered_map<std::string, std::shared_ptr<AuthContext>> _authContexts;
};

}

#endif
