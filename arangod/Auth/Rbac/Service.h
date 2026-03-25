////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2014-2026 ArangoDB GmbH, Cologne, Germany
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
/// @author Tobias Gödderz
////////////////////////////////////////////////////////////////////////////////

#pragma once

#include "Async/async.h"
#include "Auth/Rbac/Actions.h"
#include "Basics/ResultT.h"

#include <string>
#include <variant>
#include <vector>

namespace arangodb::rbac {

struct Service {
  virtual ~Service() = default;

  struct User {
    std::string jwtToken;
  };

  struct AuthorizationQuery {
    std::string action;
    std::string resource;
  };

  // Translates a Category into the corresponding authorization query.
  // Currently each Category maps to exactly one AuthorizationQuery.
  static auto toAuthorizationQueries(Category::Any const& category)
      -> std::vector<AuthorizationQuery>;

  auto may(User user, Category::Any const& category) noexcept
      -> async<ResultT<bool>>;

  [[deprecated("Use the asynchronous counterpart instead")]] auto maySync(
      User user, Category::Any const& category) noexcept -> ResultT<bool>;

  // TODO We might want to change the return type in a way that it reports
  //      which permission(s) are missing, in order to give a proper error
  //      message to the user.
  auto mayAll(User user, std::vector<Category::Any> categories) noexcept
      -> async<ResultT<bool>>;

  [[deprecated("Use the asynchronous counterpart instead")]] auto mayAllSync(
      User user, std::vector<Category::Any> categories) noexcept
      -> ResultT<bool>;

 private:
  virtual auto mayImpl(User user,
                       std::vector<AuthorizationQuery> queries) noexcept
      -> async<ResultT<bool>> = 0;
  virtual auto maySyncImpl(User user,
                           std::vector<AuthorizationQuery> queries) noexcept
      -> ResultT<bool> = 0;
};

}  // namespace arangodb::rbac
