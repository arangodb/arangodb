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

#ifndef ARANGOD_VOC_BASE_AUTH_USER_H
#define ARANGOD_VOC_BASE_AUTH_USER_H 1

#include <velocypack/Builder.h>
#include <velocypack/Slice.h>
#include <unordered_map>

namespace arangodb {

enum class AuthLevel { NONE, RO, RW };

AuthLevel convertToAuthLevel(velocypack::Slice grants);
AuthLevel convertToAuthLevel(std::string grant);
std::string convertFromAuthLevel(AuthLevel lvl);

enum class AuthSource { COLLECTION, LDAP };

class AuthContext;

class AuthUserEntry {
  friend class AuthInfo;

 public:
  std::string const& key() const { return _key; }
  std::string const& username() const { return _username; }
  std::string const& passwordMethod() const { return _passwordMethod; }
  std::string const& passwordSalt() const { return _passwordSalt; }
  std::string const& passwordHash() const { return _passwordHash; }
  bool isActive() const { return _active; }
  bool mustChangePassword() const { return _changePassword; }
  AuthSource source() const { return _source; }

  bool checkPassword(std::string const& password) const;
  void updatePassword(std::string const& password);

  std::shared_ptr<AuthContext> getAuthContext(
      std::string const& database) const;
  velocypack::Builder toVPackBuilder() const;

  void setActive(bool active) { _active = active; }
  void changePassword(bool c) { _changePassword = c; }

  void grantDatabase(std::string const& dbname, AuthLevel level);
  void removeDatabase(std::string const& dbname);
  void grantCollection(std::string const& dbname, std::string const& collection,
                       AuthLevel level);
  void removeCollection(std::string const& dbname,
                        std::string const& collection);

  static AuthUserEntry newUser(std::string const& user, std::string const& pass,
                               AuthSource source);
  static AuthUserEntry fromDocument(velocypack::Slice const&);

 private:
  AuthUserEntry() {}

 private:
  std::string _key;
  bool _active = true;
  AuthSource _source = AuthSource::COLLECTION;

  std::string _username;
  std::string _passwordMethod;
  std::string _passwordSalt;
  std::string _passwordHash;
  std::string _passwordChangeToken;
  bool _changePassword;

  std::unordered_map<std::string, std::shared_ptr<AuthContext>> _authContexts;
};
}

#endif
