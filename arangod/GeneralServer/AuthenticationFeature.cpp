////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2014-2023 ArangoDB GmbH, Cologne, Germany
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
#include "Logger/LogMacros.h"
#include "Logger/Logger.h"
#include "Logger/LoggerStream.h"
#include "ProgramOptions/Parameters.h"
#include "ProgramOptions/ProgramOptions.h"
#include "Random/RandomGenerator.h"
#include "RestServer/QueryRegistryFeature.h"

#if USE_ENTERPRISE
#include "Enterprise/Ldap/LdapAuthenticationHandler.h"
#include "Enterprise/Ldap/LdapFeature.h"
#endif

#include <limits>

using namespace arangodb::options;

namespace arangodb {

AuthenticationFeature* AuthenticationFeature::INSTANCE = nullptr;

AuthenticationFeature::AuthenticationFeature(Server& server)
    : ArangodFeature{server, *this},
      _userManager(nullptr),
      _authCache(nullptr),
      _authenticationUnixSockets(true),
      _authenticationSystemOnly(true),
      _localAuthentication(true),
      _active(true),
      _authenticationTimeout(0.0),
      _sessionTimeout(static_cast<double>(1 * std::chrono::hours(1) /
                                          std::chrono::seconds(1))) {  // 1 hour
  setOptional(false);
  startsAfter<application_features::BasicFeaturePhaseServer>();

  if constexpr (Server::contains<LdapFeature>()) {
    startsAfter<LdapFeature>();
  }
}

void AuthenticationFeature::collectOptions(
    std::shared_ptr<ProgramOptions> options) {
  options->addObsoleteOption(
      "server.disable-authentication",
      "Whether to use authentication for all client requests.", false);
  options->addObsoleteOption(
      "server.disable-authentication-unix-sockets",
      "Whether to use authentication for requests via UNIX domain sockets.",
      false);
  options->addOldOption("server.authenticate-system-only",
                        "server.authentication-system-only");

  options
      ->addOption("--server.authentication",
                  "Whether to use authentication for all client requests.",
                  new BooleanParameter(&_active))
      .setLongDescription(R"(You can set this option to `false` to turn off
authentication on the server-side, so that all clients can execute any action
without authorization and privilege checks. You should only do this if you bind
the server to `localhost` to not expose it to the public internet)");

  options
      ->addOption("--server.authentication-timeout",
                  "The timeout for the authentication cache "
                  "(in seconds, 0 = indefinitely).",
                  new DoubleParameter(&_authenticationTimeout))
      .setLongDescription(R"(This option is only necessary if you use an
external authentication system like LDAP.)");

  options
      ->addOption(
          "--server.session-timeout",
          "The lifetime for tokens (in seconds) that can be obtained from "
          "the `POST /_open/auth` endpoint. Used by the web interface "
          "for JWT-based sessions.",
          new DoubleParameter(&_sessionTimeout, /*base*/ 1.0, /*minValue*/ 1.0,
                              /*maxValue*/ std::numeric_limits<double>::max(),
                              /*minInclusive*/ false),
          arangodb::options::makeFlags(
              arangodb::options::Flags::DefaultNoComponents,
              arangodb::options::Flags::OnCoordinator,
              arangodb::options::Flags::OnSingle))
      .setIntroducedIn(30900)
      .setLongDescription(R"(The web interface uses JWT for authentication.
However, the session are renewed automatically as long as you regularly interact
with the web interface in your browser. You are not logged out while actively
using it.)");

  options
      ->addOption("--server.local-authentication",
                  "Whether to use ArangoDB's built-in authentication system.",
                  new BooleanParameter(&_localAuthentication),
                  arangodb::options::makeFlags(
                      arangodb::options::Flags::DefaultNoComponents,
                      arangodb::options::Flags::OnCoordinator,
                      arangodb::options::Flags::OnSingle))
      .setLongDescription(R"(If you set this option to `false`, only an
external authentication system like LDAP is used. If set to `true`, also use
the built-in system which uses the `_users` system collection.)");

  options
      ->addOption("--server.authentication-system-only",
                  "Use HTTP authentication only for requests to /_api and "
                  "/_admin endpoints.",
                  new BooleanParameter(&_authenticationSystemOnly))
      .setLongDescription(R"(If you set this option to `true`, then HTTP
authentication is only required for requests going to URLs starting with `/_`,
but not for other endpoints. You can thus use this option to expose custom APIs
of Foxx microservices without HTTP authentication to the outside world, but
prevent unauthorized access of ArangoDB APIs and the admin interface.

Note that checking the URL is performed after any database name prefix has been
removed. That means, if the request URL is `/_db/_system/myapp/myaction`, the
URL `/myapp/myaction` is checked for the `/_` prefix.

Authentication still needs to be enabled for the server via
`--server.authentication` in order for HTTP authentication to be forced for the
ArangoDB APIs and the web interface. Only setting
`--server.authentication-system-only` is not enough.)");

#ifdef ARANGODB_HAVE_DOMAIN_SOCKETS
  options
      ->addOption(
          "--server.authentication-unix-sockets",
          "Whether to use authentication for requests via UNIX domain sockets.",
          new BooleanParameter(&_authenticationUnixSockets),
          arangodb::options::makeFlags(arangodb::options::Flags::DefaultNoOs,
                                       arangodb::options::Flags::OsLinux,
                                       arangodb::options::Flags::OsMac))
      .setLongDescription(R"(If you set this option to `false`, authentication
for requests coming in via UNIX domain sockets is turned off on the server-side.
Clients located on the same host as the ArangoDB server can use UNIX domain
sockets to connect to the server without authentication. Requests coming in by
other means (e.g. TCP/IP) are not affected by this option.)");
#endif

  options
      ->addOption("--server.jwt-secret",
                  "The secret to use when doing JWT authentication.",
                  new StringParameter(&_jwtSecretProgramOption))
      .setDeprecatedIn(30322)
      .setDeprecatedIn(30402);

  options
      ->addOption("--server.jwt-secret-keyfile",
                  "A file containing the JWT secret to use when doing JWT "
                  "authentication.",
                  new StringParameter(&_jwtSecretKeyfileProgramOption))
      .setLongDescription(R"(ArangoDB uses JSON Web Tokens to authenticate
requests. Using this option lets you specify a JWT secret stored in a file.
The secret must be at most 64 bytes long.

**Warning**: Avoid whitespace characters in the secret because they may get
trimmed, leading to authentication problems:
- Character Tabulation (`\t`, U+0009)
- End of Line (`\n`, U+000A)
- Line Tabulation (`\v`, U+000B)
- Form Feed (`\f`, U+000C)
- Carriage Return (`\r`, U+000D)
- Space (U+0020)
- Next Line (U+0085)
- No-Nreak Space (U+00A0)

In single server setups, ArangoDB generates a secret if none is specified.

In cluster deployments which have authentication enabled, a secret must
be set consistently across all cluster nodes so they can talk to each other.

ArangoDB also supports an `--server.jwt-secret` option to pass the secret
directly (without a file). However, this is discouraged for security
reasons.

You can reload JWT secrets from disk without restarting the server or the nodes
of a cluster deployment via the `POST /_admin/server/jwt` HTTP API endpoint.
You can use this feature to roll out new JWT secrets throughout a cluster.)");

  options
      ->addOption(
          "--server.jwt-secret-folder",
          "A folder containing one or more JWT secret files to use for JWT "
          "authentication.",
          new StringParameter(&_jwtSecretFolderProgramOption),
          arangodb::options::makeDefaultFlags(
              arangodb::options::Flags::Enterprise))
      .setLongDescription(R"(Files are sorted alphabetically, the first secret
is used for signing + verifying JWT tokens (_active_ secret), and all other
secrets are only used to validate incoming JWT tokens (_passive_ secrets).
Only one secret needs to verify a JWT token for it to be accepted.

You can reload JWT secrets from disk without restarting the server or the nodes
of a cluster deployment via the `POST /_admin/server/jwt` HTTP API endpoint.
You can use this feature to roll out new JWT secrets throughout a cluster.)");
}

void AuthenticationFeature::validateOptions(
    std::shared_ptr<ProgramOptions> options) {
  if (!_jwtSecretKeyfileProgramOption.empty() &&
      !_jwtSecretFolderProgramOption.empty()) {
    LOG_TOPIC("d3515", FATAL, Logger::STARTUP)
        << "please specify either '--server.jwt-"
           "secret-keyfile' or '--server.jwt-secret-folder' but not both.";
    FATAL_ERROR_EXIT();
  }

  if (!_jwtSecretKeyfileProgramOption.empty() ||
      !_jwtSecretFolderProgramOption.empty()) {
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
      _userManager = std::make_unique<auth::UserManager>(
          server(), std::make_unique<LdapAuthenticationHandler>(
                        server().getFeature<LdapFeature>()));
    }
#endif
    if (_userManager == nullptr) {
      _userManager = std::make_unique<auth::UserManager>(server());
    }

    TRI_ASSERT(_userManager != nullptr);
  } else {
    LOG_TOPIC("713c0", DEBUG, Logger::AUTHENTICATION)
        << "Not creating user manager";
  }

  TRI_ASSERT(_authCache == nullptr);
  _authCache = std::make_unique<auth::TokenCache>(_userManager.get(),
                                                  _authenticationTimeout);

  if (_jwtSecretProgramOption.empty()) {
    LOG_TOPIC("43396", INFO, Logger::AUTHENTICATION)
        << "Jwt secret not specified, generating...";
    uint16_t m = 254;
    for (size_t i = 0; i < _maxSecretLength; i++) {
      _jwtSecretProgramOption +=
          static_cast<char>(1 + RandomGenerator::interval(m));
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

#ifdef USE_ENTERPRISE
/// verification only secrets
std::pair<std::string, std::vector<std::string>>
AuthenticationFeature::jwtSecrets() const {
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
    std::string contents =
        basics::FileUtils::slurp(_jwtSecretKeyfileProgramOption);
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
                              if (file.size() >= 4 &&
                                  file.substr(file.size() - 4, 4) == ".tmp") {
                                return true;
                              }
                              auto p = basics::FileUtils::buildFilename(
                                  _jwtSecretFolderProgramOption, file);
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
    auto p =
        basics::FileUtils::buildFilename(_jwtSecretFolderProgramOption, file);
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
