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
/// @author Manuel Baesler
////////////////////////////////////////////////////////////////////////////////

#include "ExecContext.h"
#include "GeneralServer/AuthenticationFeature.h"
#include "StorageEngine/TransactionState.h"
#include "Transaction/Methods.h"
#include "VocBase/AuthInfo.h"
#include "VocBase/vocbase.h"

using namespace arangodb;

thread_local ExecContext const* ExecContext::CURRENT = nullptr;

ExecContext* ExecContext::copy() const {
  return new ExecContext(_user, _database, _systemAuthLevel,
                         _databaseAuthLevel);
}

typedef AuthenticationFeature _AF;

ExecContext::ExecContext(std::string const& u, std::string const& db)
    : _user(u),
      _database(db),
      _systemAuthLevel(
          _AF::INSTANCE->canUseDatabase(u, TRI_VOC_SYSTEM_DATABASE)),
      _databaseAuthLevel(_AF::INSTANCE->canUseDatabase(u, db)) {}

bool ExecContext::canUseDatabase(std::string const& db,
                                 AuthLevel requested) const {
  AuthenticationFeature* auth = AuthenticationFeature::INSTANCE;
  if (auth != nullptr && auth->isActive()) {
    AuthLevel allowed = auth->authInfo()->canUseDatabase(_user, db);
    return requested <= allowed;
  }
  return true;
}

bool ExecContext::canUseCollection(std::string const& db,
                                   std::string const& coll,
                                   AuthLevel requested) const {
  AuthenticationFeature* auth = AuthenticationFeature::INSTANCE;
  if (auth != nullptr && auth->isActive()) {
    AuthLevel allowed = auth->authInfo()->canUseCollection(_user, db, coll);
    return requested <= allowed;
  }
  return true;
}
