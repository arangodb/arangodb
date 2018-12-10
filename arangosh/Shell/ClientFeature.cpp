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
/// @author Dr. Frank Celler
////////////////////////////////////////////////////////////////////////////////

#include "ClientFeature.h"

#include "ApplicationFeatures/ApplicationServer.h"
#include "Endpoint/Endpoint.h"
#include "Logger/Logger.h"
#include "ProgramOptions/ProgramOptions.h"
#include "ProgramOptions/Section.h"
#include "Shell/ConsoleFeature.h"
#include "SimpleHttpClient/GeneralClientConnection.h"
#include "SimpleHttpClient/SimpleHttpClient.h"
#include "Ssl/ssl-helper.h"

#include <iostream>
#include <thread>
#include <chrono>

using namespace arangodb::application_features;
using namespace arangodb::httpclient;
using namespace arangodb::options;

namespace arangodb {

ClientFeature::ClientFeature(
    application_features::ApplicationServer& server,
    bool allowJwtSecret,
    double connectionTimeout,
    double requestTimeout
)
    : ApplicationFeature(server, "Client"),
      _databaseName("_system"),
      _authentication(true),
      _askJwtSecret(false),
      _endpoint(Endpoint::defaultEndpoint(Endpoint::TransportType::HTTP)),
      _username("root"),
      _password(""),
      _jwtSecret(""),
      _connectionTimeout(connectionTimeout),
      _requestTimeout(requestTimeout),
      _maxPacketSize(1024 * 1024 * 1024),
      _sslProtocol(TLS_V12),
      _allowJwtSecret(allowJwtSecret),
      _retries(DEFAULT_RETRIES),
      _warn(false),
      _warnConnect(true),
      _haveServerPassword(false)
#if _WIN32
      ,_codePage(65001), // default to UTF8
      _originalCodePage(UINT16_MAX)
#endif
  {
  setOptional(true);
  requiresElevatedPrivileges(false);
  startsAfter("CommunicationPhase");
  startsAfter("GreetingsPhase");
}

void ClientFeature::collectOptions(std::shared_ptr<ProgramOptions> options) {
  options->addSection("server", "Configure a connection to the server");

  options->addOption("--server.database",
                     "database name to use when connecting",
                     new StringParameter(&_databaseName));

  options->addOption("--server.authentication",
                     "require authentication credentials when connecting (does "
                     "not affect the server-side authentication settings)",
                     new BooleanParameter(&_authentication));

  options->addOption("--server.username", "username to use when connecting",
                     new StringParameter(&_username));

  options->addOption(
      "--server.endpoint",
      "endpoint to connect to, use 'none' to start without a server",
      new StringParameter(&_endpoint));

  options->addOption("--server.password",
                     "password to use when connecting. If not specified and "
                     "authentication is required, the user will be prompted "
                     "for a password",
                     new StringParameter(&_password));

  if (_allowJwtSecret) {
    // currently the option is only present for arangosh, but none
    // of the other client tools 
    options->addOption(
        "--server.ask-jwt-secret",
        "if this option is specified, the user will be prompted "
        "for a JWT secret. This option is not compatible with "
        "--server.username or --server.password. If specified, it will be used for all "
        "connections - even when a new connection to another server is "
        "created",
        new BooleanParameter(&_askJwtSecret),
        arangodb::options::makeFlags(arangodb::options::Flags::Hidden));
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
      arangodb::options::makeFlags(arangodb::options::Flags::Hidden));

  std::unordered_set<uint64_t> const sslProtocols = availableSslProtocols();

  options->addSection("ssl", "Configure SSL communication");
  options->addOption("--ssl.protocol",
                     availableSslProtocolsDescription(),
                     new DiscreteValuesParameter<UInt64Parameter>(
                         &_sslProtocol, sslProtocols));
#if _WIN32
  options->addOption("--console.code-page", "Windows code page to use; defaults to UTF8",
                     new UInt16Parameter(&_codePage),
                     arangodb::options::makeFlags(arangodb::options::Flags::Hidden));
#endif
}

void ClientFeature::validateOptions(std::shared_ptr<ProgramOptions> options) {
  if (_sslProtocol == SslProtocol::SSL_V2) {
    LOG_TOPIC(FATAL, arangodb::Logger::SSL) << "SSLv2 is not supported any longer because of security vulnerabilities in this protocol";
    FATAL_ERROR_EXIT();
  }

  // if a username is specified explicitly, assume authentication is desired
  if (options->processingResult().touched("server.username")) {
    _authentication = true;
  }

  if (_askJwtSecret) {
    _authentication = false;
  }

  // check timeouts
  if (_connectionTimeout < 0.0) {
    LOG_TOPIC(FATAL, arangodb::Logger::FIXME) << "invalid value for --server.connect-timeout, must be >= 0";
    FATAL_ERROR_EXIT();
  } else if (_connectionTimeout == 0.0) {
    _connectionTimeout = LONG_TIMEOUT;
  }

  if (_requestTimeout < 0.0) {
    LOG_TOPIC(FATAL, arangodb::Logger::FIXME) << "invalid value for --server.request-timeout, must be positive";
    FATAL_ERROR_EXIT();
  } else if (_requestTimeout == 0.0) {
    _requestTimeout = LONG_TIMEOUT;
  }

  if (_maxPacketSize < 1 * 1024 * 1024) {
    LOG_TOPIC(FATAL, arangodb::Logger::FIXME) << "invalid value for --server.max-packet-size, must be at least 1 MB";
    FATAL_ERROR_EXIT();
  }

  // username must be non-empty
  if (_username.empty()) {
    LOG_TOPIC(FATAL, arangodb::Logger::FIXME) << "no value specified for --server.username";
    FATAL_ERROR_EXIT();
  }

  _haveServerPassword = !options->processingResult().touched("server.password");

  if (_askJwtSecret && options->processingResult().touched("server.password")) {
    LOG_TOPIC(FATAL, arangodb::Logger::FIXME)
        << "cannot specify both --server.password and --server.ask-jwt-token";
    FATAL_ERROR_EXIT();
  }

  if (_askJwtSecret && options->processingResult().touched("server.username")) {
    LOG_TOPIC(FATAL, arangodb::Logger::FIXME)
        << "cannot specify both --server.username and --server.ask-jwt-token";
    FATAL_ERROR_EXIT();
  }

  SimpleHttpClientParams::setDefaultMaxPacketSize(_maxPacketSize);
}

void ClientFeature::readPassword() {
  std::this_thread::sleep_for(std::chrono::milliseconds(10));

  try {
    ConsoleFeature* console =
        ApplicationServer::getFeature<ConsoleFeature>("Console");

    if (console->isEnabled()) {
      _password = console->readPassword("Please specify a password: ");
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
    ConsoleFeature* console =
        ApplicationServer::getFeature<ConsoleFeature>("Console");

    if (console->isEnabled()) {
      _jwtSecret = console->readPassword("Please specify the JWT secret: ");
      return;
    }
  } catch (...) {
  }

  std::cout << "Please specify the JWT secret: " << std::flush;
  _jwtSecret = ConsoleFeature::readPassword();
  std::cout << std::endl << std::flush;
}

void ClientFeature::prepare() {
  if (!isEnabled()) {
    return;
  }

  if (_askJwtSecret) {
    // ask for a jwt secret
    readJwtSecret();
  } else if (_authentication && _haveServerPassword) {
    // ask for a password
    readPassword();
  }
}

std::unique_ptr<GeneralClientConnection> ClientFeature::createConnection() {
  return createConnection(_endpoint);
}

std::unique_ptr<GeneralClientConnection> ClientFeature::createConnection(
    std::string const& definition) {
  std::unique_ptr<Endpoint> endpoint(Endpoint::clientFactory(definition));

  if (endpoint.get() == nullptr) {
    LOG_TOPIC(ERR, arangodb::Logger::FIXME)
        << "invalid value for --server.endpoint ('" << definition << "')";
    THROW_ARANGO_EXCEPTION(TRI_ERROR_BAD_PARAMETER);
  }

  std::unique_ptr<GeneralClientConnection> connection(
      GeneralClientConnection::factory(endpoint, _requestTimeout,
                                       _connectionTimeout, _retries,
                                       _sslProtocol));

  return connection;
}

std::unique_ptr<SimpleHttpClient> ClientFeature::createHttpClient() const {
  return createHttpClient(_endpoint);
}

std::unique_ptr<SimpleHttpClient> ClientFeature::createHttpClient(
    std::string const& definition) const {
  return createHttpClient(definition,
                          SimpleHttpClientParams(_requestTimeout, _warn));
}

std::unique_ptr<httpclient::SimpleHttpClient> ClientFeature::createHttpClient(
    std::string const& definition, SimpleHttpClientParams const& params) const {
  std::unique_ptr<Endpoint> endpoint(Endpoint::clientFactory(definition));

  if (endpoint.get() == nullptr) {
    LOG_TOPIC(ERR, arangodb::Logger::FIXME)
        << "invalid value for --server.endpoint ('" << definition << "')";
    THROW_ARANGO_EXCEPTION(TRI_ERROR_BAD_PARAMETER);
  }

  std::unique_ptr<GeneralClientConnection> connection(
      GeneralClientConnection::factory(endpoint, _requestTimeout,
                                       _connectionTimeout, _retries,
                                       _sslProtocol));

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

int ClientFeature::runMain(
    int argc, char* argv[],
    std::function<int(int argc, char* argv[])> const& mainFunc) {
  try {
    return mainFunc(argc, argv);
  } catch (std::exception const& ex) {
    LOG_TOPIC(ERR, arangodb::Logger::FIXME)
        << argv[0]
        << " terminated because of an unhandled exception: " << ex.what();
    return EXIT_FAILURE;
  } catch (...) {
    LOG_TOPIC(ERR, arangodb::Logger::FIXME)
        << argv[0]
        << " terminated because of an unhandled exception of unknown type";
    return EXIT_FAILURE;
  }
}
  
} // arangodb
