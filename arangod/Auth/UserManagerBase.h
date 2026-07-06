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

#include "Basics/ReadWriteLock.h"

#include <atomic>

namespace arangodb::auth {

/// @brief Abstract intermediate class that owns the in-memory user cache and
/// provides concrete, read-only implementations of the UserManager interface.
///
/// Both UserManagerImpl (production) and UserManagerTester (tests) derive from
/// this class.
///
/// Subclasses MUST always use _userCacheLock when reading or writing
/// _userCache, consistent with the existing READ_LOCKER / WRITE_LOCKER
/// discipline.
class UserManagerBase : public UserManager {
 public:
  UserManagerBase();
  ~UserManagerBase() override = default;

  void setGlobalVersion(uint64_t version) noexcept override final;
  [[nodiscard]] uint64_t globalVersion() const noexcept override final;

  Result accessUser(std::string const& user,
                    ConstUserCallback&&) override final;
  bool userExists(std::string const& user) override final;
  velocypack::Builder serializeUser(std::string const& user) override final;

  bool checkCredentials(std::string const& username, std::string const& token,
                        std::string& un) override final;

  Level databaseAuthLevel(std::string_view username, std::string_view dbname,
                          bool configured) override final;
  Level collectionAuthLevel(std::string_view username, std::string_view dbname,
                            std::string_view coll,
                            bool configured) override final;

  Result accessTokens(std::string const& user,
                      velocypack::Builder&) override final;
  Result deleteAccessToken(std::string const& user, uint64_t id) override final;
  Result createAccessToken(std::string const& user, std::string const& name,
                           double validUntil,
                           velocypack::Builder&) override final;

 protected:
  /// @brief Convert documents from _system/_users into the format used in
  /// the REST user API and Foxx.  Shared by all subclasses.
  static void ConvertLegacyFormat(velocypack::VPackSlice doc,
                                  velocypack::Builder& result);

  /// @brief Called by read-only implementations before accessing the cache.
  /// UserManagerImpl asserts that the update thread has been started.
  /// UserManagerTester provides a no-op implementation.
  virtual void checkIfUserDataIsAvailable() const = 0;

  /// @brief Translate a numeric collection ID string to a collection name.
  /// The default implementation returns the collection name unchanged, which
  /// is appropriate for test builds. UserManagerImpl overrides this to
  /// delegate to DatabaseFeature::translateCollectionName.
  virtual std::string translateCollectionName(std::string_view dbname,
                                              std::string_view coll);

  // Protect _userCache access.
  basics::ReadWriteLock _userCacheLock;
  // Used to synchronise caches across coordinators.
  std::atomic<uint64_t> _globalVersion;
  // Caches permissions and other user info.
  UserMap _userCache;

 private:
  bool checkPassword(std::string const& username, std::string const& password);
  Result extractUsername(std::string const& token, std::string& username) const;
  bool checkAccessToken(std::string const& username, std::string const& token,
                        std::string& un);
};

}  // namespace arangodb::auth
