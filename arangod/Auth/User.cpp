////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2014-2020 ArangoDB GmbH, Cologne, Germany
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

#include "Auth/User.h"
#include "Basics/ReadLocker.h"
#include "Basics/ScopeGuard.h"
#include "Basics/StaticStrings.h"
#include "Basics/StringUtils.h"
#include "Basics/VelocyPackHelper.h"
#include "Basics/WriteLocker.h"
#include "Basics/system-functions.h"
#include "Basics/tri-strings.h"
#include "Basics/tryEmplaceHelper.h"
#include "Cluster/ServerState.h"
#include "GeneralServer/GeneralServerFeature.h"
#include "Logger/LogMacros.h"
#include "Logger/Logger.h"
#include "Logger/LoggerStream.h"
#include "Random/UniformCharacter.h"
#include "RestServer/DatabaseFeature.h"
#include "Ssl/SslInterface.h"
#include "Transaction/Helpers.h"
#include "VocBase/LogicalCollection.h"
#include "VocBase/Methods/Collections.h"
#include "VocBase/Methods/Databases.h"

#include <velocypack/Iterator.h>
#include <velocypack/StringRef.h>
#include <velocypack/velocypack-aliases.h>

using namespace arangodb;
using namespace arangodb::basics;
using namespace arangodb::rest;

// private hash function
static int HexHashFromData(std::string const& hashMethod,
                           std::string const& str, std::string& outHash) {
  char* crypted = nullptr;
  size_t cryptedLength;

  try {
    if (hashMethod == "sha1") {
      arangodb::rest::SslInterface::sslSHA1(str.data(), str.size(), crypted, cryptedLength);
    } else if (hashMethod == "sha512") {
      arangodb::rest::SslInterface::sslSHA512(str.data(), str.size(), crypted, cryptedLength);
    } else if (hashMethod == "sha384") {
      arangodb::rest::SslInterface::sslSHA384(str.data(), str.size(), crypted, cryptedLength);
    } else if (hashMethod == "sha256") {
      arangodb::rest::SslInterface::sslSHA256(str.data(), str.size(), crypted, cryptedLength);
    } else if (hashMethod == "sha224") {
      arangodb::rest::SslInterface::sslSHA224(str.data(), str.size(), crypted, cryptedLength);
    } else if (hashMethod == "md5") {  // WFT?!!!
      arangodb::rest::SslInterface::sslMD5(str.data(), str.size(), crypted, cryptedLength);
    } else {
      // invalid algorithm...
      LOG_TOPIC("3c13c", DEBUG, arangodb::Logger::AUTHENTICATION)
          << "invalid algorithm for hexHashFromData: " << hashMethod;
      return TRI_ERROR_BAD_PARAMETER;
    }
  } catch (...) {
    // SslInterface::ssl....() allocates strings with new, which might throw
    // exceptions
    return TRI_ERROR_FAILED;
  }

  TRI_DEFER(delete[] crypted);

  if (crypted == nullptr || cryptedLength == 0) {
    return TRI_ERROR_OUT_OF_MEMORY;
  }

  outHash = basics::StringUtils::encodeHex(crypted, cryptedLength);

  return TRI_ERROR_NO_ERROR;
}

static void AddSource(VPackBuilder& builder, auth::Source source) {
  switch (source) {
    case auth::Source::Local:  // used to be collection
      builder.add("source", VPackValue("LOCAL"));
      break;
    case auth::Source::LDAP:
      builder.add("source", VPackValue("LDAP"));
      break;
    default:
      TRI_ASSERT(false);
  }
}

static void AddAuthLevel(VPackBuilder& builder, auth::Level lvl) {
  if (lvl == auth::Level::RW) {
    builder.add("read", VPackValue(true));
    builder.add("write", VPackValue(true));
  } else if (lvl == auth::Level::RO) {
    builder.add("read", VPackValue(true));
    builder.add("write", VPackValue(false));
  } else if (lvl == auth::Level::NONE) {
    builder.add("read", VPackValue(false));
    builder.add("write", VPackValue(false));
  } else if (lvl == auth::Level::UNDEFINED) {
    builder.add("undefined", VPackValue(true));
  }
}

static auth::Level AuthLevelFromSlice(VPackSlice const& slice) {
  TRI_ASSERT(slice.isObject());
  VPackSlice v = slice.get("write");
  if (v.isBool() && v.isTrue()) {
    return auth::Level::RW;
  }
  v = slice.get("read");
  if (v.isBool() && v.isTrue()) {
    return auth::Level::RO;
  }
  v = slice.get("undefined");
  if (v.isBool() && v.isTrue()) {
    return auth::Level::UNDEFINED;
  }
  return auth::Level::NONE;
}

// ============= static ==================

auth::User auth::User::newUser(std::string const& user,
                               std::string const& password, auth::Source source) {
  auth::User entry("", RevisionId::none());
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

  // build authentication entry
  return entry;
}

void auth::User::fromDocumentDatabases(auth::User& entry, VPackSlice const& databasesSlice,
                                       VPackSlice const& userSlice) {
  for (auto const& obj : VPackObjectIterator(databasesSlice)) {
    std::string const dbName = obj.key.copyString();

    if (obj.value.isObject()) {
      auth::Level databaseAuth = auth::Level::NONE;

      auto const permissionsSlice = obj.value.get("permissions");

      if (permissionsSlice.isObject()) {
        databaseAuth = AuthLevelFromSlice(permissionsSlice);
      }

      try {
        entry.grantDatabase(dbName, databaseAuth);
      } catch (arangodb::basics::Exception const& e) {
        LOG_TOPIC("a01a9", DEBUG, Logger::AUTHENTICATION) << e.message();
      }

      VPackSlice collectionsSlice = obj.value.get("collections");

      if (collectionsSlice.isObject()) {
        for (auto const& collection : VPackObjectIterator(collectionsSlice)) {
          std::string const cName = collection.key.copyString();
          auto const collPerSlice = collection.value.get("permissions");

          if (collPerSlice.isObject()) {
            try {
              entry.grantCollection(dbName, cName, AuthLevelFromSlice(collPerSlice));
            } catch (arangodb::basics::Exception const& e) {
              LOG_TOPIC("181fa", DEBUG, Logger::AUTHENTICATION) << e.message();
            }
          }
        }
      }
    } else {
      LOG_TOPIC("c4dd7", DEBUG, arangodb::Logger::CONFIG)
          << "updating deprecated access rights struct for user '"
          << userSlice.copyString() << "'";
      VPackValueLength length;
      char const* value = obj.value.getString(length);

      if (TRI_CaseEqualString(value, "rw", 2)) {
        entry.grantDatabase(dbName, auth::Level::RW);
        entry.grantCollection(dbName, "*", auth::Level::RW);
      } else if (TRI_CaseEqualString(value, "ro", 2)) {
        entry.grantDatabase(dbName, auth::Level::RO);
        entry.grantCollection(dbName, "*", auth::Level::RO);
      }
    }
  }
}

auth::User auth::User::fromDocument(VPackSlice const& slice) {
  if (slice.isNone() || !slice.isObject()) {
    THROW_ARANGO_EXCEPTION(TRI_ERROR_BAD_PARAMETER);
  }

  VPackSlice const keySlice = transaction::helpers::extractKeyFromDocument(slice);
  if (!keySlice.isString()) {
    THROW_ARANGO_EXCEPTION_MESSAGE(TRI_ERROR_BAD_PARAMETER,
                                   "cannot extract _key");
  }

  RevisionId rev = transaction::helpers::extractRevFromDocument(slice);
  if (rev.empty()) {
    THROW_ARANGO_EXCEPTION_MESSAGE(TRI_ERROR_BAD_PARAMETER,
                                   "cannot extract _rev");
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
    LOG_TOPIC("e159f", DEBUG, arangodb::Logger::AUTHENTICATION)
        << "cannot extract simple";
    return auth::User("", RevisionId::none());
  }

  VPackSlice const methodSlice = simpleSlice.get("method");
  VPackSlice const saltSlice = simpleSlice.get("salt");
  VPackSlice const hashSlice = simpleSlice.get("hash");

  if (!methodSlice.isString() || !saltSlice.isString() || !hashSlice.isString()) {
    LOG_TOPIC("09122", DEBUG, arangodb::Logger::AUTHENTICATION)
        << "cannot extract password internals";
    return auth::User("", RevisionId::none());
  }

  // extract "active" attribute
  VPackSlice const activeSlice = authDataSlice.get("active");

  if (!activeSlice.isBoolean()) {
    LOG_TOPIC("857e0", DEBUG, arangodb::Logger::AUTHENTICATION)
        << "cannot extract active flag";
    return auth::User("", RevisionId::none());
  }

  auth::User entry(keySlice.copyString(), rev);
  entry._active = activeSlice.getBool();
  entry._source = auth::Source::Local;
  entry._username = userSlice.copyString();
  entry._passwordMethod = methodSlice.copyString();
  entry._passwordSalt = saltSlice.copyString();
  entry._passwordHash = hashSlice.copyString();

  // extract "databases" attribute
  VPackSlice const databasesSlice = slice.get("databases");

  if (databasesSlice.isObject()) {
    fromDocumentDatabases(entry, databasesSlice, userSlice);
  }

  VPackSlice userDataSlice = slice.get("userData");
  if (userDataSlice.isObject() && !userDataSlice.isEmptyObject()) {
    entry._userData.clear();
    entry._userData.add(userDataSlice);
  }

  VPackSlice userConfigSlice = slice.get("configData");
  if (userConfigSlice.isObject() && !userConfigSlice.isEmptyObject()) {
    entry._configData.clear();
    entry._configData.add(userConfigSlice);
  }

  // ensure the root user always has the right to change permissions
  if (entry._username == "root") {
    entry.grantDatabase(StaticStrings::SystemDatabase, auth::Level::RW);
    entry.grantCollection(StaticStrings::SystemDatabase, "*", auth::Level::RW);
  }

  // build authentication entry
  return entry;
}

// ===================== Constructor =======================

auth::User::User(std::string&& key, RevisionId rid)
    : _key(std::move(key)), _rev(rid), _loaded(TRI_microtime()) {}

// ======================= Methods ==========================

void auth::User::touch() { _loaded = TRI_microtime(); }

bool auth::User::checkPassword(std::string const& password) const {
  std::string hash;
  int res = HexHashFromData(_passwordMethod, _passwordSalt + password, hash);
  if (res != TRI_ERROR_NO_ERROR) {
    THROW_ARANGO_EXCEPTION_MESSAGE(res,
                                   "Could not calculate hex-hash from input");
  }
  return _passwordHash == hash;
}

void auth::User::updatePassword(std::string const& password) {
  std::string hash;
  int res = HexHashFromData(_passwordMethod, _passwordSalt + password, hash);
  if (res != TRI_ERROR_NO_ERROR) {
    THROW_ARANGO_EXCEPTION_MESSAGE(res,
                                   "Could not calculate hex-hash from input");
  }
  _passwordHash = hash;
}

VPackBuilder auth::User::toVPackBuilder() const {
  TRI_ASSERT(!_username.empty());

  VPackBuilder builder;
  {
    VPackObjectBuilder o(&builder, true);

    if (!_key.empty()) {
      builder.add(StaticStrings::KeyString, VPackValue(_key));
    }
    if (_rev.isSet()) {
      builder.add(StaticStrings::RevString, VPackValue(_rev.toString()));
    }

    builder.add("user", VPackValue(_username));
    AddSource(builder, _source);

    // authData sub-object
    {
      VPackObjectBuilder o2(&builder, "authData", true);
      builder.add("active", VPackValue(_active));
      if (_source == auth::Source::Local) {
        VPackObjectBuilder o3(&builder, "simple", true);
        builder.add("hash", VPackValue(_passwordHash));
        builder.add("salt", VPackValue(_passwordSalt));
        builder.add("method", VPackValue(_passwordMethod));
      }
    }

    {  // databases sub-object
      VPackObjectBuilder o2(&builder, "databases", true);
      for (auto const& dbCtxPair : _dbAccess) {
        VPackObjectBuilder o3(&builder, dbCtxPair.first, true);

        // permissions
        {
          VPackObjectBuilder o4(&builder, "permissions", true);
          auth::Level lvl = dbCtxPair.second._databaseAuthLevel;
          AddAuthLevel(builder, lvl);
        }

        // collections
        {
          VPackObjectBuilder o5(&builder, "collections", true);

          for (auto const& colAccessPair : dbCtxPair.second._collectionAccess) {
            VPackObjectBuilder o6(&builder, colAccessPair.first, true);
            VPackObjectBuilder o7(&builder, "permissions", true);
            AddAuthLevel(builder, colAccessPair.second);
          }
        }
      }
    }

    if (!_userData.isEmpty() && _userData.isClosed() && _userData.slice().isObject()) {
      builder.add("userData", _userData.slice());
    }

    if (!_configData.isEmpty() && _configData.isClosed() && _configData.slice().isObject()) {
      builder.add("configData", _configData.slice());
    }
  }
  return builder;
}

void auth::User::grantDatabase(std::string const& dbname, auth::Level level) {
  if (dbname.empty() || level == auth::Level::UNDEFINED) {
    THROW_ARANGO_EXCEPTION_MESSAGE(TRI_ERROR_BAD_PARAMETER,
                                   "Cannot set rights for empty db name");
  }
  if (_username == "root" && dbname == StaticStrings::SystemDatabase &&
      level != auth::Level::RW) {
    THROW_ARANGO_EXCEPTION_MESSAGE(
        TRI_ERROR_FORBIDDEN, "Cannot lower access level of 'root' to _system");
  }
  LOG_TOPIC("b9d75", DEBUG, Logger::AUTHENTICATION)
      << _username << ": Granting " << auth::convertFromAuthLevel(level)
      << " on " << dbname;

  auto it = _dbAccess.find(dbname);
  if (it != _dbAccess.end()) {
    it->second._databaseAuthLevel = level;
  } else {
    // grantDatabase is not supposed to change any rights on the
    // collection level code which relies on the old behavior
    // will need to be adjusted
    _dbAccess.try_emplace(dbname, DBAuthContext(level, CollLevelMap()));
  }
}

/// Removes the entry, returns true if entry existed
bool auth::User::removeDatabase(std::string const& dbname) {
  if (dbname.empty()) {
    THROW_ARANGO_EXCEPTION_MESSAGE(TRI_ERROR_BAD_PARAMETER,
                                   "Cannot remove rights for empty db name");
  }
  if (_username == "root" && dbname == StaticStrings::SystemDatabase) {
    THROW_ARANGO_EXCEPTION_MESSAGE(
        TRI_ERROR_FORBIDDEN, "Cannot remove access level of 'root' to _system");
  }
  LOG_TOPIC("f1382", DEBUG, Logger::AUTHENTICATION) << _username << ": Removing grant on " << dbname;
  return _dbAccess.erase(dbname) > 0;
}

void auth::User::grantCollection(std::string const& dbname, std::string const& cname,
                                 auth::Level const level) {
  if (dbname.empty() || cname.empty() || level == auth::Level::UNDEFINED) {
    THROW_ARANGO_EXCEPTION_MESSAGE(
        TRI_ERROR_BAD_PARAMETER,
        "Cannot set rights for empty db / collection name");
  } else if (cname[0] == '_') {
    THROW_ARANGO_EXCEPTION_MESSAGE(TRI_ERROR_BAD_PARAMETER,
                                   "Cannot set rights for system collections");
  } else if (_username == "root" && dbname == StaticStrings::SystemDatabase &&
             cname == "*" && level != auth::Level::RW) {
    THROW_ARANGO_EXCEPTION_MESSAGE(TRI_ERROR_FORBIDDEN,
                                   "Cannot lower access level of 'root' to "
                                   " a system collection");
  } else if (dbname == "*" && cname != "*") {
    THROW_ARANGO_EXCEPTION_MESSAGE(TRI_ERROR_BAD_PARAMETER,
                                   "Invalid database / collection pair");
  }
  LOG_TOPIC("d333a", DEBUG, Logger::AUTHENTICATION)
      << _username << ": Granting " << auth::convertFromAuthLevel(level)
      << " on " << dbname << "/" << cname;

  auto[it, emplaced] = _dbAccess.try_emplace(
      dbname,
      arangodb::lazyConstruct([&]{
    // do not overwrite wildcard access to a database, by granting more
    // specific rights to a collection in a specific db
    auth::Level lvl = auth::Level::UNDEFINED;
        return DBAuthContext(lvl, CollLevelMap({{cname, level}}));
      })
  );
  if (!emplaced) {
    it->second._collectionAccess[cname] = level;
  }
}

/// Removes the collection right, returns true if entry existed
bool auth::User::removeCollection(std::string const& dbname, std::string const& cname) {
  if (dbname.empty() || cname.empty()) {
    THROW_ARANGO_EXCEPTION_MESSAGE(
        TRI_ERROR_BAD_PARAMETER,
        "Cannot set rights for empty db / collection name");
  }
  if (_username == "root" && dbname == StaticStrings::SystemDatabase && (cname == "*")) {
    THROW_ARANGO_EXCEPTION_MESSAGE(TRI_ERROR_FORBIDDEN,
                                   "Cannot lower access level of 'root' to "
                                   " a collection in _system");
  }
  LOG_TOPIC("78e62", DEBUG, Logger::AUTHENTICATION)
      << _username << ": Removing grant on " << dbname << "/" << cname;
  auto const& it = _dbAccess.find(dbname);
  if (it != _dbAccess.end()) {
    return it->second._collectionAccess.erase(cname) > 0;
  }
  return false;
}

// Resolve the access level for this database.
auth::Level auth::User::configuredDBAuthLevel(std::string const& dbname) const {
  auto it = _dbAccess.find(dbname);
  if (it != _dbAccess.end()) {  // found specific grant
    return it->second._databaseAuthLevel;
  }
  return auth::Level::UNDEFINED;
}

// Resolve rights for the specified collection.
auth::Level auth::User::configuredCollectionAuthLevel(std::string const& dbname,
                                                      std::string const& cname) const {
  auto it = _dbAccess.find(dbname);
  if (it != _dbAccess.end()) {
    // Second try to find a specific grant
    CollLevelMap::const_iterator pair = it->second._collectionAccess.find(cname);
    if (pair != it->second._collectionAccess.end()) {
      return pair->second;  // found specific collection grant
    }
  }
  return auth::Level::UNDEFINED;
}

auth::Level auth::User::databaseAuthLevel(std::string const& dbname) const {
  auth::Level lvl = configuredDBAuthLevel(dbname);
  if (lvl == auth::Level::UNDEFINED && dbname != "*") {
    // take best from wildcard or _system
    auto it = _dbAccess.find("*");
    if (it != _dbAccess.end()) {
      lvl = std::max(it->second._databaseAuthLevel, lvl);
    }
    if (dbname != StaticStrings::SystemDatabase) {
      it = _dbAccess.find(StaticStrings::SystemDatabase);
      if (it != _dbAccess.end()) {
        lvl = std::max(it->second._databaseAuthLevel, lvl);
      }
    }
  }

  return std::max(lvl, auth::Level::NONE);
}

/// Find the access level for a collection. Will automatically try to fall back
auth::Level auth::User::collectionAuthLevel(std::string const& dbname,
                                            std::string const& cname) const {
  if (cname.empty() || (dbname == "*" && cname != "*")) {
    return auth::Level::NONE;  // invalid collection names
  }
  // we must have got a non-empty collection name when we get here
  TRI_ASSERT(cname[0] < '0' || cname[0] > '9');

  bool isSystem = cname[0] == '_';
  if (isSystem) {
    // disallow access to _system/_users for everyone
    if (dbname == StaticStrings::SystemDatabase && cname == StaticStrings::UsersCollection) {
      return auth::Level::NONE;
    } else if (cname == StaticStrings::QueuesCollection) {
      return auth::Level::RO;
    } else if (cname == StaticStrings::FrontendCollection) {
      return auth::Level::RW;
    }
    return databaseAuthLevel(dbname);
  }

  auth::Level lvl = auth::Level::NONE;
  if (dbname != "*") {  // skip special rules for wildcard
    auto it = _dbAccess.find(dbname);
    if (it != _dbAccess.end()) {
      // Second try to find a specific grant
      CollLevelMap::const_iterator pair = it->second._collectionAccess.find(cname);
      if (pair != it->second._collectionAccess.end()) {
        return pair->second;      // found specific collection grant
      } else if (cname == "*") {  // skip special rules for wildcard
        return auth::Level::NONE;
      }

      // Fallback step 1.
      lvl = it->second._databaseAuthLevel;
      pair = it->second._collectionAccess.find("*");
      if (pair != it->second._collectionAccess.end()) {
        // found wildcard collection grant, take better default
        lvl = std::max(pair->second, lvl);
      }
    }

    if (dbname != StaticStrings::SystemDatabase) {
      // Fallback step 3. look into _system
      it = _dbAccess.find(StaticStrings::SystemDatabase);
      if (it != _dbAccess.end()) {
        lvl = std::max(it->second._databaseAuthLevel, lvl);
      }
    }
  }

  // Fallback step 2. is to look into the "*" database
  auto it = _dbAccess.find("*");
  if (it != _dbAccess.end()) {
    lvl = std::max(it->second._databaseAuthLevel, lvl);
    if (!isSystem) {
      CollLevelMap::const_iterator pair =
          it->second._collectionAccess.find("*");
      if (pair != it->second._collectionAccess.end()) {
        // found wildcard collection grant, take better default
        lvl = std::max(pair->second, lvl);
      }
    }
    // nothing found
  }

  return lvl;
}
