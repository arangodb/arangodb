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
////////////////////////////////////////////////////////////////////////////////

#include "ServiceImpl.h"

#include "Assertions/ProdAssert.h"
#include "Basics/overload.h"
#include "Basics/voc-errors.h"

#include <format>

namespace arangodb::rbac {

namespace {

// The wire string the external RBAC service expects for an action: "db:" plus
// the action name. Resource actions and admin actions share the same prefix.
auto actionToWireString(Action action) -> std::string_view {
  switch (action) {
    case Action::Create:
      return "db:Create";
    case Action::Drop:
      return "db:Drop";
    case Action::Read:
      return "db:Read";
    case Action::WriteMeta:
      return "db:WriteMeta";
    case Action::WriteData:
      return "db:WriteData";
    case Action::UseApiVersion:
      return "db:UseApiVersion";
    case Action::AdminMoveShards:
      return "db:AdminMoveShards";
    case Action::AdminMonitoring:
      return "db:AdminMonitoring";
    case Action::AdminMonitoringInternal:
      return "db:AdminMonitoringInternal";
    case Action::AdminCompaction:
      return "db:AdminCompaction";
    case Action::AdminAuthReload:
      return "db:AdminAuthReload";
    case Action::AdminCrashHandler:
      return "db:AdminCrashHandler";
    case Action::AdminApiCalls:
      return "db:AdminApiCalls";
    case Action::AdminAqlQueries:
      return "db:AdminAqlQueries";
    case Action::AdminShutdown:
      return "db:AdminShutdown";
    case Action::AdminReadLogs:
      return "db:AdminReadLogs";
    case Action::AdminSetLogLevel:
      return "db:AdminSetLogLevel";
    case Action::AdminOptions:
      return "db:AdminOptions";
    case Action::AdminSupervisionState:
      return "db:AdminSupervisionState";
    case Action::AdminRemoveServer:
      return "db:AdminRemoveServer";
    case Action::AdminClusterInfo:
      return "db:AdminClusterInfo";
    case Action::AdminMaintenance:
      return "db:AdminMaintenance";
    case Action::AdminRebalance:
      return "db:AdminRebalance";
    case Action::AdminLicense:
      return "db:AdminLicense";
    case Action::AdminBackup:
      return "db:AdminBackup";
    case Action::AdminJobs:
      return "db:AdminJobs";
    case Action::AdminReadReplicatedLog:
      return "db:AdminReadReplicatedLog";
    case Action::AdminWriteReplicatedLog:
      return "db:AdminWriteReplicatedLog";
    case Action::AdminDump:
      return "db:AdminDump";
    case Action::AdminRestore:
      return "db:AdminRestore";
    case Action::AdminWalAccess:
      return "db:AdminWalAccess";
    case Action::AdminReadAgency:
      return "db:AdminReadAgency";
    case Action::AdminReadOnlyMode:
      return "db:AdminReadOnlyMode";
    case Action::AdminReadAqlFunctions:
      return "db:AdminReadAqlFunctions";
    case Action::AdminWriteAqlFunctions:
      return "db:AdminWriteAqlFunctions";
    case Action::AdminQueryCache:
      return "db:AdminQueryCache";
    case Action::AdminReadUsers:
      return "db:AdminReadUsers";
  }
  ADB_PROD_CRASH();
}

// The wire string the external RBAC service expects for a resource. Admin
// actions carry no resource, which is represented as the empty string.
auto resourceToWireString(Resource const& resource) -> std::string {
  return std::visit(
      overload{
          [](resources::NoResource const&) { return std::string{}; },
          [](resources::Database const& r) {
            return std::format("db:database:{}", r.name);
          },
          [](resources::Collection const& r) {
            return std::format("db:collection:{}:{}", r.db, r.name);
          },
          [](resources::View const& r) {
            return std::format("db:view:{}:{}", r.db, r.name);
          },
          [](resources::Analyzer const& r) {
            return std::format("db:analyzer:{}:{}", r.db, r.name);
          },
          [](resources::Graph const& r) {
            return std::format("db:graph:{}:{}", r.db, r.name);
          },
          [](resources::User const& r) {
            return std::format("db:user:{}", r.name);
          },
          [](resources::ApiVersion const& r) {
            return std::format("db:apiversion:v{}", r.version);
          },
      },
      resource);
}

}  // namespace

ServiceImpl::ServiceImpl(std::unique_ptr<Backend> backend)
    : _backend(std::move(backend)) {}

auto ServiceImpl::check(JwtToken const& token,
                        std::span<ActionResource const> queries) noexcept
    -> Result {
  // An empty batch asks nothing, so it is trivially permitted; short-circuit to
  // avoid a needless network round-trip.
  if (queries.empty()) {
    return {};
  }

  Backend::RequestItems items;
  items.items.reserve(queries.size());
  for (auto const& query : queries) {
    items.items.push_back(Backend::RequestItem{
        .action = std::string{actionToWireString(query.action)},
        .resource = resourceToWireString(query.resource),
        .attributeValues = {}});
  }

  // Service::check (and the whole IAuth::check chain) is synchronous for now,
  // so we use the synchronous backend call directly.
  auto result = _backend->evaluateTokenManySync(token, items);

  if (!result.ok()) {
    // Transport or parsing error: propagate it verbatim.
    return result.result();
  }
  auto const& response = result.get();
  if (response.effect == Backend::Effect::Allow) {
    return {};
  }
  return {TRI_ERROR_FORBIDDEN, response.message.empty()
                                   ? "insufficient permissions"
                                   : response.message};
}

}  // namespace arangodb::rbac
