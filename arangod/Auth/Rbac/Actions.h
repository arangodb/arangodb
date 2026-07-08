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
  // Disposition of each struct with respect to the common `auth::perms`
  // vocabulary (see Auth/Permissions.h) is noted inline:
  //   [obsoleted] -- superseded by a perms:: struct (usually a See*/Use*
  //                  data permission); kept here only as an RBAC-internal
  //                  translation target for now.
  //   [moved]     -- a first-class copy now lives in perms:: (these are the
  //                  admin-class actions that used to flow through
  //                  perms::Admin, i.e. were passed to
  //                  ExecContext::canUseAdminAction/canUseHardenedAction).
  //   [unused]    -- referenced nowhere and has no direct perms:: pendant.

  struct ReadDatabase {  // [obsoleted] perms::SeeDatabase / UseDatabase(Read)
    std::string name;
  };
  struct WriteDatabase {  // [obsoleted] perms::UseDatabase(Write)
    std::string name;
  };
  struct ReadCollection {  // [obsoleted] perms::SeeCollection/UseCollection(Read)
    std::string database;
    std::string name;
  };
  struct WriteCollectionData {  // [obsoleted] perms::UseCollection(WriteData)
    std::string database;
    std::string name;
  };
  struct WriteCollectionMeta {  // [obsoleted] perms::UseCollection(WriteMeta)
    std::string database;
    std::string name;
  };
  struct ReadView {  // [obsoleted] perms::SeeView / UseView(Read)
    std::string database;
    std::string name;
  };
  struct WriteView {  // [obsoleted] perms::UseView(Modify) / ModifyView
    std::string database;
    std::string name;
  };
  struct ReadAnalyzer {  // [obsoleted] perms::SeeAnalyzer / UseAnalyzer(Read)
    std::string database;
    std::string name;
  };
  struct WriteAnalyzer {  // [obsoleted] perms::UseAnalyzer(Modify)
    std::string database;
    std::string name;
  };
  struct UseApiVersion {  // [unused] no perms:: pendant
    std::string version;
  };
  // Admin actions with a user resource:
  struct AdminReadUser {  // [moved] perms::AdminReadUser
    std::string username;
  };
  struct AdminWriteUser {  // [obsoleted] perms::WriteUser
    std::string username;
  };
  // Admin actions without a resource:
  struct AdminMoveShards {};         // [moved] perms::AdminMoveShards
  struct AdminMonitoring {};         // [moved] perms::AdminMonitoring
  struct AdminMonitoringInternal {}; // [moved] perms::AdminMonitoringInternal
  struct AdminCompaction {};         // [unused] no perms:: pendant
  struct AdminAuthReload {};         // [moved] perms::AdminAuthReload
  struct AdminCrashHandler {};       // [moved] perms::AdminCrashHandler
  struct AdminApiCalls {};           // [moved] perms::AdminApiCalls
  struct AdminAqlQueries {};         // [moved] perms::AdminAqlQueries
  struct AdminShutdown {};           // [moved] perms::AdminShutdown
  struct AdminReadLogs {};           // [moved] perms::AdminReadLogs
  struct AdminSetLogLevel {};        // [moved] perms::AdminSetLogLevel
  struct AdminOptions {};            // [moved] perms::AdminOptions
  struct AdminSupervisionState {};   // [moved] perms::AdminSupervisionState
  struct AdminRemoveServer {};       // [moved] perms::AdminRemoveServer
  struct AdminClusterInfo {};        // [moved] perms::AdminClusterInfo
  struct AdminMaintenance {};        // [moved] perms::AdminMaintenance
  struct AdminRebalance {};          // [moved] perms::AdminRebalance
  struct AdminLicense {};            // [moved] perms::AdminLicense
  struct AdminBackup {};             // [moved] perms::AdminBackup
  struct AdminJobs {};               // [unused] no perms:: pendant
  struct AdminReadReplicatedLog {};  // [moved] perms::AdminReadReplicatedLog
  struct AdminWriteReplicatedLog {}; // [moved] perms::AdminWriteReplicatedLog
  // Do we want this in RBAC, or just internally for Classic compatibility?
  struct AdminDump {};               // [moved] perms::AdminDump.
  struct AdminRestore {};            // [moved] perms::AdminRestore
  struct AdminWalAccess {};          // [moved] perms::AdminWalAccess
  struct AdminReadAgency {};         // [moved] perms::AdminReadAgency
  struct AdminReadOnlyMode {};       // [unused] no perms:: pendant
  struct AdminReadAqlFunctions {};   // [unused] no perms:: pendant
  struct AdminWriteAqlFunctions {};  // [unused] no perms:: pendant
  struct AdminQueryCache {};         // [moved] perms::AdminQueryCache
  using Any = std::variant<
      ReadDatabase, WriteDatabase, ReadCollection, WriteCollectionData,
      WriteCollectionMeta, ReadView, WriteView, ReadAnalyzer, WriteAnalyzer,
      UseApiVersion, AdminMoveShards, AdminReadUser, AdminWriteUser,
      AdminMonitoring, AdminMonitoringInternal, AdminCompaction,
      AdminAuthReload, AdminCrashHandler, AdminApiCalls, AdminAqlQueries,
      AdminShutdown, AdminReadLogs, AdminSetLogLevel, AdminOptions,
      AdminSupervisionState, AdminRemoveServer, AdminClusterInfo,
      AdminMaintenance, AdminRebalance, AdminLicense, AdminBackup, AdminJobs,
      AdminReadReplicatedLog, AdminWriteReplicatedLog, AdminDump, AdminRestore,
      AdminWalAccess, AdminReadAgency, AdminReadOnlyMode, AdminReadAqlFunctions,
      AdminWriteAqlFunctions, AdminQueryCache>;
};

}  // namespace arangodb::rbac
