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
////////////////////////////////////////////////////////////////////////////////

#pragma once

#include "Auth/AuthMode.h"
#include "Auth/Common.h"
#include "Auth/UserManager.h"
#include "Basics/StaticStrings.h"
#include "Endpoint/ConnectionInfo.h"
#include "Mocks/Auth/UserManagerTester.h"
#include "Rest/GeneralRequest.h"
#include "Utils/ExecContext.h"

#include <velocypack/Slice.h>

#include <memory>
#include <string>

namespace arangodb::tests::mocks {

/// @brief Create an ExecContext without needing access to its protected
/// constructor token.
auto inline createSharedExecContext(AuthMode authMode, bool isRestApiHardened,
                                    VocbasePtr vocbase) {
  struct EC : ExecContext {
    auto static token() { return ConstructorToken{}; }
  };
  return std::make_shared<ExecContext>(EC::token(), std::move(authMode),
                                       isRestApiHardened, std::move(vocbase));
}

/// @brief Minimal GeneralRequest subclass for use in tests.
///
/// It satisfies the four pure-virtual requirements of GeneralRequest and
/// leaves requestedApiVersion() at its default value of 0 (= V0 /
/// "no versioned prefix"). This means the "return NOT_FOUND to hide existence"
/// branches in AuthMode::Classic::check() are never taken, which is the
/// correct behaviour for tests that only check FORBIDDEN / READ_ONLY outcomes.
struct FakeGeneralRequest final : GeneralRequest {
  FakeGeneralRequest() : GeneralRequest(ConnectionInfo{}, 0) {}

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
  std::shared_ptr<auth::UserManagerTester> userManager;
  std::shared_ptr<FakeGeneralRequest> request;
  std::shared_ptr<ExecContext> execContext;
};

/// @brief Create an ExecContext backed by a real AuthMode::Classic, with a
/// UserManagerTester pre-populated to return the given access levels.
///
/// @param username   Username stored in the Classic auth context.
/// @param dbname     The "home" database: collectionAuthLevel queries for this
///                   database return dbLevel. Pass "" if no specific database
///                   is needed.
/// @param systemLevel  Access level returned for the _system database.
/// @param dbLevel      Access level returned for dbname (and its collections).
/// @param isRestApiHardened  Passed to Classic ctor; set true to require admin
///                           for hardened actions. Defaults to false.
inline ClassicExecContext makeClassicExecContext(
    std::string username, std::string dbname, auth::Level systemLevel,
    auth::Level dbLevel, bool isRestApiHardened = false) {
  auto um = std::make_shared<auth::UserManagerTester>();

  // Build a UserMap containing one entry for 'username' with the requested
  // access levels, plus a wildcard catch-all at Level::NONE.
  auth::UserMap userMap;
  auto& user = userMap.emplace(username, auth::User::newUser(username, ""))
                   .first->second;
  user.setActive(true);
  user.grantDatabase(StaticStrings::SystemDatabase, systemLevel);
  user.grantCollection(StaticStrings::SystemDatabase, "*", systemLevel);
  if (!dbname.empty() && dbname != StaticStrings::SystemDatabase) {
    user.grantDatabase(dbname, dbLevel);
    user.grantCollection(dbname, "*", dbLevel);
  }
  // Catch-all: no access to any other database.
  user.grantDatabase("*", auth::Level::NONE);
  user.grantCollection("*", "*", auth::Level::NONE);

  um->setAuthInfo(userMap);

  auto req = std::make_shared<FakeGeneralRequest>();

  auto authMode = AuthMode{AuthMode::Classic{*um, std::move(username), *req}};
  auto ctx = createSharedExecContext(std::move(authMode), isRestApiHardened,
                                     VocbasePtr{nullptr});

  return ClassicExecContext{std::move(um), std::move(req), std::move(ctx)};
}

/// @brief Ownership bundle for a Classic-auth ExecContext that borrows an
/// existing UserManager (e.g. from a server mock or the real AuthFeature).
/// The request member must outlive the execContext — keep this struct alive
/// for the duration of any ExecContextScope that wraps execContext.
struct BorrowedExecContext {
  std::shared_ptr<FakeGeneralRequest> request;
  std::shared_ptr<ExecContext> execContext;
};

/// @brief Create an ExecContext backed by a real AuthMode::Classic using an
/// already-configured UserManager (real or mock) from the test fixture.
///
/// Use this instead of makeClassicExecContext when the test already owns a
/// UserManager that it configures separately (e.g. via setAuthInfo on a
/// UserManagerTester from MockAqlServer, or via the real UserManagerImpl
/// from a full server fixture).
///
/// @param existingUserManager  The UserManager to delegate permission checks
///                             to.
/// @param username             Username stored in the Classic auth context.
/// @param isRestApiHardened    Passed to Classic ctor. Defaults to false.
///
/// IMPORTANT: Keep the returned BorrowedExecContext alive for at least as long
/// as the ExecContextScope that wraps its execContext, because the
/// FakeGeneralRequest is owned by the struct and referenced by the Classic
/// AuthMode stored inside the ExecContext.
inline BorrowedExecContext makeClassicExecContextFrom(
    auth::UserManager& existingUserManager, std::string username,
    bool isRestApiHardened = false) {
  auto req = std::make_shared<FakeGeneralRequest>();
  auto authMode = AuthMode{
      AuthMode::Classic{existingUserManager, std::move(username), *req}};
  auto ctx = createSharedExecContext(std::move(authMode), isRestApiHardened,
                                     VocbasePtr{nullptr});
  return BorrowedExecContext{std::move(req), std::move(ctx)};
}

}  // namespace arangodb::tests::mocks
