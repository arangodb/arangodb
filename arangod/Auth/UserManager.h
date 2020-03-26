////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2014-2018 ArangoDB GmbH, Cologne, Germany
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

#ifndef ARANGOD_AUTHENTICATION_USER_MANAGER_H
#define ARANGOD_AUTHENTICATION_USER_MANAGER_H 1

#include "Auth/Common.h"
#include "Auth/User.h"

#include "ApplicationFeatures/ApplicationFeature.h"
#include "Basics/Mutex.h"
#include "Basics/ReadWriteLock.h"
#include "Basics/Result.h"
#include "Basics/debugging.h"
#include "Rest/CommonDefines.h"

#ifdef USE_ENTERPRISE
#include "Auth/Handler.h"
#endif

#include <velocypack/Builder.h>
#include <velocypack/Slice.h>

namespace arangodb {
namespace aql {
class QueryRegistry;
}
namespace basics {
template <typename T>
class ReadLocker;
}

namespace auth {
class Handler;

typedef std::unordered_map<std::string, auth::User> UserMap;

/// UserManager is the sole point of access for users and permissions
/// stored in `_system/_users` as well as in external authentication
/// systems like LDAP. The permissions are cached locally if possible,
/// to avoid unnecessary disk access. An instance of this should only
/// exist on coordinators and single servers.
class UserManager {
 public:
  explicit UserManager(application_features::ApplicationServer&);
#ifdef USE_ENTERPRISE
  explicit UserManager(application_features::ApplicationServer&,
                       std::unique_ptr<arangodb::auth::Handler>);
#endif
  ~UserManager() = default;

 public:
  typedef std::function<Result(auth::User&)> UserCallback;
  typedef std::function<Result(auth::User const&)> ConstUserCallback;

  /// Tells coordinator to reload its data. Only called in HeartBeat thread
  void setGlobalVersion(uint64_t version) {
    _globalVersion.store(version, std::memory_order_release);
  }

  /// @brief reload user cache and token caches
  void triggerLocalReload() {
    _globalVersion.fetch_add(1, std::memory_order_release);
  }

  /// @brief used for caching
  uint64_t globalVersion() {
    return _globalVersion.load(std::memory_order_acquire);
  }

  /// Trigger eventual reload on all other coordinators (and in TokenCache)
  void triggerGlobalReload();

  /// Trigger cache revalidation after user restore
  void triggerCacheRevalidation();

  /// Create the root user with a default password, will fail if the user
  /// already exists. Only ever call if you can guarantee to be in charge
  void createRootUser();

  velocypack::Builder allUsers();
  /// Add user from arangodb, do not use for LDAP  users
  Result storeUser(bool replace, std::string const& user, std::string const& pass,
                   bool active, velocypack::Slice extras);

  /// Enumerate list of all users
  Result enumerateUsers(std::function<bool(auth::User&)>&&,
                        bool retryOnConflict);
  /// Update specific user
  Result updateUser(std::string const& user, UserCallback&&);
  /// Access user without modifying it
  Result accessUser(std::string const& user, ConstUserCallback&&);

  /// @brief does this user exists in the db
  bool userExists(std::string const& user);
  /// Serialize user into legacy format for REST API
  velocypack::Builder serializeUser(std::string const& user);
  Result removeUser(std::string const& user);
  Result removeAllUsers();

  /// Convenience method to check a password
  bool checkPassword(std::string const& username, std::string const& password);

  /// Convenience method to refresh user rights
  /// returns true if the user was actually refreshed and the caller may
  /// need to update its own caches
#ifdef USE_ENTERPRISE
  bool refreshUser(std::string const& username);
#else
  inline bool refreshUser(std::string const& username) { return false; }
#endif

  auth::Level databaseAuthLevel(std::string const& username,
                                std::string const& dbname, bool configured = false);
  auth::Level collectionAuthLevel(std::string const& username, std::string const& dbname,
                                  std::string const& coll, bool configured = false);

  /// Overwrite internally cached permissions, only use
  /// for testing purposes
  void setAuthInfo(auth::UserMap const& userEntryMap);

#ifdef USE_ENTERPRISE

  /// @brief apply roles to all users in cache
  void applyRolesToAllUsers();
  /// @brief apply roles to user, must lock _userCacheLock
  void applyRoles(auth::User&) const;

  /// @brief Check authorization with external system
  /// @param userCached is the user cached locally
  /// @param a read guard which may need to be released
  bool checkPasswordExt(std::string const& username,
                        std::string const& password, bool userCached,
                        basics::ReadLocker<basics::ReadWriteLock>& readGuard);
#endif

 private:
  /// @brief load users and permissions from local database
  void loadFromDB();
  /// @brief store or replace user object
  Result storeUserInternal(auth::User const& user, bool replace);

 private:
  /// underlying application server
  application_features::ApplicationServer& _server;

  /// Protected the sync process from db, always lock
  /// before locking _userCacheLock
  Mutex _loadFromDBLock;

  /// Protect the _userCache access
  basics::ReadWriteLock _userCacheLock;

  /// @brief used to update caches
  std::atomic<uint64_t> _globalVersion;
  std::atomic<uint64_t> _internalVersion;

  /// Caches permissions and other user info
  UserMap _userCache;
#ifdef USE_ENTERPRISE
  /// iterface to external authentication systems like LDAP
  std::unique_ptr<arangodb::auth::Handler> _authHandler;
#endif
};
}  // namespace auth
}  // namespace arangodb

#endif
