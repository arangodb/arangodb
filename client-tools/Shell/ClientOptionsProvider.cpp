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

#include "Shell/ClientOptionsProvider.h"

#include "Basics/Exceptions.h"
#include "Basics/FileUtils.h"
#include "Basics/files.h"
#include "Basics/application-exit.h"
#include "Endpoint/Endpoint.h"
#include "Logger/LogMacros.h"
#include "Logger/Logger.h"
#include "Logger/LoggerStream.h"
#include "ProgramOptions/Parameters.h"
#include "ProgramOptions/ProgramOptions.h"
#include "Ssl/ssl-helper.h"

namespace {
constexpr double LONG_TIMEOUT = 86400.0;
}  // anonymous namespace

namespace arangodb {

using namespace arangodb::options;

void ClientOptionsProvider::declareOptions(std::shared_ptr<ProgramOptions> opts,
                                           ClientFeatureOptions& options) {
  opts->addSection("server", "server connection");

  opts->addOption("--server.database",
                  "The database name to use when connecting.",
                  new StringParameter(&options.databaseName));

  opts->addOption("--server.authentication",
                  "Require authentication credentials when connecting (does "
                  "not affect the server-side authentication settings).",
                  new BooleanParameter(&options.authentication));

  opts->addOption(
      "--server.username",
      "The username to use when connecting.\nIf you want to specify an access "
      "token as the password, set the user name as encoded in the token.",
      new StringParameter(&options.username));

  std::string basename = TRI_Basename(opts->progname());
  bool isArangosh = basename == "arangosh";

  char const* endpointHelp;
  if (isArangosh) {
    // this option is only available in arangosh
    endpointHelp =
        "The endpoint to connect to. Use 'none' to start without a server. "
        "Use http+ssl:// as schema to connect to an SSL-secured "
        "server endpoint, otherwise http+tcp:// or unix://.";
  } else {
    endpointHelp =
        "The endpoint to connect to. Use 'none' to start without a server. "
        "Use http+ssl:// as schema to connect to an SSL-secured "
        "server endpoint, otherwise http+tcp:// or unix://";
  }

  auto& opt = opts->addOption(
      "--server.endpoint", endpointHelp,
      new VectorParameter<StringParameter>(&options.endpoints),
      arangodb::options::makeFlags(Flags::FlushOnFirst, Flags::Default));
  if (isArangosh) {
    opt.setLongDescription(R"(You can use `--server.endpoint none` to start
arangosh without connecting to a server.)");
  }

  opts->addOption(
      "--server.password",
      "The password or access token to use when connecting. If not specified "
      "and authentication is required, you are prompted for a password.\n"
      "In startup options, you can wrap the names of environment variables "
      "in at signs to use their value, like @ARANGO_PASSWORD@. This helps to "
      "expose the password less, like to the process list. "
      "Literal @ need to be escaped as @@.",
      new StringParameter(&options.password));

  if (isArangosh) {
    // this option is only available in arangosh
    opts->addOption("--server.force-json",
                    "Force to not use VelocyPack for easier debugging.",
                    new BooleanParameter(&options.forceJson),
                    arangodb::options::makeDefaultFlags(
                        arangodb::options::Flags::Uncommon));
  }

  if (options.allowJwtSecret) {
    // currently the option is only present for arangosh, but none
    // of the other client tools
    opts->addOption(
        "--server.ask-jwt-secret",
        "If enabled, you are prompted for a JWT secret. This option is not "
        "compatible with --server.username and --server.password. "
        "If specified, it is used for all connections - even if a new "
        "connection to another server is created.",
        new BooleanParameter(&options.askJwtSecret),
        arangodb::options::makeDefaultFlags(
            arangodb::options::Flags::Uncommon));

    opts->addOption(
        "--server.jwt-secret-keyfile",
        "If enabled, the JWT secret is loaded from the given file. This option "
        "is not compatible with --server.ask-jwt-secret, --server.username and "
        "--server.password. If specified, it is used for all connections - "
        "even if a new connection to another server is created.",
        new StringParameter(&options.jwtSecretFile),
        arangodb::options::makeDefaultFlags(
            arangodb::options::Flags::Uncommon));

    opts->addOption(
        "--server.jwt-token",
        "If enabled, the JWT token is used directly for authentication. You "
        "can either "
        "specify the token directly or set the value to \"-\" to get prompted "
        "for the token to not leak the token to the process list. This "
        "option is not compatible with --server.ask-jwt-secret, "
        "--server.jwt-secret-keyfile, --server.username and --server.password. "
        "If specified, it is used for all connections - even if a new "
        "connection to another server is created.",
        new StringParameter(&options.jwtToken));
  }

  opts->addOption("--server.connection-timeout",
                  "The connection timeout (in seconds).",
                  new DoubleParameter(&options.connectionTimeout));

  opts->addOption("--server.request-timeout",
                  "The request timeout (in seconds).",
                  new DoubleParameter(&options.requestTimeout));

  opts->addOption(
      "--server.jwt-renewal-threshold",
      "The time (in seconds) before JWT token expiry to trigger "
      "automatic renewal. Default is 300 seconds (5 minutes).",
      new DoubleParameter(&options.jwtRenewalThreshold),
      arangodb::options::makeDefaultFlags(arangodb::options::Flags::Uncommon));

  // note: the max-packet-size is used for all client tools that use the
  // SimpleHttpClient. fuerte does not use this
  opts->addOption(
      "--server.max-packet-size",
      "The maximum packet size (in bytes) for client/server communication.",
      new UInt64Parameter(&options.maxPacketSize),
      arangodb::options::makeDefaultFlags(arangodb::options::Flags::Uncommon));

  std::unordered_set<uint64_t> const sslProtocols = availableSslProtocols();

  opts->addSection("tls", "TLS communication");
  opts->addOldOption("--ssl.protocol", "--tls.protocol");
  opts->addOption("--tls.protocol", availableSslProtocolsDescription(),
                  new DiscreteValuesParameter<UInt64Parameter>(
                      &options.sslProtocol, sslProtocols));

  opts->addOption(
          "--compress-transfer",
          "Compress data for transport between " + basename + " and server.",
          new BooleanParameter(&options.compressTransfer))
      .setIntroducedIn(31200)
      .setLongDescription(R"(This option enables transport compression for data
received by an ArangoDB server.)");

  opts->addOption("--compress-request-threshold",
                  "The HTTP request body size from which on requests are "
                  "transparently compressed when sending them to the server.",
                  new UInt64Parameter(&options.compressRequestThreshold))
      .setIntroducedIn(31200)
      .setLongDescription(
          R"(Automatically compress outgoing HTTP requests
with the deflate compression format. Compression will only happen for
HTTP/1.1 and HTTP/2 connections, if the size of the uncompressed request
body exceeds the threshold value controlled by this startup option,
and if the request body size after compression is less than the original
request body size.
Using the value 0 disables the automatic request compression.)");
}

void ClientOptionsProvider::validateOptions(
    std::shared_ptr<ProgramOptions> opts, ClientFeatureOptions& options) {
  if (options.endpoints.size() > options.maxNumEndpoints) {
    // this is the case if we have more endpoints than allowed.
    // in versions before 3.9, it was allowed to specify `--server.endpoint`
    // multiple times, and if this was done, only the last provided endpoint
    // was used. to keep backward-compatibility, we now emulate this
    // behavior here.
    TRI_ASSERT(options.maxNumEndpoints == 1);
    std::string selectedEndpoint = options.endpoints.back();
    options.endpoints = {std::move(selectedEndpoint)};
  }

  // if a username is specified explicitly, assume authentication is desired
  if (opts->processingResult().touched("server.username")) {
    options.authentication = true;
  }

  if (options.askJwtSecret) {
    options.authentication = false;
  }

  bool hasJwtSecretFile = !options.jwtSecretFile.empty();
  bool hasJwtToken = !options.jwtToken.empty();

  if (hasJwtToken || hasJwtSecretFile) {
    options.authentication = false;
  }

  if (options.connectionTimeout < 0.0) {
    LOG_TOPIC("81598", FATAL, arangodb::Logger::FIXME)
        << "invalid value for --server.connect-timeout, must be >= 0";
    FATAL_ERROR_EXIT();
  }
  if (options.connectionTimeout == 0.0) {
    options.connectionTimeout = LONG_TIMEOUT;
  }

  if (options.requestTimeout < 0.0) {
    LOG_TOPIC("fb847", FATAL, arangodb::Logger::FIXME)
        << "invalid value for --server.request-timeout, must be positive";
    FATAL_ERROR_EXIT();
  }
  if (options.requestTimeout == 0.0) {
    options.requestTimeout = LONG_TIMEOUT;
  }

  if (options.maxPacketSize < 1 * 1024 * 1024) {
    LOG_TOPIC("f7793", FATAL, arangodb::Logger::FIXME)
        << "invalid value for --server.max-packet-size, must be at least 1 MB";
    FATAL_ERROR_EXIT();
  }

  if (options.username.empty()) {
    LOG_TOPIC("fa58c", FATAL, arangodb::Logger::FIXME)
        << "no value specified for --server.username";
    FATAL_ERROR_EXIT();
  }

  if ((options.askJwtSecret || hasJwtSecretFile) &&
      opts->processingResult().touched("server.password")) {
    LOG_TOPIC("65475", FATAL, arangodb::Logger::FIXME)
        << "cannot specify both --server.password and jwt secret source";
    FATAL_ERROR_EXIT();
  }
  options.haveServerPassword =
      !opts->processingResult().touched("server.password");

  if ((options.askJwtSecret || hasJwtSecretFile) &&
      opts->processingResult().touched("server.username")) {
    LOG_TOPIC("9d886", FATAL, arangodb::Logger::FIXME)
        << "cannot specify both --server.username and jwt secret source";
    FATAL_ERROR_EXIT();
  }

  if (options.askJwtSecret && hasJwtSecretFile) {
    LOG_TOPIC("aeaeb", FATAL, arangodb::Logger::FIXME)
        << "multiple jwt secret sources specified";
    FATAL_ERROR_EXIT();
  }

  if (hasJwtToken && opts->processingResult().touched("server.password")) {
    LOG_TOPIC("65476", FATAL, arangodb::Logger::FIXME)
        << "cannot specify both --server.password and --server.jwt-token";
    FATAL_ERROR_EXIT();
  }

  if (hasJwtToken && opts->processingResult().touched("server.username")) {
    LOG_TOPIC("9d887", FATAL, arangodb::Logger::FIXME)
        << "cannot specify both --server.username and --server.jwt-token";
    FATAL_ERROR_EXIT();
  }

  if (hasJwtToken && options.askJwtSecret) {
    LOG_TOPIC("aeaed", FATAL, arangodb::Logger::FIXME)
        << "cannot specify both --server.ask-jwt-secret and --server.jwt-token";
    FATAL_ERROR_EXIT();
  }

  if (hasJwtToken && hasJwtSecretFile) {
    LOG_TOPIC("aeaee", FATAL, arangodb::Logger::FIXME)
        << "cannot specify both --server.jwt-secret-keyfile and "
           "--server.jwt-token";
    FATAL_ERROR_EXIT();
  }

  if (!options.endpoints.empty()) {
    std::for_each(
        options.endpoints.begin(), options.endpoints.end(),
        [](auto const& endpoint) {
          if (!endpoint.empty() && (endpoint != "none") &&
              (endpoint != Endpoint::defaultEndpoint())) {
            std::unique_ptr<Endpoint> ep(Endpoint::clientFactory(endpoint));
            if (ep != nullptr && ep->isBroadcastBind()) {
              LOG_TOPIC("701fb", FATAL, arangodb::Logger::FIXME)
                  << "invalid value for --server.endpoint ('" << endpoint
                  << "') - 0.0.0.0 and :: are only allowed for servers binding "
                     "- not for clients connecting."
                  << " Choose an IP address of your machine instead."
                  << " See https://en.wikipedia.org/wiki/0.0.0.0 for more "
                     "details.";
              FATAL_ERROR_EXIT();
            }
          }
        });
  }
}

}  // namespace arangodb
