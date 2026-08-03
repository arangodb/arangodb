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

#include "AuthMode.h"

#include "Assertions/ProdAssert.h"
#include "Auth/UserManager.h"
#include "Basics/StaticStrings.h"
#include "Basics/overload.h"
#include "Basics/voc-errors.h"
#include "Cluster/ServerState.h"
#include "GeneralServer/AuthenticationFeature.h"
#include "Rbac/Service.h"
#include "Rest/ApiVersion.h"
#include "Rest/GeneralRequest.h"

#include <format>

#include <type_traits>

namespace arangodb {

namespace {
auto joinQuoted(std::span<std::string const> names) -> std::string {
  std::string result;
  for (auto const& name : names) {
    if (!result.empty()) {
      result += ", ";
    }
    result += std::format("'{}'", name);
  }
  return result;
}
auto describe(auth::perms::UseDatabase const& perm) -> std::string {
  return std::format("use database '{}' with access level '{}'", perm.name,
                     to_string(perm.level));
}
auto describe(auth::perms::UseCollection const& perm) -> std::string {
  return std::format(
      "use collection '{}' in database '{}' with access level '{}'", perm.name,
      perm.db, to_string(perm.level));
}
auto describe(auth::perms::UseView const& perm) -> std::string {
  return std::format("use view '{}' in database '{}' with access level '{}'",
                     perm.name, perm.db, to_string(perm.level));
}
auto describe(auth::perms::SeeView const& perm) -> std::string {
  return std::format("see view '{}' in database '{}'", perm.name, perm.db);
}
auto describe(auth::perms::CreateView const& perm) -> std::string {
  return std::format(
      "create view '{}' in database '{}' with linked collections [{}]",
      perm.name, perm.db, joinQuoted(perm.linkedCollections));
}
auto describe(auth::perms::RenameView const& perm) -> std::string {
  return std::format("rename view '{}' to '{}' in database '{}'", perm.oldName,
                     perm.newName, perm.db);
}
auto describe(auth::perms::UseAnalyzer const& perm) -> std::string {
  return std::format(
      "use analyzer '{}' in database '{}' with access level '{}'", perm.name,
      perm.db, to_string(perm.level));
}
auto describe(auth::perms::CreateGraph const& perm) -> std::string {
  return std::format(
      "create graph '{}' in database '{}' with collections to create [{}] and "
      "collections to read [{}]",
      perm.name, perm.db, joinQuoted(perm.collectionNamesToCreate),
      joinQuoted(perm.collectionNamesToRead));
}
// Admin permissions carry no resource, so each maps to a fixed phrase naming
// the administrative action it guards.
auto describe(auth::perms::AnyAdmin auto const& admin) -> std::string {
  namespace p = auth::perms;
  using T = std::remove_cvref_t<decltype(admin)>;
  if constexpr (std::is_same_v<T, p::AdminReadUsers>) {
    return "list all users (as admin)";
  } else if constexpr (std::is_same_v<T, p::AdminMoveShards>) {
    return "move shards (as admin)";
  } else if constexpr (std::is_same_v<T, p::AdminMonitoring>) {
    return "access monitoring data (as admin)";
  } else if constexpr (std::is_same_v<T, p::AdminMonitoringInternal>) {
    return "access internal monitoring data (as admin)";
  } else if constexpr (std::is_same_v<T, p::AdminAuthReload>) {
    return "reload authentication data (as admin)";
  } else if constexpr (std::is_same_v<T, p::AdminCrashHandler>) {
    return "access the crash handler (as admin)";
  } else if constexpr (std::is_same_v<T, p::AdminApiCalls>) {
    return "access API call statistics (as admin)";
  } else if constexpr (std::is_same_v<T, p::AdminAqlQueries>) {
    return "manage AQL queries (as admin)";
  } else if constexpr (std::is_same_v<T, p::AdminShutdown>) {
    return "shut down the server (as admin)";
  } else if constexpr (std::is_same_v<T, p::AdminReadLogs>) {
    return "read the server logs (as admin)";
  } else if constexpr (std::is_same_v<T, p::AdminSetLogLevel>) {
    return "set the log level (as admin)";
  } else if constexpr (std::is_same_v<T, p::AdminOptions>) {
    return "access server options (as admin)";
  } else if constexpr (std::is_same_v<T, p::AdminSupervisionState>) {
    return "access the supervision state (as admin)";
  } else if constexpr (std::is_same_v<T, p::AdminRemoveServer>) {
    return "remove a server (as admin)";
  } else if constexpr (std::is_same_v<T, p::AdminClusterInfo>) {
    return "access cluster information (as admin)";
  } else if constexpr (std::is_same_v<T, p::AdminMaintenance>) {
    return "manage maintenance mode (as admin)";
  } else if constexpr (std::is_same_v<T, p::AdminRebalance>) {
    return "rebalance the cluster (as admin)";
  } else if constexpr (std::is_same_v<T, p::AdminLicense>) {
    return "manage the license (as admin)";
  } else if constexpr (std::is_same_v<T, p::AdminBackup>) {
    return "manage backups (as admin)";
  } else if constexpr (std::is_same_v<T, p::AdminReadReplicatedLog>) {
    return "read a replicated log (as admin)";
  } else if constexpr (std::is_same_v<T, p::AdminWriteReplicatedLog>) {
    return "write a replicated log (as admin)";
  } else if constexpr (std::is_same_v<T, p::AdminDump>) {
    return "dump data (as admin)";
  } else if constexpr (std::is_same_v<T, p::AdminRestore>) {
    return "restore data (as admin)";
  } else if constexpr (std::is_same_v<T, p::AdminWalAccess>) {
    return "access the write-ahead log (as admin)";
  } else if constexpr (std::is_same_v<T, p::AdminReadAgency>) {
    return "read the agency (as admin)";
  } else if constexpr (std::is_same_v<T, p::AdminQueryCache>) {
    return "manage the query cache (as admin)";
  } else {
    static_assert(sizeof(T) == 0, "unmapped admin permission");
  }
}
auto describe(auth::perms::SeeDatabase const& perm) -> std::string {
  return std::format("see database '{}'", perm.name);
}
auto describe(auth::perms::CreateDatabase const& perm) -> std::string {
  return std::format("create database '{}'", perm.name);
}
auto describe(auth::perms::DropDatabase const& perm) -> std::string {
  return std::format("drop database '{}'", perm.name);
}
auto describe(auth::perms::SeeCollection const& perm) -> std::string {
  return std::format("see collection '{}' in database '{}'", perm.name,
                     perm.db);
}
auto describe(auth::perms::CreateCollection const& perm) -> std::string {
  return std::format("create collection '{}' in database '{}'", perm.name,
                     perm.db);
}
auto describe(auth::perms::DropCollection const& perm) -> std::string {
  return std::format("drop collection '{}' in database '{}'", perm.name,
                     perm.db);
}
auto describe(auth::perms::DumpCollection const& perm) -> std::string {
  return std::format("dump collection '{}' in database '{}'", perm.name,
                     perm.db);
}
auto describe(auth::perms::RestoreCollection const& perm) -> std::string {
  return std::format("restore collection '{}' in database '{}' {} overwrite",
                     perm.name, perm.db, perm.overwrite ? "with" : "without");
}
auto describe(auth::perms::RestoreCreateIndex const& perm) -> std::string {
  return std::format(
      "create index during restore on collection '{}' in database '{}'",
      perm.collName, perm.db);
}
auto describe(auth::perms::RestoreCreateView const& perm) -> std::string {
  return std::format(
      "create view '{}' during restore in database '{}' with linked "
      "collections [{}]",
      perm.viewName, perm.db, joinQuoted(perm.linkedCollNames));
}
auto describe(auth::perms::RestoreDropView const& perm) -> std::string {
  return std::format("drop view '{}' during restore in database '{}'",
                     perm.viewName, perm.db);
}
auto describe(auth::perms::RestoreWriteData const& perm) -> std::string {
  return std::format(
      "write data during restore to collection '{}' in database '{}'",
      perm.collName, perm.db);
}
auto describe(auth::perms::ModifyView const& perm) -> std::string {
  return std::format(
      "modify view '{}' in database '{}' with linked collections [{}]",
      perm.name, perm.db, joinQuoted(perm.linkedCollections));
}
auto describe(auth::perms::DropView const& perm) -> std::string {
  return std::format("drop view '{}' in database '{}'", perm.name, perm.db);
}
auto describe(auth::perms::SeeAnalyzer const& perm) -> std::string {
  return std::format("see analyzer '{}' in database '{}'", perm.name, perm.db);
}
auto describe(auth::perms::CreateAnalyzer const& perm) -> std::string {
  return std::format("create analyzer '{}' in database '{}'", perm.name,
                     perm.db);
}
auto describe(auth::perms::DropAnalyzer const& perm) -> std::string {
  return std::format("drop analyzer '{}' in database '{}'", perm.name, perm.db);
}
auto describe(auth::perms::SeeGraph const& perm) -> std::string {
  return std::format("see graph '{}' in database '{}'", perm.name, perm.db);
}
auto describe(auth::perms::DropGraph const& perm) -> std::string {
  return std::format("drop graph '{}' in database '{}' with collections [{}]",
                     perm.name, perm.db, joinQuoted(perm.collectionNames));
}
auto describe(auth::perms::UseGraph const& perm) -> std::string {
  return std::format("use graph '{}' in database '{}' with access level '{}'",
                     perm.name, perm.db, to_string(perm.level));
}
auto describe(auth::perms::ReadUser const& perm) -> std::string {
  return std::format("read user '{}'", perm.name);
}
auto describe(auth::perms::CreateUser const& perm) -> std::string {
  return std::format("create user '{}'", perm.name);
}
auto describe(auth::perms::DropUser const& perm) -> std::string {
  return std::format("drop user '{}'", perm.name);
}
auto describe(auth::perms::ModifyUserProfile const& perm) -> std::string {
  return std::format("modify profile of user '{}'", perm.name);
}
auto describe(auth::perms::GrantUserPermissions const& perm) -> std::string {
  return std::format("grant permissions to user '{}'", perm.name);
}
auto failureMessage(auto const& request, std::string_view reason)
    -> std::string {
  return std::format("Failed to {}. {}", describe(request), reason);
}
auto accessLevelMismatchReason(std::string_view subject,
                               std::string_view resource, auth::Level required,
                               auth::Level actual) -> std::string {
  return std::format(
      "{} requires {} authentication level '{}' but it has only level "
      "'{}'.",
      subject, resource, auth::convertFromAuthLevel(required),
      auth::convertFromAuthLevel(actual));
}
}  // namespace

auto AuthMode::getIAuth() -> AuthMode::IAuth& {
  return std::visit([](auto&& authMode) -> IAuth& { return authMode; },
                    authMode);
}

auto AuthMode::getIAuth() const -> const AuthMode::IAuth& {
  return std::visit(
      [](auto const& authMode) -> IAuth const& { return authMode; }, authMode);
}

bool AuthMode::isRbac() const noexcept {
  return std::holds_alternative<Rbac>(authMode);
}

bool AuthMode::isSuperuser() const noexcept {
  return std::holds_alternative<Superuser>(authMode);
}

bool AuthMode::isDisabled() const noexcept {
  return std::holds_alternative<Disabled>(authMode);
}

bool AuthMode::isUnauthenticated() const noexcept {
  return std::holds_alternative<Unauthenticated>(authMode);
}

auto AuthMode::Superuser::username() const noexcept -> std::string_view {
  return "";
}

auto AuthMode::Superuser::check(auth::Permission permission) const -> Result {
  return {};
}

auto AuthMode::Superuser::request() const noexcept
    -> std::optional<std::reference_wrapper<GeneralRequest>> {
  if (_request != nullptr) {
    return *_request;
  }
  return std::nullopt;
}

AuthMode::Classic::Classic(auth::UserManager& userManager, std::string username,
                           GeneralRequest& req)
    : _userManager(userManager),
      _username(std::move(username)),
      _request(req) {}

auto AuthMode::Classic::username() const noexcept -> std::string_view {
  return _username;
}

auto AuthMode::Classic::request() const noexcept
    -> std::optional<std::reference_wrapper<GeneralRequest>> {
  return _request;
}

auto AuthMode::Classic::check(auth::Permission permission) const -> Result {
  namespace p = auth::perms;

  // TODO While this lambda is convenient, it prevents us from
  //      specifically reporting when access is forbidden (only) due
  //      to the server being in read-only mode. I think we should
  //      change that, it seems sensible to communicate that to the
  //      user.
  auto const effectiveCollectionAuthLevel = [this](
                                                std::string_view db,
                                                std::string_view collection) {
    return _userManager.collectionAuthLevel(username(), db, collection, true);
  };
  auto const effectiveDatabaseAuthLevel = [this](std::string_view db) {
    return _userManager.databaseAuthLevel(username(), db, true);
  };
  auto const accessLevelToAuthLevel = overload{
      [](CollectionAccessLevel level) {
        switch (level) {
          case CollectionAccessLevel::Read:
            return auth::Level::RO;
          case CollectionAccessLevel::WriteData:
          case CollectionAccessLevel::WriteMeta:
            return auth::Level::RW;
        }
        ADB_PROD_CRASH();
      },
      [](ViewAccessLevel level) {
        switch (level) {
          case ViewAccessLevel::Read:
            return auth::Level::RO;
          case ViewAccessLevel::Modify:
            return auth::Level::RW;
        }
        ADB_PROD_CRASH();
      },
      [](AnalyzerAccessLevel level) {
        switch (level) {
          case AnalyzerAccessLevel::Read:
            return auth::Level::RO;
          case AnalyzerAccessLevel::Modify:
            return auth::Level::RW;
        }
        ADB_PROD_CRASH();
      },
      [](DatabaseAccessLevel level) {
        switch (level) {
          case DatabaseAccessLevel::Read:
            return auth::Level::RO;
          case DatabaseAccessLevel::Write:
            return auth::Level::RW;
        }
        ADB_PROD_CRASH();
      },
  };

  return std::visit(
      overload{
          [&](p::UseDatabase const& database) -> Result {
            auto const effectiveLevel =
                effectiveDatabaseAuthLevel(database.name);
            auto const requestedLevel = accessLevelToAuthLevel(database.level);

            if (requestedLevel <= effectiveLevel) {
              return {};
            } else if (_request.requestedApiVersion() > 0 &&
                       effectiveLevel == auth::Level::NONE) {
              // User has no access to the database at all: report as not found
              // to avoid revealing its existence.
              return {TRI_ERROR_ARANGO_DATABASE_NOT_FOUND};
            } else {
              return {TRI_ERROR_FORBIDDEN,
                      failureMessage(database,
                                     accessLevelMismatchReason(
                                         "Request", "database", requestedLevel,
                                         effectiveLevel))};
            }
          },
          [&](p::UseCollection const& collection) -> Result {
            auto const requestedLevel =
                accessLevelToAuthLevel(collection.level);

            // Handle fixed permissions of certain system collections.
            if (collection.name.starts_with('_')) {
              // _system._users: always NONE access (no user may touch it
              // through normal APIs).
              if (collection.db == StaticStrings::SystemDatabase &&
                  collection.name == StaticStrings::UsersCollection) {
                if (requestedLevel == auth::Level::NONE) {
                  return {};
                }
                // a user will never request with requestLevel == NONE,
                // therefore the error message can be simple
                return {
                    TRI_ERROR_FORBIDDEN,
                    failureMessage(collection,
                                   std::format("Access to {} collection in {} "
                                               "database is forbidden",
                                               StaticStrings::UsersCollection,
                                               StaticStrings::SystemDatabase))};
              }
              // _queues: read-only for everyone.
              if (collection.name == StaticStrings::QueuesCollection) {
                if (requestedLevel <= auth::Level::RO) {
                  return {};
                }
                return {TRI_ERROR_FORBIDDEN,
                        failureMessage(
                            collection,
                            std::format(
                                "Request requires collection authentication "
                                "level '{}' but {} collection can only be "
                                "accessed with at least level '{}'",
                                auth::convertFromAuthLevel(requestedLevel),
                                StaticStrings::QueuesCollection,
                                auth::convertFromAuthLevel(auth::Level::RO)))};
              }
              // _frontend: full access for everyone.
              if (collection.name == StaticStrings::FrontendCollection) {
                return {};
              }
            }

            auto const effectiveLevel =
                effectiveCollectionAuthLevel(collection.db, collection.name);

            if (requestedLevel > effectiveLevel) {
              // If we are using API version > 0, then we return NOT_FOUND to
              // hide the fact that the collection exists:
              if (_request.requestedApiVersion() > 0) {
                if (effectiveLevel == auth::Level::NONE) {
                  // User has no access to this collection: report as not found
                  // to avoid revealing its existence.

                  if (ServerState::instance()->isSingleServer()) {
                    return {TRI_ERROR_ARANGO_DATA_SOURCE_NOT_FOUND};
                  } else {
                    return {TRI_ERROR_ARANGO_DATA_SOURCE_NOT_FOUND,
                            "collection not found"};
                  }
                }
              }
              if (requestedLevel == arangodb::auth::Level::RW &&
                  effectiveLevel == arangodb::auth::Level::RO) {
                return {TRI_ERROR_ARANGO_READ_ONLY,
                        failureMessage(collection,
                                       accessLevelMismatchReason(
                                           "Request", "collection",
                                           requestedLevel, effectiveLevel))};
              } else {
                return {TRI_ERROR_FORBIDDEN,
                        failureMessage(collection,
                                       accessLevelMismatchReason(
                                           "Request", "collection",
                                           requestedLevel, effectiveLevel))};
              }
            }

            // WriteMeta additionally requires RW access to the database
            // (container principle: modifying a collection's meta-data
            // requires write permission on the containing database).
            if (collection.level == CollectionAccessLevel::WriteMeta) {
              auto const dbLevel = effectiveDatabaseAuthLevel(collection.db);
              if (dbLevel < auth::Level::RW) {
                return {TRI_ERROR_FORBIDDEN,
                        failureMessage(
                            collection,
                            accessLevelMismatchReason(
                                std::format("Collection access level '{}'",
                                            to_string(collection.level)),
                                "database", auth::Level::RW, dbLevel))};
              }
            }

            return {};
          },
          [&](p::DumpCollection const& collection) -> Result {
            // Behaves like UseCollection(Read), but is additionally granted
            // to identities with RW access to the _system database (which
            // corresponds to canUseAdminAction(AdminDump) with RBAC).
            if (isAdmin().ok()) {
              return {};
            }
            return check(p::UseCollection{collection.db, collection.name,
                                          CollectionAccessLevel::Read});
          },
          [&](p::RestoreCollection const& collection) -> Result {
            // Behaves like UseCollection(WriteData), but is additionally
            // granted to identities with RW access to the _system database
            // (which corresponds to canUseAdminAction(AdminRestore) with
            // RBAC).
            if (isAdmin().ok()) {
              return {};
            }
            if (collection.overwrite) {
              if (auto r =
                      check(p::DropCollection(collection.db, collection.name));
                  r.fail()) {
                return r;
              }
              if (auto r = check(
                      p::CreateCollection(collection.db, collection.name));
                  r.fail()) {
                return r;
              }
              return {};
            }
            return check(p::UseCollection{collection.db, collection.name,
                                          CollectionAccessLevel::WriteData});
          },
          [&](p::RestoreCreateIndex const& idx) -> Result {
            // Behaves like UseCollection(WriteMeta), but is additionally
            // granted to identities with RW access to the _system database
            // (which corresponds to canUseAdminAction(AdminRestore) with
            // RBAC).
            if (isAdmin().ok()) {
              return {};
            }
            return check(p::UseCollection{idx.db, idx.collName,
                                          CollectionAccessLevel::WriteMeta});
          },
          [&](p::RestoreCreateView const& view) -> Result {
            // Behaves like CreateView, but is additionally granted to
            // identities with RW access to the _system database (which
            // corresponds to canUseAdminAction(AdminRestore) with RBAC).
            if (isAdmin().ok()) {
              return {};
            }
            return check(
                p::CreateView{view.db, view.viewName, view.linkedCollNames});
          },
          [&](p::RestoreDropView const& view) -> Result {
            // Behaves like DropView, but is additionally granted to
            // identities with RW access to the _system database (which
            // corresponds to canUseAdminAction(AdminRestore) with RBAC).
            if (isAdmin().ok()) {
              return {};
            }
            return check(p::DropView{view.db, view.viewName});
          },
          [&](p::RestoreWriteData const& data) -> Result {
            // Behaves like UseCollection(WriteData), but is additionally
            // granted to identities with RW access to the _system database
            // (which corresponds to canUseAdminAction(AdminRestore) with
            // RBAC).
            if (isAdmin().ok()) {
              return {};
            }
            return check(p::UseCollection{data.db, data.collName,
                                          CollectionAccessLevel::WriteData});
          },
          [&](p::UseView const& view) -> Result {
            // In the classic system views delegate to database-level access
            // (per-view collection-level auth is not used for views).
            auto const effectiveLevel = effectiveDatabaseAuthLevel(view.db);
            auto const requestedLevel = accessLevelToAuthLevel(view.level);

            if (requestedLevel <= effectiveLevel) {
              return {};
            } else if (_request.requestedApiVersion() > 0 &&
                       effectiveLevel == auth::Level::NONE) {
              TRI_ASSERT(false);  // should never happen because database access
                                  // is required to use view
              return {TRI_ERROR_ARANGO_DATA_SOURCE_NOT_FOUND};
            } else {
              return {
                  TRI_ERROR_FORBIDDEN,
                  failureMessage(view, accessLevelMismatchReason(
                                           "Request", "view", requestedLevel,
                                           effectiveLevel))};
            }
          },
          [&](p::UseAnalyzer const& analyzer) -> Result {
            // Without RBAC, database access is the only prerequisite for
            // using an analyzer. Reading analyzers requires RO database
            // access; modifying analyzers requires RW.
            // The only exception is "Admin" (for backwards compatibility),
            // which means that RW access to _system grants all analyzer
            // permissions:
            if (isAdmin().ok()) {
              return {};
            }
            auto const dbLevel = analyzer.level == AnalyzerAccessLevel::Modify
                                     ? DatabaseAccessLevel::Write
                                     : DatabaseAccessLevel::Read;
            return check(p::UseDatabase{analyzer.db, dbLevel});
          },
          // Classic admin action requires RW access to the _system database.
          [&](p::AnyAdmin auto const&) -> Result { return isAdmin(); },
          [&](p::SeeDatabase const& database) -> Result {
            return check(
                p::UseDatabase{database.name, DatabaseAccessLevel::Read});
          },
          [&](p::CreateDatabase const& /*database*/) -> Result {
            // Creating a database requires RW access to the _system database.
            return check(
                auth::perms::UseDatabase{.name = StaticStrings::SystemDatabase,
                                         .level = DatabaseAccessLevel::Write});
          },
          [&](p::DropDatabase const& /*database*/) -> Result {
            // Dropping a database requires RW access to the _system database.
            return check(
                auth::perms::UseDatabase{.name = StaticStrings::SystemDatabase,
                                         .level = DatabaseAccessLevel::Write});
          },
          [&](p::SeeCollection const& collection) -> Result {
            // In Classic, seeing a collection is possible if and only if one
            // can read it. However, there is no rule without exception: An
            // Admin user must be able to run arangodump and thus must be
            // able to see all collections:
            if (isAdmin().ok()) {
              return {};
            }
            return check(p::UseCollection{collection.db, collection.name,
                                          CollectionAccessLevel::Read});
          },
          [&](p::CreateCollection const& collection) -> Result {
            // Creating a collection requires RW access to the database
            // (container principle).
            return check(
                p::UseDatabase{collection.db, DatabaseAccessLevel::Write});
          },
          [&](p::DropCollection const& collection) -> Result {
            // Dropping a collection requires RW access to the database
            // (container principle) as well as to the collection itself.
            auto r = check(
                p::UseDatabase{collection.db, DatabaseAccessLevel::Write});
            if (r.fail()) {
              // Note that sometimes `r` here returns the code
              // `TRI_ERROR_ARANGO_READ_ONLY`, but we **must** hand on
              // `TRI_ERROR_FORBIDDEN` here for API compatibility for the
              // API version 0!
              if (_request.requestedApiVersion() == 0) {
                return {TRI_ERROR_FORBIDDEN, r.errorMessage()};
              } else {
                return r;
              }
            }
            r = check(p::UseCollection{collection.db, collection.name,
                                       CollectionAccessLevel::WriteMeta});
            if (r.fail()) {
              // Note that sometimes `r` here returns the code
              // `TRI_ERROR_ARANGO_READ_ONLY`, but we **must** hand on
              // `TRI_ERROR_FORBIDDEN` here for API compatibility for
              // the API Version 0!
              if (_request.requestedApiVersion() == 0) {
                return {TRI_ERROR_FORBIDDEN, r.errorMessage()};
              } else {
                return r;
              }
            }
            return {};
          },
          [&](p::SeeView const& view) -> Result {
            // Database RO access is the only prerequisite and has already been
            // checked; a view is always visible if the database is.
            return {};
          },
          [&](p::CreateView const& view) -> Result {
            // Creating a view requires RW access to the database.
            if (auto r =
                    check(p::UseDatabase{view.db, DatabaseAccessLevel::Write});
                r.fail()) {
              return r;
            }
            // Also check read access to all linked collections.
            for (auto const& coll : view.linkedCollections) {
              if (auto r = check(p::UseCollection{view.db, coll,
                                                  CollectionAccessLevel::Read});
                  r.fail()) {
                return Result(
                    TRI_ERROR_FORBIDDEN,
                    failureMessage(view,
                                   std::format("Insufficient access to linked "
                                               "collection '{}': {}",
                                               coll, r.errorMessage())));
              }
            }
            return {};
          },
          [&](p::ModifyView const& view) -> Result {
            // Modifying a view requires RW access to the database.
            if (auto r =
                    check(p::UseDatabase{view.db, DatabaseAccessLevel::Write});
                !r.ok()) {
              return r;
            }
            // Also check read access to all newly linked collections.
            for (auto const& coll : view.linkedCollections) {
              if (auto r = check(p::UseCollection{view.db, coll,
                                                  CollectionAccessLevel::Read});
                  !r.ok()) {
                return r;
              }
            }
            return {};
          },
          [&](p::RenameView const& view) -> Result {
            if (view.oldName == view.newName) {
              return {TRI_ERROR_BAD_PARAMETER,
                      failureMessage(view,
                                     "New view name must be different from old "
                                     "view name.")};
            }
            // Renaming a view requires RW access to the database.
            if (auto r =
                    check(p::UseDatabase{view.db, DatabaseAccessLevel::Write});
                !r.ok()) {
              return r;
            }
            return {};
          },
          [&](p::DropView const& view) -> Result {
            // Dropping a view requires RW access to the database.
            return check(p::UseDatabase{view.db, DatabaseAccessLevel::Write});
          },
          [&](p::SeeAnalyzer const& analyzer) -> Result {
            // Database RO access is the only prerequisite and has already been
            // checked; an analyzer is always visible if the database is.
            // For the sake of readability, we perform the check here.
            // The only exception is "Admin" (for backwards compatibility),
            // which means that RW access to _system grants all analyzer
            // permissions:
            if (isAdmin().ok()) {
              return {};
            }
            return check(
                p::UseDatabase{analyzer.db, DatabaseAccessLevel::Read});
          },
          [&](p::CreateAnalyzer const& analyzer) -> Result {
            // Creating an analyzer requires RW access to the database.
            // The only exception is "Admin" (for backwards compatibility),
            // which means that RW access to _system grants all analyzer
            // permissions:
            if (isAdmin().ok()) {
              return {};
            }
            return check(
                p::UseDatabase{analyzer.db, DatabaseAccessLevel::Write});
          },
          [&](p::DropAnalyzer const& analyzer) -> Result {
            // Dropping an analyzer requires RW access to the database.
            // The only exception is "Admin" (for backwards compatibility),
            // which means that RW access to _system grants all analyzer
            // permissions:
            if (isAdmin().ok()) {
              return {};
            }
            return check(
                p::UseDatabase{analyzer.db, DatabaseAccessLevel::Write});
          },
          [&](p::SeeGraph const& graph) -> Result {
            // Seeing a graph requires at least read access to the database
            // (which grants implicit access to the _graphs system collection).
            return check(p::UseDatabase{graph.db, DatabaseAccessLevel::Read});
          },
          [&](p::CreateGraph const& graph) -> Result {
            // Creating a graph requires RW access to the database (to write
            // to _graphs), plus the ability to create/read any linked
            // collections.
            for (auto const& coll : graph.collectionNamesToCreate) {
              if (auto r = check(p::CreateCollection{graph.db, coll});
                  r.fail()) {
                return r;
              }
            }
            for (auto const& coll : graph.collectionNamesToRead) {
              if (auto r = check(p::UseCollection{graph.db, coll,
                                                  CollectionAccessLevel::Read});
                  r.fail()) {
                return r;
              }
            }
            if (auto r =
                    check(p::UseDatabase{graph.db, DatabaseAccessLevel::Write});
                r.ok()) {
              return {};
            }
            if (_request.requestedApiVersion() > 0) {
              return {TRI_ERROR_FORBIDDEN,
                      failureMessage(graph, "Cannot write to database.")};
            } else {
              return {TRI_ERROR_ARANGO_READ_ONLY,
                      failureMessage(graph, "Cannot write to database.")};
            }
          },
          [&](p::DropGraph const& graph) -> Result {
            // Dropping a graph requires RW access to the database (to write
            // to _graphs), plus the ability to drop any listed collections.
            if (auto r =
                    check(p::UseDatabase{graph.db, DatabaseAccessLevel::Write});
                !r.ok()) {
              return r;
            }
            for (auto const& coll : graph.collectionNames) {
              if (auto r = check(p::DropCollection{graph.db, coll}); !r.ok()) {
                return r;
              }
            }
            return {};
          },
          [&](p::UseGraph const& graph) -> Result {
            // In the classic system, graph access follows database-level
            // access (the _graphs collection is a system collection that is
            // read/write-accessible together with the whole database).
            switch (graph.level) {
              case GraphAccessLevel::Read:
                return check(
                    p::UseDatabase{graph.db, DatabaseAccessLevel::Read});
              case GraphAccessLevel::Modify:
                return check(
                    p::UseDatabase{graph.db, DatabaseAccessLevel::Write});
            }
            ADB_PROD_CRASH();
          },
          [&](p::ReadUser const& /*readUser*/) -> Result {
            // Reading any user record requires at least RW access to the
            // _system database.
            return check(
                auth::perms::UseDatabase{.name = StaticStrings::SystemDatabase,
                                         .level = DatabaseAccessLevel::Write});
          },
          [&](p::CreateUser const& /*createUser*/) -> Result {
            // Creating a user requires RW access to the _system database
            // (equivalent to being an admin).
            return check(
                auth::perms::UseDatabase{.name = StaticStrings::SystemDatabase,
                                         .level = DatabaseAccessLevel::Write});
          },
          [&](p::DropUser const& /*dropUser*/) -> Result {
            // Dropping a user requires RW access to the _system database
            // (equivalent to being an admin).
            return check(
                auth::perms::UseDatabase{.name = StaticStrings::SystemDatabase,
                                         .level = DatabaseAccessLevel::Write});
          },
          [&](p::ModifyUserProfile const& /*modifyUserProfile*/) -> Result {
            // Modifying a user's own profile (password, active flag, config
            // blob) requires RW access to the _system database (equivalent
            // to being an admin). Note that the self-exception is already
            // handled by ExecContext::canModifyUserProfile before this is
            // ever reached.
            return check(
                auth::perms::UseDatabase{.name = StaticStrings::SystemDatabase,
                                         .level = DatabaseAccessLevel::Write});
          },
          [&](p::GrantUserPermissions const& /*grantUserPermissions*/)
              -> Result {
            // Granting/revoking a user's permissions on databases and
            // collections requires RW access to the _system database
            // (equivalent to being an admin).
            return check(
                auth::perms::UseDatabase{.name = StaticStrings::SystemDatabase,
                                         .level = DatabaseAccessLevel::Write});
          },
      },
      permission);
}

Result AuthMode::Classic::isAdmin() const {
  auto r = check(auth::perms::UseDatabase{.name = StaticStrings::SystemDatabase,
                                          .level = DatabaseAccessLevel::Write});
  return r.ok() ? Result{}
                : Result{TRI_ERROR_FORBIDDEN,
                         std::format("Failed admin-permission check: {}",
                                     r.errorMessage())};
}

auto AuthMode::Rbac::username() const noexcept -> std::string_view {
  return _username;
}

auto AuthMode::Rbac::check(auth::Permission permission) const -> Result {
  namespace p = auth::perms;

  auto databaseAccessModeToAction =
      [](DatabaseAccessLevel level) -> rbac::Action {
    switch (level) {
      case DatabaseAccessLevel::Read:
        return rbac::Action::Read;
      case DatabaseAccessLevel::Write:
        return rbac::Action::WriteMeta;
    }
    ADB_PROD_CRASH();
  };

  auto collectionAccessModeToAction =
      [](CollectionAccessLevel level) -> rbac::Action {
    switch (level) {
      case CollectionAccessLevel::Read:
        return rbac::Action::Read;
      case CollectionAccessLevel::WriteData:
        return rbac::Action::WriteData;
      case CollectionAccessLevel::WriteMeta:
        return rbac::Action::WriteMeta;
    }
    ADB_PROD_CRASH();
  };

  auto viewAccessModeToAction = [](ViewAccessLevel level) -> rbac::Action {
    switch (level) {
      case ViewAccessLevel::Read:
        return rbac::Action::Read;
      case ViewAccessLevel::Modify:
        return rbac::Action::WriteMeta;
    }
    ADB_PROD_CRASH();
  };

  auto analyzerAccessModeToAction =
      [](AnalyzerAccessLevel level) -> rbac::Action {
    switch (level) {
      case AnalyzerAccessLevel::Read:
        return rbac::Action::Read;
      case AnalyzerAccessLevel::Modify:
        return rbac::Action::WriteMeta;
    }
    ADB_PROD_CRASH();
  };

  auto graphAccessModeToAction = [](GraphAccessLevel level) -> rbac::Action {
    switch (level) {
      case GraphAccessLevel::Read:
        return rbac::Action::Read;
      case GraphAccessLevel::Modify:
        return rbac::Action::WriteMeta;
    }
    ADB_PROD_CRASH();
  };

  // Every admin permission maps 1:1 onto its identically-named rbac::Action
  // and carries no resource. This mirrors the classic side, where all admin
  // actions collapse into a single `AnyAdmin` handler.
  auto adminAction = []<typename T>(T const&) -> rbac::Action {
    if constexpr (std::is_same_v<T, p::AdminReadUsers>) {
      return rbac::Action::AdminReadUsers;
    } else if constexpr (std::is_same_v<T, p::AdminMoveShards>) {
      return rbac::Action::AdminMoveShards;
    } else if constexpr (std::is_same_v<T, p::AdminMonitoring>) {
      return rbac::Action::AdminMonitoring;
    } else if constexpr (std::is_same_v<T, p::AdminMonitoringInternal>) {
      return rbac::Action::AdminMonitoringInternal;
    } else if constexpr (std::is_same_v<T, p::AdminAuthReload>) {
      return rbac::Action::AdminAuthReload;
    } else if constexpr (std::is_same_v<T, p::AdminCrashHandler>) {
      return rbac::Action::AdminCrashHandler;
    } else if constexpr (std::is_same_v<T, p::AdminApiCalls>) {
      return rbac::Action::AdminApiCalls;
    } else if constexpr (std::is_same_v<T, p::AdminAqlQueries>) {
      return rbac::Action::AdminAqlQueries;
    } else if constexpr (std::is_same_v<T, p::AdminShutdown>) {
      return rbac::Action::AdminShutdown;
    } else if constexpr (std::is_same_v<T, p::AdminReadLogs>) {
      return rbac::Action::AdminReadLogs;
    } else if constexpr (std::is_same_v<T, p::AdminSetLogLevel>) {
      return rbac::Action::AdminSetLogLevel;
    } else if constexpr (std::is_same_v<T, p::AdminOptions>) {
      return rbac::Action::AdminOptions;
    } else if constexpr (std::is_same_v<T, p::AdminSupervisionState>) {
      return rbac::Action::AdminSupervisionState;
    } else if constexpr (std::is_same_v<T, p::AdminRemoveServer>) {
      return rbac::Action::AdminRemoveServer;
    } else if constexpr (std::is_same_v<T, p::AdminClusterInfo>) {
      return rbac::Action::AdminClusterInfo;
    } else if constexpr (std::is_same_v<T, p::AdminMaintenance>) {
      return rbac::Action::AdminMaintenance;
    } else if constexpr (std::is_same_v<T, p::AdminRebalance>) {
      return rbac::Action::AdminRebalance;
    } else if constexpr (std::is_same_v<T, p::AdminLicense>) {
      return rbac::Action::AdminLicense;
    } else if constexpr (std::is_same_v<T, p::AdminBackup>) {
      return rbac::Action::AdminBackup;
    } else if constexpr (std::is_same_v<T, p::AdminReadReplicatedLog>) {
      return rbac::Action::AdminReadReplicatedLog;
    } else if constexpr (std::is_same_v<T, p::AdminWriteReplicatedLog>) {
      return rbac::Action::AdminWriteReplicatedLog;
    } else if constexpr (std::is_same_v<T, p::AdminDump>) {
      return rbac::Action::AdminDump;
    } else if constexpr (std::is_same_v<T, p::AdminRestore>) {
      return rbac::Action::AdminRestore;
    } else if constexpr (std::is_same_v<T, p::AdminWalAccess>) {
      return rbac::Action::AdminWalAccess;
    } else if constexpr (std::is_same_v<T, p::AdminReadAgency>) {
      return rbac::Action::AdminReadAgency;
    } else if constexpr (std::is_same_v<T, p::AdminQueryCache>) {
      return rbac::Action::AdminQueryCache;
    } else {
      static_assert(sizeof(T) == 0, "unmapped admin permission");
    }
  };

  // Each permission maps to one or more (action, resource) pairs, all of which
  // must be permitted. They are evaluated together in a single Service::check()
  // call (one network round-trip). The common single-pair case is passed as a
  // span over a stack-local pair and needs no allocation; only the composite
  // permissions (create/modify view, create/drop graph) build a small vector.
  auto checkAll = [&](std::span<rbac::ActionResource const> queries) -> Result {
    return _rbacService.check(rbac::JwtToken{_jwtToken}, queries);
  };
  auto checkOne = [&](rbac::Action action, rbac::Resource resource) -> Result {
    rbac::ActionResource query{action, std::move(resource)};
    return checkAll(std::span<rbac::ActionResource const>{&query, 1});
  };

  return std::visit(
      overload{
          // -- Admin actions ---------------------------------------------
          [&](p::AnyAdmin auto const& admin) -> Result {
            return checkOne(adminAction(admin), rbac::resources::NoResource{});
          },
          // -- Databases -------------------------------------------------
          [&](p::UseDatabase const& database) -> Result {
            return checkOne(databaseAccessModeToAction(database.level),
                            rbac::resources::Database{database.name});
          },
          [&](p::SeeDatabase const& database) -> Result {
            return checkOne(rbac::Action::Read,
                            rbac::resources::Database{database.name});
          },
          [&](p::CreateDatabase const& database) -> Result {
            return checkOne(rbac::Action::Create,
                            rbac::resources::Database{database.name});
          },
          [&](p::DropDatabase const& database) -> Result {
            return checkOne(rbac::Action::Drop,
                            rbac::resources::Database{database.name});
          },
          // -- Collections -----------------------------------------------
          [&](p::UseCollection const& collection) -> Result {
            return checkOne(
                collectionAccessModeToAction(collection.level),
                rbac::resources::Collection{collection.db, collection.name});
          },
          [&](p::SeeCollection const& collection) -> Result {
            return checkOne(
                rbac::Action::Read,
                rbac::resources::Collection{collection.db, collection.name});
          },
          [&](p::CreateCollection const& collection) -> Result {
            return checkOne(
                rbac::Action::Create,
                rbac::resources::Collection{collection.db, collection.name});
          },
          [&](p::DropCollection const& collection) -> Result {
            return checkOne(
                rbac::Action::Drop,
                rbac::resources::Collection{collection.db, collection.name});
          },
          // -- Views -----------------------------------------------------
          [&](p::UseView const& view) -> Result {
            return checkOne(viewAccessModeToAction(view.level),
                            rbac::resources::View{view.db, view.name});
          },
          [&](p::SeeView const& view) -> Result {
            return checkOne(rbac::Action::Read,
                            rbac::resources::View{view.db, view.name});
          },
          [&](p::CreateView const& view) -> Result {
            // Creating a view additionally requires read access to every
            // linked collection (mirrors the classic behaviour).
            std::vector<rbac::ActionResource> queries;
            queries.reserve(1 + view.linkedCollections.size());
            queries.push_back({rbac::Action::Create,
                               rbac::resources::View{view.db, view.name}});
            for (auto const& coll : view.linkedCollections) {
              queries.push_back({rbac::Action::Read,
                                 rbac::resources::Collection{view.db, coll}});
            }
            return checkAll(queries);
          },
          [&](p::ModifyView const& view) -> Result {
            // Modifying a view additionally requires read access to every
            // newly linked collection.
            std::vector<rbac::ActionResource> queries;
            queries.reserve(1 + view.linkedCollections.size());
            queries.push_back({rbac::Action::WriteMeta,
                               rbac::resources::View{view.db, view.name}});
            for (auto const& coll : view.linkedCollections) {
              queries.push_back({rbac::Action::Read,
                                 rbac::resources::Collection{view.db, coll}});
            }
            return checkAll(queries);
          },
          [&](p::RenameView const& view) -> Result {
            if (view.oldName == view.newName) {
              return {TRI_ERROR_BAD_PARAMETER,
                      "new view name must be different from old view name"};
            }
            // Renaming modifies the existing (old) view.
            return checkOne(rbac::Action::WriteMeta,
                            rbac::resources::View{view.db, view.oldName});
          },
          [&](p::DropView const& view) -> Result {
            return checkOne(rbac::Action::Drop,
                            rbac::resources::View{view.db, view.name});
          },
          // -- Analyzers -------------------------------------------------
          [&](p::UseAnalyzer const& analyzer) -> Result {
            return checkOne(
                analyzerAccessModeToAction(analyzer.level),
                rbac::resources::Analyzer{analyzer.db, analyzer.name});
          },
          [&](p::SeeAnalyzer const& analyzer) -> Result {
            return checkOne(
                rbac::Action::Read,
                rbac::resources::Analyzer{analyzer.db, analyzer.name});
          },
          [&](p::CreateAnalyzer const& analyzer) -> Result {
            return checkOne(
                rbac::Action::Create,
                rbac::resources::Analyzer{analyzer.db, analyzer.name});
          },
          [&](p::DropAnalyzer const& analyzer) -> Result {
            return checkOne(
                rbac::Action::Drop,
                rbac::resources::Analyzer{analyzer.db, analyzer.name});
          },
          // -- Graphs ----------------------------------------------------
          [&](p::UseGraph const& graph) -> Result {
            return checkOne(graphAccessModeToAction(graph.level),
                            rbac::resources::Graph{graph.db, graph.name});
          },
          [&](p::SeeGraph const& graph) -> Result {
            return checkOne(rbac::Action::Read,
                            rbac::resources::Graph{graph.db, graph.name});
          },
          [&](p::CreateGraph const& graph) -> Result {
            // Creating a graph additionally requires the ability to create any
            // collections it introduces and to read the ones it links (mirrors
            // the classic behaviour).
            std::vector<rbac::ActionResource> queries;
            queries.reserve(1 + graph.collectionNamesToCreate.size() +
                            graph.collectionNamesToRead.size());
            queries.push_back({rbac::Action::Create,
                               rbac::resources::Graph{graph.db, graph.name}});
            for (auto const& coll : graph.collectionNamesToCreate) {
              queries.push_back({rbac::Action::Create,
                                 rbac::resources::Collection{graph.db, coll}});
            }
            for (auto const& coll : graph.collectionNamesToRead) {
              queries.push_back({rbac::Action::Read,
                                 rbac::resources::Collection{graph.db, coll}});
            }
            return checkAll(queries);
          },
          [&](p::DropGraph const& graph) -> Result {
            // Dropping a graph additionally requires the ability to drop the
            // listed collections.
            std::vector<rbac::ActionResource> queries;
            queries.reserve(1 + graph.collectionNames.size());
            queries.push_back({rbac::Action::Drop,
                               rbac::resources::Graph{graph.db, graph.name}});
            for (auto const& coll : graph.collectionNames) {
              queries.push_back({rbac::Action::Drop,
                                 rbac::resources::Collection{graph.db, coll}});
            }
            return checkAll(queries);
          },
          // -- Dump / Restore --------------------------------------------
          // These mirror the classic delegations, minus the classic-only
          // "_system RW" admin bypass -- under RBAC that access is granted
          // through roles rather than a hard-coded exception.
          [&](p::DumpCollection const& collection) -> Result {
            // Dumping reads collection data.
            return check(p::UseCollection{collection.db, collection.name,
                                          CollectionAccessLevel::Read});
          },
          [&](p::RestoreCollection const& collection) -> Result {
            // Restoring writes collection data; with `overwrite` the
            // collection is dropped and recreated first.
            if (collection.overwrite) {
              if (auto r =
                      check(p::DropCollection{collection.db, collection.name});
                  !r.ok()) {
                return r;
              }
              return check(p::CreateCollection{collection.db, collection.name});
            }
            return check(p::UseCollection{collection.db, collection.name,
                                          CollectionAccessLevel::WriteData});
          },
          [&](p::RestoreCreateIndex const& idx) -> Result {
            return check(p::UseCollection{idx.db, idx.collName,
                                          CollectionAccessLevel::WriteMeta});
          },
          [&](p::RestoreCreateView const& view) -> Result {
            return check(
                p::CreateView{view.db, view.viewName, view.linkedCollNames});
          },
          [&](p::RestoreDropView const& view) -> Result {
            return check(p::DropView{view.db, view.viewName});
          },
          [&](p::RestoreWriteData const& data) -> Result {
            return check(p::UseCollection{data.db, data.collName,
                                          CollectionAccessLevel::WriteData});
          },
          // -- Users -----------------------------------------------------
          // Each user operation maps to the identically-scoped action on the
          // db:user:<name> resource.
          [&](p::ReadUser const& user) -> Result {
            return checkOne(rbac::Action::Read,
                            rbac::resources::User{user.name});
          },
          [&](p::CreateUser const& user) -> Result {
            return checkOne(rbac::Action::Create,
                            rbac::resources::User{user.name});
          },
          [&](p::DropUser const& user) -> Result {
            return checkOne(rbac::Action::Drop,
                            rbac::resources::User{user.name});
          },
          [&](p::ModifyUserProfile const& user) -> Result {
            return checkOne(rbac::Action::WriteMeta,
                            rbac::resources::User{user.name});
          },
          [&](p::GrantUserPermissions const& user) -> Result {
            return checkOne(rbac::Action::WriteMeta,
                            rbac::resources::User{user.name});
          },
      },
      permission);
}

auto AuthMode::Rbac::request() const noexcept
    -> std::optional<std::reference_wrapper<GeneralRequest>> {
  return _request;
}

AuthMode::Unauthenticated::Unauthenticated(std::string username,
                                           GeneralRequest& req)
    : _username(std::move(username)), _request(req) {}

auto AuthMode::Unauthenticated::username() const noexcept -> std::string_view {
  return _username;
}

auto AuthMode::Unauthenticated::request() const noexcept
    -> std::optional<std::reference_wrapper<GeneralRequest>> {
  return _request;
}

auto AuthMode::Unauthenticated::check(auth::Permission permission) const
    -> Result {
  return std::visit(
      overload{
          // An unauthenticated identity has no permissions at all. Since the
          // perms API only ever asks about access that is actually required
          // (there is no "None" level anymore), every question is denied.
          [](auto const& perm) -> Result {
            return {TRI_ERROR_FORBIDDEN,
                    failureMessage(perm, "Not authenticated.")};
          },
      },
      permission);
}

AuthMode::Disabled::Disabled(std::string username, GeneralRequest& req)
    : _username(std::move(username)), _request(req) {}

auto AuthMode::Disabled::username() const noexcept -> std::string_view {
  return _username;
}

auto AuthMode::Disabled::request() const noexcept
    -> std::optional<std::reference_wrapper<GeneralRequest>> {
  return _request;
}

auto AuthMode::Disabled::check(auth::Permission permission) const -> Result {
  return {};
}

#ifdef ARANGODB_USE_GOOGLE_TESTS
auto AuthMode::Mockable::username() const noexcept -> std::string_view {
  ADB_PROD_ASSERT(mock != nullptr);
  return mock->username();
}

auto AuthMode::Mockable::check(auth::Permission permission) const -> Result {
  ADB_PROD_ASSERT(mock != nullptr);
  return mock->check(permission);
}

auto AuthMode::Mockable::request() const noexcept
    -> std::optional<std::reference_wrapper<GeneralRequest>> {
  ADB_PROD_ASSERT(mock != nullptr);
  return mock->request();
}
#endif

}  // namespace arangodb
