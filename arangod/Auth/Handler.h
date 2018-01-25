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

#ifndef ARANGOD_AUTHENTICATION_HANDLER_H
#define ARANGOD_AUTHENTICATION_HANDLER_H 1

#include "Auth/Common.h"
#include "Basics/Result.h"

#include <velocypack/Slice.h>

namespace arangodb {
namespace auth {

class HandlerResult : public arangodb::Result {
 public:
  explicit HandlerResult(arangodb::auth::Source const& source)
      : HandlerResult(TRI_ERROR_FAILED, source) {}

  HandlerResult(int errorNumber, arangodb::auth::Source const& source)
      : Result(errorNumber), _authSource(source) {}

  HandlerResult(std::unordered_map<std::string, auth::Level> const& permissions,
                std::unordered_set<std::string> const& roles,
                auth::Source const& source)
      : Result(0),
        _authSource(source),
        _permissions(permissions),
        _roles(roles) {}

 public:
  arangodb::auth::Source source() const { return _authSource; }

  std::unordered_map<std::string, auth::Level> permissions() const {
    return _permissions;
  }

  std::unordered_set<std::string> roles() const { return _roles; }

 protected:
  arangodb::auth::Source _authSource;
  std::unordered_map<std::string, auth::Level> _permissions;
  std::unordered_set<std::string> _roles;
};

class Handler {
 public:
  virtual HandlerResult authenticate(std::string const& username,
                                     std::string const& password) = 0;
  virtual ~Handler() {}
};

class DefaultHandler : public Handler {
 public:
  DefaultHandler() {}
  HandlerResult authenticate(std::string const& username,
                             std::string const& password) override;
  virtual ~DefaultHandler() {}
};
}  // auth
}  // arangodb

#endif
