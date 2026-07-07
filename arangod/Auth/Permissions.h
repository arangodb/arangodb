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

#include "Auth/Common.h"
#include "Auth/Rbac/Actions.h"

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
enum class CollectionAccessLevel { None = 0, Read, WriteData, WriteMeta };
// TODO We call ::Write for DB, but ::Modify for View and Analyzer.
//      Should we keep it consistent?
enum class DatabaseAccessLevel { None = 0, Read, Write };
enum class ViewAccessLevel { None = 0, Read, Modify };
enum class AnalyzerAccessLevel { None = 0, Read, Modify };
enum class GraphAccessLevel { None = 0, Read, Modify };

using AccessLevel = CollectionAccessLevel;

auto to_string(CollectionAccessLevel level) -> std::string_view;
auto to_string(DatabaseAccessLevel level) -> std::string_view;
auto to_string(ViewAccessLevel level) -> std::string_view;
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
// Admin and hardened-admin actions
// ---------------------------------------------------------------------------

// TODO rbac::Category::Any is too broad: it contains more than only the admin
//      actions.

// Is the current identity allowed to execute this admin-class RBAC action?
// In the classic system this collapses to "RW on _system".
//
// TODO This struct is transitional. All of its possible `action` values now
//      also exist as first-class `perms::Admin*` structs below (moved out of
//      rbac::Category). The next step is to remove `Admin` entirely and let
//      `ExecContext::canUseAdminAction`/`canUseHardenedAction` accept only the
//      admin-class `perms::` structs -- either via a dedicated variant or a
//      concept constraining the parameter.
struct Admin {
  rbac::Category::Any action;
};

// Admin-class actions. These are the concrete actions that used to be passed
// through `perms::Admin` (i.e. handed to
// ExecContext::canUseAdminAction/canUseHardenedAction) as a
// `rbac::Category::Any`. They have been moved here so that `perms::` is the
// single, flat home for every authorization question. Fields mirror their
// former rbac::Category counterparts.
//
// TODO Once `Admin` is gone, gather these into a single type list to derive
//      both the admin-only variant (parameter of canUseAdminAction) and a
//      concept, instead of listing them by hand here and in `Permission`.

// Admin action carrying a user resource. Note this is the admin-level
// "may I enumerate/read users at all" question, distinct from the per-user
// `perms::ReadUser` above.
struct AdminReadUser {
  std::string username;
};

// Admin actions without a resource.
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

// ---------------------------------------------------------------------------
// Views
// ---------------------------------------------------------------------------
//
// View lifecycle operations optionally carry the list of collections the
// view links to. When known, the classic implementation checks access on
// the linked collections at the same time (mirroring the current
// auth::Can::{create,modify,drop}View(..., std::span<std::string>) overloads).
// Leave `linkedCollections` empty when the caller does not know them (yet).

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
  ViewAccessLevel level;
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

struct WriteUser {
  std::string name;
};

}  // namespace perms

// TODO When the dust has settled, we need to think about consolidating
//      auth::Permission and its types, with rbac::Category and its types.
//      They *are* different and have a different role, but they are closely
//      related. Maybe they need to stay separate, but if we can reduce some
//      of the duplication, that would be worth thinking about.
//      Currently, Permission::*Admin already uses the *Admin types from
//      rbac::Category, though it can't stay as it is (i.e, we should not
//      use rbac::Category::Any). See the TODO comment above, before
//      `struct Admin`.

// Closed sum of every authorization question `IAuth` can be asked. Useful
// for internal dispatch, batching and logging; a `std::variant` is
// implicitly constructible from any of its alternatives, so callers just
// pass a `perms::Xxx{...}` and it is wrapped automatically.
using Permission = std::variant<
    // admin actions
    // TODO `Admin` is transitional and will be removed once all callers use
    //      the flat `perms::Admin*` alternatives below directly.
    perms::Admin,
    // admin actions
    perms::AdminReadUser, perms::AdminMoveShards, perms::AdminMonitoring,
    perms::AdminMonitoringInternal, perms::AdminAuthReload,
    perms::AdminCrashHandler, perms::AdminApiCalls, perms::AdminAqlQueries,
    perms::AdminShutdown, perms::AdminReadLogs, perms::AdminSetLogLevel,
    perms::AdminOptions, perms::AdminSupervisionState, perms::AdminRemoveServer,
    perms::AdminClusterInfo, perms::AdminMaintenance, perms::AdminRebalance,
    perms::AdminLicense, perms::AdminBackup, perms::AdminReadReplicatedLog,
    perms::AdminWriteReplicatedLog, perms::AdminDump, perms::AdminRestore,
    perms::AdminWalAccess, perms::AdminReadAgency, perms::AdminQueryCache,
    // database permissions
    perms::SeeDatabase, perms::CreateDatabase, perms::DropDatabase,
    perms::UseDatabase,
    // collection permissions
    perms::SeeCollection, perms::CreateCollection, perms::DropCollection,
    perms::UseCollection,
    // view permissions
    perms::SeeView, perms::CreateView, perms::ModifyView, perms::RenameView,
    perms::DropView, perms::UseView,
    // analyzer permissions
    perms::SeeAnalyzer, perms::CreateAnalyzer, perms::DropAnalyzer,
    perms::UseAnalyzer,
    // graph permissions
    perms::SeeGraph, perms::CreateGraph, perms::DropGraph, perms::UseGraph,
    // user permissions
    perms::ReadUser, perms::WriteUser
    //
    >;

}  // namespace arangodb::auth
