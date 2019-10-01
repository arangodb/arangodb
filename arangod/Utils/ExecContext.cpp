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

ExecContext ExecContext::Admin(auth::AuthUser{"admin"},
                               auth::DatabaseResource{""},
                               auth::Level::RW,
			       auth::Level::NONE);
ExecContext ExecContext::Nobody(auth::AuthUser{"nobody"},
                                auth::DatabaseResource{""},
				auth::Level::NONE,
				auth::Level::NONE);
ExecContext ExecContext::ReadOnlySuperuser(auth::Level::RO);
ExecContext ExecContext::Superuser(auth::Level::RW);

ExecContext const& ExecContext::current() {
  if (ExecContext::CURRENT != nullptr) {
    return *ExecContext::CURRENT;
  }
  return ExecContext::Superuser;
}

ExecContext::ExecContext(auth::Level dbLevel)
    : _type(ExecContext::Type::Internal),
      _user(auth::AuthUser{""}),
      _database(auth::DatabaseResource{}),
      _canceled(false),
      _systemDbAuthLevel(dbLevel),
      _databaseAuthLevel(dbLevel) {
  TRI_ASSERT(_systemDbAuthLevel != auth::Level::UNDEFINED);
  TRI_ASSERT(_databaseAuthLevel != auth::Level::UNDEFINED);
}

ExecContext::ExecContext(auth::AuthUser const& user, auth::DatabaseResource&& database,
                         auth::Level systemLevel, auth::Level dbLevel)
    : _type(ExecContext::Type::User),
      _isAuthEnabled(true),
      _user(user),
      _database(database),
      _canceled(false),
      _systemDbAuthLevel(systemLevel),
      _databaseAuthLevel(dbLevel) {
  TRI_ASSERT(_systemDbAuthLevel != auth::Level::UNDEFINED);
  TRI_ASSERT(_databaseAuthLevel != auth::Level::UNDEFINED);

  AuthenticationFeature* af = AuthenticationFeature::instance();
  _isAuthEnabled = (af != nullptr && af->isActive());
}

bool ExecContext::isAuthEnabled() {
  AuthenticationFeature* af = AuthenticationFeature::instance();
  return af != nullptr && af->isActive();
}

std::unique_ptr<ExecContext> ExecContext::create(auth::AuthUser const& user,
                                                 auth::DatabaseResource const& database) {
  auth::DatabaseResource db(database);

  return create(user, std::move(db));
}

std::unique_ptr<ExecContext> ExecContext::create(auth::AuthUser const& user,
                                                 auth::DatabaseResource&& database) {
  AuthenticationFeature* af = AuthenticationFeature::instance();
  TRI_ASSERT(af != nullptr);

  if (af == nullptr && !af->isActive()) {
    // you cannot use make_unique here because the constructor is protected
    return std::unique_ptr<ExecContext>(new ExecContext(auth::Level::RW));
  }

  auth::UserManager* um = af->userManager();
  TRI_ASSERT(um != nullptr);

  if (um == nullptr) {
    THROW_ARANGO_EXCEPTION_MESSAGE(TRI_ERROR_INTERNAL,
                                   "unable to find userManager instance");
  }

  auth::Level dbLvl = um->databaseAuthLevel(user.internalUsername(), database.database());
  auth::Level sysLvl = dbLvl;

  if (database.database() != TRI_VOC_SYSTEM_DATABASE) {
    sysLvl = um->databaseAuthLevel(user.internalUsername(), TRI_VOC_SYSTEM_DATABASE);
  }

  // you cannot use make_unique here because the constructor is protected
  return std::unique_ptr<ExecContext>(
      new ExecContext(user, std::move(database), sysLvl, dbLvl));
}

std::unique_ptr<ExecContext> ExecContext::createSuperuser() {
  // you cannot use make_unique here because the constructor is protected
  return std::unique_ptr<ExecContext>(new ExecContext(auth::Level::RW));
}

auth::Level ExecContext::authLevel(auth::DatabaseResource const& database) const {
  if (_type == Type::Internal || database.equals(_database)) {
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

  return um->databaseAuthLevel(_user.internalUsername(), database.database());
}

auth::Level ExecContext::authLevel(auth::CollectionResource const& collection) const {
  if (_type == Type::Internal) {
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
  if (collection.database() == TRI_VOC_SYSTEM_DATABASE &&
      collection.collection() == TRI_COL_NAME_USERS) {
    return auth::Level::NONE;
  } else if (collection.collection() == "_queues") {
    return auth::Level::RO;
  } else if (collection.collection() == "_frontend") {
    return auth::Level::RW;
  }  // intentional fall through

  auth::UserManager* um = af->userManager();
  TRI_ASSERT(um != nullptr);

  if (um == nullptr) {
    THROW_ARANGO_EXCEPTION_MESSAGE(TRI_ERROR_INTERNAL,
                                   "unable to find userManager instance");
  }

  return um->collectionAuthLevel(_user.internalUsername(), collection.database(),
                                 collection.collection());
}
}  // namespace arangodb
