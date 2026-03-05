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
#include "Service.h"

#include "Basics/overload.h"

#include <format>

namespace arangodb::rbac {

auto Service::toAuthorizationQuery(Permission permission,
                                   Category::Any const& category)
    -> AuthorizationQuery {
  using namespace std::string_view_literals;
  auto const perm = permission == Permission::Read ? "Read"sv : "Write"sv;

  return std::visit(
      overload{
          [&](Category::Databases const&) -> AuthorizationQuery {
            return {std::format("db:{}Databases", perm), ""};
          },
          [&](Category::Database const& c) -> AuthorizationQuery {
            return {std::format("db:{}Database", perm),
                    std::format("db:database:{}", c.name)};
          },
          [&](Category::Collection const& c) -> AuthorizationQuery {
            return {std::format("db:{}Collection", perm),
                    std::format("db:collection:{}:{}", c.database, c.name)};
          },
          [&](Category::View const& c) -> AuthorizationQuery {
            return {std::format("db:{}View", perm),
                    std::format("db:view:{}:{}", c.database, c.name)};
          },
          [&](Category::Analyzer const& c) -> AuthorizationQuery {
            return {std::format("db:{}Analyzer", perm),
                    std::format("db:analyzer:{}:{}", c.database, c.name)};
          },
          [&](Category::Documents const& c) -> AuthorizationQuery {
            return {std::format("db:{}Documents", perm),
                    std::format("db:collection:{}:{}", c.database,
                                c.collection)};
          },
      },
      category);
}

auto Service::may(User user, Permission permission,
                  Category::Any category) noexcept -> ResultT<bool> {
  auto query = toAuthorizationQuery(permission, category);
  return mayImpl(std::move(user), std::move(query.action),
                 std::move(query.resource));
}

}  // namespace arangodb::rbac
