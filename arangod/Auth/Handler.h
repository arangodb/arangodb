////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2014-2024 ArangoDB GmbH, Cologne, Germany
/// Copyright 2004-2014 triAGENS GmbH, Cologne, Germany
///
/// Licensed under the Business Source License 1.1 (the "License");
/// you may not use this file except in compliance with the License.
/// You may obtain a copy of the License at
///
///     https://github.com/arangodb/arangodb/blob/devel/LICENSE
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

#pragma once

#include "Auth/Common.h"
#include "Basics/Result.h"
#include "Basics/voc-errors.h"

#include <velocypack/Slice.h>
#include <set>

namespace arangodb {
namespace auth {

class HandlerResult {
 public:
  explicit HandlerResult() : HandlerResult(TRI_ERROR_FAILED) {}

  HandlerResult(ErrorCode errorNumber) : _result(errorNumber) {}

  HandlerResult(std::set<std::string> const& roles)
      : _result(TRI_ERROR_NO_ERROR), _roles(roles) {}

  std::set<std::string> roles() const { return _roles; }

  // forwarded methods
  bool ok() const { return _result.ok(); }
  bool fail() const { return _result.fail(); }
  ErrorCode errorNumber() const { return _result.errorNumber(); }
  std::string_view errorMessage() const { return _result.errorMessage(); }

 protected:
  Result _result;
  std::set<std::string> _roles;
};

class Handler {
 public:
  /// Refresh rate for users from this source in seconds
  virtual double refreshRate() const = 0;
  virtual bool allowOfflineCacheUsage() const = 0;

  /// Authenticate user and return user permissions and roles
  virtual HandlerResult authenticate(std::string const& username,
                                     std::string const& password) = 0;
  /// Read user permissions assuming he was already authenticated once
  virtual HandlerResult readPermissions(std::string const& username) = 0;
  virtual ~Handler() = default;
};

}  // namespace auth
}  // namespace arangodb
