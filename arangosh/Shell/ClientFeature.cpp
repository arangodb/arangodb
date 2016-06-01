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
      _sslProtocol(4),
      _section("server"),
      _retries(DEFAULT_RETRIES),
      _warn(false) {
  setOptional(true);
  requiresElevatedPrivileges(false);
  startsAfter("Logger");
}

void ClientFeature::collectOptions(std::shared_ptr<ProgramOptions> options) {
  options->addSection(_section, "Configure a connection to the server");

  options->addOption("--" + _section + ".database",
                     "database name to use when connecting",
                     new StringParameter(&_databaseName));

  options->addOption("--" + _section + ".authentication",
                     "require authentication when connecting",
                     new BooleanParameter(&_authentication));

  options->addOption("--" + _section + ".username",
                     "username to use when connecting",
                     new StringParameter(&_username));

  options->addOption(
      "--" + _section + ".endpoint",
      "endpoint to connect to, use 'none' to start without a server",
      new StringParameter(&_endpoint));

  options->addOption("--" + _section + ".password",
                     "password to use when connection. If not specified and "
                     "authentication is required, the user will be prompted "
                     "for a password.",
                     new StringParameter(&_password));

  options->addOption("--" + _section + ".connection-timeout",
                     "connection timeout in seconds",
                     new DoubleParameter(&_connectionTimeout));

  options->addOption("--" + _section + ".request-timeout",
                     "request timeout in seconds",
                     new DoubleParameter(&_requestTimeout));

  std::unordered_set<uint64_t> sslProtocols = {1, 2, 3, 4};

  options->addOption("--" + _section + ".ssl-protocol",
                     "1 = SSLv2, 2 = SSLv23, 3 = SSLv3, 4 = TLSv1",
                     new DiscreteValuesParameter<UInt64Parameter>(
                         &_sslProtocol, sslProtocols));
}

void ClientFeature::validateOptions(std::shared_ptr<ProgramOptions> options) {
  // if a username is specified explicitly, assume authentication is desired
  if (options->processingResult().touched(_section + ".username")) {
    _authentication = true;
  }

  // check timeouts
  if (_connectionTimeout < 0.0) {
    LOG(FATAL) << "invalid value for --" << _section
               << ".connect-timeout, must be >= 0";
    FATAL_ERROR_EXIT();
  } else if (_connectionTimeout == 0.0) {
    _connectionTimeout = LONG_TIMEOUT;
  }

  if (_requestTimeout < 0.0) {
    LOG(FATAL) << "invalid value for --" << _section
               << ".request-timeout, must be positive";
    FATAL_ERROR_EXIT();
  } else if (_requestTimeout == 0.0) {
    _requestTimeout = LONG_TIMEOUT;
  }

  // username must be non-empty
  if (_username.empty()) {
    LOG(FATAL) << "no value specified for --" << _section << ".username";
    FATAL_ERROR_EXIT();
  }

  // ask for a password
  if (_authentication &&
      !options->processingResult().touched(_section + ".password")) {
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
    std::getline(std::cin, _password);
  }
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
