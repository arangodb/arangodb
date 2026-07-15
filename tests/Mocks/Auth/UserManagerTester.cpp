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

#include "UserManagerTester.h"

#include "Basics/ReadLocker.h"
#include "Basics/StaticStrings.h"
#include "Basics/WriteLocker.h"

#include <velocypack/Builder.h>
#include <velocypack/Iterator.h>

#include <optional>

namespace arangodb::auth {

using namespace arangodb::basics;
using namespace arangodb::velocypack;

// ---- extra methods
// -----------------------------------------------------------

void UserManagerTester::setAuthInfo(UserMap const& userEntryMap) {
  WRITE_LOCKER(writeGuard, _userCacheLock);
  _userCache = userEntryMap;
  _internalVersion.fetch_add(1, std::memory_order_relaxed);
}

uint64_t UserManagerTester::internalVersion() const noexcept {
  return _internalVersion.load(std::memory_order_relaxed);
}

// ---- in-memory implementations
// -----------------------------------------------

void UserManagerTester::createRootUser() {
  User user = User::newUser("root", "");
  user.setActive(true);
  user.grantDatabase(StaticStrings::SystemDatabase, Level::RW);
  user.grantCollection(StaticStrings::SystemDatabase, "*", Level::RW);
  user.grantDatabase("*", Level::RW);
  user.grantCollection("*", "*", Level::RW);
  WRITE_LOCKER(writeGuard, _userCacheLock);
  _userCache.try_emplace("root", std::move(user));
}

VPackBuilder UserManagerTester::allUsers() {
  READ_LOCKER(readGuard, _userCacheLock);
  VPackBuilder result;
  {
    VPackArrayBuilder a(&result);
    for (auto const& pair : _userCache) {
      VPackBuilder tmp = pair.second.toVPackBuilder();
      ConvertLegacyFormat(tmp.slice(), result);
    }
  }
  return result;
}

Result UserManagerTester::storeUser(bool const replace,
                                    std::string const& username,
                                    std::string const& pass, bool const active,
                                    VPackSlice extras) {
  if (username.empty()) {
    return TRI_ERROR_USER_INVALID_NAME;
  }

  WRITE_LOCKER(writeGuard, _userCacheLock);
  auto it = _userCache.find(username);

  if (replace && it == _userCache.end()) {
    return TRI_ERROR_USER_NOT_FOUND;
  }
  if (!replace && it != _userCache.end()) {
    return TRI_ERROR_USER_DUPLICATE;
  }

  User user = User::newUser(username, pass);
  user.setActive(active);
  if (extras.isObject() && !extras.isEmptyObject()) {
    user.setUserData(VPackBuilder(extras));
  }

  if (replace) {
    it->second = std::move(user);
  } else {
    _userCache.emplace(username, std::move(user));
  }
  return TRI_ERROR_NO_ERROR;
}

Result UserManagerTester::enumerateUsers(std::function<bool(User&)>&& func,
                                         RetryOnConflict /*retryOnConflict*/) {
  std::vector<std::pair<std::string, User>> updates;
  {
    READ_LOCKER(readGuard, _userCacheLock);
    for (auto& pair : _userCache) {
      User user = pair.second;  // copy
      if (func(user)) {
        updates.emplace_back(pair.first, std::move(user));
      }
    }
  }
  if (!updates.empty()) {
    WRITE_LOCKER(writeGuard, _userCacheLock);
    for (auto& [name, user] : updates) {
      _userCache.insert_or_assign(name, std::move(user));
    }
  }
  return TRI_ERROR_NO_ERROR;
}

Result UserManagerTester::updateUser(std::string_view name, UserCallback&& func,
                                     RetryOnConflict /*retryOnConflict*/) {
  if (name.empty()) {
    return TRI_ERROR_USER_NOT_FOUND;
  }

  std::optional<User> userOpt;
  {
    READ_LOCKER(readGuard, _userCacheLock);
    auto it = _userCache.find(name);
    if (it == _userCache.end()) {
      return TRI_ERROR_USER_NOT_FOUND;
    }
    userOpt = it->second;  // copy
  }

  Result r = func(*userOpt);
  if (r.fail()) {
    return r;
  }

  WRITE_LOCKER(writeGuard, _userCacheLock);
  auto it = _userCache.find(name);
  if (it == _userCache.end()) {
    return TRI_ERROR_USER_NOT_FOUND;
  }
  it->second = std::move(*userOpt);
  return TRI_ERROR_NO_ERROR;
}

Result UserManagerTester::removeUser(std::string const& user) {
  if (user.empty()) {
    return TRI_ERROR_USER_NOT_FOUND;
  }

  WRITE_LOCKER(writeGuard, _userCacheLock);
  auto it = _userCache.find(user);
  if (it == _userCache.end()) {
    return TRI_ERROR_USER_NOT_FOUND;
  }
  _userCache.erase(it);
  return TRI_ERROR_NO_ERROR;
}

Result UserManagerTester::removeAllUsers() {
  WRITE_LOCKER(writeGuard, _userCacheLock);
  _userCache.clear();
  return TRI_ERROR_NO_ERROR;
}

}  // namespace arangodb::auth
