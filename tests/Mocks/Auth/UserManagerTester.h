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

#include <atomic>
#include <functional>

namespace arangodb::auth {

/// @brief Test-only implementation of UserManagerBase that stores all user
/// state purely in _userCache (no ApplicationServer / DB required).
///
/// This class is guarded by the build system — it is compiled only when
/// ARANGODB_USE_GOOGLE_TESTS is defined.
///
/// Extra public methods (not on the UserManager interface):
///   setAuthInfo(UserMap const&) — replaces _userCache; bumps _internalVersion
///   internalVersion() const noexcept — read _internalVersion for assertions
class UserManagerTester final : public UserManagerBase {
 public:
  UserManagerTester() = default;
  ~UserManagerTester() override = default;

  // ---------- no-op lifecycle methods ----------------------------------------

  void loadUserCacheAndStartUpdateThread() noexcept override {}
  void triggerGlobalReload() const override {}
  void triggerCacheRevalidation() override {}
  void shutdown() override {}

  // ---------- pure-in-memory implementations ---------------------------------

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

  // ---------- extra methods only on UserManagerTester ------------------------

  /// @brief Replace the entire user cache. Bumps _internalVersion.
  void setAuthInfo(UserMap const& userEntryMap);

  /// @brief Read the internal version (for test assertions).
  uint64_t internalVersion() const noexcept;

 protected:
  // No thread / version check needed in tests.
  void checkIfUserDataIsAvailable() const override {}

 private:
  std::atomic<uint64_t> _internalVersion{0};
};

}  // namespace arangodb::auth
