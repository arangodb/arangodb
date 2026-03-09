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
#include <iterator>

namespace arangodb::rbac {

auto Service::toAuthorizationQueries(Permission permission,
                                     Category::Any const& category)
    -> std::vector<AuthorizationQuery> {
  using namespace std::string_view_literals;
  auto const perm = permission == Permission::Read ? "Read"sv : "Write"sv;

  // TODO For now, I've done this as I understand the design document.
  //      However, I currently have doubts that this is how we would like to
  //      implement it: e.g., requiring *write* permissions on both a database
  //      and collection, just to write documents, seems questionable to me.
  //      - Tobias
  // TODO Refactor this to make it a little more readable. Some thoughts:
  //      - Maybe expand the "perm" cases, trading some added redundancy for
  //        readability.
  //      - Maybe factor out the format calls, by e.g. introducing one local
  //        lambda that dispatches actions, and one for resources, and reuse
  //        them in this dispatch.
  return std::visit(
      overload{
          [&](Category::Databases const&) -> std::vector<AuthorizationQuery> {
            return {{std::format("db:{}Databases", perm), ""}};
          },
          [&](Category::Database const& c) -> std::vector<AuthorizationQuery> {
            return {{std::format("db:{}Database", perm),
                     std::format("db:database:{}", c.name)}};
          },
          [&](Category::Collection const& c)
              -> std::vector<AuthorizationQuery> {
            return {{std::format("db:{}Collection", perm),
                     std::format("db:collection:{}:{}", c.database, c.name)},
                    {std::format("db:{}Database", perm),
                     std::format("db:database:{}", c.database)}};
          },
          [&](Category::View const& c) -> std::vector<AuthorizationQuery> {
            return {{std::format("db:{}View", perm),
                     std::format("db:view:{}:{}", c.database, c.name)},
                    {std::format("db:{}Database", perm),
                     std::format("db:database:{}", c.database)}};
          },
          [&](Category::Analyzer const& c) -> std::vector<AuthorizationQuery> {
            return {{std::format("db:{}Analyzer", perm),
                     std::format("db:analyzer:{}:{}", c.database, c.name)},
                    {std::format("db:{}Database", perm),
                     std::format("db:database:{}", c.database)}};
          },
          [&](Category::Documents const& c) -> std::vector<AuthorizationQuery> {
            return {
                {std::format("db:{}Documents", perm),
                 std::format("db:collection:{}:{}", c.database, c.collection)},
                {std::format("db:{}Collection", perm),
                 std::format("db:collection:{}:{}", c.database, c.collection)},
                {std::format("db:{}Database", perm),
                 std::format("db:database:{}", c.database)}};
          },
      },
      category);
}

auto Service::may(User user, Permission permission,
                  Category::Any const& category) noexcept
    -> async<ResultT<bool>> {
  auto queries = toAuthorizationQueries(permission, category);
  return mayImpl(std::move(user), std::move(queries));
}

auto Service::maySync(Service::User user, Service::Permission permission,
                      Service::Category::Any const& category) noexcept
    -> ResultT<bool> {
  auto queries = toAuthorizationQueries(permission, category);
  return maySyncImpl(std::move(user), std::move(queries));
}

auto Service::mayAll(User user, std::vector<PermissionQuery> queries) noexcept
    -> async<ResultT<bool>> {
  std::vector<AuthorizationQuery> authQueries;
  for (auto& q : queries) {
    auto expanded = toAuthorizationQueries(q.permission, q.category);
    authQueries.insert(authQueries.end(),
                       std::make_move_iterator(expanded.begin()),
                       std::make_move_iterator(expanded.end()));
  }
  return mayImpl(std::move(user), std::move(authQueries));
}

auto Service::mayAllSync(User user,
                         std::vector<PermissionQuery> queries) noexcept
    -> ResultT<bool> {
  std::vector<AuthorizationQuery> authQueries;
  for (auto& q : queries) {
    auto expanded = toAuthorizationQueries(q.permission, q.category);
    authQueries.insert(authQueries.end(),
                       std::make_move_iterator(expanded.begin()),
                       std::make_move_iterator(expanded.end()));
  }
  return maySyncImpl(std::move(user), std::move(authQueries));
}

}  // namespace arangodb::rbac
