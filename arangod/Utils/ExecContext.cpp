////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2017-2019 ArangoDB GmbH, Cologne, Germany
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

#include "Auth/CollectionResource.h"
#include "GeneralServer/AuthenticationFeature.h"
#include "VocBase/vocbase.h"

namespace arangodb {
// should always contain a reference to current user context
thread_local ExecContext const* ExecContext::CURRENT = nullptr;

ExecContext ExecContext::Superuser(auth::Level::RW, auth::Level::RW);

/*static*/ ExecContext const& ExecContext::current() {
  if (ExecContext::CURRENT != nullptr) {
    return *ExecContext::CURRENT;
  }
  return ExecContext::Superuser;
}

ExecContext::ExecContext(auth::Level systemLevel, auth::Level dbLevel)
  : _type(ExecContext::Type::Internal),
    _user(auth::AuthUser{""}),
    _database(auth::DatabaseResource{}),
    _canceled(false),
    _systemDbAuthLevel(systemLevel),
    _databaseAuthLevel(dbLevel) {
  TRI_ASSERT(_systemDbAuthLevel != auth::Level::UNDEFINED);
  TRI_ASSERT(_databaseAuthLevel != auth::Level::UNDEFINED);
}

ExecContext::ExecContext(auth::AuthUser const& user,
			 auth::DatabaseResource&& database, auth::Level systemLevel, auth::Level dbLevel)
: _type(ExecContext::Type::User),
  _user(user),
  _database(database),
  _canceled(false),
  _systemDbAuthLevel(systemLevel),
  _databaseAuthLevel(dbLevel) {
  TRI_ASSERT(_systemDbAuthLevel != auth::Level::UNDEFINED);
  TRI_ASSERT(_databaseAuthLevel != auth::Level::UNDEFINED);
}

bool ExecContext::isAuthEnabled() {
  AuthenticationFeature* af = AuthenticationFeature::instance();
  TRI_ASSERT(af != nullptr);
  return af != nullptr && af->isActive();
}

std::unique_ptr<ExecContext> ExecContext::create(auth::AuthUser const& user,
                                                 auth::DatabaseResource&& database) {
  AuthenticationFeature* af = AuthenticationFeature::instance();
  TRI_ASSERT(af != nullptr);

  if (af == nullptr && !af->isActive()) {
    // you cannot use make_unique here because the constructor is protected
    return std::unique_ptr<ExecContext>(new ExecContext(Superuser));
  }

  auth::UserManager* um = af->userManager();
  TRI_ASSERT(um != nullptr);

  if (um == nullptr) {
    THROW_ARANGO_EXCEPTION_MESSAGE(TRI_ERROR_INTERNAL,
				   "unable to find userManager instance");
  }

  auth::Level dbLvl = um->databaseAuthLevel(user.internalUsername(), database._database);
  auth::Level sysLvl = dbLvl;

  if (database._database != TRI_VOC_SYSTEM_DATABASE) {
    sysLvl = um->databaseAuthLevel(user.internalUsername(), TRI_VOC_SYSTEM_DATABASE);
  }

  // you cannot use make_unique here because the constructor is protected
  return std::unique_ptr<ExecContext>(new ExecContext(user, std::move(database), sysLvl, dbLvl));
}

auth::Level ExecContext::authLevel(auth::DatabaseResource const& database) const {
  if (isInternal() || database.equals(_database)) {
    // should be RW for superuser, RO for read-only
    return _databaseAuthLevel;
  }

  AuthenticationFeature* af = AuthenticationFeature::instance();
  TRI_ASSERT(af != nullptr);

  auth::UserManager* um = af->userManager();
  TRI_ASSERT(um != nullptr);

  if (um == nullptr) {
    THROW_ARANGO_EXCEPTION_MESSAGE(TRI_ERROR_INTERNAL,
				   "unable to find userManager instance");
  }

  return um->databaseAuthLevel(_user.internalUsername(), database._database);
}

auth::Level ExecContext::authLevel(auth::CollectionResource const& collection) const {
  if (isInternal()) {
    // should be RW for superuser, RO for read-only
    return _databaseAuthLevel;
  }

  AuthenticationFeature* af = AuthenticationFeature::instance();
  TRI_ASSERT(af != nullptr);

  if (!af->isActive()) {
    return auth::Level::RW;
  }

  // handle fixed permissions here outside auth module.
  // TODO: move this block above, such that it takes effect
  //       when authentication is disabled
  if (collection._database == TRI_VOC_SYSTEM_DATABASE && collection._collection == TRI_COL_NAME_USERS) {
    return auth::Level::NONE;
  } else if (collection._collection == "_queues") {
    return auth::Level::RO;
  } else if (collection._collection == "_frontend") {
    return auth::Level::RW;
  }  // intentional fall through

  auth::UserManager* um = af->userManager();
  TRI_ASSERT(um != nullptr);

  if (um == nullptr) {
    THROW_ARANGO_EXCEPTION_MESSAGE(TRI_ERROR_INTERNAL,
				   "unable to find userManager instance");
  }

  return um->collectionAuthLevel(_user.internalUsername(), collection._database, collection._collection);
}
}  // namespace arangodb

// ================================================================================
// ================================================================================
// ================================================================================
// ================================================================================
// ================================================================================

/*
std::string const& user, std::string const& dbname) {
  AuthenticationFeature* af = AuthenticationFeature::instance();
  TRI_ASSERT(af != nullptr);
  auth::Level dbLvl = auth::Level::RW;
  auth::Level sysLvl = auth::Level::RW;
  if (af->isActive()) {
    auth::UserManager* um = af->userManager();
    TRI_ASSERT(um != nullptr);
    if (um == nullptr) {
      THROW_ARANGO_EXCEPTION_MESSAGE(TRI_ERROR_INTERNAL,
                                     "unable to find userManager instance");
    }
    dbLvl = sysLvl = um->databaseAuthLevel(user, dbname);
    if (dbname != TRI_VOC_SYSTEM_DATABASE) {
      sysLvl = um->databaseAuthLevel(user, TRI_VOC_SYSTEM_DATABASE);
    }
  }
  auto* ptr = new ExecContext(ExecContext::Type::Default, user, dbname, sysLvl,
dbLvl); return std::unique_ptr<ExecContext>(ptr);
}
*/

/*
bool ExecContext::canUseDatabase(std::string const& db, auth::Level requested)
const { if (isInternal() || _database == db) {
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
*/

/*
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
  // handle fixed permissions here outside auth module.
  // TODO: move this block above, such that it takes effect
  //       when authentication is disabled
  if (dbname == TRI_VOC_SYSTEM_DATABASE && coll == TRI_COL_NAME_USERS) {
    return auth::Level::NONE;
  } else if (coll == "_queues") {
    return auth::Level::RO;
  } else if (coll == "_frontend") {
    return auth::Level::RW;
  }  // intentional fall through

  auth::UserManager* um = af->userManager();
  TRI_ASSERT(um != nullptr);
  if (um == nullptr) {
    THROW_ARANGO_EXCEPTION_MESSAGE(TRI_ERROR_INTERNAL,
                                   "unable to find userManager instance");
  }
  return um->collectionAuthLevel(_user, dbname, coll);
}
*/
