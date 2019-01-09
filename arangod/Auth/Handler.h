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

  HandlerResult(std::set<std::string> const& roles, auth::Source const& source)
      : Result(TRI_ERROR_NO_ERROR), _authSource(source), _roles(roles) {}

 public:
  arangodb::auth::Source source() const { return _authSource; }

  std::set<std::string> roles() const { return _roles; }

 protected:
  arangodb::auth::Source _authSource;
  std::set<std::string> _roles;
};

class Handler {
 public:
  /// Refresh rate for users from this source in seconds
  virtual double refreshRate() const = 0;
  virtual auth::Source source() const = 0;

  /// Authenticate user and return user permissions and roles
  virtual HandlerResult authenticate(std::string const& username,
                                     std::string const& password) = 0;
  /// Read user permissions assuming he was already authenticated once
  virtual HandlerResult readPermissions(std::string const& username) = 0;
  virtual ~Handler() {}
};

}  // namespace auth
}  // namespace arangodb

#endif
