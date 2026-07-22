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

}  // namespace resources

using Resource =
    std::variant<resources::NoResource, resources::Database,
                 resources::Collection, resources::View, resources::Analyzer,
                 resources::Graph, resources::User>;

// A single authorization question: may the token perform `action` on
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

}  // namespace arangodb::rbac
