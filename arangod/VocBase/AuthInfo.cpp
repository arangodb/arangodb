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


static AuthEntry CreateAuthEntry(VPackSlice const& slice, AuthSource source) {
  if (slice.isNone() || !slice.isObject()) {
    return AuthEntry();
  }

  // extract "user" attribute
  VPackSlice const userSlice = slice.get("user");

  if (!userSlice.isString()) {
    LOG_TOPIC(DEBUG, arangodb::Logger::FIXME) << "cannot extract username";
    return AuthEntry();
  }

  VPackSlice const authDataSlice = slice.get("authData");

  if (!authDataSlice.isObject()) {
    LOG_TOPIC(DEBUG, arangodb::Logger::FIXME) << "cannot extract authData";
    return AuthEntry();
  }

  VPackSlice const simpleSlice = authDataSlice.get("simple");

  if (!simpleSlice.isObject()) {
    LOG_TOPIC(DEBUG, arangodb::Logger::FIXME) << "cannot extract simple";
    return AuthEntry();
  }

  VPackSlice const methodSlice = simpleSlice.get("method");
  VPackSlice const saltSlice = simpleSlice.get("salt");
  VPackSlice const hashSlice = simpleSlice.get("hash");

  if (!methodSlice.isString() || !saltSlice.isString() ||
      !hashSlice.isString()) {
    LOG_TOPIC(DEBUG, arangodb::Logger::FIXME) << "cannot extract password internals";
    return AuthEntry();
  }

  // extract "active" attribute
  bool active;
  VPackSlice const activeSlice = authDataSlice.get("active");

  if (!activeSlice.isBoolean()) {
    LOG_TOPIC(DEBUG, arangodb::Logger::FIXME) << "cannot extract active flag";
    return AuthEntry();
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
    LOG_TOPIC(INFO, arangodb::Logger::FIXME) << userSlice.copyString() << " Database " << ctx.first;
    ctx.second->systemAuthLevel(systemDatabaseLevel);
    ctx.second->dump();
  }

  // build authentication entry
  return AuthEntry(userSlice.copyString(), methodSlice.copyString(),
                   saltSlice.copyString(), hashSlice.copyString(), active, mustChange, source, std::move(authContexts));
}

AuthLevel AuthEntry::canUseDatabase(std::string const& dbname) const {
  return getAuthContext(dbname)->databaseAuthLevel();
}

std::shared_ptr<AuthContext> AuthEntry::getAuthContext(std::string const& dbname) const {
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

AuthInfo::AuthInfo()
    : _outdated(true),
    _authJwtCache(16384),
    _noneAuthContext(std::make_shared<AuthContext>(AuthLevel::NONE, std::unordered_map<std::string, AuthLevel>({{"*", AuthLevel::NONE}}))),
    _jwtSecret(""),
    _queryRegistry(nullptr) {}

void AuthInfo::setJwtSecret(std::string const& jwtSecret) {
  WRITE_LOCKER(writeLocker, _authJwtLock);
  _jwtSecret = jwtSecret;
  _authJwtCache.clear();
}

std::string AuthInfo::jwtSecret() {
  READ_LOCKER(writeLocker, _authJwtLock);
  return _jwtSecret;
}

// private, must be called with _authInfoLock in write mode
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

// private, must be called with _authInfoLock in write mode
bool AuthInfo::populate(VPackSlice const& slice) {
  TRI_ASSERT(slice.isArray());

  _authInfo.clear();
  _authBasicCache.clear();

  for (VPackSlice const& authSlice : VPackArrayIterator(slice)) {
    VPackSlice const& s = authSlice.resolveExternal();

    if (s.hasKey("source") && s.get("source").isString() && s.get("source").copyString() == "LDAP") {
      LOG_TOPIC(TRACE, arangodb::Logger::CONFIG) << "LDAP: skip user in collection _users: " << s.get("user").copyString();
      continue;
    }
    AuthEntry auth = CreateAuthEntry(s, AuthSource::COLLECTION);

    if (auth.isActive()) {
      _authInfo.emplace(auth.username(), std::move(auth));
    }
  }

  return true;
}

// private, will acquire _authInfoLock in write-mode and release it
void AuthInfo::reload() {
  auto role = ServerState::instance()->getRole();

  if (role != ServerState::ROLE_SINGLE
      && role != ServerState::ROLE_COORDINATOR) {
    _outdated = false;
    return;
  }

  // TODO: is this correct?
  if (_authenticationHandler == nullptr) {
    _authenticationHandler.reset(application_features::ApplicationServer::getFeature<AuthenticationFeature>("Authentication")->getHandler());
  }
  
  {
    WRITE_LOCKER(writeLocker, _authInfoLock);
    insertInitial();
  }

  TRI_vocbase_t* vocbase = DatabaseFeature::DATABASE->systemDatabase();

  if (vocbase == nullptr) {
    LOG_TOPIC(DEBUG, arangodb::Logger::FIXME) << "system database is unknown, cannot load authentication "
               << "and authorization information";
    return;
  }
  
  MUTEX_LOCKER(locker, _queryLock);
  if (!_outdated) {
    return;
  }
  std::string const queryStr("FOR user IN _users RETURN user");
  auto emptyBuilder = std::make_shared<VPackBuilder>();
  
  arangodb::aql::Query query(false, vocbase, arangodb::aql::QueryString(queryStr),
                             emptyBuilder, emptyBuilder,
                             arangodb::aql::PART_MAIN);

  LOG_TOPIC(DEBUG, arangodb::Logger::FIXME) << "starting to load authentication and authorization information";
  TRI_ASSERT(_queryRegistry != nullptr);
  auto queryResult = query.execute(_queryRegistry);
  
  if (queryResult.code != TRI_ERROR_NO_ERROR) {
    if (queryResult.code == TRI_ERROR_REQUEST_CANCELED ||
        (queryResult.code == TRI_ERROR_QUERY_KILLED)) {
      THROW_ARANGO_EXCEPTION(TRI_ERROR_REQUEST_CANCELED);
    }
    _outdated = false;
    return;
  }
  
  VPackSlice usersSlice = queryResult.result->slice();

  if (usersSlice.isNone()) {
    THROW_ARANGO_EXCEPTION(TRI_ERROR_OUT_OF_MEMORY);
  }
  if (!usersSlice.isArray()) {
    LOG_TOPIC(ERR, arangodb::Logger::FIXME) << "cannot read users from _users collection";
    return;
  }

  {
    WRITE_LOCKER(writeLocker, _authInfoLock);

    if (usersSlice.length() == 0) {
      insertInitial();
    } else {
      populate(usersSlice);
    }
  }

  _outdated = false;
}



// protected
HexHashResult AuthInfo::hexHashFromData(std::string const& hashMethod, char const* data, size_t len) {
  char* crypted = nullptr;
  size_t cryptedLength;
  char* hex;

  try {
    if (hashMethod == "sha1") {
      arangodb::rest::SslInterface::sslSHA1(data, len, crypted,
                                            cryptedLength);
    } else if (hashMethod == "sha512") {
      arangodb::rest::SslInterface::sslSHA512(data, len, crypted,
                                              cryptedLength);
    } else if (hashMethod == "sha384") {
      arangodb::rest::SslInterface::sslSHA384(data, len, crypted,
                                              cryptedLength);
    } else if (hashMethod == "sha256") {
      arangodb::rest::SslInterface::sslSHA256(data, len, crypted,
                                              cryptedLength);
    } else if (hashMethod == "sha224") {
      arangodb::rest::SslInterface::sslSHA224(data, len, crypted,
                                              cryptedLength);
    } else if (hashMethod == "md5") {
      arangodb::rest::SslInterface::sslMD5(data, len, crypted,
                                           cryptedLength);
    } else {
      // invalid algorithm...
      LOG_TOPIC(DEBUG, arangodb::Logger::FIXME) << "invalid algorithm for hexHashFromData: " << hashMethod;
      return HexHashResult(TRI_ERROR_FAILED); // TODO: fix to correct error number
    }
  } catch (...) {
    // SslInterface::ssl....() allocate strings with new, which might throw
    // exceptions
    return HexHashResult(TRI_ERROR_FAILED);
  }

  if (crypted == nullptr ||
      cryptedLength == 0) {
    delete[] crypted;
    return HexHashResult(TRI_ERROR_OUT_OF_MEMORY);
  }

  size_t hexLen;
  hex = TRI_EncodeHexString(crypted, cryptedLength, &hexLen);
  delete[] crypted;

  if (hex == nullptr) {
    return HexHashResult(TRI_ERROR_OUT_OF_MEMORY);
  }

  HexHashResult result(std::string(hex, hexLen));
  TRI_FreeString(TRI_CORE_MEM_ZONE, hex);

  return result;
}

// public
AuthResult AuthInfo::checkPassword(std::string const& username,
                                   std::string const& password) {
  if (_outdated) {
    reload();
  }

  AuthResult result(username);

  WRITE_LOCKER(writeLocker, _authInfoLock);
  auto it = _authInfo.find(username);

  if (it == _authInfo.end() || (it->second.source() == AuthSource::LDAP)) { // && it->second.created() < TRI_microtime() - 60)) {
    TRI_ASSERT(_authenticationHandler != nullptr);
    AuthenticationResult authResult = _authenticationHandler->authenticate(username, password);

    if (!authResult.ok()) {
      return result;
    }

    if (authResult.source() == AuthSource::LDAP) { // user authed, add to _authInfo and _users
      if (it != _authInfo.end()) { //  && it->second.created() < TRI_microtime() - 60) {
        _authInfo.erase(username);
        it = _authInfo.end();
      }

      if (it == _authInfo.end()) {
        TRI_vocbase_t* vocbase = DatabaseFeature::DATABASE->systemDatabase();

        if (vocbase == nullptr) {
          LOG_TOPIC(DEBUG, arangodb::Logger::FIXME) << "system database is unknown, cannot load authentication "
                    << "and authorization information";
          THROW_ARANGO_EXCEPTION_MESSAGE(TRI_ERROR_FAILED, "_system databse is unknown");
        }

        MUTEX_LOCKER(locker, _queryLock);

        std::string const queryStr("UPSERT {user: @username} INSERT @user REPLACE @user IN _users");
        auto emptyBuilder = std::make_shared<VPackBuilder>();

        VPackBuilder binds;
        binds.openObject();
        binds.add("username", VPackValue(username));

        binds.add("user", VPackValue(VPackValueType::Object));

        binds.add("user", VPackValue(username));
        binds.add("source", VPackValue("LDAP"));

        binds.add("databases", VPackValue(VPackValueType::Object));
        for(auto const& permission : authResult.permissions() ) {
          binds.add(permission.first, VPackValue(permission.second));
        }
        binds.close();

        binds.add("configData", VPackValue(VPackValueType::Object));
        binds.close();

        binds.add("userData", VPackValue(VPackValueType::Object));
        binds.close();

        binds.add("authData", VPackValue(VPackValueType::Object));
        binds.add("active", VPackValue(true));
        binds.add("changePassword", VPackValue(false));

        binds.add("simple", VPackValue(VPackValueType::Object));
        binds.add("method", VPackValue("sha256"));

        std::string salt = UniformCharacter(8, "0123456789abcdef").random();
        binds.add("salt", VPackValue(salt));

        std::string saltedPassword = salt + password;
        HexHashResult hex = hexHashFromData("sha256", saltedPassword.data(), saltedPassword.size());
        if (!hex.ok()) {
          THROW_ARANGO_EXCEPTION_MESSAGE(TRI_ERROR_FAILED, "Could not calculate hex-hash from data");
        }
        binds.add("hash", VPackValue(hex.hexHash()));

        binds.close();  // simple
        binds.close(); // authData

        binds.close(); // user
        binds.close(); // obj

        arangodb::aql::Query query(false, vocbase, arangodb::aql::QueryString(queryStr),
                                   std::make_shared<VPackBuilder>(binds), emptyBuilder,
                                   arangodb::aql::PART_MAIN);

        TRI_ASSERT(_queryRegistry != nullptr);
        auto queryResult = query.execute(_queryRegistry);

        if (queryResult.code != TRI_ERROR_NO_ERROR) {
          if (queryResult.code == TRI_ERROR_REQUEST_CANCELED ||
              (queryResult.code == TRI_ERROR_QUERY_KILLED)) {
            THROW_ARANGO_EXCEPTION(TRI_ERROR_REQUEST_CANCELED);
          }
          _outdated = false;
          THROW_ARANGO_EXCEPTION_MESSAGE(TRI_ERROR_FAILED, "query error");
        }

        VPackBuilder builder;
        builder.openObject();

        // username
        builder.add("user", VPackValue(username));
        builder.add("source", VPackValue("LDAP"));
        builder.add("authData", VPackValue(VPackValueType::Object));

        // simple auth
        builder.add("simple", VPackValue(VPackValueType::Object));
        builder.add("method", VPackValue("sha256"));

        builder.add("salt", VPackValue(salt));

        builder.add("hash", VPackValue(hex.hexHash()));

        builder.close();  // simple

        builder.add("active", VPackValue(true));

        builder.close();  // authData

        builder.add("databases", VPackValue(VPackValueType::Object));
        for(auto const& permission : authResult.permissions() ) {
          builder.add(permission.first, VPackValue(permission.second));
        }
        builder.close();
        builder.close();  // The Object

        AuthEntry auth = CreateAuthEntry(builder.slice().resolveExternal(), AuthSource::LDAP);
        _authInfo.emplace(auth.username(), std::move(auth));

        it = _authInfo.find(username);
      }
    } // AuthSource::LDAP
  }

  if (it == _authInfo.end()) {
    return result;
  }

  AuthEntry const& auth = it->second;

  if (!auth.isActive()) {
    return result;
  }

  result._mustChange = auth.mustChange();

  std::string salted = auth.passwordSalt() + password;
  HexHashResult hexHash = hexHashFromData(auth.passwordMethod(), salted.data(), salted.size());

  if (!hexHash.ok()) {
    THROW_ARANGO_EXCEPTION_MESSAGE(TRI_ERROR_INTERNAL, "hexcalc did not work");
  }

  result._authorized = auth.checkPasswordHash(hexHash.hexHash());

  return result;
}

// public
AuthLevel AuthInfo::canUseDatabase(std::string const& username,
                                   std::string const& dbname) {
  if (_outdated) {
    reload();
  }

  READ_LOCKER(readLocker, _authInfoLock);

  auto const& it = _authInfo.find(username);

  if (it == _authInfo.end()) {
    return AuthLevel::NONE;
  }

  AuthEntry const& entry = it->second;

  return entry.canUseDatabase(dbname);
}

// public called from VocbaseContext.cpp
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

// private
AuthResult AuthInfo::checkAuthenticationBasic(std::string const& secret) {
  {
    READ_LOCKER(readLocker, _authInfoLock);
    auto const& it = _authBasicCache.find(secret);

    if (it != _authBasicCache.end()) {
      return it->second;
    }
  }

  std::string const up = StringUtils::decodeBase64(secret);
  std::string::size_type n = up.find(':', 0);

  if (n == std::string::npos || n == 0 || n + 1 > up.size()) {
    LOG_TOPIC(TRACE, arangodb::Logger::FIXME) << "invalid authentication data found, cannot extract "
                  "username/password";
    return AuthResult();
  }

  std::string username = up.substr(0, n);
  std::string password = up.substr(n + 1);

  AuthResult result = checkPassword(username, password);

  {
    WRITE_LOCKER(readLocker, _authInfoLock);

    if (result._authorized) {
      if (!_authBasicCache.emplace(secret, result).second) {
        // insertion did not work - probably another thread did insert the 
        // same data right now
        // erase it and re-insert our version
        _authBasicCache.erase(secret);
        _authBasicCache.emplace(secret, result);
      }
    } else {
      _authBasicCache.erase(secret);
    }
  } 

  return result;
}

AuthResult AuthInfo::checkAuthenticationJWT(std::string const& jwt) {
  try {
    READ_LOCKER(readLocker, _authJwtLock);
    auto result = _authJwtCache.get(jwt);
    if (result._expires) {
      std::chrono::system_clock::time_point now =
        std::chrono::system_clock::now();

      if (now >= result._expireTime) {
        readLocker.unlock();
        WRITE_LOCKER(writeLocker, _authJwtLock);
        result = _authJwtCache.get(jwt);
        if (result._expires && now >= result._expireTime) {
          try {
            _authJwtCache.remove(jwt);
          } catch (std::range_error const&) {
          }
        }
        return AuthResult();
      }
    }
    return (AuthResult) result;
  } catch (std::range_error const&) {
    // mop: not found
  }

  std::vector<std::string> const parts = StringUtils::split(jwt, '.');

  if (parts.size() != 3) {
    LOG_TOPIC(TRACE, arangodb::Logger::FIXME) << "Secret contains " << parts.size() << " parts";
    return AuthResult();
  }
  
  std::string const& header = parts[0];
  std::string const& body = parts[1];
  std::string const& signature = parts[2];

  if (!validateJwtHeader(header)) {
    LOG_TOPIC(TRACE, arangodb::Logger::FIXME) << "Couldn't validate jwt header " << header;
    return AuthResult();
  }

  AuthJwtResult result = validateJwtBody(body);
  if (!result._authorized) {
    LOG_TOPIC(TRACE, arangodb::Logger::FIXME) << "Couldn't validate jwt body " << body;
    return AuthResult();
  }

  std::string const message = header + "." + body;

  if (!validateJwtHMAC256Signature(message, signature)) {
    LOG_TOPIC(TRACE, arangodb::Logger::FIXME) << "Couldn't validate jwt signature " << signature << " " << _jwtSecret;
    return AuthResult();
  }

  WRITE_LOCKER(writeLocker, _authJwtLock);
  _authJwtCache.put(jwt, result);
  return (AuthResult) result;
}

std::shared_ptr<VPackBuilder> AuthInfo::parseJson(std::string const& str,
                                                  std::string const& hint) {
  std::shared_ptr<VPackBuilder> result;
  VPackParser parser;
  try {
    parser.parse(str);
    result = parser.steal();
  } catch (std::bad_alloc const&) {
    LOG_TOPIC(ERR, arangodb::Logger::FIXME) << "Out of memory parsing " << hint << "!";
  } catch (VPackException const& ex) {
    LOG_TOPIC(DEBUG, arangodb::Logger::FIXME) << "Couldn't parse " << hint << ": " << ex.what();
  } catch (...) {
    LOG_TOPIC(ERR, arangodb::Logger::FIXME) << "Got unknown exception trying to parse " << hint;
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

AuthJwtResult AuthInfo::validateJwtBody(std::string const& body) {
  std::shared_ptr<VPackBuilder> bodyBuilder =
      parseJson(StringUtils::decodeBase64(body), "jwt body");
  AuthJwtResult authResult;
  if (bodyBuilder.get() == nullptr) {
    return authResult;
  }

  VPackSlice const bodySlice = bodyBuilder->slice();
  if (!bodySlice.isObject()) {
    return authResult;
  }

  VPackSlice const issSlice = bodySlice.get("iss");
  if (!issSlice.isString()) {
    return authResult;
  }

  if (issSlice.copyString() != "arangodb") {
    return authResult;
  }
  
  if (bodySlice.hasKey("preferred_username")) {
    VPackSlice const usernameSlice = bodySlice.get("preferred_username");
    if (!usernameSlice.isString()) {
      return authResult;
    }
    authResult._username = usernameSlice.copyString();
  } else if (bodySlice.hasKey("server_id")) {
    // mop: hmm...nothing to do here :D
    // authResult._username = "root";
  } else {
    return authResult;
  }

  // mop: optional exp (cluster currently uses non expiring jwts)
  if (bodySlice.hasKey("exp")) {
    VPackSlice const expSlice = bodySlice.get("exp");

    if (!expSlice.isNumber()) {
      return authResult;
    }

    std::chrono::system_clock::time_point expires(
        std::chrono::seconds(expSlice.getNumber<uint64_t>()));
    std::chrono::system_clock::time_point now =
        std::chrono::system_clock::now();

    if (now >= expires) {
      return authResult;
    }
    authResult._expires = true;
    authResult._expireTime = expires;
  }

  authResult._authorized = true;
  return authResult;
}

bool AuthInfo::validateJwtHMAC256Signature(std::string const& message,
                                           std::string const& signature) {
  std::string decodedSignature = StringUtils::decodeBase64U(signature);
  
  return verifyHMAC(_jwtSecret.c_str(), _jwtSecret.length(), message.c_str(),
                    message.length(), decodedSignature.c_str(),
                    decodedSignature.length(),
                    SslInterface::Algorithm::ALGORITHM_SHA256);
}

std::string AuthInfo::generateRawJwt(VPackBuilder const& bodyBuilder) {
  VPackBuilder headerBuilder;
  {
    VPackObjectBuilder h(&headerBuilder);
    headerBuilder.add("alg", VPackValue("HS256"));
    headerBuilder.add("typ", VPackValue("JWT"));
  }

  std::string fullMessage(StringUtils::encodeBase64(headerBuilder.toJson()) +
                          "." +
                          StringUtils::encodeBase64(bodyBuilder.toJson()));

  std::string signature =
      sslHMAC(_jwtSecret.c_str(), _jwtSecret.length(), fullMessage.c_str(),
              fullMessage.length(), SslInterface::Algorithm::ALGORITHM_SHA256);

  return fullMessage + "." + StringUtils::encodeBase64U(signature);
}

std::string AuthInfo::generateJwt(VPackBuilder const& payload) {
  if (!payload.slice().isObject()) {
    std::string error = "Need an object to generate a JWT. Got: ";
    error += payload.slice().typeName();
    throw std::runtime_error(error);
  }
  bool hasIss = payload.slice().hasKey("iss");
  bool hasIat = payload.slice().hasKey("iat");
  VPackBuilder bodyBuilder;
  if (hasIss && hasIat) {
    bodyBuilder = payload;
  } else {
    VPackObjectBuilder p(&bodyBuilder);
    if (!hasIss) {
      bodyBuilder.add("iss", VPackValue("arangodb"));
    }
    if (!hasIat) {
      bodyBuilder.add("iat", VPackValue(TRI_microtime() / 1000));
    }
    for (auto const& obj : VPackObjectIterator(payload.slice())) {
      bodyBuilder.add(obj.key.copyString(), obj.value);
    }
  }
  return generateRawJwt(bodyBuilder);
}

std::shared_ptr<AuthContext> AuthInfo::getAuthContext(std::string const& username, std::string const& database) {
  auto it = _authInfo.find(username);

  if (it == _authInfo.end()) {
    return _noneAuthContext;
  }

  // AuthEntry const& entry =
  return it->second.getAuthContext(database);

  //return entry.
  // return std::shared_ptr<AuthContext>(new AuthContext());
  // return std::make_shared<AuthContext>();
}
