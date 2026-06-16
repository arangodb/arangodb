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

#include "AuthenticationOptionsProvider.h"

#include "Basics/application-exit.h"
#include "Logger/LogMacros.h"
#include "Logger/Logger.h"
#include "ProgramOptions/Parameters.h"
#include "ProgramOptions/ProgramOptions.h"

#include <limits>

namespace arangodb {

using namespace arangodb::options;

void AuthenticationOptionsProvider::declareOptions(
    std::shared_ptr<ProgramOptions> opts, AuthenticationOptions& options) {
  opts->addObsoleteOption(
      "server.disable-authentication",
      "Whether to use authentication for all client requests.", false);
  opts->addObsoleteOption(
      "server.disable-authentication-unix-sockets",
      "Whether to use authentication for requests via UNIX domain sockets.",
      false);
  opts->addOldOption("server.authenticate-system-only",
                     "server.authentication-system-only");

  opts->addOption("--server.authentication",
                  "Whether to use authentication for all client requests.",
                  new BooleanParameter(&options.active))
      .setLongDescription(R"(You can set this option to `false` to turn off
authentication on the server-side, so that all clients can execute any action
without authorization and privilege checks. You should only do this if you bind
the server to `localhost` to not expose it to the public internet)");

  opts->addOption("--server.authentication-timeout",
                  "The timeout for the authentication cache "
                  "(in seconds, 0 = indefinitely).",
                  new DoubleParameter(&options.authenticationTimeout));

  opts->addOption(
          "--server.session-timeout",
          "The lifetime for tokens (in seconds) that can be obtained from "
          "the `POST /_open/auth` endpoint. Used by the web interface "
          "for JWT-based sessions.",
          new DoubleParameter(&options.sessionTimeout, /*base*/ 1.0,
                              /*minValue*/ 1.0,
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

  opts->addOption(
          "--auth.minimal-jwt-expiry-time",
          "The minimal expiry time (in seconds) allowed for JWT tokens "
          "requested via the `POST /_open/auth` endpoint.",
          new DoubleParameter(&options.minimalJwtExpiryTime, /*base*/ 1.0,
                              /*minValue*/ 1.0,
                              /*maxValue*/ std::numeric_limits<double>::max(),
                              /*minInclusive*/ false),
          arangodb::options::makeFlags(
              arangodb::options::Flags::DefaultNoComponents,
              arangodb::options::Flags::OnCoordinator,
              arangodb::options::Flags::OnSingle))
      .setIntroducedIn(31206)
      .setLongDescription(R"(This option sets the minimum lifetime that can be
requested for JWT tokens via the `expiryTime` parameter in the `POST /_open/auth`
endpoint. Requests with expiry times below this value will be rejected.)");

  opts->addOption(
          "--auth.maximal-jwt-expiry-time",
          "The maximal expiry time (in seconds) allowed for JWT tokens "
          "requested via the `POST /_open/auth` endpoint.",
          new DoubleParameter(&options.maximalJwtExpiryTime, /*base*/ 1.0,
                              /*minValue*/ 1.0,
                              /*maxValue*/ std::numeric_limits<double>::max(),
                              /*minInclusive*/ false),
          arangodb::options::makeFlags(
              arangodb::options::Flags::DefaultNoComponents,
              arangodb::options::Flags::OnCoordinator,
              arangodb::options::Flags::OnSingle))
      .setIntroducedIn(31206)
      .setLongDescription(R"(This option sets the maximum lifetime that can be
requested for JWT tokens via the `expiryTime` parameter in the `POST /_open/auth`
endpoint. Requests with expiry times above this value will be rejected.)");

  opts->addOption("--server.external-rbac-service",
                  "Enable role-based access control (RBAC) and set the "
                  "external RBAC service endpoint. If the string is empty, "
                  "RBAC is disabled.",
                  new StringParameter(&options.externalRBACservice),
                  arangodb::options::makeFlags(
                      arangodb::options::Flags::DefaultNoComponents,
                      arangodb::options::Flags::OnCoordinator,
                      arangodb::options::Flags::OnSingle,
                      arangodb::options::Flags::Uncommon,
                      arangodb::options::Flags::Experimental))
      .setLongDescription(
          R"(When set to a non-empty string, this must be the HTTP or HTTPS
endpoint of an external RBAC authorization service for use by Coordinators and
single servers. In this case, all requests use role-based-access-control
(RBAC) via the specified service for authorization decisions. When set to an
empty string, RBAC is disabled and instead the old permission system is used.)");

  opts->addObsoleteOption(
      "--server.local-authentication",
      "Whether to use ArangoDB's built-in authentication system.", false);

  opts->addOption("--server.authentication-system-only",
                  "Use HTTP authentication only for requests to /_api and "
                  "/_admin endpoints.",
                  new BooleanParameter(&options.authenticationSystemOnly))
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
  opts->addOption(
          "--server.authentication-unix-sockets",
          "Whether to use authentication for requests via UNIX domain sockets.",
          new BooleanParameter(&options.authenticationUnixSockets),
          arangodb::options::makeFlags())
      .setLongDescription(R"(If you set this option to `false`, authentication
for requests coming in via UNIX domain sockets is turned off on the server-side.
Clients located on the same host as the ArangoDB server can use UNIX domain
sockets to connect to the server without authentication. Requests coming in by
other means (e.g. TCP/IP) are not affected by this option.)");
#endif

  opts->addOption("--server.jwt-secret",
                  "The secret to use when doing JWT authentication.",
                  new StringParameter(&options.jwtSecretProgramOption))
      .setDeprecatedIn(30322)
      .setDeprecatedIn(30402);

  opts->addOption("--server.jwt-secret-keyfile",
                  "A file containing the JWT secret to use when doing JWT "
                  "authentication.",
                  new StringParameter(&options.jwtSecretKeyfileProgramOption))
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
- No-Break Space (U+00A0)

In single server setups, ArangoDB generates a secret if none is specified.

In cluster deployments which have authentication enabled, a secret must
be set consistently across all cluster nodes so they can talk to each other.

ArangoDB also supports an `--server.jwt-secret` option to pass the secret
directly (without a file). However, this is discouraged for security
reasons.

You can reload JWT secrets from disk without restarting the server or the nodes
of a cluster deployment via the `POST /_admin/server/jwt` HTTP API endpoint.
You can use this feature to roll out new JWT secrets throughout a cluster.)");

  opts->addOption(
          "--server.jwt-secret-folder",
          "A folder containing one or more JWT secret files to use for JWT "
          "authentication.",
          new StringParameter(&options.jwtSecretFolderProgramOption))
      .setLongDescription(R"(Files are sorted alphabetically, the first secret
is used for signing + verifying JWT tokens (_active_ secret), and all other
secrets are only used to validate incoming JWT tokens (_passive_ secrets).
Only one secret needs to verify a JWT token for it to be accepted.

You can reload JWT secrets from disk without restarting the server or the nodes
of a cluster deployment via the `POST /_admin/server/jwt` HTTP API endpoint.
You can use this feature to roll out new JWT secrets throughout a cluster.)");
}

void AuthenticationOptionsProvider::validateOptions(
    std::shared_ptr<ProgramOptions> opts, AuthenticationOptions& options) {
  if (options.minimalJwtExpiryTime > options.maximalJwtExpiryTime) {
    LOG_TOPIC("a4b5c", FATAL, Logger::STARTUP)
        << "--auth.minimal-jwt-expiry-time (" << options.minimalJwtExpiryTime
        << ") must not be greater than --auth.maximal-jwt-expiry-time ("
        << options.maximalJwtExpiryTime << ")";
    FATAL_ERROR_EXIT();
  }

  if (!options.externalRBACservice.empty()) {
    if (!options.externalRBACservice.starts_with("http://") &&
        !options.externalRBACservice.starts_with("https://")) {
      LOG_TOPIC("1aaaf", FATAL, arangodb::Logger::AUTHENTICATION)
          << "--server.external-rbac-service must start with http:// or "
             "https://";
      FATAL_ERROR_EXIT();
    }
  }
}

}  // namespace arangodb
