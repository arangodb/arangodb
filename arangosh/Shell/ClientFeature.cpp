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

using namespace arangodb;
using namespace arangodb::application_features;
using namespace arangodb::httpclient;
using namespace arangodb::options;

ClientFeature::ClientFeature(application_features::ApplicationServer* server,
                             double connectionTimeout, double requestTimeout)
    : ApplicationFeature(server, "Client"),
      _databaseName("_system"),
      _authentication(true),
      _endpoint(Endpoint::defaultEndpoint(Endpoint::TransportType::HTTP)),
      _username("root"),
      _password(""),
      _connectionTimeout(connectionTimeout),
      _requestTimeout(requestTimeout),
      _maxPacketSize(128 * 1024 * 1024),
      _sslProtocol(4),
      _retries(DEFAULT_RETRIES),
      _warn(false) {
  setOptional(true);
  requiresElevatedPrivileges(false);
  startsAfter("Logger");
}

void ClientFeature::collectOptions(std::shared_ptr<ProgramOptions> options) {
  options->addSection("server", "Configure a connection to the server");

  options->addOption("--server.database",
                     "database name to use when connecting",
                     new StringParameter(&_databaseName));

  options->addOption("--server.authentication",
                     "require authentication when connecting",
                     new BooleanParameter(&_authentication));

  options->addOption("--server.username",
                     "username to use when connecting",
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

  options->addOption("--server.connection-timeout",
                     "connection timeout in seconds",
                     new DoubleParameter(&_connectionTimeout));

  options->addOption("--server.request-timeout",
                     "request timeout in seconds",
                     new DoubleParameter(&_requestTimeout));
  
  options->addOption("--server.max-packet-size",
                     "maximum packet size (in bytes) for client/server communication",
                     new UInt64Parameter(&_maxPacketSize));

  std::unordered_set<uint64_t> sslProtocols = {1, 2, 3, 4, 5};

  options->addSection("ssl", "Configure SSL communication");
  options->addOption("--ssl.protocol",
                     "ssl protocol (1 = SSLv2, 2 = SSLv23, 3 = SSLv3, 4 = "
                     "TLSv1, 5 = TLSV1.2 (recommended)",
                     new DiscreteValuesParameter<UInt64Parameter>(
                         &_sslProtocol, sslProtocols));
}

void ClientFeature::validateOptions(std::shared_ptr<ProgramOptions> options) {
  // if a username is specified explicitly, assume authentication is desired
  if (options->processingResult().touched("server.username")) {
    _authentication = true;
  }

  // check timeouts
  if (_connectionTimeout < 0.0) {
    LOG(FATAL) << "invalid value for --server.connect-timeout, must be >= 0";
    FATAL_ERROR_EXIT();
  } else if (_connectionTimeout == 0.0) {
    _connectionTimeout = LONG_TIMEOUT;
  }

  if (_requestTimeout < 0.0) {
    LOG(FATAL) << "invalid value for --server.request-timeout, must be positive";
    FATAL_ERROR_EXIT();
  } else if (_requestTimeout == 0.0) {
    _requestTimeout = LONG_TIMEOUT;
  }

  if (_maxPacketSize < 1 * 1024 * 1024) {
    LOG(FATAL) << "invalid value for --server.max-packet-size, must be at least 1 MB";
    FATAL_ERROR_EXIT();
  }

  // username must be non-empty
  if (_username.empty()) {
    LOG(FATAL) << "no value specified for --server.username";
    FATAL_ERROR_EXIT();
  }

  // ask for a password
  if (_authentication &&
      !options->processingResult().touched("server.password")) {
    usleep(10 * 1000);

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

  SimpleHttpClient::setMaxPacketSize(_maxPacketSize);
}

std::unique_ptr<GeneralClientConnection> ClientFeature::createConnection() {
  return createConnection(_endpoint);
}

std::unique_ptr<GeneralClientConnection> ClientFeature::createConnection(
    std::string const& definition) {
  std::unique_ptr<Endpoint> endpoint(Endpoint::clientFactory(definition));

  if (endpoint.get() == nullptr) {
    LOG(ERR) << "invalid value for --server.endpoint ('" << definition << "')";
    THROW_ARANGO_EXCEPTION(TRI_ERROR_BAD_PARAMETER);
  }

  std::unique_ptr<GeneralClientConnection> connection(
      GeneralClientConnection::factory(endpoint, _requestTimeout,
                                       _connectionTimeout, _retries,
                                       _sslProtocol));

  return connection;
}

std::unique_ptr<SimpleHttpClient> ClientFeature::createHttpClient() {
  return createHttpClient(_endpoint);
}

std::unique_ptr<SimpleHttpClient> ClientFeature::createHttpClient(
    std::string const& definition) {
  std::unique_ptr<Endpoint> endpoint(Endpoint::clientFactory(definition));

  if (endpoint.get() == nullptr) {
    LOG(ERR) << "invalid value for --server.endpoint ('" << definition << "')";
    THROW_ARANGO_EXCEPTION(TRI_ERROR_BAD_PARAMETER);
  }

  std::unique_ptr<GeneralClientConnection> connection(
      GeneralClientConnection::factory(endpoint, _requestTimeout,
                                       _connectionTimeout, _retries,
                                       _sslProtocol));

  return std::make_unique<SimpleHttpClient>(connection, _requestTimeout, _warn);
}

std::vector<std::string> ClientFeature::httpEndpoints() {
  std::string http = Endpoint::uriForm(_endpoint);

  if (http.empty()) {
    return {};
  }

  return { http };
}
