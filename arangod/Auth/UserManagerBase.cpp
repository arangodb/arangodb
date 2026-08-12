////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2014-2025 ArangoDB GmbH, Cologne, Germany
/// Copyright 2004-2014 triAGENS GmbH, Cologne, Germany
///
/// Licensed under the Business Source License 1.1 (the "License");
/// you may not use this file except in compliance with the License.
/// You may obtain a copy of the License at
///
///     https://github.com/arangodb/arangodb/blob/devel/LICENSE
///
/// Unless required by applicable law or agreed to in writing, software
/// distributed under the License is distributed on an "AS IS" BASIS,
/// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
/// See the License for the specific language governing permissions and
/// limitations under the License.
///
/// Copyright holder is ArangoDB GmbH, Cologne, Germany
///
////////////////////////////////////////////////////////////////////////////////

#include "UserManagerBase.h"

#include "Basics/ReadLocker.h"
#include "Basics/StaticStrings.h"
#include "Basics/StringBuffer.h"
#include "Basics/StringUtils.h"
#include "Basics/WriteLocker.h"
#include "Cluster/ServerState.h"
#include "Logger/LogMacros.h"
#include "Logger/Logger.h"
#include "Logger/LoggerStream.h"
#include "absl/strings/str_cat.h"

#include <velocypack/Builder.h>
#include <velocypack/Collection.h>
#include <velocypack/Iterator.h>
#include <velocypack/Parser.h>

namespace arangodb::auth {

using namespace arangodb::basics;
using namespace arangodb::velocypack;

/// @brief Convert documents from _system/_users into the format used in
/// the REST user API and Foxx.
void UserManagerBase::ConvertLegacyFormat(VPackSlice doc,
                                          VPackBuilder& result) {
  doc = doc.resolveExternals();
  VPackSlice authDataSlice = doc.get("authData");
  {
    VPackObjectBuilder b(&result, true);
    result.add("user", doc.get("user"));
    result.add("active", authDataSlice.get("active"));
    VPackSlice extra = doc.get("userData");
    result.add("extra",
               extra.isNone() ? VPackSlice::emptyObjectSlice() : extra);
  }
}

UserManagerBase::UserManagerBase() : _globalVersion(1), _internalVersion(0) {}

std::string UserManagerBase::translateCollectionName(
    std::string_view /*dbname*/, std::string_view coll) {
  return std::string(coll);
}

void UserManagerBase::setGlobalVersion(uint64_t const version) noexcept {
  uint64_t previous = _globalVersion.load(std::memory_order_relaxed);
  while (version > previous) {
    if (_globalVersion.compare_exchange_strong(previous, version,
                                               std::memory_order_release,
                                               std::memory_order_relaxed)) {
      _globalVersion.notify_all();
      return;
    }
  }
}

/// @brief used for caching
uint64_t UserManagerBase::globalVersion() const noexcept {
  return _globalVersion.load(std::memory_order_acquire);
}

uint64_t UserManagerBase::internalVersion() const noexcept {
  return _internalVersion.load(std::memory_order_acquire);
}

Result UserManagerBase::accessUser(std::string const& user,
                                   ConstUserCallback&& func) {
  if (user.empty()) {
    return TRI_ERROR_USER_NOT_FOUND;
  }

  checkIfUserDataIsAvailable();

  READ_LOCKER(readGuard, _userCacheLock);
  UserMap::iterator const& it = _userCache.find(user);
  if (it != _userCache.end()) {
    return func(it->second);
  }
  return TRI_ERROR_USER_NOT_FOUND;
}

bool UserManagerBase::userExists(std::string const& user) {
  if (user.empty()) {
    return false;
  }

  checkIfUserDataIsAvailable();
  READ_LOCKER(readGuard, _userCacheLock);
  UserMap::iterator const& it = _userCache.find(user);
  return it != _userCache.end();
}

VPackBuilder UserManagerBase::serializeUser(std::string const& user) {
  checkIfUserDataIsAvailable();

  READ_LOCKER(readGuard, _userCacheLock);

  UserMap::iterator const& it = _userCache.find(user);
  if (it != _userCache.end()) {
    VPackBuilder tmp = it->second.toVPackBuilder();
    if (!tmp.isEmpty() && !tmp.slice().isNone()) {
      VPackBuilder result;
      ConvertLegacyFormat(tmp.slice(), result);
      return result;
    }
  }
  THROW_ARANGO_EXCEPTION(TRI_ERROR_USER_NOT_FOUND);  // FIXME do not use
}

bool UserManagerBase::checkPassword(std::string const& username,
                                    std::string const& password) {
  if (username.empty()) {
    return false;  // we cannot authenticate during bootstrap
  }

  READ_LOCKER(readGuard, _userCacheLock);
  UserMap::iterator it = _userCache.find(username);

  if (it != _userCache.end()) {
    User const& user = it->second;
    if (user.isActive()) {
      return user.checkPassword(password);
    }
  }

  return false;
}

Result UserManagerBase::extractUsername(std::string const& token,
                                        std::string& username) const {
  if (token.starts_with("v1.")) {
    std::string unhex =
        StringUtils::decodeHex(token.c_str() + 3, token.size() - 3);

    StringBuffer in;
    in.appendText(unhex);

    std::shared_ptr<VPackBuilder> json;
    try {
      json = VPackParser::fromJson(in.toString());
    } catch (std::exception const& e) {
      return {TRI_ERROR_BAD_PARAMETER,
              absl::StrCat("Error parsing JSON: ", e.what())};
    }
    VPackSlice at = json->slice();

    if (!at.isObject()) {
      return {TRI_ERROR_BAD_PARAMETER};
    }

    VPackSlice user = at.get("u");

    if (!user.isString()) {
      return {TRI_ERROR_BAD_PARAMETER};
    }

    username = user.copyString();

    return {TRI_ERROR_NO_ERROR};
  } else {
    return {TRI_ERROR_INCOMPATIBLE_VERSION};
  }
}

bool UserManagerBase::checkAccessToken(std::string const& username,
                                       std::string const& token,
                                       std::string& un,
                                       std::optional<double>& validUntil) {
  Result result = extractUsername(token, un);

  if (!result.ok()) {
    return false;
  }

  if (!username.empty() && username != un) {
    return false;
  }

  READ_LOCKER(readGuard, _userCacheLock);
  UserMap::iterator it = _userCache.find(un);

  if (it != _userCache.end()) {
    User const& user = it->second;
    if (user.isActive()) {
      return user.checkAccessToken(token, validUntul);
    }
  }

  return false;
}

bool UserManagerBase::checkCredentials(std::string const& username,
                                       std::string const& password,
                                       std::string& un,
                                       std::optional<double>& tokenValidUntil) {
  un.clear();
  bool authorized = !username.empty() && checkPassword(username, password);

  if (authorized) {
    un = username;
  } else {
    authorized = checkAccessToken(username, password, un, tokenValidUntil);
  }

  return authorized;
}

Level UserManagerBase::databaseAuthLevel(std::string_view user,
                                         std::string_view dbname,
                                         bool configured) {
  if (dbname.empty()) {
    return Level::NONE;
  }

  checkIfUserDataIsAvailable();
  READ_LOCKER(readGuard, _userCacheLock);

  UserMap::iterator const& it = _userCache.find(user);
  if (it == _userCache.end()) {
    LOG_TOPIC("aa27c", TRACE, Logger::AUTHORIZATION)
        << "User not found: " << user;
    return Level::NONE;
  }

  Level level = it->second.databaseAuthLevel(dbname);
  if (!configured) {
    if (level > Level::RO && ServerState::readOnly()) {
      return Level::RO;
    }
  }
  TRI_ASSERT(level != Level::UNDEFINED);  // not allowed here
  return level;
}

Level UserManagerBase::collectionAuthLevel(std::string_view user,
                                           std::string_view dbname,
                                           std::string_view coll,
                                           bool configured) {
  if (coll.empty()) {
    return Level::NONE;
  }

  checkIfUserDataIsAvailable();
  READ_LOCKER(readGuard, _userCacheLock);

  UserMap::iterator const& it = _userCache.find(user);
  if (it == _userCache.end()) {
    LOG_TOPIC("6d0d4", TRACE, Logger::AUTHORIZATION)
        << "User not found: " << user;
    return Level::NONE;  // no user found
  }

  TRI_ASSERT(!coll.empty());
  Level level = Level::UNDEFINED;
  if (coll[0] >= '0' && coll[0] <= '9') {
    std::string tmpColl = translateCollectionName(dbname, coll);
    level = it->second.collectionAuthLevel(dbname, tmpColl);
  } else {
    level = it->second.collectionAuthLevel(dbname, coll);
  }

  if (!configured) {
    static_assert(Level::RO < Level::RW, "ro < rw");
    if (level > Level::RO && ServerState::readOnly()) {
      return Level::RO;
    }
  }
  TRI_ASSERT(level != Level::UNDEFINED);  // not allowed here
  return level;
}

Result UserManagerBase::accessTokens(std::string const& user,
                                     velocypack::Builder& builder) {
  Result result = accessUser(
      user, [&](User const& u) { return u.getAccessTokens(builder); });

  return result;
}

Result UserManagerBase::deleteAccessToken(std::string const& user,
                                          uint64_t id) {
  Result result = updateUser(
      user, [&](User& u) { return u.deleteAccessToken(id); },
      RetryOnConflict::Yes);

  return result;
}

Result UserManagerBase::createAccessToken(std::string const& user,
                                          std::string const& name,
                                          double validUntil,
                                          velocypack::Builder& builder) {
  Result result = updateUser(
      user,
      [&](User& u) { return u.createAccessToken(name, validUntil, builder); },
      RetryOnConflict::No);

  return result;
}

}  // namespace arangodb::auth
