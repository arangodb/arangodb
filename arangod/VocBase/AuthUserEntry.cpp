////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2014-2017 ArangoDB GmbH, Cologne, Germany
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
/// @author Dr. Frank Celler
////////////////////////////////////////////////////////////////////////////////

#include "AuthInfo.h"

#include "Aql/Query.h"
#include "Aql/QueryString.h"
#include "Basics/ReadLocker.h"
#include "Basics/VelocyPackHelper.h"
#include "Basics/WriteLocker.h"
#include "Basics/tri-strings.h"
#include "Cluster/ServerState.h"
#include "GeneralServer/AuthenticationFeature.h"
#include "GeneralServer/GeneralServerFeature.h"
#include "Logger/Logger.h"
#include "RestServer/DatabaseFeature.h"
#include "Ssl/SslInterface.h"
#include "Random/UniformCharacter.h"

#include <velocypack/Builder.h>
#include <velocypack/Iterator.h>
#include <velocypack/velocypack-aliases.h>

using namespace arangodb;
using namespace arangodb::basics;
using namespace arangodb::velocypack;
using namespace arangodb::rest;


static AuthUserEntry CreateAuthUserEntry(VPackSlice const& slice, AuthSource source) {
  if (slice.isNone() || !slice.isObject()) {
    return AuthUserEntry();
  }

  // extract "user" attribute
  VPackSlice const userSlice = slice.get("user");

  if (!userSlice.isString()) {
    LOG_TOPIC(DEBUG, arangodb::Logger::FIXME) << "cannot extract username";
    return AuthUserEntry();
  }

  VPackSlice const authDataSlice = slice.get("authData");

  if (!authDataSlice.isObject()) {
    LOG_TOPIC(DEBUG, arangodb::Logger::FIXME) << "cannot extract authData";
    return AuthUserEntry();
  }

  VPackSlice const simpleSlice = authDataSlice.get("simple");

  if (!simpleSlice.isObject()) {
    LOG_TOPIC(DEBUG, arangodb::Logger::FIXME) << "cannot extract simple";
    return AuthUserEntry();
  }

  VPackSlice const methodSlice = simpleSlice.get("method");
  VPackSlice const saltSlice = simpleSlice.get("salt");
  VPackSlice const hashSlice = simpleSlice.get("hash");

  if (!methodSlice.isString() || !saltSlice.isString() ||
      !hashSlice.isString()) {
    LOG_TOPIC(DEBUG, arangodb::Logger::FIXME) << "cannot extract password internals";
    return AuthUserEntry();
  }

  // extract "active" attribute
  bool active;
  VPackSlice const activeSlice = authDataSlice.get("active");

  if (!activeSlice.isBoolean()) {
    LOG_TOPIC(DEBUG, arangodb::Logger::FIXME) << "cannot extract active flag";
    return AuthUserEntry();
  }

  active = activeSlice.getBool();

  // extract "changePassword" attribute
  bool mustChange =
      VelocyPackHelper::getBooleanValue(authDataSlice, "changePassword", false);

  // extract "databases" attribute
  VPackSlice const databasesSlice = slice.get("databases");

  std::unordered_map<std::string, std::shared_ptr<AuthContext>> authContexts;

  if (databasesSlice.isObject()) {
    for (auto const& obj : VPackObjectIterator(databasesSlice)) {

      if (obj.value.isObject()) {
        std::unordered_map<std::string, AuthLevel> collections;
        AuthLevel databaseAuth = AuthLevel::NONE;

        if (obj.value.hasKey("permissions") && obj.value.get("permissions").isObject()) {
          auto const permissionsSlice = obj.value.get("permissions");

          if (permissionsSlice.hasKey("read") && permissionsSlice.get("read").isTrue() ) {
            databaseAuth = AuthLevel::RO;
          }

          if (permissionsSlice.hasKey("write") && permissionsSlice.get("write").isTrue() ) {
            databaseAuth = AuthLevel::RW;
          }
        }

        if (obj.value.hasKey("collections") && obj.value.get("collections").isObject()) {
          for (auto const& collection : VPackObjectIterator(obj.value.get("collections"))) {

            if (collection.value.hasKey("permissions") && collection.value.get("permissions").isObject()) {
              auto const permissionsSlice = obj.value.get("permissions");
              std::string const collectionName = collection.key.copyString();
              AuthLevel collectionAuth = AuthLevel::NONE;

              if (permissionsSlice.hasKey("read") && permissionsSlice.get("read").isTrue() ) {
                collectionAuth = AuthLevel::RO;
              }

              if (permissionsSlice.hasKey("write") && permissionsSlice.get("write").isTrue() ) {
                collectionAuth = AuthLevel::RW;
              }
              collections.emplace(collectionName, collectionAuth);
            } // if
          } // for
        } // if

        authContexts.emplace(obj.key.copyString(), std::make_shared<AuthContext>(databaseAuth, std::move(collections)));

      } else {
        LOG_TOPIC(INFO, arangodb::Logger::CONFIG) << "Deprecation Warning: Update access rights for user '" << userSlice.copyString() << "'";
        ValueLength length;
        char const* value = obj.value.getString(length);

        if (TRI_CaseEqualString(value, "rw", 2)) {
          authContexts.emplace(obj.key.copyString(), std::make_shared<AuthContext>(AuthLevel::RW,
                                                   std::unordered_map<std::string, AuthLevel>({{"*", AuthLevel::RW}}) ));

        } else if (TRI_CaseEqualString(value, "ro", 2)) {
          authContexts.emplace(obj.key.copyString(), std::make_shared<AuthContext>(AuthLevel::RO,
                                                   std::unordered_map<std::string, AuthLevel>({{"*", AuthLevel::RO}}) ));
        }
      }
    } // for
  } // if

  AuthLevel systemDatabaseLevel = AuthLevel::RO;
  for (auto const& colName : std::vector<std::string>{"_system", "*"}) {
    auto const& it = authContexts.find(colName);
    if (it != authContexts.end()) {
      systemDatabaseLevel = it->second->databaseAuthLevel();
      break;
    }
  }

  for (auto const& ctx : authContexts) {
    LOG_TOPIC(DEBUG, arangodb::Logger::AUTHENTICATION) << userSlice.copyString() << " Database " << ctx.first;
    ctx.second->systemAuthLevel(systemDatabaseLevel);
    ctx.second->dump();
  }

  // build authentication entry
  return AuthUserEntry(userSlice.copyString(), methodSlice.copyString(),
                   saltSlice.copyString(), hashSlice.copyString(), active, mustChange, source, std::move(authContexts));
}

AuthLevel AuthUserEntry::canUseDatabase(std::string const& dbname) const {
  return getAuthContext(dbname)->databaseAuthLevel();
}

std::shared_ptr<AuthContext> AuthUserEntry::getAuthContext(std::string const& dbname) const {
  // std::unordered_map<std::string, std::shared_ptr<AuthContext>> _authContexts;

  for (auto const& database : std::vector<std::string>({dbname, "*"})) {
    auto const& it = _authContexts.find(database);

    if (it == _authContexts.end()) {
      continue;
    }
    return it->second;
  }

  return std::make_shared<AuthContext>(AuthLevel::NONE, std::unordered_map<std::string, AuthLevel>({{"*", AuthLevel::NONE}}));
}
