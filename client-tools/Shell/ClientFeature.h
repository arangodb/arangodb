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
/// @author Dr. Frank Celler
////////////////////////////////////////////////////////////////////////////////

#pragma once

#include <functional>
#include <string>
#include <string_view>

#include "Shell/arangosh.h"
#include "ApplicationFeatures/HttpEndpointProvider.h"

namespace arangodb {

class Endpoint;

namespace httpclient {

class GeneralClientConnection;
class SimpleHttpClient;
struct SimpleHttpClientParams;

}  // namespace httpclient

class ClientFeature final : public HttpEndpointProvider {
 public:
  constexpr static double const DEFAULT_REQUEST_TIMEOUT = 1200.0;
  constexpr static double const DEFAULT_CONNECTION_TIMEOUT = 5.0;
  constexpr static size_t const DEFAULT_RETRIES = 2;
  constexpr static double const LONG_TIMEOUT = 86400.0;
  constexpr static std::string_view name() noexcept { return "Client"; }

  template<typename Server>
  ClientFeature(Server& server, bool allowJwtSecret, size_t maxNumEndpoints = 1,
                double connectionTimeout = DEFAULT_CONNECTION_TIMEOUT,
                double requestTimeout = DEFAULT_REQUEST_TIMEOUT)
      : ClientFeature{server,
                      server.template getFeature<CommunicationFeaturePhase>(),
                      Server::template id<HttpEndpointProvider>(),
                      allowJwtSecret,
                      maxNumEndpoints,
                      connectionTimeout,
                      requestTimeout} {
    static_assert(Server::template isCreatedAfter<HttpEndpointProvider,
                                                  CommunicationFeaturePhase>());

    if constexpr (Server::template contains<ShellConsoleFeature>()) {
      static_assert(Server::template isCreatedAfter<HttpEndpointProvider,
                                                    ShellConsoleFeature>());
      _console = &server.template getFeature<ShellConsoleFeature>();
    }

    startsAfter<CommunicationFeaturePhase, Server>();
    startsAfter<GreetingsFeaturePhase, Server>();
  }

  void collectOptions(std::shared_ptr<options::ProgramOptions>) override final;
  void validateOptions(std::shared_ptr<options::ProgramOptions>) override final;
  void prepare() override final;
  void start() override final;
  void stop() override final;

  std::string const& databaseName() const { return _databaseName; }
  void setDatabaseName(std::string const& databaseName);
  bool authentication() const noexcept { return _authentication; }
  // get single endpoint. used by client tools that can handle only one endpoint
  std::string const& endpoint() const { return _endpoints[0]; }
  // set single endpoint
  void setEndpoint(std::string const& value) { _endpoints[0] = value; }
  std::string const& username() const { return _username; }
  void setUsername(std::string const& value) { _username = value; }
  std::string const& password() const { return _password; }
  void setPassword(std::string const& value) { _password = value; }
  std::string const& jwtSecret() const { return _jwtSecret; }
  void setJwtSecret(std::string_view jwtSecret) { _jwtSecret = jwtSecret; }
  double connectionTimeout() const noexcept { return _connectionTimeout; }
  double requestTimeout() const noexcept { return _requestTimeout; }
  void requestTimeout(double value) noexcept { _requestTimeout = value; }
  uint64_t maxPacketSize() const noexcept { return _maxPacketSize; }
  uint64_t sslProtocol() const noexcept { return _sslProtocol; }
  bool forceJson() const noexcept { return _forceJson; }
  void setForceJson(bool value) noexcept { _forceJson = value; }
  void setRetries(size_t retries) noexcept { _retries = retries; }
  void setWarn(bool warn) noexcept { _warn = warn; }
  bool getWarn() const noexcept { return _warn; }
  void setWarnConnect(bool warnConnect) { _warnConnect = warnConnect; }
  bool getWarnConnect() const noexcept { return _warnConnect; }

  std::unique_ptr<httpclient::GeneralClientConnection> createConnection(
      std::string const& definition);
  std::unique_ptr<httpclient::SimpleHttpClient> createHttpClient(
      size_t threadNumber = 0) const;
  std::unique_ptr<httpclient::SimpleHttpClient> createHttpClient(
      std::string const& definition) const;
  std::unique_ptr<httpclient::SimpleHttpClient> createHttpClient(
      std::string const& definition,
      httpclient::SimpleHttpClientParams const&) const;
  std::vector<std::string> httpEndpoints() override;

  CommunicationFeaturePhase& getCommFeaturePhase() { return _comm; }

  ApplicationServer& server() const noexcept;

  static std::string buildConnectedMessage(
      std::string const& endpointSpecification, std::string const& version,
      std::string const& role, std::string const& mode,
      std::string const& databaseName, std::string const& user);

  static int runMain(
      int argc, char* argv[],
      std::function<int(int argc, char* argv[])> const& mainFunc);

 private:
  ClientFeature(ApplicationServer& server, CommunicationFeaturePhase& comm,
                size_t registration, bool allowJwtSecret,
                size_t maxNumEndpoints = 1,
                double connectionTimeout = DEFAULT_CONNECTION_TIMEOUT,
                double requestTimeout = DEFAULT_REQUEST_TIMEOUT);

  void readPassword();
  void readJwtSecret();
  void loadJwtSecretFile();

  CommunicationFeaturePhase& _comm;
  ShellConsoleFeature* _console;
  std::string _databaseName;
  std::vector<std::string> _endpoints;
  size_t _maxNumEndpoints;
  std::string _username;
  std::string _password;
  std::string _jwtSecret;
  std::string _jwtSecretFile;
  double _connectionTimeout;
  double _requestTimeout;
  uint64_t _maxPacketSize;
  uint64_t _sslProtocol;

  size_t _retries;

#if _WIN32
  uint16_t _codePage;
  uint16_t _originalCodePage;
#endif

  bool const _allowJwtSecret;
  bool _authentication;
  bool _askJwtSecret;
  bool _warn;
  bool _warnConnect;
  bool _haveServerPassword;
  bool _forceJson;
};

}  // namespace arangodb
