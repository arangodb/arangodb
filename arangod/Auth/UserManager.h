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

#include "Auth/Common.h"
#include "Auth/User.h"

#include "Basics/Result.h"
#include "Rest/CommonDefines.h"

#include <velocypack/Builder.h>
#include <velocypack/Slice.h>

namespace arangodb::auth {

using UserMap = std::unordered_map<std::string, User>;

// UserManager is the sole point of access for users and permissions
// stored in `_system/_users`. The permissions are cached locally if possible,
// to avoid unnecessary disk access. An instance of this should only
// exist on coordinators and single servers.
class UserManager {
 public:
  virtual ~UserManager() = default;

  /*
   * This will first try to load the initial userCache and afterward start the
   * internal update thread. This is blocking, it will repeat until it succeeds.
   * This is not thread safe.
   */
  virtual void loadUserCacheAndStartUpdateThread() noexcept = 0;

  using UserCallback = std::function<Result(User&)>;
  using ConstUserCallback = std::function<Result(User const&)>;

  // Tells coordinator to reload its data. Only called in HeartBeat thread
  virtual void setGlobalVersion(uint64_t version) noexcept = 0;

  // used for caching
  [[nodiscard]] virtual uint64_t globalVersion() const noexcept = 0;

  // Trigger eventual reload on all other coordinators (and in TokenCache)
  virtual void triggerGlobalReload() const = 0;

  // Will trigger a global reload and block until the versions are
  // incremented at least once. To ensure that the internal thread executed
  // loadFromDB at least once.
  virtual void triggerCacheRevalidation() = 0;

  // Create the root user with a default password, will fail if the user
  // already exists. Only ever call if you can guarantee to be in charge
  virtual void createRootUser() = 0;

  virtual velocypack::Builder allUsers() = 0;
  // Add user from arangodb
  virtual Result storeUser(bool replace, std::string const& user,
                           std::string const& pass, bool active,
                           velocypack::Slice extras) = 0;

  // Enumerate list of all users
  virtual Result enumerateUsers(std::function<bool(User&)>&&,
                                bool retryOnConflict) = 0;

  // Update specific user
  virtual Result updateUser(std::string const& user, UserCallback&&) = 0;

  // Access user without modifying it
  virtual Result accessUser(std::string const& user, ConstUserCallback&&) = 0;

  // does this user exists in the db
  virtual bool userExists(std::string const& user) = 0;

  // Serialize user into legacy format for REST API
  virtual velocypack::Builder serializeUser(std::string const& user) = 0;

  // Remove one user or all users
  virtual Result removeUser(std::string const& user) = 0;
  virtual Result removeAllUsers() = 0;

  // Convenience methods to check a password or access token
  virtual bool checkCredentials(std::string const& username,
                                std::string const& token, std::string& un) = 0;

  virtual Level databaseAuthLevel(std::string const& username,
                                  std::string const& dbname,
                                  bool configured) = 0;
  virtual Level collectionAuthLevel(std::string const& username,
                                    std::string const& dbname,
                                    std::string_view coll, bool configured) = 0;

  // Return all access tokens of an user
  virtual Result accessTokens(std::string const& user,
                              velocypack::Builder&) = 0;

  // Delete an access tokens of an user
  virtual Result deleteAccessToken(std::string const& user, uint64_t id) = 0;

  // Creates an access tokens for an user
  virtual Result createAccessToken(std::string const& user,
                                   std::string const& name, double validUntil,
                                   velocypack::Builder&) = 0;

  // This will shut down the running thread on demand.
  // It should only be use when the UserManager is being destroyed.
  virtual void shutdown() = 0;

#ifdef ARANGODB_USE_GOOGLE_TESTS
  // Overwrite internally cached permissions, only use
  // for testing purposes.
  // This will assert that the underlying UpdateThread was started.
  virtual void setAuthInfo(UserMap const& userEntryMap) = 0;

  // Need this to find out if the loadFromDB was run and the internal version
  // was updated
  [[nodiscard]] virtual uint64_t internalVersion() const noexcept = 0;
#endif  // ARANGODB_USE_GOOGLE_TESTS
};
}  // namespace arangodb::auth
