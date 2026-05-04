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

AuthMode::Classic::Classic(auth::UserManager& userManager, std::string username,
                           bool apiHardened)
    : _userManager(userManager),
      _username(std::move(username)),
      _apiHardened(apiHardened) {}

auto AuthMode::Classic::username() const noexcept -> std::string_view {
  return _username;
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

  // TODO Instead of recursing in the following visit, implement
  //      methods resembling canUseDatabase and canUseCollection, like
  //      they existed on ExecContext previously.

  return std::visit(
      overload{
          [&](p::UseDatabase const& database) -> Result {
            auto effectiveLevel = effectiveDatabaseAuthLevel(database.name);
            auto requestedLevel = accessLevelToAuthLevel(database.level);

            if (requestedLevel <= effectiveLevel) {
              return {};
            } else {
              return {TRI_ERROR_FORBIDDEN,
                      "insufficient database access level for '" +
                          database.name + "'"};
            }
          },
          [&](p::UseCollection const& collection) -> Result {
            auto const requestedLevel =
                accessLevelToAuthLevel(collection.level);

            // handle fixed permissions of certain system collections
            if (collection.name.starts_with('_')) {
              // _system._users
              if (collection.db == StaticStrings::SystemDatabase &&
                  collection.name == StaticStrings::UsersCollection) {
                if (requestedLevel == auth::Level::NONE) {
                  return {};
                }
                return {TRI_ERROR_FORBIDDEN,
                        std::format("access to {} is restricted",
                                    StaticStrings::UsersCollection)};
              }
              // _queues
              if (collection.name == StaticStrings::QueuesCollection) {
                if (requestedLevel <= auth::Level::RO) {
                  return {};
                }
                return {TRI_ERROR_FORBIDDEN,
                        std::format("write access to {} is restricted",
                                    StaticStrings::QueuesCollection)};
              }
              // _frontend
              if (collection.name == StaticStrings::FrontendCollection) {
                return {};
              }
            }

            auto const effectiveLevel =
                effectiveCollectionAuthLevel(collection.db, collection.name);

            if (requestedLevel <= effectiveLevel) {
              return {};
            } else {
              return {TRI_ERROR_FORBIDDEN,
                      "insufficient collection access level for '" +
                          collection.name + "' in database '" + collection.db +
                          "'"};
            }
          },
          [&](p::UseView const& view) -> Result {
            auto const effectiveLevel =
                effectiveCollectionAuthLevel(view.db, view.name);
            auto const requestedLevel = accessLevelToAuthLevel(view.level);

            if (requestedLevel <= effectiveLevel) {
              return {};
            } else {
              return {TRI_ERROR_FORBIDDEN,
                      "insufficient view access level for '" + view.name +
                          "' in database '" + view.db + "'"};
            }
          },
          [&](p::UseAnalyzer const& analyzer) -> Result {
            auto const effectiveLevel =
                effectiveCollectionAuthLevel(analyzer.db, analyzer.name);
            auto const requestedLevel = accessLevelToAuthLevel(analyzer.level);

            if (requestedLevel <= effectiveLevel) {
              return {};
            } else {
              return {TRI_ERROR_FORBIDDEN,
                      "insufficient analyzer access level for '" +
                          analyzer.name + "' in database '" + analyzer.db +
                          "'"};
            }
          },
          [&](p::Admin const& admin) -> Result {
            // recurses into check() with a UseDatabase permission
            return isAdmin();
          },
          // TODO Implement proper classic-system checks for these.
          [&](p::HardenedAdmin const& admin) -> Result {
            if (!_apiHardened) {
              return {};
            } else {
              // recurses into check() with a UseDatabase permission
              return isAdmin();
            }
          },
          [&](p::SeeDatabase const& database) -> Result {
            // recurses into check() with a UseDatabase permission
            return check(
                p::UseDatabase{database.name, DatabaseAccessLevel::Read});
          },
          [&](p::CreateDatabase const&) -> Result { return {}; },
          [&](p::DropDatabase const&) -> Result { return {}; },
          [&](p::SeeCollection const&) -> Result { return {}; },
          [&](p::CreateCollection const&) -> Result { return {}; },
          [&](p::DropCollection const&) -> Result { return {}; },
          [&](p::SeeView const&) -> Result { return {}; },
          [&](p::CreateView const&) -> Result { return {}; },
          [&](p::ModifyView const&) -> Result { return {}; },
          [&](p::RenameView const&) -> Result { return {}; },
          [&](p::DropView const&) -> Result {
            // TODO this must check
            //      * RW access to the database
            //      * RO access to each collection
            //      and possibly some access to the view itself, but I
            //      haven't looked where that check currently happens.
            return {};
          },
          [&](p::SeeAnalyzer const&) -> Result { return {}; },
          [&](p::CreateAnalyzer const&) -> Result { return {}; },
          [&](p::DropAnalyzer const&) -> Result { return {}; },
          [&](p::SeeGraph const&) -> Result { return {}; },
          [&](p::CreateGraph const&) -> Result { return {}; },
          [&](p::DropGraph const&) -> Result { return {}; },
          [&](p::UseGraph const&) -> Result { return {}; },
          [&](p::ReadUser const&) -> Result { return {}; },
          [&](p::WriteUser const&) -> Result { return {}; },
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

AuthMode::Unauthenticated::Unauthenticated(std::string username)
    : _username(std::move(username)) {}

auto AuthMode::Unauthenticated::username() const noexcept -> std::string_view {
  return _username;
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

AuthMode::Disabled::Disabled(std::string username)
    : _username(std::move(username)) {}

auto AuthMode::Disabled::username() const noexcept -> std::string_view {
  return _username;
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

}  // namespace arangodb
