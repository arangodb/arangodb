////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2014-2024 ArangoDB GmbH, Cologne, Germany
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
/// @author Simon Grätzer
////////////////////////////////////////////////////////////////////////////////

#include "ExecContext.h"

#include "GeneralServer/AuthenticationFeature.h"

using namespace arangodb;

thread_local std::shared_ptr<ExecContext const> ExecContext::CURRENT = nullptr;

std::shared_ptr<ExecContext const> const ExecContext::Superuser =
    std::make_shared<ExecContext const>(ExecContext::ConstructorToken{},
                                        AuthMode{AuthMode::Superuser{}});

/// Should always contain a reference to current user context
/*static*/ ExecContext const& ExecContext::current() {
  if (ExecContext::CURRENT != nullptr) {
    return *ExecContext::CURRENT;
  }
  return *ExecContext::Superuser;
}
/// Note that this intentionally returns CURRENT, even if it is a nullptr: This
/// makes it suitable to set CURRENT in another thread.
/*static*/ std::shared_ptr<ExecContext const> ExecContext::currentAsShared() {
  return ExecContext::CURRENT;
}

/// @brief an internal superuser context, is
///        a singleton instance, deleting is an error
/*static*/ ExecContext const& ExecContext::superuser() {
  return *ExecContext::Superuser;
}
/*static*/ std::shared_ptr<ExecContext const> ExecContext::superuserAsShared() {
  return ExecContext::Superuser;
}

ExecContext::ExecContext(ExecContext::ConstructorToken, AuthMode authMode)
    : _authMode(std::move(authMode)) {}

// TODO make this non-static, use _authenticationFeature instead
bool ExecContext::isAuthEnabled() {
  AuthenticationFeature* af = AuthenticationFeature::instance();
  TRI_ASSERT(af != nullptr);
  return af->isActive();
}

bool ExecContext::canUseDatabase(std::string const& db,
                                 AccessLevel requested) const {
  return _authMode.getIAuth().canUse(
      {Permission::Database{.name = db, .level = requested}});
}

/// @brief returns auth level for user
auth::Level ExecContext::collectionAuthLevel(std::string_view dbname,
                                             std::string_view coll) const {
  std::abort();  // TODO remove this method
  //  if (isInternal()) {
  //    // should be RW for superuser, RO for read-only
  //    return _databaseAuthLevel;
  //  }
  //
  //  AuthenticationFeature* af = AuthenticationFeature::instance();
  //  TRI_ASSERT(af != nullptr);
  //  if (!af->isActive()) {
  //    return auth::Level::RW;
  //  }
  //
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
  //
  //  auth::UserManager* um = af->userManager();
  //  TRI_ASSERT(um != nullptr);
  //  if (um == nullptr) {
  //    THROW_ARANGO_EXCEPTION_MESSAGE(TRI_ERROR_INTERNAL,
  //                                   "unable to find userManager instance");
  //  }
  //  return um->collectionAuthLevel(_user, dbname, coll, false);
}

/// @brief returns AccessLevel for user
AccessLevel ExecContext::collectionAccessLevel(
    std::string_view dbname, std::string_view collection) const {
  std::abort();
}

/// @brief returns true if the user can be read
bool ExecContext::canReadUser(std::string_view user) const {
  // TODO
  // Pseudocode:
  // if superuser: true
  // if self: true
  // if rbac:
  //   return AdminReadUser(user)
  // else:
  //   return RW(_system)
  return true;
}

/// @brief returns true if the user can be modified, note that everybody
/// can modify themselves (if only to change the password).
bool ExecContext::canWriteUser(std::string_view user) const {
  // TODO
  // Pseudocode:
  // if superuser: true
  // if self: true
  // if rbac:
  //   return AdminWriteUser(user)
  // else:
  //   return RW(_system)
  return true;
}

/// @brief returns true if a database can be created or dropped
bool ExecContext::canCreateOrDropDatabase(std::string_view db) const {
  return true;
}

/// @brief returns true for each user which can be read
std::vector<bool> ExecContext::canReadUsers(
    std::vector<std::string> users) const {
  return std::vector<bool>(users.size());
}

ExecContextScope::ExecContextScope(std::shared_ptr<ExecContext const> exe)
    : _old(std::move(exe)) {
  std::swap(ExecContext::CURRENT, _old);
}

ExecContextScope::~ExecContextScope() { std::swap(ExecContext::CURRENT, _old); }
