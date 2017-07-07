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

#include "AuthUserEntry.h"

#include "Basics/ReadLocker.h"
#include "Basics/StringRef.h"
#include "Basics/VelocyPackHelper.h"
#include "Basics/WriteLocker.h"
#include "Basics/tri-strings.h"
#include "Cluster/ServerState.h"
#include "GeneralServer/GeneralServerFeature.h"
#include "Logger/Logger.h"
#include "Random/UniformCharacter.h"
#include "RestServer/DatabaseFeature.h"
#include "Ssl/SslInterface.h"
#include "Transaction/Helpers.h"
#include "VocBase/LogicalCollection.h"
#include "VocBase/Methods/Collections.h"
#include "VocBase/Methods/Databases.h"

#include <velocypack/Iterator.h>
#include <velocypack/velocypack-aliases.h>

using namespace arangodb;
using namespace arangodb::basics;
using namespace arangodb::rest;

static AuthLevel _convertToAuthLevel(arangodb::StringRef ref) {
  if (ref.compare("rw") == 0) {
    return AuthLevel::RW;
  } else if (ref.compare("ro") == 0) {
    return AuthLevel::RO;
  } else if (ref.compare("none") == 0 || ref.empty()) {
    return AuthLevel::NONE;
  }
  THROW_ARANGO_EXCEPTION_MESSAGE(TRI_ERROR_BAD_PARAMETER,
                                 "expecting access type 'rw', 'ro' or 'none'");
}

AuthLevel arangodb::convertToAuthLevel(velocypack::Slice grants) {
  return _convertToAuthLevel(StringRef(grants));
}

AuthLevel arangodb::convertToAuthLevel(std::string const& grants) {
  return _convertToAuthLevel(StringRef(grants));
}

std::string arangodb::convertFromAuthLevel(AuthLevel lvl) {
  if (lvl == AuthLevel::RW) {
    return "rw";
  } else if (lvl == AuthLevel::RO) {
    return "ro";
  } else {
    return "none";
  }
}

// private hash function
static int HexHashFromData(std::string const& hashMethod,
                           std::string const& str, std::string& outHash) {
  char* crypted = nullptr;
  size_t cryptedLength;
  char* hex;

  try {
    if (hashMethod == "sha1") {
      arangodb::rest::SslInterface::sslSHA1(str.data(), str.size(), crypted,
                                            cryptedLength);
    } else if (hashMethod == "sha512") {
      arangodb::rest::SslInterface::sslSHA512(str.data(), str.size(), crypted,
                                              cryptedLength);
    } else if (hashMethod == "sha384") {
      arangodb::rest::SslInterface::sslSHA384(str.data(), str.size(), crypted,
                                              cryptedLength);
    } else if (hashMethod == "sha256") {
      arangodb::rest::SslInterface::sslSHA256(str.data(), str.size(), crypted,
                                              cryptedLength);
    } else if (hashMethod == "sha224") {
      arangodb::rest::SslInterface::sslSHA224(str.data(), str.size(), crypted,
                                              cryptedLength);
    } else if (hashMethod == "md5") {  // WFT?!!!
      arangodb::rest::SslInterface::sslMD5(str.data(), str.size(), crypted,
                                           cryptedLength);
    } else {
      // invalid algorithm...
      LOG_TOPIC(DEBUG, arangodb::Logger::FIXME)
          << "invalid algorithm for hexHashFromData: " << hashMethod;
      return TRI_ERROR_BAD_PARAMETER;
    }
  } catch (...) {
    // SslInterface::ssl....() allocate strings with new, which might throw
    // exceptions
    return TRI_ERROR_FAILED;
  }

  if (crypted == nullptr || cryptedLength == 0) {
    delete[] crypted;
    return TRI_ERROR_OUT_OF_MEMORY;
  }

  size_t hexLen;
  hex = TRI_EncodeHexString(crypted, cryptedLength, &hexLen);
  delete[] crypted;

  if (hex == nullptr) {
    return TRI_ERROR_OUT_OF_MEMORY;
  }

  outHash = std::string(hex, hexLen);
  TRI_FreeString(TRI_CORE_MEM_ZONE, hex);
  return TRI_ERROR_NO_ERROR;
}

static void AddSource(VPackBuilder& builder, AuthSource source) {
  switch (source) {
    case AuthSource::COLLECTION:
      builder.add("source", VPackValue("COLLECTION"));
      break;
    case AuthSource::LDAP:
      builder.add("source", VPackValue("LDAP"));
      break;
    default:
      TRI_ASSERT(false);
  }
}

static void AddAuthLevel(VPackBuilder& builder, AuthLevel lvl) {
  if (lvl == AuthLevel::RW) {
    builder.add("read", VPackValue(true));
    builder.add("write", VPackValue(true));
  } else if (lvl == AuthLevel::RO) {
    builder.add("read", VPackValue(true));
    builder.add("write", VPackValue(false));
  } else {
    builder.add("read", VPackValue(false));
    builder.add("write", VPackValue(false));
  }
}

static AuthLevel AuthLevelFromSlice(VPackSlice const& slice) {
  if (slice.hasKey("write") && slice.get("write").isTrue()) {
    return AuthLevel::RW;
  } else if (slice.hasKey("read") && slice.get("read").isTrue()) {
    return AuthLevel::RO;
  }
  return AuthLevel::NONE;
}

// ============= public ==================

AuthUserEntry AuthUserEntry::newUser(std::string const& user,
                                     std::string const& password,
                                     AuthSource source) {
  AuthUserEntry entry;
  entry._active = true;
  entry._source = source;

  entry._username = user;
  entry._passwordMethod = "sha256";

  std::string salt = UniformCharacter(8, "0123456789abcdef").random();
  std::string hash;
  int res = HexHashFromData("sha256", salt + password, hash);
  if (res != TRI_ERROR_NO_ERROR) {
    THROW_ARANGO_EXCEPTION_MESSAGE(res,
                                   "Could not calculate hex-hash from data");
  }

  entry._passwordSalt = salt;
  entry._passwordHash = hash;
  entry._passwordChangeToken = std::string();
  entry._changePassword = false;

  // build authentication entry
  return entry;
}

AuthUserEntry AuthUserEntry::fromDocument(VPackSlice const& slice) {
  if (slice.isNone() || !slice.isObject()) {
    THROW_ARANGO_EXCEPTION(TRI_ERROR_BAD_PARAMETER);
  }

  VPackSlice const keySlice =
      transaction::helpers::extractKeyFromDocument(slice);
  if (!keySlice.isString()) {
    THROW_ARANGO_EXCEPTION_MESSAGE(TRI_ERROR_BAD_PARAMETER,
                                   "cannot extract key");
  }

  // extract "user" attribute
  VPackSlice const userSlice = slice.get("user");
  if (!userSlice.isString()) {
    THROW_ARANGO_EXCEPTION_MESSAGE(TRI_ERROR_BAD_PARAMETER,
                                   "cannot extract username");
  }

  VPackSlice const authDataSlice = slice.get("authData");

  if (!authDataSlice.isObject()) {
    THROW_ARANGO_EXCEPTION_MESSAGE(TRI_ERROR_BAD_PARAMETER,
                                   "cannot extract authData");
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
    LOG_TOPIC(DEBUG, arangodb::Logger::FIXME)
        << "cannot extract password internals";
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

  // extract "passwordToken"
  VPackSlice passwordTokenSlice = authDataSlice.get("passwordToken");
  if (!passwordTokenSlice.isString()) {
    passwordTokenSlice = VelocyPackHelper::EmptyString();
  }

  // extract "databases" attribute
  VPackSlice const databasesSlice = slice.get("databases");

  std::unordered_map<std::string, std::shared_ptr<AuthContext>> authContexts;

  if (databasesSlice.isObject()) {
    for (auto const& obj : VPackObjectIterator(databasesSlice)) {
      std::string const dbName = obj.key.copyString();

      // check if database exists
      TRI_vocbase_t* vocbase = nullptr;
      if (dbName != "*") {
        vocbase = methods::Databases::lookup(dbName);
        if (vocbase == nullptr) {
          continue;
        }
      }

      if (obj.value.isObject()) {
        std::unordered_map<std::string, AuthLevel> collections;
        AuthLevel databaseAuth = AuthLevel::NONE;

        auto const permissionsSlice = obj.value.get("permissions");
        if (permissionsSlice.isObject()) {
          databaseAuth = AuthLevelFromSlice(permissionsSlice);
        }

        VPackSlice collectionsSlice = obj.value.get("collections");
        if (collectionsSlice.isObject()) {
          for (auto const& collection : VPackObjectIterator(collectionsSlice)) {
            std::string const cName = collection.key.copyString();
            // skip nonexisting collections
            bool exists = dbName == "*" || cName == "*" ||
                          methods::Collections::lookupCollection(
                              vocbase, cName, [&](LogicalCollection*) {});
            if (exists) {
              auto const permissionsSlice = collection.value.get("permissions");
              if (permissionsSlice.isObject()) {
                collections.emplace(cName,
                                    AuthLevelFromSlice(permissionsSlice));
              }  // if
            }    // if

          }  // for
        }    // if

        authContexts.emplace(dbName, std::make_shared<AuthContext>(
                                         databaseAuth, std::move(collections)));

      } else {
        LOG_TOPIC(INFO, arangodb::Logger::CONFIG)
            << "Deprecation Warning: Update access rights for user '"
            << userSlice.copyString() << "'";
        VPackValueLength length;
        char const* value = obj.value.getString(length);

        if (TRI_CaseEqualString(value, "rw", 2)) {
          authContexts.emplace(
              obj.key.copyString(),
              std::make_shared<AuthContext>(
                  AuthLevel::RW, std::unordered_map<std::string, AuthLevel>(
                                     {{"*", AuthLevel::RW}})));

        } else if (TRI_CaseEqualString(value, "ro", 2)) {
          authContexts.emplace(
              obj.key.copyString(),
              std::make_shared<AuthContext>(
                  AuthLevel::RO, std::unordered_map<std::string, AuthLevel>(
                                     {{"*", AuthLevel::RO}})));
        }
      }
    }  // for
  }    // if

  AuthLevel systemDatabaseLevel = AuthLevel::RO;
  for (auto const& dbName : std::vector<std::string>{"_system", "*"}) {
    auto const& it = authContexts.find(dbName);
    if (it != authContexts.end()) {
      systemDatabaseLevel = it->second->databaseAuthLevel();
      break;
    }
  }

  for (auto const& ctx : authContexts) {
    LOG_TOPIC(DEBUG, arangodb::Logger::AUTHENTICATION)
        << userSlice.copyString() << " Database " << ctx.first;
    ctx.second->_systemAuthLevel = systemDatabaseLevel;
    ctx.second->dump();
  }

  AuthUserEntry entry;
  entry._key = keySlice.copyString();
  entry._active = active;
  entry._source = AuthSource::COLLECTION;

  entry._username = userSlice.copyString();
  entry._passwordMethod = methodSlice.copyString();
  entry._passwordSalt = saltSlice.copyString();
  entry._passwordHash = hashSlice.copyString();
  entry._passwordChangeToken = passwordTokenSlice.copyString();
  entry._changePassword = mustChange;
  entry._authContexts = std::move(authContexts);

  // ensure the root user always has the right to change permissions
  if (entry._username == "root") {
    entry.grantDatabase(StaticStrings::SystemDatabase, AuthLevel::RW);
    entry.grantCollection(StaticStrings::SystemDatabase, "*", AuthLevel::RW);
  }

  // build authentication entry
  return entry;
}

bool AuthUserEntry::checkPassword(std::string const& password) const {
  std::string hash;
  int res = HexHashFromData(_passwordMethod, _passwordSalt + password, hash);
  if (res != TRI_ERROR_NO_ERROR) {
    THROW_ARANGO_EXCEPTION_MESSAGE(res,
                                   "Could not calculate hex-hash from input");
  }
  return _passwordHash == hash;
}

void AuthUserEntry::updatePassword(std::string const& password) {
  std::string hash;
  int res = HexHashFromData(_passwordMethod, _passwordSalt + password, hash);
  if (res != TRI_ERROR_NO_ERROR) {
    THROW_ARANGO_EXCEPTION_MESSAGE(res,
                                   "Could not calculate hex-hash from input");
  }
  _passwordHash = hash;
}

std::shared_ptr<AuthContext> AuthUserEntry::getAuthContext(
    std::string const& dbname) const {
  auto const& it = _authContexts.find(dbname);
  if (it != _authContexts.end()) {
    return it->second;
  }
  auto const& it2 = _authContexts.find("*");
  if (it2 != _authContexts.end()) {
    return it2->second;
  }
  return std::shared_ptr<AuthContext>();
}

VPackBuilder AuthUserEntry::toVPackBuilder() const {
  TRI_ASSERT(!_username.empty());

  VPackBuilder builder;
  VPackObjectBuilder o(&builder, true);
  if (!_key.empty()) {
    builder.add(StaticStrings::KeyString, VPackValue(_key));
  }
  builder.add("user", VPackValue(_username));
  AddSource(builder, _source);

  {  // authData sub-object
    VPackObjectBuilder o2(&builder, "authData", true);
    builder.add("active", VPackValue(_active));
    if (!_passwordChangeToken.empty()) {
      builder.add("changePassword", VPackValue(_changePassword));
    }
    if (_changePassword) {
      builder.add("changePassword", VPackValue(_changePassword));
    }
    if (_source == AuthSource::COLLECTION) {
      VPackObjectBuilder o3(&builder, "simple", true);
      builder.add("hash", VPackValue(_passwordHash));
      builder.add("salt", VPackValue(_passwordSalt));
      builder.add("method", VPackValue(_passwordMethod));
    }
  }
  {  // databases sub-object
    VPackObjectBuilder o2(&builder, "databases", true);
    for (auto const& dbCtxPair : _authContexts) {
      TRI_ASSERT(dbCtxPair.first != StaticStrings::SystemDatabase ||
                 dbCtxPair.second->databaseAuthLevel() ==
                 dbCtxPair.second->systemAuthLevel());

      VPackObjectBuilder o3(&builder, dbCtxPair.first, true);
      {  // permissions
        VPackObjectBuilder o4(&builder, "permissions", true);
        AuthLevel lvl = dbCtxPair.second->databaseAuthLevel();
        AddAuthLevel(builder, lvl);
      }
      {  // collections
        VPackObjectBuilder o4(&builder, "collections", true);
        for (auto const& colAccessPair : dbCtxPair.second->_collectionAccess) {
          VPackObjectBuilder o4(&builder, colAccessPair.first, true);
          VPackObjectBuilder o5(&builder, "permissions", true);
          AddAuthLevel(builder, colAccessPair.second);
        }
      }
    }
  }

  return builder;
}

void AuthUserEntry::grantDatabase(std::string const& dbname, AuthLevel level) {
  if (dbname.empty()) {
    THROW_ARANGO_EXCEPTION_MESSAGE(TRI_ERROR_BAD_PARAMETER,
                                   "Cannot set rights for empty db name");
  }
  if (_username == "root" && dbname == StaticStrings::SystemDatabase &&
      level != AuthLevel::RW) {
    THROW_ARANGO_EXCEPTION_MESSAGE(
        TRI_ERROR_FORBIDDEN, "Cannot lower access level of 'root' to _system");
  }

  auto it = _authContexts.find(dbname);
  if (it != _authContexts.end()) {
    it->second->_databaseAuthLevel = level;
  } else {
    _authContexts.emplace(
        dbname,
        std::make_shared<AuthContext>(
            level, std::unordered_map<std::string, AuthLevel>({{"*", level}})));
  }
  if (dbname == StaticStrings::SystemDatabase ||
      (dbname == "*" &&
       _authContexts.find(StaticStrings::SystemDatabase) ==
           _authContexts.end())) {
    _authContexts[dbname]->_systemAuthLevel = level;
  }
}

void AuthUserEntry::removeDatabase(std::string const& dbname) {
  if (dbname.empty()) {
    THROW_ARANGO_EXCEPTION_MESSAGE(TRI_ERROR_BAD_PARAMETER,
                                   "Cannot remove rights for empty db name");
  }
  if (_username == "root" && dbname == StaticStrings::SystemDatabase) {
    THROW_ARANGO_EXCEPTION_MESSAGE(
        TRI_ERROR_FORBIDDEN, "Cannot remove access level of 'root' to _system");
  }
  _authContexts.erase(dbname);
}

void AuthUserEntry::grantCollection(std::string const& dbname,
                                    std::string const& coll,
                                    AuthLevel const level) {
  if (dbname.empty() || coll.empty()) {
    THROW_ARANGO_EXCEPTION_MESSAGE(
        TRI_ERROR_BAD_PARAMETER,
        "Cannot set rights for empty db / collection name");
  }
  if (_username == "root" && dbname == StaticStrings::SystemDatabase &&
      (coll[0] == '_' || coll == "*") && level != AuthLevel::RW) {
    THROW_ARANGO_EXCEPTION_MESSAGE(TRI_ERROR_FORBIDDEN,
                                   "Cannot lower access level of 'root' to "
                                   " a system collection");
  }
  auto it = _authContexts.find(dbname);
  if (it != _authContexts.end()) {
    it->second->_collectionAccess[coll] = level;
  } else {
    // do not overwrite wildcard access to a database, by granting more
    // specific rights to a collection in a specific db
    AuthLevel dbLevel = level;
    it = _authContexts.find("*");
    if (it != _authContexts.end()) {
      dbLevel = it->second->databaseAuthLevel();
    }
    _authContexts.emplace(
        dbname, std::make_shared<AuthContext>(
                    dbLevel, std::unordered_map<std::string, AuthLevel>(
                                 {{coll, level}})));
    if (dbname == StaticStrings::SystemDatabase) {
      _authContexts[dbname]->_systemAuthLevel = dbLevel;
    }
  }
}

void AuthUserEntry::removeCollection(std::string const& dbname,
                                     std::string const& coll) {
  if (dbname.empty() || coll.empty()) {
    THROW_ARANGO_EXCEPTION_MESSAGE(
        TRI_ERROR_BAD_PARAMETER,
        "Cannot set rights for empty db / collection name");
  }
  if (_username == "root" && dbname == StaticStrings::SystemDatabase &&
      (coll == "*")) {
    THROW_ARANGO_EXCEPTION_MESSAGE(TRI_ERROR_FORBIDDEN,
                                   "Cannot lower access level of 'root' to "
                                   " a collection in _system");
  }
  auto it = _authContexts.find(dbname);
  if (it != _authContexts.end()) {
    it->second->_collectionAccess.erase(coll);
  }
}
