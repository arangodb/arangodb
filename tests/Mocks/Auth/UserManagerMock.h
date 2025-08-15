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

#include <gmock/gmock.h>

namespace arangodb::auth {

struct UserManagerMock : UserManager {
  ~UserManagerMock() override = default;

  MOCK_METHOD(void, loadUserCacheAndStartUpdateThread, (),
              (noexcept, override));
  MOCK_METHOD(void, setGlobalVersion, (uint64_t), (noexcept, override));
  MOCK_METHOD(uint64_t, globalVersion, (), (const, noexcept, override));
  MOCK_METHOD(void, triggerGlobalReload, (), (const, override));
  MOCK_METHOD(void, triggerCacheRevalidation, (), (override));
  MOCK_METHOD(void, createRootUser, (), (override));
  MOCK_METHOD(velocypack::Builder, allUsers, (), (override));
  MOCK_METHOD(Result, storeUser,
              (bool, std::string const&, std::string const&, bool,
               velocypack::Slice),
              (override));
  MOCK_METHOD(Result, enumerateUsers, (std::function<bool(User&)>&&, bool),
              (override));
  MOCK_METHOD(Result, updateUser, (std::string const&, UserCallback&&),
              (override));
  MOCK_METHOD(Result, accessUser, (std::string const&, ConstUserCallback&&),
              (override));
  MOCK_METHOD(bool, userExists, (std::string const&), (override));
  MOCK_METHOD(velocypack::Builder, serializeUser, (std::string const&),
              (override));
  MOCK_METHOD(Result, removeUser, (std::string const&), (override));
  MOCK_METHOD(Result, removeAllUsers, (), (override));
  MOCK_METHOD(bool, checkCredentials,
              (std::string const&, std::string const&, std::string&),
              (override));
  MOCK_METHOD(Level, databaseAuthLevel,
              (std::string const&, std::string const&, bool), (override));
  MOCK_METHOD(Level, collectionAuthLevel,
              (std::string const&, std::string const&, std::string_view, bool),
              (override));
  MOCK_METHOD(Result, accessTokens, (std::string const&, velocypack::Builder&),
              (override));
  MOCK_METHOD(Result, deleteAccessToken, (std::string const&, uint64_t),
              (override));
  MOCK_METHOD(Result, createAccessToken,
              (std::string const&, std::string const&, double,
               velocypack::Builder&),
              (override));
  MOCK_METHOD(void, shutdown, (), (override));
  MOCK_METHOD(void, setAuthInfo, (UserMap const&), (override));
  MOCK_METHOD(uint64_t, internalVersion, (), (const, noexcept, override));
};

}  // namespace arangodb::auth
