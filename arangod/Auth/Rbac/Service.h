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
#include "Basics/ResultT.h"

#include <string>
#include <variant>

namespace arangodb::rbac {

struct Service {
  virtual ~Service() = default;

  struct User {
    std::string jwtToken;
  };

  struct Category {
    struct Databases {};
    struct Database {
      std::string name;
    };
    struct Collection {
      std::string database;
      std::string name;
    };
    struct View {
      std::string database;
      std::string name;
    };
    struct Analyzer {
      std::string database;
      std::string name;
    };
    struct Documents {
      std::string database;
      std::string collection;
    };
    using Any = std::variant<Databases, Database, Collection, View, Analyzer,
                             Documents>;
  };

  enum class Permission { Read, Write };

  struct AuthorizationQuery {
    std::string action;
    std::string resource;
  };

  static auto toAuthorizationQuery(Permission permission,
                                   Category::Any const& category)
      -> AuthorizationQuery;

  auto may(User user, Permission permission,
           Category::Any const& category) noexcept -> async<ResultT<bool>>;

  [[deprecated("Use the asynchronous counterpart instead")]]
  auto maySync(User user, Permission permission,
               Category::Any const& category) noexcept -> ResultT<bool>;

 private:
  virtual auto mayImpl(User user, std::string action,
                       std::string resource) noexcept
      -> async<ResultT<bool>> = 0;
  virtual auto maySyncImpl(User user, std::string action,
                           std::string resource) noexcept -> ResultT<bool> = 0;
};

}  // namespace arangodb::rbac
