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
#include "GeneralServer/AuthenticationFeature.h"
#include "VocBase/AuthInfo.h"
#include "VocBase/vocbase.h"

using namespace arangodb;

thread_local ExecContext const* ExecContext::CURRENT = nullptr;

ExecContext ExecContext::SUPERUSER(true, "", "", AuthLevel::RW, AuthLevel::RW);

bool ExecContext::isAuthEnabled() {
  AuthenticationFeature* auth = AuthenticationFeature::INSTANCE;
  TRI_ASSERT(auth != nullptr);
  return auth->isActive();
}

/// @brief an internal superuser context, is
///        a singleton instance, deleting is an error
ExecContext const* ExecContext::superuser() { return &ExecContext::SUPERUSER; }

ExecContext* ExecContext::create(std::string const& user,
                                 std::string const& dbname) {
  AuthenticationFeature* auth = AuthenticationFeature::INSTANCE;
  TRI_ASSERT(auth != nullptr);
  AuthLevel dbLvl = auth->authInfo()->canUseDatabase(user, dbname);
  AuthLevel sysLvl = dbLvl;
  if (dbname != TRI_VOC_SYSTEM_DATABASE) {
    sysLvl = auth->authInfo()->canUseDatabase(user, TRI_VOC_SYSTEM_DATABASE);
  }
  return new ExecContext(false, user, dbname, sysLvl, dbLvl);
}

bool ExecContext::canUseDatabase(std::string const& db,
                                 AuthLevel requested) const {
  if (_internal || _database == db) {
    // should be RW for superuser, RO for read-only
    return requested <= _databaseAuthLevel;
  }
  
  AuthenticationFeature* auth = AuthenticationFeature::INSTANCE;
  TRI_ASSERT(auth != nullptr);
  if (auth->isActive()) {
    AuthLevel allowed = auth->authInfo()->canUseDatabase(_user, db);
    return requested <= allowed;
  }
  return true;
}

/// @brief returns auth level for user
AuthLevel ExecContext::collectionAuthLevel(std::string const& dbname,
                                           std::string const& coll) const {
  if (_internal) {
    // should be RW for superuser, RO for read-only
    return _databaseAuthLevel;
  }
  
  AuthenticationFeature* auth = AuthenticationFeature::INSTANCE;
  TRI_ASSERT(auth != nullptr);
  if (auth->isActive()) {
    // handle fixed permissions here outside auth module.
    // TODO: move this block above, such that it takes effect
    //       when authentication is disabled
    if (dbname == TRI_VOC_SYSTEM_DATABASE && coll == TRI_COL_NAME_USERS) {
      return AuthLevel::NONE;
    } else if (coll == "_queues") {
      return AuthLevel::RO;
    } else if (coll == "_frontend") {
      return AuthLevel::RW;
    }  // fall through
    return auth->authInfo()->canUseCollection(_user, dbname, coll);
  }
  return AuthLevel::RW;
}
