////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2017 ArangoDB GmbH, Cologne, Germany
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

#include "Cluster/ServerState.h"
#include "GeneralServer/AuthenticationFeature.h"
#include "VocBase/vocbase.h"

using namespace arangodb;

thread_local ExecContext const* ExecContext::CURRENT = nullptr;

ExecContext ExecContext::SUPERUSER(ExecContext::Type::Internal, "", "",
                                   auth::Level::RW, auth::Level::RW);

bool ExecContext::isAuthEnabled() {
  AuthenticationFeature* af = AuthenticationFeature::instance();
  TRI_ASSERT(af != nullptr);
  return af->isActive();
}

/// @brief an internal superuser context, is
///        a singleton instance, deleting is an error
ExecContext const* ExecContext::superuser() { return &ExecContext::SUPERUSER; }

ExecContext* ExecContext::create(std::string const& user,
                                 std::string const& dbname) {
  AuthenticationFeature* af = AuthenticationFeature::instance();
  TRI_ASSERT(af != nullptr);
  auth::Level dbLvl = auth::Level::RW;
  auth::Level sysLvl = auth::Level::RW;
  if (af->isActive()) {
    auth::UserManager* um = af->userManager();
    TRI_ASSERT(um != nullptr);
    if (um == nullptr) {
      THROW_ARANGO_EXCEPTION_MESSAGE(TRI_ERROR_INTERNAL, "unable to find userManager instance");
    }
    dbLvl = sysLvl = um->databaseAuthLevel(user, dbname);
    if (dbname != TRI_VOC_SYSTEM_DATABASE) {
      sysLvl = um->databaseAuthLevel(user, TRI_VOC_SYSTEM_DATABASE);
    }
  }
  return new ExecContext(ExecContext::Type::Default, user, dbname, sysLvl, dbLvl);
}

bool ExecContext::canUseDatabase(std::string const& db,
                                 auth::Level requested) const {
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
      THROW_ARANGO_EXCEPTION_MESSAGE(TRI_ERROR_INTERNAL, "unable to find userManager instance");
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
    THROW_ARANGO_EXCEPTION_MESSAGE(TRI_ERROR_INTERNAL, "unable to find userManager instance");
  }
  return um->collectionAuthLevel(_user, dbname, coll);
}
