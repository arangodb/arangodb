////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2014-2020 ArangoDB GmbH, Cologne, Germany
/// Copyright 2004-2014 triAGENS GmbH, Cologne, Germany
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

#include "AuthenticationFeature.h"

#include "ApplicationFeatures/ApplicationServer.h"
#include "Auth/Common.h"
#include "Auth/Handler.h"
#include "Basics/FileUtils.h"
#include "Basics/StringUtils.h"
#include "Basics/application-exit.h"
#include "Cluster/ServerState.h"
#include "FeaturePhases/BasicFeaturePhaseServer.h"
#include "Logger/LogMacros.h"
#include "Logger/Logger.h"
#include "Logger/LoggerStream.h"
#include "ProgramOptions/ProgramOptions.h"
#include "Random/RandomGenerator.h"
#include "RestServer/QueryRegistryFeature.h"

#if USE_ENTERPRISE
#include "Enterprise/Ldap/LdapAuthenticationHandler.h"
#include "Enterprise/Ldap/LdapFeature.h"
#endif

using namespace arangodb::options;

namespace arangodb {

AuthenticationFeature* AuthenticationFeature::INSTANCE = nullptr;

AuthenticationFeature::AuthenticationFeature(application_features::ApplicationServer& server)
    : ApplicationFeature(server, "Authentication"),
      _userManager(nullptr),
      _authCache(nullptr),
      _authenticationUnixSockets(true),
      _authenticationSystemOnly(true),
      _localAuthentication(true),
      _active(true),
      _authenticationTimeout(0.0) {
  setOptional(false);
  startsAfter<application_features::BasicFeaturePhaseServer>();

#ifdef USE_ENTERPRISE
  startsAfter<LdapFeature>();
#endif
}

void AuthenticationFeature::collectOptions(std::shared_ptr<ProgramOptions> options) {
  options->addSection("server", "Server features");

  options->addOldOption("server.disable-authentication",
                        "server.authentication");
  options->addOldOption("server.disable-authentication-unix-sockets",
                        "server.authentication-unix-sockets");
  options->addOldOption("server.authenticate-system-only",
                        "server.authentication-system-only");
  options->addOldOption("server.allow-method-override",
                        "http.allow-method-override");
  options->addOldOption("server.hide-product-header",
                        "http.hide-product-header");
  options->addOldOption("server.keep-alive-timeout", "http.keep-alive-timeout");
  options->addOldOption("server.default-api-compatibility", "");
  options->addOldOption("no-server", "server.rest-server");

  options->addOption("--server.authentication",
                     "enable authentication for ALL client requests",
                     new BooleanParameter(&_active));

  options->addOption(
      "--server.authentication-timeout",
      "timeout for the authentication cache in seconds (0 = indefinitely)",
      new DoubleParameter(&_authenticationTimeout));

  options->addOption("--server.local-authentication",
                     "enable authentication using the local user database",
                     new BooleanParameter(&_localAuthentication));

  options->addOption(
      "--server.authentication-system-only",
      "use HTTP authentication only for requests to /_api and /_admin",
      new BooleanParameter(&_authenticationSystemOnly));

#ifdef ARANGODB_HAVE_DOMAIN_SOCKETS
  options->addOption("--server.authentication-unix-sockets",
                     "authentication for requests via UNIX domain sockets",
                     new BooleanParameter(&_authenticationUnixSockets));
#endif

  // Maybe deprecate this option in devel
  options
      ->addOption("--server.jwt-secret",
                  "secret to use when doing jwt authentication",
                  new StringParameter(&_jwtSecretProgramOption))
      .setDeprecatedIn(30322)
      .setDeprecatedIn(30402);

  options->addOption(
      "--server.jwt-secret-keyfile",
      "file containing jwt secret to use when doing jwt authentication.",
      new StringParameter(&_jwtSecretKeyfileProgramOption));

  options->addOption(
      "--server.jwt-secret-folder",
      "folder containing one or more jwt secret files to use for jwt "
      "authentication. Files are sorted alphabetically: First secret "
      "is used for signing + verifying JWT tokens. The latter secrets "
      "are only used for verifying.",
      new StringParameter(&_jwtSecretFolderProgramOption),
      arangodb::options::makeDefaultFlags(arangodb::options::Flags::Enterprise))
      .setIntroducedIn(30700);
}

void AuthenticationFeature::validateOptions(std::shared_ptr<ProgramOptions> options) {
  if (!_jwtSecretKeyfileProgramOption.empty() && !_jwtSecretFolderProgramOption.empty()) {
    LOG_TOPIC("d3515", FATAL, Logger::STARTUP)
        << "please specify either '--server.jwt-"
           "secret-keyfile' or '--server.jwt-secret-folder' but not both.";
    FATAL_ERROR_EXIT();
  }

  if (!_jwtSecretKeyfileProgramOption.empty() || !_jwtSecretFolderProgramOption.empty()) {
    Result res = loadJwtSecretsFromFile();
    if (res.fail()) {
      LOG_TOPIC("d3617", FATAL, Logger::STARTUP) << res.errorMessage();
      FATAL_ERROR_EXIT();
    }
  }
  if (!_jwtSecretProgramOption.empty()) {
    if (_jwtSecretProgramOption.length() > _maxSecretLength) {
      LOG_TOPIC("9abfc", FATAL, arangodb::Logger::STARTUP)
          << "Given JWT secret too long. Max length is " << _maxSecretLength;
      FATAL_ERROR_EXIT();
    }
  }

  if (options->processingResult().touched("server.jwt-secret")) {
    LOG_TOPIC("1aaae", WARN, arangodb::Logger::AUTHENTICATION)
        << "--server.jwt-secret is insecure. Use --server.jwt-secret-keyfile "
           "instead.";
  }
}

void AuthenticationFeature::prepare() {
  TRI_ASSERT(isEnabled());
  TRI_ASSERT(_userManager == nullptr);

  ServerState::RoleEnum role = ServerState::instance()->getRole();
  TRI_ASSERT(role != ServerState::RoleEnum::ROLE_UNDEFINED);
  if (ServerState::isSingleServer(role) || ServerState::isCoordinator(role)) {
#if USE_ENTERPRISE
    if (server().getFeature<LdapFeature>().isEnabled()) {
      _userManager.reset(
          new auth::UserManager(server(), std::make_unique<LdapAuthenticationHandler>(
                                              server().getFeature<LdapFeature>())));
    } else {
      _userManager.reset(new auth::UserManager(server()));
    }
#else
    _userManager.reset(new auth::UserManager(server()));
#endif
  } else {
    LOG_TOPIC("713c0", DEBUG, Logger::AUTHENTICATION)
        << "Not creating user manager";
  }

  TRI_ASSERT(_authCache == nullptr);
  _authCache.reset(new auth::TokenCache(_userManager.get(), _authenticationTimeout));

  if (_jwtSecretProgramOption.empty()) {
    LOG_TOPIC("43396", INFO, Logger::AUTHENTICATION)
        << "Jwt secret not specified, generating...";
    uint16_t m = 254;
    for (size_t i = 0; i < _maxSecretLength; i++) {
      _jwtSecretProgramOption += static_cast<char>(1 + RandomGenerator::interval(m));
    }
  }

#if USE_ENTERPRISE
  _authCache->setJwtSecrets(_jwtSecretProgramOption, _jwtPassiveSecrets);
#else
  _authCache->setJwtSecret(_jwtSecretProgramOption);
#endif

  INSTANCE = this;
}

void AuthenticationFeature::start() {
  TRI_ASSERT(isEnabled());
  std::ostringstream out;

  out << "Authentication is turned " << (_active ? "on" : "off");

  if (_active && _authenticationSystemOnly) {
    out << " (system only)";
  }

#ifdef ARANGODB_HAVE_DOMAIN_SOCKETS
  out << ", authentication for unix sockets is turned "
      << (_authenticationUnixSockets ? "on" : "off");
#endif

  LOG_TOPIC("3844e", INFO, arangodb::Logger::AUTHENTICATION) << out.str();
}

void AuthenticationFeature::unprepare() { INSTANCE = nullptr; }

bool AuthenticationFeature::hasUserdefinedJwt() const {
  std::lock_guard<std::mutex> guard(_jwtSecretsLock);
  return !_jwtSecretProgramOption.empty();
}

/// secret used for signing & verification of secrets
std::string AuthenticationFeature::jwtActiveSecret() const {
  std::lock_guard<std::mutex> guard(_jwtSecretsLock);
  return _jwtSecretProgramOption;
}

#ifdef USE_ENTERPRISE
/// verification only secrets
std::pair<std::string, std::vector<std::string>> AuthenticationFeature::jwtSecrets() const {
  std::lock_guard<std::mutex> guard(_jwtSecretsLock);
  return {_jwtSecretProgramOption, _jwtPassiveSecrets};
}
#endif

Result AuthenticationFeature::loadJwtSecretsFromFile() {
  std::lock_guard<std::mutex> guard(_jwtSecretsLock);
  if (!_jwtSecretFolderProgramOption.empty()) {
    return loadJwtSecretFolder();
  } else if (!_jwtSecretKeyfileProgramOption.empty()) {
    return loadJwtSecretKeyfile();
  }
  return Result(TRI_ERROR_BAD_PARAMETER, "no JWT secret file was specified");
}

/// load JWT secret from file specified at startup
Result AuthenticationFeature::loadJwtSecretKeyfile() {
  try {
    // Note that the secret is trimmed for whitespace, because whitespace
    // at the end of a file can easily happen. We do not base64-encode,
    // though, so the bytes count as given. Zero bytes might be a problem
    // here.
    std::string contents = basics::FileUtils::slurp(_jwtSecretKeyfileProgramOption);
    _jwtSecretProgramOption = basics::StringUtils::trim(contents, " \t\n\r");
  } catch (std::exception const& ex) {
    std::string msg("unable to read content of jwt-secret file '");
    msg.append(_jwtSecretKeyfileProgramOption)
        .append("': ")
        .append(ex.what())
        .append(". please make sure the file/directory is readable for the ")
        .append("arangod process and user");
    return Result(TRI_ERROR_CANNOT_READ_FILE, std::move(msg));
  }
  return Result();
}

/// load JWT secrets from folder
Result AuthenticationFeature::loadJwtSecretFolder() try {
  TRI_ASSERT(!_jwtSecretFolderProgramOption.empty());

  LOG_TOPIC("4922f", INFO, arangodb::Logger::AUTHENTICATION)
      << "loading JWT secrets from folder " << _jwtSecretFolderProgramOption;

  auto list = basics::FileUtils::listFiles(_jwtSecretFolderProgramOption);

  // filter out empty filenames, hidden files, tmp files and symlinks
  list.erase(std::remove_if(list.begin(), list.end(),
      [this](std::string const& file) {
        if (file.empty() || file[0] == '.') {
          return true;
        }
        if (file.size() >= 4 && file.substr(file.size() - 4, 4) == ".tmp") {
          return true;
        }
        auto p = basics::FileUtils::buildFilename(_jwtSecretFolderProgramOption, file);
        if (basics::FileUtils::isSymbolicLink(p)) {
          return true;
        }
        return false;
      }),
      list.end());

  if (list.empty()) {
    return Result(TRI_ERROR_BAD_PARAMETER, "empty JWT secrets directory");
  }

  auto slurpy = [&](std::string const& file) {
    auto p = basics::FileUtils::buildFilename(_jwtSecretFolderProgramOption, file);
    std::string contents = basics::FileUtils::slurp(p);
    return basics::StringUtils::trim(contents, " \t\n\r");
  };

  std::sort(std::begin(list), std::end(list));
  std::string activeSecret = slurpy(list[0]);

  const std::string msg = "Given JWT secret too long. Max length is 64";
  if (activeSecret.length() > _maxSecretLength) {
    return Result(TRI_ERROR_BAD_PARAMETER, msg);
  }

#ifdef USE_ENTERPRISE
  std::vector<std::string> passiveSecrets;
  if (list.size() > 1) {
    list.erase(list.begin());
    for (auto const& file : list) {
      std::string secret = slurpy(file);
      if (secret.length() > _maxSecretLength) {
        return Result(TRI_ERROR_BAD_PARAMETER, msg);
      }
      if (!secret.empty()) {  // ignore
        passiveSecrets.push_back(std::move(secret));
      }
    }
  }
  _jwtPassiveSecrets = std::move(passiveSecrets);

  LOG_TOPIC("4a34f", INFO, arangodb::Logger::AUTHENTICATION)
      << "have " << _jwtPassiveSecrets.size() << " passive JWT secrets";
#endif

  _jwtSecretProgramOption = std::move(activeSecret);

  return Result();
} catch (basics::Exception const& ex) {
  std::string msg("unable to read content of jwt-secret-folder '");
  msg.append(_jwtSecretFolderProgramOption)
      .append("': ")
      .append(ex.what())
      .append(". please make sure the file/directory is readable for the ")
      .append("arangod process and user");
  return Result(TRI_ERROR_CANNOT_READ_FILE, std::move(msg));
}

}  // namespace arangodb
