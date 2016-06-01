////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2014-2016 ArangoDB GmbH, Cologne, Germany
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

#include <velocypack/Iterator.h>
#include <velocypack/velocypack-aliases.h>

#include "Basics/ReadLocker.h"
#include "Basics/VelocyPackHelper.h"
#include "Basics/WriteLocker.h"
#include "Logger/Logger.h"
#include "RestServer/DatabaseFeature.h"
#include "Utils/SingleCollectionTransaction.h"
#include "Utils/StandaloneTransactionContext.h"
#include "VocBase/MasterPointer.h"
#include "VocBase/collection.h"

using namespace arangodb;
using namespace arangodb::basics;

static AuthEntry CreateAuthEntry(VPackSlice const& slice) {
  if (slice.isNone() || !slice.isObject()) {
    return AuthEntry();
  }

  // extract "user" attribute
  VPackSlice const userSlice = slice.get("user");

  if (!userSlice.isString()) {
    LOG(DEBUG) << "cannot extract username";
    return AuthEntry();
  }

  VPackSlice const authDataSlice = slice.get("authData");

  if (!authDataSlice.isObject()) {
    LOG(DEBUG) << "cannot extract authData";
    return AuthEntry();
  }

  VPackSlice const simpleSlice = authDataSlice.get("simple");

  if (!simpleSlice.isObject()) {
    LOG(DEBUG) << "cannot extract simple";
    return AuthEntry();
  }

  VPackSlice const methodSlice = simpleSlice.get("method");
  VPackSlice const saltSlice = simpleSlice.get("salt");
  VPackSlice const hashSlice = simpleSlice.get("hash");

  if (!methodSlice.isString() || !saltSlice.isString() ||
      !hashSlice.isString()) {
    LOG(DEBUG) << "cannot extract password internals";
    return AuthEntry();
  }

  // extract "active" attribute
  bool active;
  VPackSlice const activeSlice = authDataSlice.get("active");

  if (!activeSlice.isBoolean()) {
    LOG(DEBUG) << "cannot extract active flag";
    return AuthEntry();
  }

  active = activeSlice.getBool();

  // extract "changePassword" attribute
  bool mustChange =
      VelocyPackHelper::getBooleanValue(slice, "changePassword", false);

  return AuthEntry(userSlice.copyString(), methodSlice.copyString(),
                   saltSlice.copyString(), hashSlice.copyString(), active,
                   mustChange);
}

void AuthInfo::clear() {
  _authInfo.clear();
  _authCache.clear();
  _authInfoLoaded = false;
}

bool AuthInfo::reload() {
  TRI_vocbase_t* vocbase = DatabaseFeature::DATABASE->vocbase();

  LOG(DEBUG) << "starting to load authentication and authorization information";

  WRITE_LOCKER(writeLocker, _authInfoLock);

  TRI_vocbase_col_t* collection =
      TRI_LookupCollectionByNameVocBase(vocbase, TRI_COL_NAME_USERS);

  if (collection == nullptr) {
    LOG(INFO) << "collection '_users' does not exist, no authentication will "
                 "be available";
    return false;
  }

  SingleCollectionTransaction trx(StandaloneTransactionContext::Create(vocbase),
                                  collection->_cid, TRI_TRANSACTION_READ);

  int res = trx.begin();

  if (res != TRI_ERROR_NO_ERROR) {
    return false;
  }

  auto work = [&](TRI_doc_mptr_t const* ptr) {
    VPackSlice slice(ptr->vpack());
    AuthEntry auth = CreateAuthEntry(slice);

    if (auth.isActive()) {
      _authInfo[auth.username()] = auth;
    }

    return true;
  };

  clear();

  trx.invokeOnAllElements(collection->_name, work);

  trx.finish(TRI_ERROR_NO_ERROR);

  _authInfoLoaded = true;
  return true;
}

bool AuthInfo::populate(VPackSlice const& slice) {
  TRI_ASSERT(slice.isArray());

  WRITE_LOCKER(writeLocker, _authInfoLock);

  clear();

  for (VPackSlice const& authSlice : VPackArrayIterator(slice)) {
    AuthEntry auth = CreateAuthEntry(authSlice);

    if (auth.isActive()) {
      _authInfo.emplace(auth.username(), auth);
    }
  }

  return true;
}

std::string AuthInfo::checkCache(std::string const& authorizationField,
                                 bool* mustChange) {
  READ_LOCKER(readLocker, _authInfoLock);

  auto const& it = _authCache.find(authorizationField);

  if (it != _authCache.end()) {
    AuthCache const& cached = it->second;

#warning expires
    *mustChange = cached.mustChange();
    return cached.username();
  }

  // sorry, not found
  return "";
}

bool AuthInfo::canUseDatabase(std::string const& username,
                              char const* databaseName) {
#warning TODO
#if 0
  READ_LOCKER(readLocker, _authInfoLock);

  AuthEntry const& entry = findUser(username);

  if (!entry.isActive()) {
    return false;
  }

  return entry._databases.find(databaseName) != entry.databases.end();
#endif
  return true;
}

AuthResult AuthInfo::checkAuthentication(std::string const& authorizationField,
                                         char const* databaseName) {
  return AuthResult();
}

bool AuthInfo::insertInitial() {
  try {
    VPackBuilder builder;
    builder.openArray();

    // The only users object
    builder.add(VPackValue(VPackValueType::Object));

    // username
    builder.add("user", VPackValue("root"));
    builder.add("authData", VPackValue(VPackValueType::Object));

    // simple auth
    builder.add("simple", VPackValue(VPackValueType::Object));
    builder.add("method", VPackValue("sha256"));

    char const* salt = "c776f5f4";
    builder.add("salt", VPackValue(salt));

    char const* hash =
        "ef74bc6fd59ac713bf5929c5ac2f42233e50d4d58748178132ea46dec433bd5b";
    builder.add("hash", VPackValue(hash));

    builder.close();  // simple

    builder.add("active", VPackValue(true));

    builder.add("databases", VPackValue(VPackValueType::Array));
    builder.add(VPackValue(TRI_VOC_SYSTEM_DATABASE));
    builder.close();

    builder.close();  // authData
    builder.close();  // The user object
    builder.close();  // The Array

    populate(builder.slice());

    return true;
  } catch (...) {
    // No action
  }
  // We get here only through error
  return false;
}

#if 0

    // no entry found in cache, decode the basic auth info and look it up
    std::string const up = StringUtils::decodeBase64(auth);
    std::string::size_type n = up.find(':', 0);

    if (n == std::string::npos || n == 0 || n + 1 > up.size()) {
      LOG(TRACE) << "invalid authentication data found, cannot extract "
                    "username/password";
      return GeneralResponse::ResponseCode::BAD;
    }

    username = up.substr(0, n);

    LOG(TRACE) << "checking authentication for user '" << username << "'";

////////////////////////////////////////////////////////////////////////////////
/// @brief check if a user can see a database
/// note: "seeing" here does not necessarily mean the user can access the db.
/// it only means there is a user account (with whatever password) present
/// in the database
////////////////////////////////////////////////////////////////////////////////

static bool CanUseDatabase(TRI_vocbase_t* vocbase, char const* username) {
  if (!vocbase->_settings.requireAuthentication) {
    // authentication is turned off
    return true;
  }

  if (strlen(username) == 0) {
    // will happen if username is "" (when converting it from a null value)
    // this will happen if authentication is turned off
    return true;
  }

  return TRI_ExistsAuthenticationAuthInfo(vocbase, username);
}

#endif
