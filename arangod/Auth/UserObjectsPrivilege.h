////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2019 ArangoDB GmbH, Cologne, Germany
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

#include "Auth/Privilege.h"

namespace arangodb {
namespace auth {
class UserObjectsPrivilege : public Privilege {
 public:
  UserObjectsPrivilege(std::string const& username, std::string const& owner)
      : _username(username), _owner(owner) {}

  std::string const& username() const { return _username; }
  std::string const& owner() const { return _owner; }

 private:
  std::string const _username;
  std::string const _owner;
};
}  // namespace auth
}  // namespace arangodb
