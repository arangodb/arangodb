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

#include "Auth/TokenCache.h"
#include "Auth/UserManager.h"
#include "RestServer/arangod.h"

namespace arangodb {

class AuthenticationFeature final : public ArangodFeature {
 private:
  const size_t _maxSecretLength = 64;

 public:
  static constexpr std::string_view name() noexcept { return "Authentication"; }

  static AuthenticationFeature* instance() { return INSTANCE; }

  explicit AuthenticationFeature(Server& server);

  void collectOptions(std::shared_ptr<options::ProgramOptions>) override final;
  void validateOptions(std::shared_ptr<options::ProgramOptions>) override final;
  void prepare() override final;
  void start() override final;
  void unprepare() override final;

  bool isActive() const { return _active && isEnabled(); }

  bool authenticationUnixSockets() const { return _authenticationUnixSockets; }
  bool authenticationSystemOnly() const { return _authenticationSystemOnly; }

  /// @return Cache to deal with authentication tokens
  auth::TokenCache& tokenCache() const noexcept {
    TRI_ASSERT(_authCache);
    return *_authCache.get();
  }

  /// @brief user manager may be null on DBServers and Agency
  /// @return user manager singleton
  auth::UserManager* userManager() const noexcept { return _userManager.get(); }

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

 private:
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

  static AuthenticationFeature* INSTANCE;
};

}  // namespace arangodb
