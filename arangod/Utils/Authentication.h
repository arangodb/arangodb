////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2017 ArangoDB GmbH, Cologne, Germany
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
/// @author Manuel Baesler
/// @author Simon Grätzer
////////////////////////////////////////////////////////////////////////////////

#ifndef UTILS_AUTHENTICATION_HANDLER_H
#define UTILS_AUTHENTICATION_HANDLER_H 1

#include <velocypack/Slice.h>
#include "Basics/Result.h"

namespace arangodb {

enum class AuthLevel { NONE, RO, RW };
enum class AuthSource { COLLECTION, LDAP };

AuthLevel convertToAuthLevel(velocypack::Slice grants);
AuthLevel convertToAuthLevel(std::string const& grant);
std::string convertFromAuthLevel(AuthLevel lvl);

class AuthenticationResult : public arangodb::Result {
 public:
  explicit AuthenticationResult(AuthSource const& source)
      : AuthenticationResult(TRI_ERROR_FAILED, source) {}

  AuthenticationResult(int errorNumber, AuthSource const& source)
      : Result(errorNumber), _authSource(source) {}
  AuthenticationResult(
      std::unordered_map<std::string, std::string> const& permissions,
      AuthSource const& source)
      : Result(0), _authSource(source), _permissions(permissions) {}

  AuthSource source() const { return _authSource; }
  std::unordered_map<std::string, std::string> permissions() const {
    return _permissions;
  }

 protected:
  AuthSource _authSource;
  std::unordered_map<std::string, std::string> _permissions;
};

class AuthenticationHandler {
 public:
  virtual AuthenticationResult authenticate(std::string const& username,
                                            std::string const& password) = 0;
  virtual ~AuthenticationHandler() {}
};

class DefaultAuthenticationHandler : public AuthenticationHandler {
 public:
  DefaultAuthenticationHandler() {}
  AuthenticationResult authenticate(std::string const& username,
                                    std::string const& password) override;
  virtual ~DefaultAuthenticationHandler() {}
};
};

#endif
