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

#include "Basics/Meta/TypeList.h"

#include <cstdint>
#include <iosfwd>
#include <span>
#include <string>
#include <string_view>
#include <variant>
#include <vector>

namespace arangodb {

// Access-level ladders. Kept at top-level (not in `auth`) because they are
// already referenced from many call sites unqualified. They belong here
// side-by-side with the permission vocabulary because `auth::perms::UseX`
// takes one of these as its level.
// Note: these ladders intentionally have no `None` level. The perms API only
// ever asks about access that is actually required; "no access" is expressed
// by not asking at all. (The UserManager's stored levels are a separate enum,
// `auth::Level`, which does have NONE.)
enum class CollectionAccessLevel { Read, WriteData, WriteMeta };
// TODO We call ::Write for DB, but ::Modify for View and Analyzer.
//      Should we keep it consistent?
enum class DatabaseAccessLevel { Read, Write };
enum class AnalyzerAccessLevel { Read, Modify };
enum class GraphAccessLevel { Read, Modify };

using AccessLevel = CollectionAccessLevel;

auto to_string(CollectionAccessLevel level) -> std::string_view;
auto to_string(DatabaseAccessLevel level) -> std::string_view;
auto to_string(AnalyzerAccessLevel level) -> std::string_view;
auto to_string(GraphAccessLevel level) -> std::string_view;

}  // namespace arangodb

namespace arangodb::auth {

// Permission "vocabulary": each struct describes one authorization question
// that callers can ask the authorization subsystem. The
// answer (Result) is computed by the active AuthMode implementation.
//
// Intended usage at call sites:
//
//   using namespace arangodb::auth::perms;
//   if (auto r = execContext.can(SeeCollection{.db = db, .name = coll});
//       !r.ok()) { /* ... */ }
//
// Keep `perms` a leaf namespace that contains nothing but these plain
// value types, so that opening it with `using namespace` inside a function
// body is safe (no overload-resolution hazards).
namespace perms {

// TODO When both Classic and RBAC implementations are broadly working,
//      let's review whether we want these types to be owning (e.g.
//      using std::string, std::vector<std::string> etc.), or views
//      (e.g. std::string_view, std::span<std::string_view>).

// ---------------------------------------------------------------------------
// Admin permissions
// ---------------------------------------------------------------------------

struct AdminReadUsers {};
struct AdminMoveShards {};
struct AdminMonitoring {};
struct AdminMonitoringInternal {};
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
struct AdminReadReplicatedLog {};
struct AdminWriteReplicatedLog {};
struct AdminDump {};
struct AdminRestore {};
struct AdminWalAccess {};
struct AdminReadAgency {};
struct AdminQueryCache {};

namespace detail {
using AdminList = meta::TypeList<
    AdminReadUsers, AdminMoveShards, AdminMonitoring, AdminMonitoringInternal,
    AdminAuthReload, AdminCrashHandler, AdminApiCalls, AdminAqlQueries,
    AdminShutdown, AdminReadLogs, AdminSetLogLevel, AdminOptions,
    AdminSupervisionState, AdminRemoveServer, AdminClusterInfo,
    AdminMaintenance, AdminRebalance, AdminLicense, AdminBackup,
    AdminReadReplicatedLog, AdminWriteReplicatedLog, AdminDump, AdminRestore,
    AdminWalAccess, AdminReadAgency, AdminQueryCache>;
}

template<typename T>
concept AnyAdmin = meta::InList<T, detail::AdminList>;

// ---------------------------------------------------------------------------
// Databases
// ---------------------------------------------------------------------------

// May the database be listed/visible to the current identity? Distinct from
// UseDatabase(Read): the doc explicitly foresees eventually separating
// "existence is visible" from "data is readable".
struct SeeDatabase {
  std::string name;
};

// Create/drop live outside the UseDatabase level ladder; see
// path_permissions.md.
struct CreateDatabase {
  std::string name;
};

struct DropDatabase {
  std::string name;
};

// Use the database at the requested access level.
struct UseDatabase {
  std::string name;
  DatabaseAccessLevel level;
};

// ---------------------------------------------------------------------------
// Collections
// ---------------------------------------------------------------------------

struct SeeCollection {
  std::string db;
  std::string name;
};

struct CreateCollection {
  std::string db;
  std::string name;
};

struct DropCollection {
  std::string db;
  std::string name;
};

struct UseCollection {
  std::string db;
  std::string name;
  CollectionAccessLevel level;
};

// May the current identity dump (read out via arangodump) this collection?
// Behaves like `UseCollection(Read)`, except that in the classic system it
// is additionally granted to identities with RW access to the `_system`
// database (i.e. "admins", equivalent to `Admin{AdminDump}`).
struct DumpCollection {
  std::string db;
  std::string name;
};

// May the current identity restore (write via arangorestore) this
// collection? Behaves like `UseCollection(WriteData)`, except that in the
// classic system it is additionally granted to identities with RW access to
// the `_system` database (i.e. "admins", equivalent to
// `Admin{AdminRestore}`).
// The flag `overwrite` indicates if we need to be able to drop and recreate
// the collection!
struct RestoreCollection {
  std::string db;
  std::string name;
  bool overwrite;
};

// For the create index process during restore we need this:
struct RestoreCreateIndex {
  std::string db;
  std::string collName;
};

// For the create view process during restore we need this:
struct RestoreCreateView {
  std::string db;
  std::string viewName;
  std::vector<std::string> linkedCollNames;
};

// For the drop view process during restore we need this:
struct RestoreDropView {
  std::string db;
  std::string viewName;
};

// For the write data process during restore we need this:
struct RestoreWriteData {
  std::string db;
  std::string collName;
};

// ---------------------------------------------------------------------------
// Views
// ---------------------------------------------------------------------------

struct SeeView {
  std::string db;
  std::string name;
};

struct CreateView {
  std::string db;
  std::string name;
  std::vector<std::string> linkedCollections;
};

struct ModifyView {
  std::string db;
  std::string name;
  std::vector<std::string> linkedCollections;
};

struct RenameView {
  std::string db;
  std::string oldName;
  std::string newName;
  std::vector<std::string> linkedCollections;
};

struct DropView {
  std::string db;
  std::string name;
  std::vector<std::string> linkedCollections;
};

struct UseView {
  std::string db;
  std::string name;
};

// ---------------------------------------------------------------------------
// Analyzers
// ---------------------------------------------------------------------------

struct SeeAnalyzer {
  std::string db;
  std::string name;
};

struct CreateAnalyzer {
  std::string db;
  std::string name;
};

struct DropAnalyzer {
  std::string db;
  std::string name;
};

struct UseAnalyzer {
  std::string db;
  std::string name;
  AnalyzerAccessLevel level;
};

// ---------------------------------------------------------------------------
// Graphs
// ---------------------------------------------------------------------------

struct SeeGraph {
  std::string db;
  std::string name;
};

struct CreateGraph {
  std::string db;
  std::string name;
  std::span<std::string> collectionNamesToCreate;
  std::span<std::string> collectionNamesToRead;
};

struct DropGraph {
  std::string db;
  std::string name;
  std::span<std::string> collectionNames;
};

struct UseGraph {
  std::string db;
  std::string name;
  GraphAccessLevel level;
};

// ---------------------------------------------------------------------------
// Users
// ---------------------------------------------------------------------------

struct ReadUser {
  std::string name;
};

// Create a new user record.
struct CreateUser {
  std::string name;
};

// Drop (remove) an existing user record.
struct DropUser {
  std::string name;
};

// Modify a user's own profile data: password, active flag, config blob.
// Note that (with Classic auth) everybody may modify their own profile.
struct ModifyUserProfile {
  std::string name;
};

// Grant or revoke a user's permissions on databases and collections.
struct GrantUserPermissions {
  std::string name;
};

// ---------------------------------------------------------------------------
// API versions
// ---------------------------------------------------------------------------

// Grant permission to a specific api version
struct UseApiVersion {
  uint32_t version;
};

namespace detail {
// Currently there's no need to subdivide this list, but feel free to
// do that when it becomes useful.
using NonAdminList = meta::TypeList<
    // database permissions
    SeeDatabase, CreateDatabase, DropDatabase, UseDatabase,
    // collection permissions
    SeeCollection, CreateCollection, DropCollection, UseCollection,
    DumpCollection, RestoreCollection, RestoreCreateIndex, RestoreCreateView,
    RestoreDropView, RestoreWriteData,
    // view permissions
    SeeView, CreateView, ModifyView, RenameView, DropView, UseView,
    // analyzer permissions
    SeeAnalyzer, CreateAnalyzer, DropAnalyzer, UseAnalyzer,
    // graph permissions
    SeeGraph, CreateGraph, DropGraph, UseGraph,
    // user permissions
    ReadUser, CreateUser, DropUser, ModifyUserProfile, GrantUserPermissions,
    // api version permissions
    UseApiVersion>;

using CompleteList = meta::detail::Union<AdminList, NonAdminList>::type;
}  // namespace detail

}  // namespace perms

// Closed sum of every authorization question `IAuth` can be asked. Useful
// for internal dispatch, batching and logging; a `std::variant` is
// implicitly constructible from any of its alternatives, so callers just
// pass a `perms::Xxx{...}` and it is wrapped automatically.
using Permission = perms::detail::CompleteList::asVariant;

namespace perms {

// Streams one authorization question as a human- and machine-readable list of
// the permission's type name followed by its fields, e.g.
//
//   UseCollection db=_system name=foo level=read
//   AdminBackup
//
// Used by `ExecContext::can()` to trace every authorization question on
// `Logger::AUTHORIZATION`; the format is asserted on by
// tests/js/client/server_permissions/authorization-questions*.js, so treat it
// as a (loose) contract. Values never contain whitespace, so a reader can
// tokenize on spaces and split each token at its first '='.
//
// NOTE: this has to be declared in `perms`, not in `auth`, even though it is
// about `auth::Permission`: that is an alias for `std::variant<perms::...>`,
// so ADL only ever considers `std` and `perms`.
std::ostream& operator<<(std::ostream& os, Permission const& permission);

}  // namespace perms

}  // namespace arangodb::auth
