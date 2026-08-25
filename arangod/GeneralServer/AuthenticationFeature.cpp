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
////////////////////////////////////////////////////////////////////////////////

#include "AuthenticationFeature.h"
#include "Assertions/ProdAssert.h"
#include "AuthenticationOptionsProvider.h"

#include "ApplicationFeatures/ApplicationServer.h"
#include "Basics/voc-errors.h"
#include "FeaturePhases/BasicFeaturePhaseServer.h"
#include "Auth/Handler.h"
#include "Auth/TokenCache.h"
#include "Auth/UserManagerImpl.h"
#include "Basics/FileUtils.h"
#include "Basics/StringUtils.h"
#include "Basics/application-exit.h"
#include "Cluster/ServerState.h"
#include "Logger/LogMacros.h"
#include "Logger/Logger.h"
#include "Logger/LoggerStream.h"
#include "ProgramOptions/Parameters.h"
#include "ProgramOptions/ProgramOptions.h"
#include "RestServer/QueryRegistryFeature.h"
#include "VocBase/Methods/Tasks.h"

#include <limits>

#include <filesystem>
#include <variant>

using namespace arangodb::options;

namespace arangodb {

std::atomic<AuthenticationFeature*> AuthenticationFeature::INSTANCE = nullptr;

AuthenticationFeature::AuthenticationFeature(
    application_features::ApplicationServer& server)
    : AuthenticationFeature(server, AuthenticationOptions{}) {}

AuthenticationFeature::AuthenticationFeature(
    application_features::ApplicationServer& server,
    AuthenticationOptions options)
    : ApplicationFeature{server, *this},
      _options(std::move(options)),
      _userManager(nullptr),
      _authCache(nullptr) {
  setOptional(false);
  startsAfter<application_features::BasicFeaturePhaseServer>();

  auto res = auth::loadJwtSecretString(_options.jwtSecretProgramOption);
  if (res.ok()) {
    _authInfo.assign(res.get());
  } else {
    LOG_TOPIC("d3617", FATAL, Logger::STARTUP) << res.errorMessage();
    FATAL_ERROR_EXIT();
  }

  if (!_options.jwtSecretKeyfileProgramOption.empty() ||
      !_options.jwtSecretFolderProgramOption.empty()) {
    Result res = loadJwtSecretsFromFile();
    if (res.fail()) {
      LOG_TOPIC("d3617", FATAL, Logger::STARTUP) << res.errorMessage();
      FATAL_ERROR_EXIT();
    }
  }
}

AuthenticationFeature::~AuthenticationFeature() = default;

void AuthenticationFeature::prepare() {
  TRI_ASSERT(isEnabled());
  TRI_ASSERT(_userManager == nullptr);

  ServerState::RoleEnum role = ServerState::instance()->getRole();
  TRI_ASSERT(role != ServerState::RoleEnum::ROLE_UNDEFINED);
  if (ServerState::isSingleServer(role) || ServerState::isCoordinator(role)) {
    if (_userManager == nullptr) {
      _userManager = std::make_unique<auth::UserManagerImpl>(server());
    }

    TRI_ASSERT(_userManager != nullptr);
  } else {
    LOG_TOPIC("713c0", DEBUG, Logger::AUTHENTICATION)
        << "Not creating user manager";
  }

  TRI_ASSERT(_authCache == nullptr);
  _authCache = std::make_unique<auth::TokenCache>(
      _userManager.get(), _options.authenticationTimeout);

  if (!_authInfo.getLockedGuard()->has_value()) {
    LOG_TOPIC("43396", INFO, Logger::AUTHENTICATION)
        << "Jwt secret not specified, generating...";
    _authInfo.assign(auth::generateRandomHS256AuthInfo());
  }

  _authCache->setJwtSecrets(*_authInfo.copy());
  INSTANCE.store(this, std::memory_order_release);
}

void AuthenticationFeature::start() {
  TRI_ASSERT(isEnabled());
  std::ostringstream out;

  out << "Authentication is turned " << (_options.active ? "on" : "off");

  if (_options.active && _options.authenticationSystemOnly) {
    out << " (system only)";
  }

#ifdef ARANGODB_HAVE_DOMAIN_SOCKETS
  out << ", authentication for unix sockets is turned "
      << (_options.authenticationUnixSockets ? "on" : "off");
#endif

  LOG_TOPIC("3844e", INFO, arangodb::Logger::AUTHENTICATION) << out.str();
}

void AuthenticationFeature::stop() {
  if (_userManager) {
    _userManager->shutdown();
  }
}

void AuthenticationFeature::unprepare() {
  INSTANCE.store(nullptr, std::memory_order_relaxed);
}

AuthenticationFeature* AuthenticationFeature::instance() noexcept {
  return INSTANCE.load(std::memory_order_acquire);
}

bool AuthenticationFeature::isActive() const noexcept {
  return _options.active && isEnabled();
}

bool AuthenticationFeature::authenticationUnixSockets() const noexcept {
  return _options.authenticationUnixSockets;
}

bool AuthenticationFeature::authenticationSystemOnly() const noexcept {
  return _options.authenticationSystemOnly;
}

std::string_view AuthenticationFeature::externalRbacService() const noexcept {
  return _options.externalRbacService;
}

bool AuthenticationFeature::rbacEnabled() const noexcept {
  return !_options.externalRbacService.empty();
}

/// @return Cache to deal with authentication tokens
auth::TokenCache& AuthenticationFeature::tokenCache() const noexcept {
  TRI_ASSERT(_authCache);
  return *_authCache;
}

/// @brief user manager may be null on DBServers and Agency
/// @return user manager singleton
auth::UserManager* AuthenticationFeature::userManager() const noexcept {
  return _userManager.get();
}

bool AuthenticationFeature::hasUserdefinedJwt() const {
  return _authInfo.getLockedGuard()->has_value();
}

auth::AuthInfo AuthenticationFeature::jwtSecrets() const {
  auto v = _authInfo.copy();
  if (!v.has_value()) {
    ADB_PROD_ASSERT(v.has_value())
        << "jwt secrets were requested, but none were set";
  }
  return *v;
}

Result AuthenticationFeature::loadJwtSecretsFromFile() {
  auto r = std::invoke([&]() -> ResultT<auth::AuthInfo> {
    if (!_options.jwtSecretFolderProgramOption.empty()) {
      return auth::loadJwtSecretFolder(_options.jwtSecretFolderProgramOption);
    } else if (!_options.jwtSecretKeyfileProgramOption.empty()) {
      return auth::loadJwtSecretFile(_options.jwtSecretKeyfileProgramOption);
    }
    return Result(TRI_ERROR_BAD_PARAMETER, "no JWT secret file was specified");
  });
  if (!r.ok()) {
    return r.result();
  }

  _authInfo.assign(std::move(r.get()));
  return Result();
}

#ifdef ARANGODB_USE_GOOGLE_TESTS
void AuthenticationFeature::setUserManager(
    std::unique_ptr<auth::UserManager> um) {
  _userManager.swap(um);
}
#endif  // ARANGODB_USE_GOOGLE_TESTS

}  // namespace arangodb
