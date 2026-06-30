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

#include "Auth/UserManagerBase.h"

#include "ApplicationFeatures/ApplicationFeature.h"
#include "ApplicationFeatures/ApplicationServer.h"
#include "Basics/Result.h"

#include <thread>

namespace arangodb {
namespace auth {

class UserManagerImpl final : public UserManagerBase {
 public:
  explicit UserManagerImpl(application_features::ApplicationServer&);
  ~UserManagerImpl() override;

  void loadUserCacheAndStartUpdateThread() noexcept override;

  static void triggerGlobalReload(application_features::ApplicationServer&);
  void triggerGlobalReload() const override;
  void triggerCacheRevalidation() override;
  void createRootUser() override;

  velocypack::Builder allUsers() override;

  Result storeUser(bool replace, std::string const& user,
                   std::string const& pass, bool active,
                   velocypack::Slice extras) override;

  Result enumerateUsers(std::function<bool(User&)>&&,
                        RetryOnConflict retryOnConflict) override;

  Result updateUser(std::string_view user, UserCallback&&,
                    RetryOnConflict retryOnConflict) override;

  Result removeUser(std::string const& user) override;
  Result removeAllUsers() override;

  void shutdown() override;

  // Returns the internal version — the version last loaded from DB.
  // Exposed (non-virtually) for integration tests that use a real
  // UserManagerImpl (e.g. UserManagerClusterTest).
  uint64_t internalVersion() const noexcept;

 private:
  // Load users and permissions from local database.
  // Returns the version that was loaded and written to the _internalVersion.
  // Will be 0 if the load failed for any reason.
  uint64_t loadFromDB() noexcept;

  // This function will throw if the thread was not yet started
  // and the user-cache was not yet preloaded.
  // Basically guards most of the functions from being called too early.
  void checkIfUserDataIsAvailable() const override final;

  // Translate a numeric collection ID to a name using DatabaseFeature.
  std::string translateCollectionName(std::string_view dbname,
                                      std::string_view coll) override;

  // store or replace user object
  Result storeUserInternal(User const& user, bool replace);

  // underlying application server
  application_features::ApplicationServer& _server;

  std::atomic<uint64_t> _internalVersion;
  std::jthread _userCacheUpdateThread;
};
}  // namespace auth
}  // namespace arangodb
