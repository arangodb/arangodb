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
#include <vector>

namespace arangodb::rbac {

struct Service {
  virtual ~Service() = default;

  struct User {
    std::string jwtToken;
  };

  struct Category {
    struct ReadDatabase {
      std::string name;
    };
    struct WriteDatabase {
      std::string name;
    };
    struct ReadCollection {
      std::string database;
      std::string name;
    };
    struct WriteCollectionData {
      std::string database;
      std::string name;
    };
    struct WriteCollectionMeta {
      std::string database;
      std::string name;
    };
    struct ReadView {
      std::string database;
      std::string name;
    };
    struct WriteView {
      std::string database;
      std::string name;
    };
    struct ReadAnalyzer {
      std::string database;
      std::string name;
    };
    struct WriteAnalyzer {
      std::string database;
      std::string name;
    };
    struct UseApiVersion {
      std::string version;
    };
    // Admin actions with a collection resource:
    struct AdminChangeDataDist {
      std::string database;
      std::string collection;
    };
    // Admin actions with a user resource:
    struct AdminReadUser {
      std::string username;
    };
    struct AdminWriteUser {
      std::string username;
    };
    // Admin actions without a resource:
    struct AdminMonitoring {};
    struct AdminMonitoringInternal {};
    struct AdminCompaction {};
    struct AdminAuthReload {};
    struct AdminCrashHandler {};
    struct AdminApiCalls {};
    struct AdminAqlQueries {};
    struct AdminShutdown {};
    struct AdminReadLogs {};
    struct AdminSetLogLevel {};
    struct AdminOptions {};
    struct AdminSupervisionState {};
    struct AdminRemoveServer {};
    struct AdminClusterInfo {};
    struct AdminMaintenance {};
    struct AdminLicense {};
    struct AdminBackup {};
    struct AdminJobs {};
    struct AdminTasks {};
    struct AdminReadReplicatedLog {};
    struct AdminWriteReplicatedLog {};
    struct AdminWalAccess {};
    struct AdminReadAgency {};
    struct AdminReadOnlyMode {};
    struct AdminFoxx {};
    struct AdminReadAqlFunctions {};
    struct AdminWriteAqlFunctions {};
    using Any = std::variant<
        ReadDatabase, WriteDatabase, ReadCollection, WriteCollectionData,
        WriteCollectionMeta, ReadView, WriteView, ReadAnalyzer, WriteAnalyzer,
        UseApiVersion, AdminChangeDataDist, AdminReadUser, AdminWriteUser,
        AdminMonitoring, AdminMonitoringInternal, AdminCompaction,
        AdminAuthReload, AdminCrashHandler, AdminApiCalls, AdminAqlQueries,
        AdminShutdown, AdminReadLogs, AdminSetLogLevel, AdminOptions,
        AdminSupervisionState, AdminRemoveServer, AdminClusterInfo,
        AdminMaintenance, AdminLicense, AdminBackup, AdminJobs, AdminTasks,
        AdminReadReplicatedLog, AdminWriteReplicatedLog, AdminWalAccess,
        AdminReadAgency, AdminReadOnlyMode, AdminFoxx, AdminReadAqlFunctions,
        AdminWriteAqlFunctions>;
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
