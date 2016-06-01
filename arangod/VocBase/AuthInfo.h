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

#include "Basics/ReadWriteLock.h"

namespace arangodb {
namespace velocypack {
class Slice;
}

class AuthResult {
 public:
  std::string _username;
  bool _authorized;
  bool _mustChange;
};

class AuthEntry {
 public:
  AuthEntry() : _active(false), _mustChange(false) {}

  AuthEntry(std::string const& username, std::string const& passwordMethod,
            std::string const& passwordSalt, std::string const& passwordHash,
            bool active, bool mustChange)
      : _username(username),
        _passwordMethod(passwordMethod),
        _passwordSalt(passwordSalt),
        _passwordHash(passwordHash),
        _active(active),
        _mustChange(mustChange) {}

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

 private:
  std::string _username;
  std::string _passwordMethod;
  std::string _passwordSalt;
  std::string _passwordHash;
  bool _active;
  bool _mustChange;
};

class AuthCache {
 public:
  AuthCache(std::string const& authorizationField, AuthEntry const& authEntry,
            double expires)
      : _authorizationField(authorizationField),
        _username(authEntry.username()),
        _mustChange(authEntry.mustChange()),
        _expires(expires) {}

 public:
  std::string const& username() const { return _username; }
  bool mustChange() const { return _mustChange; }

 private:
  std::string const _authorizationField;
  std::string const _username;
  bool const _mustChange;
  double const _expires;
};

class AuthInfo {
 public:
  bool canUseDatabase(std::string const& username, char const* databaseName);

  AuthResult checkAuthentication(std::string const& authorizationField,
                                 char const* databaseName);

  bool reload();
  bool insertInitial();
  bool populate(velocypack::Slice const& slice);

 private:
  void clear();
  std::string checkCache(std::string const& authorizationField,
                         bool* mustChange);

 private:
  std::unordered_map<std::string, arangodb::AuthEntry> _authInfo;
  std::unordered_map<std::string, arangodb::AuthCache> _authCache;
  basics::ReadWriteLock _authInfoLock;
  bool _authInfoLoaded = false;
};
}

#endif
