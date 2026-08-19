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
#include "Permissions.h"

#include "Basics/overload.h"

#include <Assertions/ProdAssert.h>

#include <ostream>
#include <variant>

namespace arangodb {

auto to_string(CollectionAccessLevel level) -> std::string_view {
  switch (level) {
    case CollectionAccessLevel::Read:
      return "read";
    case CollectionAccessLevel::WriteData:
      return "writedata";
    case CollectionAccessLevel::WriteMeta:
      return "writemeta";
  }
  ADB_PROD_CRASH();
}

auto to_string(DatabaseAccessLevel level) -> std::string_view {
  switch (level) {
    case DatabaseAccessLevel::Read:
      return "read";
    case DatabaseAccessLevel::Write:
      return "write";
  }
  ADB_PROD_CRASH();
}

auto to_string(AnalyzerAccessLevel level) -> std::string_view {
  switch (level) {
    case AnalyzerAccessLevel::Read:
      return "read";
    case AnalyzerAccessLevel::Modify:
      return "modify";
  }
  ADB_PROD_CRASH();
}

auto to_string(GraphAccessLevel level) -> std::string_view {
  switch (level) {
    case GraphAccessLevel::Read:
      return "read";
    case GraphAccessLevel::Modify:
      return "modify";
  }
  ADB_PROD_CRASH();
}

}  // namespace arangodb

namespace arangodb::auth::perms {

namespace {

// Prints a list of names as `[a,b,c]` - no spaces, so that the whole
// permission stays tokenizable on whitespace.
void printNames(std::ostream& os, std::span<std::string const> names) {
  os << '[';
  bool first = true;
  for (auto const& name : names) {
    if (!first) {
      os << ',';
    }
    first = false;
    os << name;
  }
  os << ']';
}

}  // namespace

std::ostream& operator<<(std::ostream& os, Permission const& permission) {
  std::visit(
      overload{
          // admin actions - no fields, the type name says it all
          [&](AdminReadUsers const&) { os << "AdminReadUsers"; },
          [&](AdminMoveShards const&) { os << "AdminMoveShards"; },
          [&](AdminMonitoring const&) { os << "AdminMonitoring"; },
          [&](AdminMonitoringInternal const&) {
            os << "AdminMonitoringInternal";
          },
          [&](AdminAuthReload const&) { os << "AdminAuthReload"; },
          [&](AdminCrashHandler const&) { os << "AdminCrashHandler"; },
          [&](AdminApiCalls const&) { os << "AdminApiCalls"; },
          [&](AdminAqlQueries const&) { os << "AdminAqlQueries"; },
          [&](AdminShutdown const&) { os << "AdminShutdown"; },
          [&](AdminReadLogs const&) { os << "AdminReadLogs"; },
          [&](AdminSetLogLevel const&) { os << "AdminSetLogLevel"; },
          [&](AdminOptions const&) { os << "AdminOptions"; },
          [&](AdminSupervisionState const&) { os << "AdminSupervisionState"; },
          [&](AdminRemoveServer const&) { os << "AdminRemoveServer"; },
          [&](AdminClusterInfo const&) { os << "AdminClusterInfo"; },
          [&](AdminMaintenance const&) { os << "AdminMaintenance"; },
          [&](AdminRebalance const&) { os << "AdminRebalance"; },
          [&](AdminLicense const&) { os << "AdminLicense"; },
          [&](AdminBackup const&) { os << "AdminBackup"; },
          [&](AdminReadReplicatedLog const&) {
            os << "AdminReadReplicatedLog";
          },
          [&](AdminWriteReplicatedLog const&) {
            os << "AdminWriteReplicatedLog";
          },
          [&](AdminDump const&) { os << "AdminDump"; },
          [&](AdminRestore const&) { os << "AdminRestore"; },
          [&](AdminWalAccess const&) { os << "AdminWalAccess"; },
          [&](AdminReadAgency const&) { os << "AdminReadAgency"; },
          [&](AdminQueryCache const&) { os << "AdminQueryCache"; },

          // databases
          [&](SeeDatabase const& p) { os << "SeeDatabase name=" << p.name; },
          [&](CreateDatabase const& p) {
            os << "CreateDatabase name=" << p.name;
          },
          [&](DropDatabase const& p) { os << "DropDatabase name=" << p.name; },
          [&](UseDatabase const& p) {
            os << "UseDatabase name=" << p.name
               << " level=" << to_string(p.level);
          },

          // collections
          [&](SeeCollection const& p) {
            os << "SeeCollection db=" << p.db << " name=" << p.name;
          },
          [&](CreateCollection const& p) {
            os << "CreateCollection db=" << p.db << " name=" << p.name;
          },
          [&](DropCollection const& p) {
            os << "DropCollection db=" << p.db << " name=" << p.name;
          },
          [&](UseCollection const& p) {
            os << "UseCollection db=" << p.db << " name=" << p.name
               << " level=" << to_string(p.level);
          },
          [&](DumpCollection const& p) {
            os << "DumpCollection db=" << p.db << " name=" << p.name;
          },
          [&](RestoreCollection const& p) {
            os << "RestoreCollection db=" << p.db << " name=" << p.name
               << " overwrite=" << (p.overwrite ? "true" : "false");
          },
          [&](RestoreCreateIndex const& p) {
            os << "RestoreCreateIndex db=" << p.db
               << " collName=" << p.collName;
          },
          [&](RestoreCreateView const& p) {
            os << "RestoreCreateView db=" << p.db << " viewName=" << p.viewName
               << " linkedCollNames=";
            printNames(os, p.linkedCollNames);
          },
          [&](RestoreDropView const& p) {
            os << "RestoreDropView db=" << p.db << " viewName=" << p.viewName;
          },
          [&](RestoreWriteData const& p) {
            os << "RestoreWriteData db=" << p.db << " collName=" << p.collName;
          },

          // views
          [&](SeeView const& p) {
            os << "SeeView db=" << p.db << " name=" << p.name;
          },
          [&](CreateView const& p) {
            os << "CreateView db=" << p.db << " name=" << p.name
               << " linkedCollections=";
            printNames(os, p.linkedCollections);
          },
          [&](ModifyView const& p) {
            os << "ModifyView db=" << p.db << " name=" << p.name
               << " linkedCollections=";
            printNames(os, p.linkedCollections);
          },
          [&](RenameView const& p) {
            os << "RenameView db=" << p.db << " oldName=" << p.oldName
               << " newName=" << p.newName;
          },
          [&](DropView const& p) {
            os << "DropView db=" << p.db << " name=" << p.name;
          },
          [&](UseView const& p) {
            os << "UseView db=" << p.db << " name=" << p.name;
          },

          // analyzers
          [&](SeeAnalyzer const& p) {
            os << "SeeAnalyzer db=" << p.db << " name=" << p.name;
          },
          [&](CreateAnalyzer const& p) {
            os << "CreateAnalyzer db=" << p.db << " name=" << p.name;
          },
          [&](DropAnalyzer const& p) {
            os << "DropAnalyzer db=" << p.db << " name=" << p.name;
          },
          [&](UseAnalyzer const& p) {
            os << "UseAnalyzer db=" << p.db << " name=" << p.name
               << " level=" << to_string(p.level);
          },

          // graphs
          [&](SeeGraph const& p) {
            os << "SeeGraph db=" << p.db << " name=" << p.name;
          },
          [&](CreateGraph const& p) {
            os << "CreateGraph db=" << p.db << " name=" << p.name
               << " collectionNamesToCreate=";
            printNames(os, p.collectionNamesToCreate);
            os << " collectionNamesToRead=";
            printNames(os, p.collectionNamesToRead);
          },
          [&](DropGraph const& p) {
            os << "DropGraph db=" << p.db << " name=" << p.name
               << " collectionNames=";
            printNames(os, p.collectionNames);
          },
          [&](UseGraph const& p) {
            os << "UseGraph db=" << p.db << " name=" << p.name
               << " level=" << to_string(p.level);
          },

          // users
          [&](ReadUser const& p) { os << "ReadUser name=" << p.name; },
          [&](CreateUser const& p) { os << "CreateUser name=" << p.name; },
          [&](DropUser const& p) { os << "DropUser name=" << p.name; },
          [&](ModifyUserProfile const& p) {
            os << "ModifyUserProfile name=" << p.name;
          },
          [&](GrantUserPermissions const& p) {
            os << "GrantUserPermissions name=" << p.name;
          },

          // api versions
          [&](UseApiVersion const& p) {
            os << "UseApiVersion version=" << p.version;
          },
      },
      permission);
  return os;
}

}  // namespace arangodb::auth::perms
