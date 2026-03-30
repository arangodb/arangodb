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

bool AuthMode::isSuperuser() const noexcept {
  return std::holds_alternative<Superuser>(authMode);
}

auto AuthMode::Superuser::username() const noexcept -> std::string_view {
  return "";
}

auto AuthMode::Superuser::canUse(Permission permission) const -> bool {
  return true;
}

AuthMode::Classic::Classic(auth::UserManager& userManager, std::string username)
    : _userManager(userManager), _username(std::move(username)) {}

auto AuthMode::Classic::username() const noexcept -> std::string_view {
  return _username;
}

auto AuthMode::Classic::canUse(Permission permission) const -> bool {
  return std::visit(
      overload{
          [&](Permission::Database const& database) -> bool {
            auto const storedLevel =
                _userManager.databaseAuthLevel(username(), database.name, true);
            auto const maxLevel =
                ServerState::readOnly() ? auth::Level::RO : auth::Level::RW;
            auto const level = std::min(storedLevel, maxLevel);

            switch (database.level) {
              case DatabaseAccessLevel::None:
                return true;
              case DatabaseAccessLevel::Read:
                return level >= auth::Level::RO;
              case DatabaseAccessLevel::Write:
                return level >= auth::Level::RW;
            }
            ADB_PROD_CRASH();
          },
          [&](Permission::Collection const& collection) -> bool {
            auto const storedLevel = _userManager.collectionAuthLevel(
                username(), collection.database, collection.name, true);
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
                return true;
              case AccessLevel::Read:
                return level >= auth::Level::RO;
              case AccessLevel::WriteData:
              case AccessLevel::WriteMeta:
                return level >= auth::Level::RW;
            }
            ADB_PROD_CRASH();
          },
          [&](Permission::View const& view) -> bool {
            auto const storedLevel = _userManager.collectionAuthLevel(
                username(), view.database, view.name, true);
            auto const maxLevel =
                ServerState::readOnly() ? auth::Level::RO : auth::Level::RW;
            auto const level = std::min(storedLevel, maxLevel);

            switch (view.level) {
              case ViewAccessLevel::None:
                return true;
              case ViewAccessLevel::Read:
                return level >= auth::Level::RO;
              case ViewAccessLevel::Drop:
              case ViewAccessLevel::Create:
              case ViewAccessLevel::Modify:
                return level >= auth::Level::RW;
            }
            ADB_PROD_CRASH();
          },
          [&](Permission::Admin const& admin) -> bool {
            // this recurses to canUse, but with a Database permission
            return isAdmin();
          },
      },
      permission.permission);
}

bool AuthMode::Classic::isAdmin() const {
  return canUse({Permission::Database{.name = StaticStrings::SystemDatabase,
                                      .level = DatabaseAccessLevel::Write}});
}

auto AuthMode::Rbac::username() const noexcept -> std::string_view {
  return _username;
}

auto AuthMode::Rbac::canUse(Permission permission) const -> bool {
  std::abort();  // TODO implement
  // NOTE Remember to handle "supportsRbac" flag for collections; we will need
  // access to the collection somehow!
}

AuthMode::Unauthenticated::Unauthenticated(std::string username)
    : _username(std::move(username)) {}

auto AuthMode::Unauthenticated::username() const noexcept -> std::string_view {
  return _username;
}

auto AuthMode::Unauthenticated::canUse(Permission permission) const -> bool {
  return std::visit(
      [](auto const& perm) -> bool {
        using T = std::decay_t<decltype(perm)>;
        if constexpr (std::is_same_v<T, Permission::Database>) {
          return perm.level <= DatabaseAccessLevel::None;
        } else if constexpr (std::is_same_v<T, Permission::Collection>) {
          return perm.level <= CollectionAccessLevel::None;
        } else if constexpr (std::is_same_v<T, Permission::View>) {
          return perm.level <= ViewAccessLevel::None;
        } else {
          return false;
        }
      },
      permission.permission);
}

AuthMode::Disabled::Disabled(std::string username)
    : _username(std::move(username)) {}

auto AuthMode::Disabled::username() const noexcept -> std::string_view {
  return _username;
}

auto AuthMode::Disabled::canUse(Permission permission) const -> bool {
  return true;
}

auto AuthMode::Mockable::username() const noexcept -> std::string_view {
  ADB_PROD_ASSERT(mock != nullptr);
  return mock->username();
}

auto AuthMode::Mockable::canUse(Permission permission) const -> bool {
  ADB_PROD_ASSERT(mock != nullptr);
  return mock->canUse(permission);
}

}  // namespace arangodb
