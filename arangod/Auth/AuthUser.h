////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2017-2019 ArangoDB GmbH, Cologne, Germany
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
/// @author Frank Celler
////////////////////////////////////////////////////////////////////////////////

#pragma once

#include <string>

namespace arangodb {
namespace auth {
class AuthUser {
 public:
  AuthUser(std::string const& username) : _username(username) {}

  std::string const& internalUsername() const { return _username; }

 private:
  std::string const _username;
};
}  // namespace auth
}  // namespace arangodb

/*
class LoginMethod {
 private:
  Authenticator _authenticator;
  Authorizator _authorizator;
};

// ================================================================================

class LoginUser {
 public:
  bool isValidated() const;
  std::string const& externalName() const;
  std::string const& internalName() const;

 private:
  std::set<std::string> _roles;
};

// ================================================================================

class LoginManager {
 public:
  LoginUserResult loginUser(Credentials const&);
  LoginResult validate(LoginUser&);

 private:
  std::vector<LoginMethod> _methods;
};
*/
