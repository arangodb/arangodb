////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2016 ArangoDB GmbH, Cologne, Germany
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
/// @author Andreas Streichardt <andreas@arangodb.com>
////////////////////////////////////////////////////////////////////////////////

#ifndef APPLICATION_FEATURES_AUTHENTICATION_FEATURE_H
#define APPLICATION_FEATURES_AUTHENTICATION_FEATURE_H 1

#include "ApplicationFeatures/ApplicationFeature.h"
#include "Auth/TokenCache.h"
#include "Auth/UserManager.h"

namespace arangodb {

class AuthenticationFeature final
    : public application_features::ApplicationFeature {
 private:
  const size_t _maxSecretLength = 64;

 public:
  explicit AuthenticationFeature(
    application_features::ApplicationServer& server
  );

  static inline AuthenticationFeature* instance() {
    return INSTANCE;
  }

  void collectOptions(std::shared_ptr<options::ProgramOptions>) override final;
  void validateOptions(std::shared_ptr<options::ProgramOptions>) override final;
  void prepare() override final;
  void start() override final;
  void unprepare() override final;

  bool isActive() const { return _active && isEnabled(); }

  bool authenticationUnixSockets() const { return _authenticationUnixSockets; }
  bool authenticationSystemOnly() const { return _authenticationSystemOnly; }
  std::string jwtSecret() const { return _authCache->jwtSecret(); }
  bool hasUserdefinedJwt() const { return !_jwtSecretProgramOption.empty(); }

  double authenticationTimeout() const noexcept { return _authenticationTimeout; }
  /// Enable or disable standalone authentication
  bool localAuthentication() const noexcept { return _localAuthentication; }

  /// @return Cache to deal with authentication tokens
  inline auth::TokenCache& tokenCache() const noexcept {
    TRI_ASSERT(_authCache);
    return *_authCache.get();
  }

  /// @brief user manager may be null on DBServers and Agency
  /// @return user manager singleton
  inline auth::UserManager* userManager() const noexcept {
    return _userManager.get();
  }

 private:
  std::unique_ptr<auth::UserManager> _userManager;
  std::unique_ptr<auth::TokenCache> _authCache;
  bool _authenticationUnixSockets;
  bool _authenticationSystemOnly;
  bool _localAuthentication;
  bool _active;
  double _authenticationTimeout;

  std::string _jwtSecretProgramOption;

  static AuthenticationFeature* INSTANCE;

};

} // arangodb

#endif
