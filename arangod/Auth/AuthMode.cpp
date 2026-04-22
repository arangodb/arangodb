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

AuthMode::Classic::Classic(auth::UserManager& userManager, std::string username)
    : _userManager(userManager), _username(std::move(username)) {}

auto AuthMode::Classic::username() const noexcept -> std::string_view {
  return _username;
}

auto AuthMode::Classic::check(auth::Permission permission) const -> Result {
  namespace p = auth::perms;

  return std::visit(
      overload{
          [&](p::UseDatabase const& database) -> Result {
            auto const storedLevel =
                _userManager.databaseAuthLevel(username(), database.name, true);
            auto const maxLevel =
                ServerState::readOnly() ? auth::Level::RO : auth::Level::RW;
            auto const level = std::min(storedLevel, maxLevel);

            switch (database.level) {
              case DatabaseAccessLevel::None:
                return {};
              case DatabaseAccessLevel::Read:
                if (level >= auth::Level::RO) {
                  return {};
                }
                return {TRI_ERROR_FORBIDDEN,
                        "insufficient database access level for '" +
                            database.name + "'"};
              case DatabaseAccessLevel::Write:
                if (level >= auth::Level::RW) {
                  return {};
                }
                return {TRI_ERROR_FORBIDDEN,
                        "insufficient database access level for '" +
                            database.name + "'"};
            }
            ADB_PROD_CRASH();
          },
          [&](p::UseCollection const& collection) -> Result {
            auto const storedLevel = _userManager.collectionAuthLevel(
                username(), collection.db, collection.name, true);
            auto const maxLevel =
                ServerState::readOnly() ? auth::Level::RO : auth::Level::RW;
            auto const level = std::min(storedLevel, maxLevel);

            // TODO translate the following code, copied from ExecContext, to
            //      work in this function!
            //      i.e. handle permissions for special collections.
            //  if (coll.starts_with('_')) {
            //    // handle fixed permissions here outside auth module.
            //    // TODO: move this block above, such that it takes effect
            //    //       when authentication is disabled
            //    if (dbname == StaticStrings::SystemDatabase &&
            //        coll == StaticStrings::UsersCollection) {
            //      // _users (only present in _system database)
            //      return auth::Level::NONE;
            //    }
            //    if (coll == StaticStrings::QueuesCollection) {
            //      // _queues
            //      return auth::Level::RO;
            //    }
            //    if (coll == StaticStrings::FrontendCollection) {
            //      // _frontend
            //      return auth::Level::RW;
            //    }  // intentional fall through
            //  }

            switch (collection.level) {
              case AccessLevel::None:
                return {};
              case AccessLevel::Read:
                if (level >= auth::Level::RO) {
                  return {};
                }
                return {TRI_ERROR_FORBIDDEN,
                        "insufficient collection access level for '" +
                            collection.name + "' in database '" +
                            collection.db + "'"};
              case AccessLevel::WriteData:
              case AccessLevel::WriteMeta:
                if (level >= auth::Level::RW) {
                  return {};
                }
                return {TRI_ERROR_FORBIDDEN,
                        "insufficient collection access level for '" +
                            collection.name + "' in database '" +
                            collection.db + "'"};
            }
            ADB_PROD_CRASH();
          },
          [&](p::UseView const& view) -> Result {
            auto const storedLevel = _userManager.collectionAuthLevel(
                username(), view.db, view.name, true);
            auto const maxLevel =
                ServerState::readOnly() ? auth::Level::RO : auth::Level::RW;
            auto const level = std::min(storedLevel, maxLevel);

            switch (view.level) {
              case ViewAccessLevel::None:
                return {};
              case ViewAccessLevel::Read:
                if (level >= auth::Level::RO) {
                  return {};
                }
                return {TRI_ERROR_FORBIDDEN,
                        "insufficient view access level for '" + view.name +
                            "' in database '" + view.db + "'"};
              case ViewAccessLevel::Modify:
                if (level >= auth::Level::RW) {
                  return {};
                }
                return {TRI_ERROR_FORBIDDEN,
                        "insufficient view access level for '" + view.name +
                            "' in database '" + view.db + "'"};
            }
            ADB_PROD_CRASH();
          },
          [&](p::UseAnalyzer const& analyzer) -> Result {
            auto const storedLevel = _userManager.collectionAuthLevel(
                username(), analyzer.db, analyzer.name, true);
            auto const maxLevel =
                ServerState::readOnly() ? auth::Level::RO : auth::Level::RW;
            auto const level = std::min(storedLevel, maxLevel);

            switch (analyzer.level) {
              case AnalyzerAccessLevel::None:
                return {};
              case AnalyzerAccessLevel::Read:
                if (level >= auth::Level::RO) {
                  return {};
                }
                return {TRI_ERROR_FORBIDDEN,
                        "insufficient analyzer access level for '" +
                            analyzer.name + "' in database '" + analyzer.db +
                            "'"};
              case AnalyzerAccessLevel::Modify:
                if (level >= auth::Level::RW) {
                  return {};
                }
                return {TRI_ERROR_FORBIDDEN,
                        "insufficient analyzer access level for '" +
                            analyzer.name + "' in database '" + analyzer.db +
                            "'"};
            }
            ADB_PROD_CRASH();
          },
          [&](p::Admin const& admin) -> Result {
            // recurses into check() with a UseDatabase permission
            return isAdmin();
          },
          // TODO Implement proper classic-system checks for these.
          [&](p::HardenedAdmin const& admin) -> Result { return {}; },
          [&](p::SeeDatabase const&) -> Result { return {}; },
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
