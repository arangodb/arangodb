////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2014-2021 ArangoDB GmbH, Cologne, Germany
/// Copyright 2004-2014 triAGENS GmbH, Cologne, Germany
///
/// Licensed under the Apache License, Version 2.0 (the "License");
/// you may not use this file except in compliance with the License.
/// You may obtain a copy of the License at
///
///     http://www.apache.org/licenses/LICENSE-2.0
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

#include "Auth/UserManager.h"
#include "Basics/StaticStrings.h"
#include "Cluster/ServerState.h"
#include "GeneralServer/AuthenticationFeature.h"
#include "VocBase/vocbase.h"

using namespace arangodb;

thread_local ExecContext const* ExecContext::CURRENT = nullptr;

ExecContext const ExecContext::Superuser(ExecContext::Type::Internal, /*name*/"", /*db*/"",
                                         auth::Level::RW, auth::Level::RW, true);

/// Should always contain a reference to current user context
/*static*/ ExecContext const& ExecContext::current() {
  if (ExecContext::CURRENT != nullptr) {
    return *ExecContext::CURRENT;
  }
  return ExecContext::Superuser;
}

/// @brief an internal superuser context, is
///        a singleton instance, deleting is an error
/*static*/ ExecContext const& ExecContext::superuser() { return ExecContext::Superuser; }

ExecContext::ExecContext(ExecContext::Type type, std::string const& user,
            std::string const& database, auth::Level systemLevel, auth::Level dbLevel,
            bool isAdminUser)
      : _user(user),
        _database(database),
        _type(type),
        _isAdminUser(isAdminUser),
        _systemDbAuthLevel(systemLevel),
        _databaseAuthLevel(dbLevel) {
  TRI_ASSERT(_systemDbAuthLevel != auth::Level::UNDEFINED);
  TRI_ASSERT(_databaseAuthLevel != auth::Level::UNDEFINED);
}

/*static*/ bool ExecContext::isAuthEnabled() {
  AuthenticationFeature* af = AuthenticationFeature::instance();
  TRI_ASSERT(af != nullptr);
  return af->isActive();
}

std::unique_ptr<ExecContext> ExecContext::create(std::string const& user, std::string const& dbname) {
  AuthenticationFeature* af = AuthenticationFeature::instance();
  TRI_ASSERT(af != nullptr);
  auth::Level dbLvl = auth::Level::RW;
  auth::Level sysLvl = auth::Level::RW;
  bool isAdminUser = true;
  if (af->isActive()) {
    auth::UserManager* um = af->userManager();
    TRI_ASSERT(um != nullptr);
    if (um == nullptr) {
      THROW_ARANGO_EXCEPTION_MESSAGE(TRI_ERROR_INTERNAL,
                                     "unable to find userManager instance");
    }
    dbLvl = sysLvl = um->databaseAuthLevel(user, dbname, false);
    if (dbname != StaticStrings::SystemDatabase) {
      sysLvl = um->databaseAuthLevel(user, StaticStrings::SystemDatabase, false);
    }
    isAdminUser = (sysLvl == auth::Level::RW);
    if (!isAdminUser && ServerState::readOnly()) {
      isAdminUser = um->databaseAuthLevel(user, StaticStrings::SystemDatabase, true) == auth::Level::RW;
    }
  }
  // we cannot use std::make_unique here, as ExecContext has a protected constructor
  auto* ptr = new ExecContext(ExecContext::Type::Default, user, dbname, sysLvl, dbLvl, isAdminUser);
  return std::unique_ptr<ExecContext>(ptr);
}

bool ExecContext::canUseDatabase(std::string const& db, auth::Level requested) const {
  if (isInternal() || _database == db) {
    // should be RW for superuser, RO for read-only
    return requested <= _databaseAuthLevel;
  }

  AuthenticationFeature* af = AuthenticationFeature::instance();
  TRI_ASSERT(af != nullptr);
  if (af->isActive()) {
    auth::UserManager* um = af->userManager();
    TRI_ASSERT(um != nullptr);
    if (um == nullptr) {
      THROW_ARANGO_EXCEPTION_MESSAGE(TRI_ERROR_INTERNAL,
                                     "unable to find userManager instance");
    }
    auth::Level allowed = um->databaseAuthLevel(_user, db);
    return requested <= allowed;
  }
  return true;
}

/// @brief returns auth level for user
auth::Level ExecContext::collectionAuthLevel(std::string const& dbname,
                                             std::string const& coll) const {
  if (isInternal()) {
    // should be RW for superuser, RO for read-only
    return _databaseAuthLevel;
  }

  AuthenticationFeature* af = AuthenticationFeature::instance();
  TRI_ASSERT(af != nullptr);
  if (!af->isActive()) {
    return auth::Level::RW;
  }
  
  if (coll.size() >= 5 && coll[0] == '_') {
    // _users, _queues, _frontend

    // handle fixed permissions here outside auth module.
    // TODO: move this block above, such that it takes effect
    //       when authentication is disabled
    if (dbname == StaticStrings::SystemDatabase && coll == StaticStrings::UsersCollection) {
      return auth::Level::NONE;
    } else if (coll == StaticStrings::QueuesCollection) {
      return auth::Level::RO;
    } else if (coll == StaticStrings::FrontendCollection) {
      return auth::Level::RW;
    }  // intentional fall through
  }

  auth::UserManager* um = af->userManager();
  TRI_ASSERT(um != nullptr);
  if (um == nullptr) {
    THROW_ARANGO_EXCEPTION_MESSAGE(TRI_ERROR_INTERNAL,
                                   "unable to find userManager instance");
  }
  return um->collectionAuthLevel(_user, dbname, coll);
}
