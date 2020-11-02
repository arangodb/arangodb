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
/// @author Dr. Frank Celler
////////////////////////////////////////////////////////////////////////////////

#include "ClientFeature.h"

#include "ApplicationFeatures/ApplicationServer.h"
#include "ApplicationFeatures/CommunicationFeaturePhase.h"
#include "ApplicationFeatures/GreetingsFeaturePhase.h"
#include "Basics/FileUtils.h"
#include "Basics/StaticStrings.h"
#include "Basics/application-exit.h"
#include "Endpoint/Endpoint.h"
#include "Logger/Logger.h"
#include "ProgramOptions/ProgramOptions.h"
#include "ProgramOptions/Section.h"
#include "Shell/ConsoleFeature.h"
#include "SimpleHttpClient/GeneralClientConnection.h"
#include "SimpleHttpClient/SimpleHttpClient.h"
#include "Ssl/ssl-helper.h"

#include <chrono>
#include <iostream>
#include <thread>

using namespace arangodb::application_features;
using namespace arangodb::httpclient;
using namespace arangodb::options;

namespace arangodb {

ClientFeature::ClientFeature(application_features::ApplicationServer& server,
                             bool allowJwtSecret, double connectionTimeout, double requestTimeout)
    : HttpEndpointProvider(server, "Client"),
      _databaseName(StaticStrings::SystemDatabase),
      _endpoint(Endpoint::defaultEndpoint(Endpoint::TransportType::HTTP)),
      _username("root"),
      _password(""),
      _jwtSecret(""),
      _connectionTimeout(connectionTimeout),
      _requestTimeout(requestTimeout),
      _maxPacketSize(1024 * 1024 * 1024),
      _sslProtocol(TLS_V12),
      _retries(DEFAULT_RETRIES),
      _authentication(true),
      _askJwtSecret(false),
      _allowJwtSecret(allowJwtSecret),
      _warn(false),
      _warnConnect(true),
      _haveServerPassword(false),
      _forceJson(false)
#if _WIN32
      ,
      _codePage(65001),  // default to UTF8
      _originalCodePage(UINT16_MAX)
#endif
{
  setOptional(true);
  requiresElevatedPrivileges(false);
  startsAfter<CommunicationFeaturePhase>();
  startsAfter<GreetingsFeaturePhase>();
}

void ClientFeature::collectOptions(std::shared_ptr<ProgramOptions> options) {
  options->addSection("server", "Configure a connection to the server");

  options->addOption("--server.database", "database name to use when connecting",
                     new StringParameter(&_databaseName));

  options->addOption("--server.authentication",
                     "require authentication credentials when connecting (does "
                     "not affect the server-side authentication settings)",
                     new BooleanParameter(&_authentication));

  options->addOption("--server.username", "username to use when connecting",
                     new StringParameter(&_username));

  options->addOption(
      "--server.endpoint",
      "endpoint to connect to. Use 'none' to start without a server. "
      "Use http+ssl:// or vst+ssl:// as schema to connect to an SSL-secured "
      "server endpoint, otherwise http+tcp://, vst+tcp:// or unix://",
      new StringParameter(&_endpoint));

  options->addOption("--server.password",
                     "password to use when connecting. If not specified and "
                     "authentication is required, the user will be prompted "
                     "for a password",
                     new StringParameter(&_password));

  options->addOption("--server.force-json",
                     "force to not use VelocyPack for easier debugging",
                     new BooleanParameter(&_forceJson),
                     arangodb::options::makeDefaultFlags(arangodb::options::Flags::Hidden))
    .setIntroducedIn(30600);

  if (_allowJwtSecret) {
    // currently the option is only present for arangosh, but none
    // of the other client tools
    options->addOption(
        "--server.ask-jwt-secret",
        "if this option is specified, the user will be prompted "
        "for a JWT secret. This option is not compatible with "
        "--server.username or --server.password. If specified, it will be used "
        "for all "
        "connections - even when a new connection to another server is "
        "created",
        new BooleanParameter(&_askJwtSecret),
        arangodb::options::makeDefaultFlags(arangodb::options::Flags::Hidden));

    options->addOption("--server.jwt-secret-keyfile",
                       "if this option is specified, the jwt secret will be loaded "
                       "from the given file. This option is not compatible with "
                       "--server.ask-jwt-secret, --server.username or --server.password. "
                       "If specified, it will be used for all "
                       "connections - even when a new connection to another server is "
                       "created",
                       new StringParameter(&_jwtSecretFile),
                       arangodb::options::makeDefaultFlags(arangodb::options::Flags::Hidden));
  }

  options->addOption("--server.connection-timeout",
                     "connection timeout in seconds",
                     new DoubleParameter(&_connectionTimeout));

  options->addOption("--server.request-timeout", "request timeout in seconds",
                     new DoubleParameter(&_requestTimeout));

  // note: the max-packet-size is used for all client tools that use the
  // SimpleHttpClient. fuerte does not use this
  options->addOption(
      "--server.max-packet-size",
      "maximum packet size (in bytes) for client/server communication",
      new UInt64Parameter(&_maxPacketSize),
      arangodb::options::makeDefaultFlags(arangodb::options::Flags::Hidden));

  std::unordered_set<uint64_t> const sslProtocols = availableSslProtocols();

  options->addSection("ssl", "Configure SSL communication");
  options->addOption("--ssl.protocol", availableSslProtocolsDescription(),
                     new DiscreteValuesParameter<UInt64Parameter>(&_sslProtocol, sslProtocols));
#if _WIN32
  options->addOption("--console.code-page",
                     "Windows code page to use; defaults to UTF8",
                     new UInt16Parameter(&_codePage),
                     arangodb::options::makeFlags(arangodb::options::Flags::DefaultNoOs, arangodb::options::Flags::OsWindows, arangodb::options::Flags::Hidden));
#endif
}

void ClientFeature::validateOptions(std::shared_ptr<ProgramOptions> options) {
  if (_sslProtocol == SslProtocol::SSL_V2) {
    LOG_TOPIC("64f4f", FATAL, arangodb::Logger::SSL)
        << "SSLv2 is not supported any longer because of security "
           "vulnerabilities in this protocol";
    FATAL_ERROR_EXIT();
  }

  // if a username is specified explicitly, assume authentication is desired
  if (options->processingResult().touched("server.username")) {
    _authentication = true;
  }

  if (_askJwtSecret) {
    _authentication = false;
  }

  bool hasJwtSecretFile = !_jwtSecretFile.empty();

  // check timeouts
  if (_connectionTimeout < 0.0) {
    LOG_TOPIC("81598", FATAL, arangodb::Logger::FIXME)
        << "invalid value for --server.connect-timeout, must be >= 0";
    FATAL_ERROR_EXIT();
  } else if (_connectionTimeout == 0.0) {
    _connectionTimeout = LONG_TIMEOUT;
  }

  if (_requestTimeout < 0.0) {
    LOG_TOPIC("fb847", FATAL, arangodb::Logger::FIXME)
        << "invalid value for --server.request-timeout, must be positive";
    FATAL_ERROR_EXIT();
  } else if (_requestTimeout == 0.0) {
    _requestTimeout = LONG_TIMEOUT;
  }

  if (_maxPacketSize < 1 * 1024 * 1024) {
    LOG_TOPIC("f7793", FATAL, arangodb::Logger::FIXME)
        << "invalid value for --server.max-packet-size, must be at least 1 MB";
    FATAL_ERROR_EXIT();
  }

  // username must be non-empty
  if (_username.empty()) {
    LOG_TOPIC("fa58c", FATAL, arangodb::Logger::FIXME)
        << "no value specified for --server.username";
    FATAL_ERROR_EXIT();
  }

  _haveServerPassword = !options->processingResult().touched("server.password");

  if ((_askJwtSecret || hasJwtSecretFile) && options->processingResult().touched("server.password")) {
    LOG_TOPIC("65475", FATAL, arangodb::Logger::FIXME)
        << "cannot specify both --server.password and jwt secret source";
    FATAL_ERROR_EXIT();
  }

  if ((_askJwtSecret || hasJwtSecretFile) && options->processingResult().touched("server.username")) {
    LOG_TOPIC("9d886", FATAL, arangodb::Logger::FIXME)
        << "cannot specify both --server.username and jwt secret source";
    FATAL_ERROR_EXIT();
  }

  if (_askJwtSecret && hasJwtSecretFile) {
    LOG_TOPIC("aeaeb", FATAL, arangodb::Logger::FIXME)
        << "multiple jwt secret sources specified";
    FATAL_ERROR_EXIT();
  }
  
  if (!_endpoint.empty() &&
      (_endpoint != "none") &&
      (_endpoint != Endpoint::defaultEndpoint(Endpoint::TransportType::HTTP))) {
    std::unique_ptr<Endpoint> endpoint(Endpoint::clientFactory(_endpoint));
    if (endpoint != nullptr && endpoint->isBroadcastBind()) {
      LOG_TOPIC("701fb", FATAL, arangodb::Logger::FIXME)
        << "invalid value for --server.endpoint ('" << _endpoint <<
        "') - 0.0.0.0 and :: are only allowed for servers binding - not for clients connecting." <<
        " Choose an IP address of your machine instead." <<
        " See https://en.wikipedia.org/wiki/0.0.0.0 for more details.";
      FATAL_ERROR_EXIT();
    }
  }
  SimpleHttpClientParams::setDefaultMaxPacketSize(_maxPacketSize);
}

void ClientFeature::readPassword() {
  std::this_thread::sleep_for(std::chrono::milliseconds(10));

  try {
    ConsoleFeature& console = server().getFeature<ConsoleFeature>();

    if (console.isEnabled()) {
      _password = console.readPassword("Please specify a password: ");
      return;
    }
  } catch (...) {
  }

  std::cout << "Please specify a password: " << std::flush;
  _password = ConsoleFeature::readPassword();
  std::cout << std::endl << std::flush;
}

void ClientFeature::readJwtSecret() {
  std::this_thread::sleep_for(std::chrono::milliseconds(10));

  try {
    ConsoleFeature& console = server().getFeature<ConsoleFeature>();

    if (console.isEnabled()) {
      _jwtSecret = console.readPassword("Please specify the JWT secret: ");
      return;
    }
  } catch (...) {
  }

  std::cout << "Please specify the JWT secret: " << std::flush;
  _jwtSecret = ConsoleFeature::readPassword();
  std::cout << std::endl << std::flush;
}

void ClientFeature::loadJwtSecretFile() {
  try {
      // Note that the secret is trimmed for whitespace, because whitespace
      // at the end of a file can easily happen. We do not base64-encode,
      // though, so the bytes count as given. Zero bytes might be a problem
      // here.
      _jwtSecret = basics::StringUtils::trim(
          basics::FileUtils::slurp(_jwtSecretFile),
          " \t\n\r");
    } catch (std::exception const& ex) {
      LOG_TOPIC("aeaec", FATAL, Logger::STARTUP)
          << "unable to read content of jwt-secret file '"
          << _jwtSecretFile << "': " << ex.what()
          << ". please make sure the file/directory is readable for the "
             "arangod process and user";
      FATAL_ERROR_EXIT();
    }
}

void ClientFeature::prepare() {
  if (!isEnabled()) {
    return;
  }

  if (_askJwtSecret) {
    // ask for a jwt secret
    readJwtSecret();
  } else if (!_jwtSecretFile.empty()) {
    loadJwtSecretFile();
  } else if (_authentication && _haveServerPassword) {
    // ask for a password
    readPassword();
  }
}

std::unique_ptr<GeneralClientConnection> ClientFeature::createConnection() {
  return createConnection(_endpoint);
}

std::unique_ptr<GeneralClientConnection> ClientFeature::createConnection(std::string const& definition) {
  std::unique_ptr<Endpoint> endpoint(Endpoint::clientFactory(definition));

  if (endpoint.get() == nullptr) {
    LOG_TOPIC("701fa", ERR, arangodb::Logger::FIXME)
        << "invalid value for --server.endpoint ('" << definition << "')";
    THROW_ARANGO_EXCEPTION(TRI_ERROR_BAD_PARAMETER);
  }

  std::unique_ptr<GeneralClientConnection> connection(
      GeneralClientConnection::factory(server(), endpoint, _requestTimeout,
                                       _connectionTimeout, _retries, _sslProtocol));

  return connection;
}

std::unique_ptr<SimpleHttpClient> ClientFeature::createHttpClient() const {
  return createHttpClient(_endpoint);
}

std::unique_ptr<SimpleHttpClient> ClientFeature::createHttpClient(std::string const& definition) const {
  return createHttpClient(definition, SimpleHttpClientParams(_requestTimeout, _warn));
}

std::unique_ptr<httpclient::SimpleHttpClient> ClientFeature::createHttpClient(
    std::string const& definition, SimpleHttpClientParams const& params) const {
  std::unique_ptr<Endpoint> endpoint(Endpoint::clientFactory(definition));

  if (endpoint.get() == nullptr) {
    LOG_TOPIC("2fac8", ERR, arangodb::Logger::FIXME)
        << "invalid value for --server.endpoint ('" << definition << "')";
    THROW_ARANGO_EXCEPTION(TRI_ERROR_BAD_PARAMETER);
  }

  std::unique_ptr<GeneralClientConnection> connection(
      GeneralClientConnection::factory(server(), endpoint, _requestTimeout,
                                       _connectionTimeout, _retries, _sslProtocol));

  return std::make_unique<SimpleHttpClient>(connection, params);
}

std::vector<std::string> ClientFeature::httpEndpoints() {
  std::string http = Endpoint::uriForm(_endpoint);

  if (http.empty()) {
    return {};
  }

  return {http};
}

void ClientFeature::start() {
#if _WIN32
  _originalCodePage = GetConsoleOutputCP();
  if (IsValidCodePage(_codePage)) {
    SetConsoleOutputCP(_codePage);
  }
#endif
}

void ClientFeature::stop() {
#if _WIN32
  if (IsValidCodePage(_originalCodePage)) {
    SetConsoleOutputCP(_originalCodePage);
  }
#endif
}

std::string ClientFeature::buildConnectedMessage(
    std::string const& endpointSpecification,
    std::string const& version,
    std::string const& role,
    std::string const& mode,
    std::string const& databaseName,
    std::string const& user
) {
  return std::string("Connected to ArangoDB '") + endpointSpecification +
         ((version.empty() || version == "arango") ? "" : ", version: " + version) +
         (role.empty() ? "" : " [" + role + ", " + mode + "]") +
         ", database: '" + databaseName + "', username: '" + user + "'";
}

int ClientFeature::runMain(int argc, char* argv[],
                           std::function<int(int argc, char* argv[])> const& mainFunc) {
  try {
    return mainFunc(argc, argv);
  } catch (std::exception const& ex) {
    LOG_TOPIC("5b00f", ERR, arangodb::Logger::FIXME)
        << argv[0] << " terminated because of an unhandled exception: " << ex.what();
    return EXIT_FAILURE;
  } catch (...) {
    LOG_TOPIC("98466", ERR, arangodb::Logger::FIXME)
        << argv[0] << " terminated because of an unhandled exception of unknown type";
    return EXIT_FAILURE;
  }
}

}  // namespace arangodb
