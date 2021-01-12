////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2014-2021 ArangoDB GmbH, Cologne, Germany
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

#ifndef ARANGODB_APPLICATION_FEATURES_CLIENT_FEATURE_H
#define ARANGODB_APPLICATION_FEATURES_CLIENT_FEATURE_H 1

#include <functional>

#include "ApplicationFeatures/ApplicationFeature.h"
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

  ClientFeature(application_features::ApplicationServer& server, bool allowJwtSecret,
                double connectionTimeout = DEFAULT_CONNECTION_TIMEOUT,
                double requestTimeout = DEFAULT_REQUEST_TIMEOUT);

  void collectOptions(std::shared_ptr<options::ProgramOptions>) override final;
  void validateOptions(std::shared_ptr<options::ProgramOptions>) override final;
  void prepare() override final;
  void start() override final;
  void stop() override final;

  std::string const& databaseName() const { return _databaseName; }
  bool authentication() const { return _authentication; }
  std::string const& endpoint() const { return _endpoint; }
  void setEndpoint(std::string const& value) { _endpoint = value; }
  std::string const& username() const { return _username; }
  void setUsername(std::string const& value) { _username = value; }
  std::string const& password() const { return _password; }
  void setPassword(std::string const& value) { _password = value; }
  std::string const& jwtSecret() const { return _jwtSecret; }
  double connectionTimeout() const { return _connectionTimeout; }
  double requestTimeout() const { return _requestTimeout; }
  void requestTimeout(double value) { _requestTimeout = value; }
  uint64_t maxPacketSize() const { return _maxPacketSize; }
  uint64_t sslProtocol() const { return _sslProtocol; }
  bool forceJson() const { return _forceJson; }
  void setForceJson(bool value) { _forceJson = value; }
  
  std::unique_ptr<httpclient::GeneralClientConnection> createConnection();
  std::unique_ptr<httpclient::GeneralClientConnection> createConnection(std::string const& definition);
  std::unique_ptr<httpclient::SimpleHttpClient> createHttpClient() const;
  std::unique_ptr<httpclient::SimpleHttpClient> createHttpClient(std::string const& definition) const;
  std::unique_ptr<httpclient::SimpleHttpClient> createHttpClient(
      std::string const& definition, httpclient::SimpleHttpClientParams const&) const;
  std::vector<std::string> httpEndpoints() override;

  void setDatabaseName(std::string const& databaseName) {
    _databaseName = databaseName;
  }

  void setRetries(size_t retries) { _retries = retries; }

  void setWarn(bool warn) { _warn = warn; }

  bool getWarn() { return _warn; }

  void setWarnConnect(bool warnConnect) { _warnConnect = warnConnect; }

  bool getWarnConnect() { return _warnConnect; }

  static std::string buildConnectedMessage(
    std::string const& endpointSpecification,
    std::string const& version,
    std::string const& role,
    std::string const& mode,
    std::string const& databaseName,
    std::string const& user
  );

  static int runMain(int argc, char* argv[],
                     std::function<int(int argc, char* argv[])> const& mainFunc);

 private:
  void readPassword();
  void readJwtSecret();
  void loadJwtSecretFile();

  std::string _databaseName;
  std::string _endpoint;
  std::string _username;
  std::string _password;
  std::string _jwtSecret;
  std::string _jwtSecretFile;
  double _connectionTimeout;
  double _requestTimeout;
  uint64_t _maxPacketSize;
  uint64_t _sslProtocol;

  size_t _retries;
  bool _authentication;
  bool _askJwtSecret;

  bool _allowJwtSecret;
  bool _warn;
  bool _warnConnect;
  bool _haveServerPassword;
  bool _forceJson;

#if _WIN32
  uint16_t _codePage;
  uint16_t _originalCodePage;
#endif
};

}  // namespace arangodb

#endif
