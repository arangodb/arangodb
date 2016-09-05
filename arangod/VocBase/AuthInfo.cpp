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

#include <chrono>

#include <velocypack/Builder.h>
#include <velocypack/Iterator.h>
#include <velocypack/velocypack-aliases.h>

#include "Basics/ReadLocker.h"
#include "Basics/VelocyPackHelper.h"
#include "Basics/WriteLocker.h"
#include "Basics/tri-strings.h"
#include "GeneralServer/GeneralServerFeature.h"
#include "Logger/Logger.h"
#include "RestServer/DatabaseFeature.h"
#include "Ssl/SslInterface.h"
#include "Utils/SingleCollectionTransaction.h"
#include "Utils/StandaloneTransactionContext.h"
#include "VocBase/MasterPointer.h"

using namespace arangodb;
using namespace arangodb::basics;
using namespace arangodb::velocypack;
using namespace arangodb::rest;

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

  // extract "databases" attribute
  VPackSlice const databasesSlice = slice.get("databases");
  std::unordered_map<std::string, AuthLevel> databases;
  AuthLevel allDatabases = AuthLevel::NONE;

  if (databasesSlice.isObject()) {
    for (auto const& obj : VPackObjectIterator(databasesSlice)) {
      std::string const key = obj.key.copyString();

      ValueLength length;
      char const* value = obj.value.getString(length);

      if (TRI_CaseEqualString(value, "rw", 2)) {
        if (key == "*") {
          allDatabases = AuthLevel::RW;
        } else {
          databases.emplace(key, AuthLevel::RW);
        }
      } else if (TRI_CaseEqualString(value, "ro", 2)) {
        if (key == "*") {
          allDatabases = AuthLevel::RO;
        } else {
          databases.emplace(key, AuthLevel::RO);
        }
      }
    }
  }

  // build authentication entry
  return AuthEntry(userSlice.copyString(), methodSlice.copyString(),
                   saltSlice.copyString(), hashSlice.copyString(), databases,
                   allDatabases, active, mustChange);
}

AuthLevel AuthEntry::canUseDatabase(std::string const& dbname) const {
  auto const& it = _databases.find(dbname);

  if (it == _databases.end()) {
    return _allDatabases;
  }

  return it->second;
}

void AuthInfo::clear() {
  _authInfo.clear();
  _authBasicCache.clear();
}

void AuthInfo::insertInitial() {
  if (!_authInfo.empty()) {
    return;
  }

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

    builder.close();  // authData

    builder.add("databases", VPackValue(VPackValueType::Object));
    builder.add("*", VPackValue("rw"));
    builder.close();

    builder.close();  // The user object
    builder.close();  // The Array

    populate(builder.slice());
  } catch (...) {
    // No action
  }
}

bool AuthInfo::populate(VPackSlice const& slice) {
  TRI_ASSERT(slice.isArray());

  WRITE_LOCKER(writeLocker, _authInfoLock);

  clear();

  for (VPackSlice const& authSlice : VPackArrayIterator(slice)) {
    AuthEntry auth = CreateAuthEntry(authSlice.resolveExternal());

    if (auth.isActive()) {
      _authInfo.emplace(auth.username(), auth);
    }
  }

  return true;
}

void AuthInfo::reload() {
  insertInitial();

  TRI_vocbase_t* vocbase = DatabaseFeature::DATABASE->systemDatabase();

  if (vocbase == nullptr) {
    LOG(DEBUG) << "system database is unknown, cannot load authentication "
               << "and authorization information";
    return;
  }

  LOG(DEBUG) << "starting to load authentication and authorization information";

  SingleCollectionTransaction trx(StandaloneTransactionContext::Create(vocbase),
                                  TRI_COL_NAME_USERS, TRI_TRANSACTION_READ);

  int res = trx.begin();

  if (res != TRI_ERROR_NO_ERROR) {
    LOG(ERR) << "cannot start transaction to load authentication";
    return;
  }

  OperationResult users =
      trx.all(TRI_COL_NAME_USERS, 0, UINT64_MAX, OperationOptions());

  trx.finish(users.code);

  if (users.failed()) {
    LOG(ERR) << "cannot read users from _users collection";
    return;
  }

  auto usersSlice = users.slice();

  if (!usersSlice.isArray()) {
    LOG(ERR) << "cannot read users from _users collection";
    return;
  }

  if (usersSlice.length() == 0) {
    insertInitial();
  } else {
    populate(usersSlice);
  }

  _outdated = false;
}

AuthResult AuthInfo::checkPassword(std::string const& username,
                                   std::string const& password) {
  if (_outdated) {
    reload();
  }

  AuthResult result;

  // look up username
  READ_LOCKER(readLocker, _authInfoLock);

  auto it = _authInfo.find(username);

  if (it == _authInfo.end()) {
    return result;
  }

  AuthEntry const& auth = it->second;

  if (!auth.isActive()) {
    return result;
  }

  result._username = username;
  result._mustChange = auth.mustChange();

  std::string salted = auth.passwordSalt() + password;
  size_t len = salted.size();

  std::string const& passwordMethod = auth.passwordMethod();

  // default value is false
  char* crypted = nullptr;
  size_t cryptedLength;

  try {
    if (passwordMethod == "sha1") {
      arangodb::rest::SslInterface::sslSHA1(salted.c_str(), len, crypted,
                                            cryptedLength);
    } else if (passwordMethod == "sha512") {
      arangodb::rest::SslInterface::sslSHA512(salted.c_str(), len, crypted,
                                              cryptedLength);
    } else if (passwordMethod == "sha384") {
      arangodb::rest::SslInterface::sslSHA384(salted.c_str(), len, crypted,
                                              cryptedLength);
    } else if (passwordMethod == "sha256") {
      arangodb::rest::SslInterface::sslSHA256(salted.c_str(), len, crypted,
                                              cryptedLength);
    } else if (passwordMethod == "sha224") {
      arangodb::rest::SslInterface::sslSHA224(salted.c_str(), len, crypted,
                                              cryptedLength);
    } else if (passwordMethod == "md5") {
      arangodb::rest::SslInterface::sslMD5(salted.c_str(), len, crypted,
                                           cryptedLength);
    } else {
      // invalid algorithm...
    }
  } catch (...) {
    // SslInterface::ssl....() allocate strings with new, which might throw
    // exceptions
  }

  if (crypted != nullptr) {
    if (0 < cryptedLength) {
      size_t hexLen;
      char* hex = TRI_EncodeHexString(crypted, cryptedLength, &hexLen);

      if (hex != nullptr) {
        result._authorized = auth.checkPasswordHash(hex);
        TRI_FreeString(TRI_CORE_MEM_ZONE, hex);
      }
    }

    delete[] crypted;
  }

  return result;
}

AuthLevel AuthInfo::canUseDatabase(std::string const& username,
                                   std::string const& dbname) {
  if (_outdated) {
    reload();
  }

  auto const& it = _authInfo.find(username);

  if (it == _authInfo.end()) {
    return AuthLevel::NONE;
  }

  AuthEntry const& entry = it->second;

  return entry.canUseDatabase(dbname);
}

AuthResult AuthInfo::checkAuthentication(AuthType authType,
                                         std::string const& secret) {
  if (_outdated) {
    reload();
  }

  switch (authType) {
    case AuthType::BASIC:
      return checkAuthenticationBasic(secret);

    case AuthType::JWT:
      return checkAuthenticationJWT(secret);
  }

  return AuthResult();
}

AuthResult AuthInfo::checkAuthenticationBasic(std::string const& secret) {
  auto const& it = _authBasicCache.find(secret);

  if (it != _authBasicCache.end()) {
    return it->second;
  }

  std::string const up = StringUtils::decodeBase64(secret);
  std::string::size_type n = up.find(':', 0);

  if (n == std::string::npos || n == 0 || n + 1 > up.size()) {
    LOG(TRACE) << "invalid authentication data found, cannot extract "
                  "username/password";
    return AuthResult();
  }

  std::string username = up.substr(0, n);
  std::string password = up.substr(n + 1);

  AuthResult result = checkPassword(username, password);

  if (result._authorized) {
    _authBasicCache.emplace(secret, result);
  }

  return result;
}

AuthResult AuthInfo::checkAuthenticationJWT(std::string const& secret) {
  std::vector<std::string> const parts = StringUtils::split(secret, '.');

  if (parts.size() != 3) {
    LOG(DEBUG) << "Secret contains " << parts.size() << " parts";
    return AuthResult();
  }

  std::string const& header = parts[0];
  std::string const& body = parts[1];
  std::string const& signature = parts[2];

  std::string const message = header + "." + body;

  if (!validateJwtHeader(header)) {
    LOG(DEBUG) << "Couldn't validate jwt header " << header;
    return AuthResult();
  }

  std::string username;
  if (!validateJwtBody(body, &username)) {
    LOG(DEBUG) << "Couldn't validate jwt body " << body;
    return AuthResult();
  }

  if (!validateJwtHMAC256Signature(message, signature)) {
    LOG(DEBUG) << "Couldn't validate jwt signature " << signature;
    return AuthResult();
  }

  AuthResult result;
  result._username = username;
  result._authorized = true;

  return result;
}

std::shared_ptr<VPackBuilder> AuthInfo::parseJson(std::string const& str,
                                                  std::string const& hint) {
  std::shared_ptr<VPackBuilder> result;
  VPackParser parser;
  try {
    parser.parse(str);
    result = parser.steal();
  } catch (std::bad_alloc const&) {
    LOG(ERR) << "Out of memory parsing " << hint << "!";
  } catch (VPackException const& ex) {
    LOG(DEBUG) << "Couldn't parse " << hint << ": " << ex.what();
  } catch (...) {
    LOG(ERR) << "Got unknown exception trying to parse " << hint;
  }

  return result;
}

bool AuthInfo::validateJwtHeader(std::string const& header) {
  std::shared_ptr<VPackBuilder> headerBuilder =
      parseJson(StringUtils::decodeBase64(header), "jwt header");
  if (headerBuilder.get() == nullptr) {
    return false;
  }

  VPackSlice const headerSlice = headerBuilder->slice();
  if (!headerSlice.isObject()) {
    return false;
  }

  VPackSlice const algSlice = headerSlice.get("alg");
  VPackSlice const typSlice = headerSlice.get("typ");

  if (!algSlice.isString()) {
    return false;
  }

  if (!typSlice.isString()) {
    return false;
  }

  if (algSlice.copyString() != "HS256") {
    return false;
  }

  std::string typ = typSlice.copyString();
  if (typ != "JWT") {
    return false;
  }

  return true;
}

bool AuthInfo::validateJwtBody(std::string const& body, std::string* username) {
  std::shared_ptr<VPackBuilder> bodyBuilder =
      parseJson(StringUtils::decodeBase64(body), "jwt body");
  if (bodyBuilder.get() == nullptr) {
    return false;
  }

  VPackSlice const bodySlice = bodyBuilder->slice();
  if (!bodySlice.isObject()) {
    return false;
  }

  VPackSlice const issSlice = bodySlice.get("iss");
  if (!issSlice.isString()) {
    return false;
  }

  if (issSlice.copyString() != "arangodb") {
    return false;
  }

  VPackSlice const usernameSlice = bodySlice.get("preferred_username");
  if (!usernameSlice.isString()) {
    return false;
  }

  *username = usernameSlice.copyString();

  // mop: optional exp (cluster currently uses non expiring jwts)
  if (bodySlice.hasKey("exp")) {
    VPackSlice const expSlice = bodySlice.get("exp");

    if (!expSlice.isNumber()) {
      return false;
    }

    std::chrono::system_clock::time_point expires(
        std::chrono::seconds(expSlice.getNumber<uint64_t>()));
    std::chrono::system_clock::time_point now =
        std::chrono::system_clock::now();

    if (now >= expires) {
      return false;
    }
  }
  return true;
}

bool AuthInfo::validateJwtHMAC256Signature(std::string const& message,
                                           std::string const& signature) {
  std::string decodedSignature = StringUtils::decodeBase64U(signature);

  std::string const& jwtSecret = GeneralServerFeature::getJwtSecret();
  return verifyHMAC(jwtSecret.c_str(), jwtSecret.length(), message.c_str(),
                    message.length(), decodedSignature.c_str(),
                    decodedSignature.length(),
                    SslInterface::Algorithm::ALGORITHM_SHA256);
}
