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

#pragma once

#include <cstdint>
#include <string>
#include <variant>

namespace arangodb::rbac {

enum class Action {

  /* Resource Actions */
  Create,
  Drop,
  Read,
  WriteMeta,
  WriteData,
  UseApiVersion,

  /* Admin Actions */
  AdminMoveShards = 100,
  AdminMonitoring,
  AdminMonitoringInternal,
  AdminCompaction,
  AdminAuthReload,
  AdminCrashHandler,
  AdminApiCalls,
  AdminAqlQueries,
  AdminShutdown,
  AdminReadLogs,
  AdminSetLogLevel,
  AdminOptions,
  AdminSupervisionState,
  AdminRemoveServer,
  AdminClusterInfo,
  AdminMaintenance,
  AdminRebalance,
  AdminLicense,
  AdminBackup,
  AdminJobs,
  AdminReadReplicatedLog,
  AdminWriteReplicatedLog,
  AdminDump,
  AdminRestore,
  AdminWalAccess,
  AdminReadAgency,
  AdminReadOnlyMode,
  AdminReadAqlFunctions,
  AdminWriteAqlFunctions,
  AdminQueryCache,
  AdminReadUsers,
};

namespace resources {

struct NoResource {};

struct Database {
  std::string_view name;
};

struct Collection {
  std::string_view db;
  std::string_view name;
};

struct View {
  std::string_view db;
  std::string_view name;
};

struct Analyzer {
  std::string_view db;
  std::string_view name;
};

struct Graph {
  std::string_view db;
  std::string_view name;
};

struct User {
  std::string_view name;
};

struct ApiVersion {
  uint32_t version;
};

}  // namespace resources

using Resource =
    std::variant<resources::NoResource, resources::Database,
                 resources::Collection, resources::View, resources::Analyzer,
                 resources::Graph, resources::User, resources::ApiVersion>;

// A single authorization question: may the subject perform `action` on
// `resource`? A permission check may consist of several of these, which are
// evaluated together (see Service::check).
struct ActionResource {
  Action action;
  Resource resource;
};

// The JWT that identifies the caller of an authorization request. Owns its
// string so it can be held across a network round-trip (including the
// asynchronous path).
struct JwtToken {
  std::string jwtToken;
};

// The name of a caller that authenticated without a JWT, i.e. via HTTP Basic
// or a personal access token. arangod has already verified the credentials at
// this point; the authorization service resolves the user's bindings by name.
// Owns its string for the same reason as JwtToken.
struct Username {
  std::string name;
};

// Who a permission check is asked for. Also selects the endpoint used to ask
// it: a JwtToken goes to evaluate-token-many, a Username to evaluate-many.
using Subject = std::variant<JwtToken, Username>;

}  // namespace arangodb::rbac
