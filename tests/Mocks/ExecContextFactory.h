////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2014-2026 ArangoDB GmbH, Cologne, Germany
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
/// @author Tobias Gödderz
////////////////////////////////////////////////////////////////////////////////

#pragma once

#include "Auth/AuthMode.h"
#include "Auth/Common.h"
#include "Auth/UserManager.h"
#include "Endpoint/ConnectionInfo.h"
#include "Mocks/Auth/UserManagerMock.h"
#include "Rest/GeneralRequest.h"
#include "Utils/ExecContext.h"

#include <gmock/gmock.h>
#include <velocypack/Slice.h>

#include <memory>
#include <string>

namespace arangodb::tests::mocks {

/// @brief Thin subclass of ExecContext whose sole purpose is to expose the
/// protected ConstructorToken to the factory functions in this file.
/// Never subclass this for any other reason.
struct ExecContextAccessor final : public arangodb::ExecContext {
  static std::shared_ptr<arangodb::ExecContext> make(AuthMode authMode,
                                                     VocbasePtr vocbase) {
    // std::make_shared cannot call a private constructor, so we use new
    // directly here, which is valid inside the class scope.
    return std::shared_ptr<arangodb::ExecContext>(new ExecContextAccessor(
        ConstructorToken{}, std::move(authMode), std::move(vocbase)));
  }

 private:
  ExecContextAccessor(ConstructorToken token, AuthMode authMode,
                      VocbasePtr vocbase)
      : arangodb::ExecContext(token, std::move(authMode), std::move(vocbase)) {}
};

/// @brief Minimal GeneralRequest subclass for use in tests.
///
/// It satisfies the four pure-virtual requirements of GeneralRequest and
/// leaves requestedApiVersion() at its default value of 0 (= V0 /
/// "no versioned prefix"). This means the "return NOT_FOUND to hide existence"
/// branches in AuthMode::Classic::check() are never taken, which is the
/// correct behaviour for tests that only check FORBIDDEN / READ_ONLY outcomes.
struct FakeGeneralRequest final : public arangodb::GeneralRequest {
  FakeGeneralRequest() : arangodb::GeneralRequest(ConnectionInfo{}, 0) {}

  size_t contentLength() const noexcept override { return 0; }
  std::string_view rawPayload() const override { return {}; }
  velocypack::Slice payload(bool /*strictValidation*/) override {
    return velocypack::Slice::noneSlice();
  }
  void setDefaultContentType() noexcept override {}
};

/// @brief Bundled ownership for a Classic-auth ExecContext created by the
/// factory. All three members must outlive any ExecContextScope that uses
/// execContext, because AuthMode::Classic holds raw references to userManager
/// and request.
struct ClassicExecContext {
  std::shared_ptr<testing::NiceMock<auth::UserManagerMock>> userManager;
  std::shared_ptr<FakeGeneralRequest> request;
  std::shared_ptr<arangodb::ExecContext> execContext;
};

/// @brief Create an ExecContext backed by a real AuthMode::Classic, with the
/// UserManager mock pre-configured to return the given access levels.
///
/// @param username   Username stored in the Classic auth context.
/// @param dbname     The "home" database: collectionAuthLevel queries for this
///                   database return dbLevel. Pass "" if no specific database
///                   is needed.
/// @param systemLevel  Access level returned for the _system database.
/// @param dbLevel      Access level returned for dbname (and its collections).
/// @param apiHardened  Passed to Classic ctor; set true to require admin for
///                     hardened actions. Defaults to false.
///
/// ON_CALL rules are used (not EXPECT_CALL), so calls are not mandatory and
/// no unmet-expectation failures are produced. NiceMock suppresses warnings
/// for any unmocked method calls.
///
/// ON_CALL registration order: general default first, then specific matchers
/// so that gmock's "last-registered wins" rule gives the specific rules
/// priority over the catch-all default.
inline ClassicExecContext makeClassicExecContext(std::string username,
                                                 std::string dbname,
                                                 auth::Level systemLevel,
                                                 auth::Level dbLevel,
                                                 bool apiHardened = false) {
  using ::testing::_;
  using ::testing::NiceMock;
  using ::testing::Return;

  auto um = std::make_shared<NiceMock<auth::UserManagerMock>>();

  // ---- databaseAuthLevel ------------------------------------------------
  // Default: no access to any database.
  ON_CALL(*um, databaseAuthLevel(_, _, _))
      .WillByDefault(Return(auth::Level::NONE));
  // _system database gets systemLevel.
  ON_CALL(*um, databaseAuthLevel(_, "_system", _))
      .WillByDefault(Return(systemLevel));
  // The home database gets dbLevel (only when it is not _system and not empty).
  if (!dbname.empty() && dbname != "_system") {
    ON_CALL(*um, databaseAuthLevel(_, dbname, _))
        .WillByDefault(Return(dbLevel));
  }

  // ---- collectionAuthLevel -----------------------------------------------
  // Default: no access.
  ON_CALL(*um, collectionAuthLevel(_, _, _, _))
      .WillByDefault(Return(auth::Level::NONE));
  // Collections in _system follow systemLevel.
  ON_CALL(*um, collectionAuthLevel(_, "_system", _, _))
      .WillByDefault(Return(systemLevel));
  // Collections in the home database follow dbLevel.
  if (!dbname.empty() && dbname != "_system") {
    ON_CALL(*um, collectionAuthLevel(_, dbname, _, _))
        .WillByDefault(Return(dbLevel));
  }

  auto req = std::make_shared<FakeGeneralRequest>();

  auto authMode =
      AuthMode{AuthMode::Classic{*um, std::move(username), apiHardened, *req}};
  auto ctx =
      ExecContextAccessor::make(std::move(authMode), VocbasePtr{nullptr});

  return ClassicExecContext{std::move(um), std::move(req), std::move(ctx)};
}

/// @brief Ownership bundle for a Classic-auth ExecContext that borrows an
/// existing UserManager (e.g. from a server mock or the real AuthFeature).
/// The request member must outlive the execContext — keep this struct alive
/// for the duration of any ExecContextScope that wraps execContext.
struct BorrowedExecContext {
  std::shared_ptr<FakeGeneralRequest> request;
  std::shared_ptr<arangodb::ExecContext> execContext;
};

/// @brief Create an ExecContext backed by a real AuthMode::Classic using an
/// already-configured UserManager (real or mock) from the test fixture.
///
/// Use this instead of makeClassicExecContext when the test already owns a
/// UserManager that it configures separately (e.g. via setAuthInfo /
/// EXPECT_CALL on a UserManagerMock from MockAqlServer, or via the real
/// UserManagerImpl from a full server fixture).
///
/// @param existingUserManager  The UserManager to delegate permission checks
/// to.
/// @param username             Username stored in the Classic auth context.
/// @param apiHardened          Passed to Classic ctor. Defaults to false.
///
/// IMPORTANT: Keep the returned BorrowedExecContext alive for at least as long
/// as the ExecContextScope that wraps its execContext, because the
/// FakeGeneralRequest is owned by the struct and referenced by the Classic
/// AuthMode stored inside the ExecContext.
inline BorrowedExecContext makeClassicExecContextFrom(
    auth::UserManager& existingUserManager, std::string username,
    bool apiHardened = false) {
  auto req = std::make_shared<FakeGeneralRequest>();
  auto authMode = AuthMode{AuthMode::Classic{
      existingUserManager, std::move(username), apiHardened, *req}};
  auto ctx =
      ExecContextAccessor::make(std::move(authMode), VocbasePtr{nullptr});
  return BorrowedExecContext{std::move(req), std::move(ctx)};
}

}  // namespace arangodb::tests::mocks
