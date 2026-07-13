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

#include "AuthMode.h"

#include "Assertions/ProdAssert.h"
#include "Auth/UserManager.h"
#include "Basics/StaticStrings.h"
#include "Basics/overload.h"
#include "Basics/voc-errors.h"
#include "Cluster/ServerState.h"
#include "GeneralServer/AuthenticationFeature.h"
#include "Rest/GeneralRequest.h"

namespace arangodb {

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
  auto const effectiveCollectionAuthLevel =
      [this](std::string_view db, std::string_view collection) {
        auto const storedLevel =
            _userManager.collectionAuthLevel(username(), db, collection, true);
        auto const maxLevel =
            ServerState::readOnly() ? auth::Level::RO : auth::Level::RW;
        return std::min(storedLevel, maxLevel);
      };
  auto const effectiveDatabaseAuthLevel = [this](std::string_view db) {
    auto const storedLevel =
        _userManager.databaseAuthLevel(username(), db, true);
    auto const maxLevel =
        ServerState::readOnly() ? auth::Level::RO : auth::Level::RW;
    return std::min(storedLevel, maxLevel);
  };
  auto const accessLevelToAuthLevel = overload{
      [](CollectionAccessLevel level) {
        switch (level) {
          case CollectionAccessLevel::None:
            return auth::Level::NONE;
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
          case ViewAccessLevel::None:
            return auth::Level::NONE;
          case ViewAccessLevel::Read:
            return auth::Level::RO;
          case ViewAccessLevel::Modify:
            return auth::Level::RW;
        }
        ADB_PROD_CRASH();
      },
      [](AnalyzerAccessLevel level) {
        switch (level) {
          case AnalyzerAccessLevel::None:
            return auth::Level::NONE;
          case AnalyzerAccessLevel::Read:
            return auth::Level::RO;
          case AnalyzerAccessLevel::Modify:
            return auth::Level::RW;
        }
        ADB_PROD_CRASH();
      },
      [](DatabaseAccessLevel level) {
        switch (level) {
          case DatabaseAccessLevel::None:
            return auth::Level::NONE;
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
              return {TRI_ERROR_ARANGO_DATABASE_NOT_FOUND,
                      "database not accessible: '" + database.name + "'"};
            } else {
              return {TRI_ERROR_FORBIDDEN,
                      "insufficient database access level for '" +
                          database.name + "'"};
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
                return {TRI_ERROR_FORBIDDEN,
                        std::format("access to {} is restricted",
                                    StaticStrings::UsersCollection)};
              }
              // _queues: read-only for everyone.
              if (collection.name == StaticStrings::QueuesCollection) {
                if (requestedLevel <= auth::Level::RO) {
                  return {};
                }
                return {TRI_ERROR_FORBIDDEN,
                        std::format("write access to {} is restricted",
                                    StaticStrings::QueuesCollection)};
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
                  return {TRI_ERROR_ARANGO_DATA_SOURCE_NOT_FOUND,
                          "collection or view not found: '" + collection.name +
                              "' in database '" + collection.db + "'"};
                }
              }
              return {TRI_ERROR_FORBIDDEN,
                      "insufficient collection access level for '" +
                          collection.name + "' in database '" + collection.db +
                          "'"};
            }

            // WriteMeta additionally requires RW access to the database
            // (container principle: modifying a collection's meta-data
            // requires write permission on the containing database).
            if (collection.level == CollectionAccessLevel::WriteMeta) {
              auto const dbLevel = effectiveDatabaseAuthLevel(collection.db);
              if (dbLevel < auth::Level::RW) {
                return {TRI_ERROR_FORBIDDEN,
                        "insufficient database access level for write-meta "
                        "operation on collection '" +
                            collection.name + "' in database '" +
                            collection.db + "'"};
              }
            }

            return {};
          },
          [&](p::UseView const& view) -> Result {
            // In the classic system views delegate to database-level access
            // (per-view collection-level auth is not used for views).
            auto const effectiveLevel = effectiveDatabaseAuthLevel(view.db);
            auto const requestedLevel = accessLevelToAuthLevel(view.level);

            if (requestedLevel <= effectiveLevel) {
              return {};
            } else if (effectiveLevel == auth::Level::NONE) {
              // No database access at all: report the view as not found.
              return {TRI_ERROR_ARANGO_DATA_SOURCE_NOT_FOUND,
                      "view not accessible: '" + view.name + "' in database '" +
                          view.db + "'"};
            } else {
              return {TRI_ERROR_FORBIDDEN,
                      "insufficient access level for view '" + view.name +
                          "' in database '" + view.db + "'"};
            }
          },
          [&](p::UseAnalyzer const& analyzer) -> Result {
            // Without RBAC, RO access to the database is the only
            // prerequisite for using an analyzer, and that has already been
            // verified before this is called. No further check needed.
            // For the sake of readabilty, we perform the check:
            return check(
                p::UseDatabase{analyzer.db, DatabaseAccessLevel::Read});
          },
          [&](p::Admin const& /*admin*/) -> Result {
            // Classic admin action requires RW access to the _system database.
            return isAdmin();
          },
          [&](p::SeeDatabase const& database) -> Result {
            return check(
                p::UseDatabase{database.name, DatabaseAccessLevel::Read});
          },
          [&](p::CreateDatabase const& /*database*/) -> Result {
            // Creating a database requires RW access to the _system database.
            return isAdmin();
          },
          [&](p::DropDatabase const& /*database*/) -> Result {
            // Dropping a database requires RW access to the _system database.
            return isAdmin();
          },
          [&](p::SeeCollection const& /*collection*/) -> Result {
            // Database RO access is the only prerequisite and has already been
            // checked; a collection is always visible if the database is.
            return {};
          },
          [&](p::CreateCollection const& collection) -> Result {
            // Creating a collection requires RW access to the database
            // (container principle).
            return check(
                p::UseDatabase{collection.db, DatabaseAccessLevel::Write});
          },
          [&](p::DropCollection const& collection) -> Result {
            // Dropping a collection requires RW access to the database
            // (container principle).
            return check(
                p::UseDatabase{collection.db, DatabaseAccessLevel::Write});
          },
          [&](p::SeeView const& /*view*/) -> Result {
            // Database RO access is the only prerequisite and has already been
            // checked; a view is always visible if the database is.
            return {};
          },
          [&](p::CreateView const& view) -> Result {
            // Creating a view requires RW access to the database.
            return check(p::UseDatabase{view.db, DatabaseAccessLevel::Write});
          },
          [&](p::ModifyView const& view) -> Result {
            // Modifying a view requires RW access to the database.
            return check(p::UseDatabase{view.db, DatabaseAccessLevel::Write});
          },
          [&](p::RenameView const& view) -> Result {
            if (view.oldName == view.newName) {
              return {TRI_ERROR_BAD_PARAMETER,
                      "new view name must be different from old view name"};
            }
            // Renaming a view requires RW access to the database.
            return check(p::UseDatabase{view.db, DatabaseAccessLevel::Write});
          },
          [&](p::DropView const& view) -> Result {
            // Dropping a view requires RW access to the database.
            if (auto r =
                    check(p::UseDatabase{view.db, DatabaseAccessLevel::Write});
                !r.ok()) {
              return r;
            }
            // We also need read access on all linked collections.
            for (auto const& coll : view.linkedCollections) {
              if (auto r = check(p::UseCollection{view.db, coll,
                                                  CollectionAccessLevel::Read});
                  !r.ok()) {
                return r;
              }
            }
            return {};
          },
          [&](p::SeeAnalyzer const& analyzer) -> Result {
            // Database RO access is the only prerequisite and has already been
            // checked; an analyzer is always visible if the database is.
            // For the sake of readabilty, we perform the check:
            return check(
                p::UseDatabase{analyzer.db, DatabaseAccessLevel::Read});
          },
          [&](p::CreateAnalyzer const& analyzer) -> Result {
            // Creating an analyzer requires RW access to the database.
            return check(
                p::UseDatabase{analyzer.db, DatabaseAccessLevel::Write});
          },
          [&](p::DropAnalyzer const& analyzer) -> Result {
            // Dropping an analyzer requires RW access to the database.
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
            if (auto r =
                    check(p::UseDatabase{graph.db, DatabaseAccessLevel::Write});
                !r.ok()) {
              return r;
            }
            for (auto const& coll : graph.collectionNamesToCreate) {
              if (auto r = check(p::CreateCollection{graph.db, coll});
                  !r.ok()) {
                return r;
              }
            }
            for (auto const& coll : graph.collectionNamesToRead) {
              if (auto r = check(p::UseCollection{graph.db, coll,
                                                  CollectionAccessLevel::Read});
                  !r.ok()) {
                return r;
              }
            }
            return {};
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
              case GraphAccessLevel::None:
                return {};
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
            return isAdmin();
          },
          [&](p::WriteUser const& /*writeUser*/) -> Result {
            // Writing a user record requires RW access to the _system
            // database (equivalent to being an admin).
            return isAdmin();
          },
      },
      permission);
}

Result AuthMode::Classic::isAdmin() const {
  return check(auth::perms::UseDatabase{.name = StaticStrings::SystemDatabase,
                                        .level = DatabaseAccessLevel::Write});
}

auto AuthMode::Rbac::username() const noexcept -> std::string_view {
  return _username;
}

auto AuthMode::Rbac::check(auth::Permission permission) const -> Result {
  std::abort();  // TODO implement
  // NOTE Remember to handle "supportsRbac" flag for collections; we will need
  // access to the collection somehow!
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
  namespace p = auth::perms;

  return std::visit(
      overload{
          // Level-based checks: allow if the requested level is "None",
          // otherwise deny (not authenticated).
          [](p::UseDatabase const& perm) -> Result {
            if (perm.level <= DatabaseAccessLevel::None) {
              return {};
            }
            return {TRI_ERROR_FORBIDDEN, "not authenticated"};
          },
          [](p::UseCollection const& perm) -> Result {
            if (perm.level <= CollectionAccessLevel::None) {
              return {};
            }
            return {TRI_ERROR_FORBIDDEN, "not authenticated"};
          },
          [](p::UseView const& perm) -> Result {
            if (perm.level <= ViewAccessLevel::None) {
              return {};
            }
            return {TRI_ERROR_FORBIDDEN, "not authenticated"};
          },
          [](p::UseAnalyzer const& perm) -> Result {
            if (perm.level <= AnalyzerAccessLevel::None) {
              return {};
            }
            return {TRI_ERROR_FORBIDDEN, "not authenticated"};
          },
          // Everything else: no authentication → no permissions.
          [](auto const&) -> Result {
            return {TRI_ERROR_FORBIDDEN, "not authenticated"};
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

}  // namespace arangodb
