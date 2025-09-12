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
/// @author Dr. Frank Celler
////////////////////////////////////////////////////////////////////////////////

#pragma once

#include "Auth/UserManager.h"

#include "Auth/Common.h"
#include "Auth/User.h"

#include "ApplicationFeatures/ApplicationFeature.h"
#include "Basics/ReadWriteLock.h"
#include "Basics/Result.h"
#include "RestServer/arangod.h"

#include <thread>
#include <velocypack/Builder.h>
#include <velocypack/Slice.h>

namespace arangodb {
namespace basics {
template<typename T>
class ReadLocker;
}

namespace auth {

class UserManagerImpl final : public UserManager {
 public:
  explicit UserManagerImpl(ArangodServer&);
  ~UserManagerImpl() override;

  void loadUserCacheAndStartUpdateThread() noexcept override;
  void setGlobalVersion(uint64_t version) noexcept override;
  [[nodiscard]] uint64_t globalVersion() const noexcept override;

  static void triggerGlobalReload(ArangodServer&);
  void triggerGlobalReload() const override;
  void triggerCacheRevalidation() override;
  void createRootUser() override;

  velocypack::Builder allUsers() override;

  Result storeUser(bool replace, std::string const& user,
                   std::string const& pass, bool active,
                   velocypack::Slice extras) override;

  Result enumerateUsers(std::function<bool(User&)>&&,
                        RetryOnConflict retryOnConflict) override;

  Result updateUser(std::string const& user, UserCallback&&,
                    RetryOnConflict retryOnConflict) override;

  Result accessUser(std::string const& user, ConstUserCallback&&) override;

  bool userExists(std::string const& user) override;

  velocypack::Builder serializeUser(std::string const& user) override;

  Result removeUser(std::string const& user) override;
  Result removeAllUsers() override;

  bool checkCredentials(std::string const& username, std::string const& token,
                        std::string& un) override;

  Level databaseAuthLevel(std::string const& username,
                          std::string const& dbname, bool configured) override;
  Level collectionAuthLevel(std::string const& username,
                            std::string const& dbname, std::string_view coll,
                            bool configured) override;

  Result accessTokens(std::string const& user, velocypack::Builder&) override;
  Result deleteAccessToken(std::string const& user, uint64_t id) override;
  Result createAccessToken(std::string const& user, std::string const& name,
                           double validUntil, velocypack::Builder&) override;

  void shutdown() override;

#ifdef ARANGODB_USE_GOOGLE_TESTS
  void setAuthInfo(UserMap const& userEntryMap) override;

  [[nodiscard]] uint64_t internalVersion() const noexcept override;
#endif  // ARANGODB_USE_GOOGLE_TESTS

 private:
  bool checkPassword(std::string const& username, std::string const& password);
  bool checkAccessToken(std::string const& username, std::string const& token,
                        std::string& un);

  // Load users and permissions from local database.
  // Returns the version that was loaded and written to the _internalVersion.
  // Will be 0 if the load failed for any reason.
  uint64_t loadFromDB() noexcept;

  // This function will throw if the thread was not yet started
  // and the user-cache was not yet preloaded.
  // Basically guards most of the functions from being called too early.
  void checkIfUserDataIsAvailable() const;

  // store or replace user object
  Result storeUserInternal(User const& user, bool replace);

  // extract the username from the access token
  Result extractUsername(std::string const& token, std::string& username) const;

  // underlying application server
  ArangodServer& _server;

  // Protect the _userCache access
  basics::ReadWriteLock _userCacheLock;

  // used to update caches
  std::atomic<uint64_t> _globalVersion;
  std::atomic<uint64_t> _internalVersion;
  std::jthread _userCacheUpdateThread;

  // Caches permissions and other user info
  UserMap _userCache;
};
}  // namespace auth
}  // namespace arangodb
