////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2014-2024 ArangoDB GmbH, Cologne, Germany
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
/// @author Andreas Streichardt <andreas@arangodb.com>
////////////////////////////////////////////////////////////////////////////////

#pragma once

#include "Basics/Result.h"
#include "RestServer/arangod.h"

#include <atomic>
#include <cstddef>
#include <mutex>
#include <string>
#include <vector>

namespace arangodb {
namespace auth {
class TokenCache;
class UserManager;
}  // namespace auth

class AuthenticationFeature final : public ArangodFeature {
 public:
  static constexpr std::string_view name() noexcept { return "Authentication"; }

  explicit AuthenticationFeature(Server& server);
  ~AuthenticationFeature();

  void collectOptions(std::shared_ptr<options::ProgramOptions>) override final;
  void validateOptions(std::shared_ptr<options::ProgramOptions>) override final;
  void prepare() override final;
  void start() override final;
  void unprepare() override final;

  static AuthenticationFeature* instance() noexcept;

  bool isActive() const noexcept;

  bool authenticationUnixSockets() const noexcept;
  bool authenticationSystemOnly() const noexcept;

  /// @return Cache to deal with authentication tokens
  auth::TokenCache& tokenCache() const noexcept;

  /// @brief user manager may be null on DBServers and Agency
  /// @return user manager singleton
  auth::UserManager* userManager() const noexcept;

  bool hasUserdefinedJwt() const;
#ifdef USE_ENTERPRISE
  /// verification only secrets
  std::pair<std::string, std::vector<std::string>> jwtSecrets() const;
#endif

  double sessionTimeout() const { return _sessionTimeout; }

  // load secrets from file(s)
  [[nodiscard]] Result loadJwtSecretsFromFile();

 private:
  /// load JWT secret from file specified at startup
  [[nodiscard]] Result loadJwtSecretKeyfile();

  /// load JWT secrets from folder
  [[nodiscard]] Result loadJwtSecretFolder();

  static constexpr size_t kMaxSecretLength = 64;

  std::unique_ptr<auth::UserManager> _userManager;
  std::unique_ptr<auth::TokenCache> _authCache;
  bool _authenticationUnixSockets;
  bool _authenticationSystemOnly;
  bool _active;
  double _authenticationTimeout;
  double _sessionTimeout;

  mutable std::mutex _jwtSecretsLock;

  std::string _jwtSecretProgramOption;
  std::string _jwtSecretKeyfileProgramOption;
  std::string _jwtSecretFolderProgramOption;

#ifdef USE_ENTERPRISE
  /// verification only secrets
  std::vector<std::string> _jwtPassiveSecrets;
#endif

  static std::atomic<AuthenticationFeature*> INSTANCE;
};

}  // namespace arangodb
