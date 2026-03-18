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

#include <string>
#include <variant>

namespace arangodb::rbac {

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
  struct AdminRebalance {};
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
      AdminMaintenance, AdminRebalance, AdminLicense, AdminBackup, AdminJobs,
      AdminTasks, AdminReadReplicatedLog, AdminWriteReplicatedLog,
      AdminWalAccess, AdminReadAgency, AdminReadOnlyMode, AdminFoxx,
      AdminReadAqlFunctions, AdminWriteAqlFunctions>;
};

}  // namespace arangodb::rbac
