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

auto Service::toAuthorizationQueries(Category::Any const& category)
    -> std::vector<AuthorizationQuery> {
  return std::visit(
      overload{
          [](Category::ReadDatabase const& c)
              -> std::vector<AuthorizationQuery> {
            return {{"db:ReadDatabase", std::format("db:database:{}", c.name)}};
          },
          [](Category::WriteDatabase const& c)
              -> std::vector<AuthorizationQuery> {
            return {
                {"db:WriteDatabase", std::format("db:database:{}", c.name)}};
          },
          [](Category::ReadCollection const& c)
              -> std::vector<AuthorizationQuery> {
            return {{"db:ReadCollection",
                     std::format("db:collection:{}:{}", c.database, c.name)}};
          },
          [](Category::WriteCollectionData const& c)
              -> std::vector<AuthorizationQuery> {
            return {{"db:WriteCollectionData",
                     std::format("db:collection:{}:{}", c.database, c.name)}};
          },
          [](Category::WriteCollectionMeta const& c)
              -> std::vector<AuthorizationQuery> {
            return {{"db:WriteCollectionMeta",
                     std::format("db:collection:{}:{}", c.database, c.name)}};
          },
          [](Category::ReadView const& c) -> std::vector<AuthorizationQuery> {
            return {{"db:ReadView",
                     std::format("db:view:{}:{}", c.database, c.name)}};
          },
          [](Category::WriteView const& c) -> std::vector<AuthorizationQuery> {
            return {{"db:WriteView",
                     std::format("db:view:{}:{}", c.database, c.name)}};
          },
          [](Category::ReadAnalyzer const& c)
              -> std::vector<AuthorizationQuery> {
            return {{"db:ReadAnalyzer",
                     std::format("db:analyzer:{}:{}", c.database, c.name)}};
          },
          [](Category::WriteAnalyzer const& c)
              -> std::vector<AuthorizationQuery> {
            return {{"db:WriteAnalyzer",
                     std::format("db:analyzer:{}:{}", c.database, c.name)}};
          },
          [](Category::UseApiVersion const& c)
              -> std::vector<AuthorizationQuery> {
            return {{"db:UseApiVersion",
                     std::format("db:apiversion:{}", c.version)}};
          },
          [](Category::AdminReadUser const& c)
              -> std::vector<AuthorizationQuery> {
            return {
                {"db:AdminReadUser", std::format("db:user:{}", c.username)}};
          },
          [](Category::AdminWriteUser const& c)
              -> std::vector<AuthorizationQuery> {
            return {
                {"db:AdminWriteUser", std::format("db:user:{}", c.username)}};
          },
          [](Category::AdminMoveShards const&)
              -> std::vector<AuthorizationQuery> {
            return {{"db:AdminMoveShards", ""}};
          },
          [](Category::AdminMonitoring const&)
              -> std::vector<AuthorizationQuery> {
            return {{"db:AdminMonitoring", ""}};
          },
          [](Category::AdminMonitoringInternal const&)
              -> std::vector<AuthorizationQuery> {
            return {{"db:AdminMonitoringInternal", ""}};
          },
          [](Category::AdminCompaction const&)
              -> std::vector<AuthorizationQuery> {
            return {{"db:AdminCompaction", ""}};
          },
          [](Category::AdminAuthReload const&)
              -> std::vector<AuthorizationQuery> {
            return {{"db:AdminAuthReload", ""}};
          },
          [](Category::AdminCrashHandler const&)
              -> std::vector<AuthorizationQuery> {
            return {{"db:AdminCrashHandler", ""}};
          },
          [](Category::AdminApiCalls const&)
              -> std::vector<AuthorizationQuery> {
            return {{"db:AdminApiCalls", ""}};
          },
          [](Category::AdminAqlQueries const&)
              -> std::vector<AuthorizationQuery> {
            return {{"db:AdminAqlQueries", ""}};
          },
          [](Category::AdminShutdown const&)
              -> std::vector<AuthorizationQuery> {
            return {{"db:AdminShutdown", ""}};
          },
          [](Category::AdminReadLogs const&)
              -> std::vector<AuthorizationQuery> {
            return {{"db:AdminReadLogs", ""}};
          },
          [](Category::AdminSetLogLevel const&)
              -> std::vector<AuthorizationQuery> {
            return {{"db:AdminSetLogLevel", ""}};
          },
          [](Category::AdminOptions const&) -> std::vector<AuthorizationQuery> {
            return {{"db:AdminOptions", ""}};
          },
          [](Category::AdminSupervisionState const&)
              -> std::vector<AuthorizationQuery> {
            return {{"db:AdminSupervisionState", ""}};
          },
          [](Category::AdminRemoveServer const&)
              -> std::vector<AuthorizationQuery> {
            return {{"db:AdminRemoveServer", ""}};
          },
          [](Category::AdminClusterInfo const&)
              -> std::vector<AuthorizationQuery> {
            return {{"db:AdminClusterInfo", ""}};
          },
          [](Category::AdminMaintenance const&)
              -> std::vector<AuthorizationQuery> {
            return {{"db:AdminMaintenance", ""}};
          },
          [](Category::AdminRebalance const&)
              -> std::vector<AuthorizationQuery> {
            return {{"db:AdminRebalance", ""}};
          },
          [](Category::AdminLicense const&) -> std::vector<AuthorizationQuery> {
            return {{"db:AdminLicense", ""}};
          },
          [](Category::AdminBackup const&) -> std::vector<AuthorizationQuery> {
            return {{"db:AdminBackup", ""}};
          },
          [](Category::AdminJobs const&) -> std::vector<AuthorizationQuery> {
            return {{"db:AdminJobs", ""}};
          },
          [](Category::AdminTasks const&) -> std::vector<AuthorizationQuery> {
            return {{"db:AdminTasks", ""}};
          },
          [](Category::AdminReadReplicatedLog const&)
              -> std::vector<AuthorizationQuery> {
            return {{"db:AdminReadReplicatedLog", ""}};
          },
          [](Category::AdminWriteReplicatedLog const&)
              -> std::vector<AuthorizationQuery> {
            return {{"db:AdminWriteReplicatedLog", ""}};
          },
          [](Category::AdminDump const&) -> std::vector<AuthorizationQuery> {
            return {{"db:AdminDump", ""}};
          },
          [](Category::AdminRestore const&) -> std::vector<AuthorizationQuery> {
            return {{"db:AdminRestore", ""}};
          },
          [](Category::AdminWalAccess const&)
              -> std::vector<AuthorizationQuery> {
            return {{"db:AdminWalAccess", ""}};
          },
          [](Category::AdminReadAgency const&)
              -> std::vector<AuthorizationQuery> {
            return {{"db:AdminReadAgency", ""}};
          },
          [](Category::AdminReadOnlyMode const&)
              -> std::vector<AuthorizationQuery> {
            return {{"db:AdminReadOnlyMode", ""}};
          },
          [](Category::AdminFoxx const&) -> std::vector<AuthorizationQuery> {
            return {{"db:AdminFoxx", ""}};
          },
          [](Category::AdminReadAqlFunctions const&)
              -> std::vector<AuthorizationQuery> {
            return {{"db:AdminReadAqlFunctions", ""}};
          },
          [](Category::AdminWriteAqlFunctions const&)
              -> std::vector<AuthorizationQuery> {
            return {{"db:AdminWriteAqlFunctions", ""}};
          },
          [](Category::AdminQueryCache const&)
              -> std::vector<AuthorizationQuery> {
            return {{"db:AdminQueryCache", ""}};
          },
      },
      category);
}

auto Service::may(User user, Category::Any const& category) noexcept
    -> async<ResultT<bool>> {
  auto queries = toAuthorizationQueries(category);
  return mayImpl(std::move(user), std::move(queries));
}

auto Service::maySync(Service::User user,
                      Category::Any const& category) noexcept -> ResultT<bool> {
  auto queries = toAuthorizationQueries(category);
  return maySyncImpl(std::move(user), std::move(queries));
}

auto Service::mayAll(User user, std::vector<Category::Any> categories) noexcept
    -> async<ResultT<bool>> {
  std::vector<AuthorizationQuery> authQueries;
  for (auto& c : categories) {
    auto expanded = toAuthorizationQueries(c);
    authQueries.insert(authQueries.end(),
                       std::make_move_iterator(expanded.begin()),
                       std::make_move_iterator(expanded.end()));
  }
  return mayImpl(std::move(user), std::move(authQueries));
}

auto Service::mayAllSync(User user,
                         std::vector<Category::Any> categories) noexcept
    -> ResultT<bool> {
  std::vector<AuthorizationQuery> authQueries;
  for (auto& c : categories) {
    auto expanded = toAuthorizationQueries(c);
    authQueries.insert(authQueries.end(),
                       std::make_move_iterator(expanded.begin()),
                       std::make_move_iterator(expanded.end()));
  }
  return maySyncImpl(std::move(user), std::move(authQueries));
}

}  // namespace arangodb::rbac
