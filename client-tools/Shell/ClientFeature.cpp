////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2014-2024 ArangoDB GmbH, Cologne, Germany
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

#include "ClientFeature.h"

#include "ApplicationFeatures/ApplicationServer.h"
#include "ApplicationFeatures/CommunicationFeaturePhase.h"
#include "ApplicationFeatures/GreetingsFeaturePhase.h"
#include "Basics/FileUtils.h"
#include "Basics/ReadLocker.h"
#include "Basics/StaticStrings.h"
#include "Basics/WriteLocker.h"
#include "Basics/application-exit.h"
#include "Endpoint/Endpoint.h"
#include "Logger/Logger.h"
#include "Logger/LogMacros.h"
#include "ProgramOptions/Parameters.h"
#include "ProgramOptions/ProgramOptions.h"
#include "Shell/ShellConsoleFeature.h"
#include "SimpleHttpClient/GeneralClientConnection.h"
#include "SimpleHttpClient/SimpleHttpClient.h"
#include "SimpleHttpClient/SimpleHttpResult.h"
#include "Ssl/ssl-helper.h"
#include "Utils/ClientManager.h"
#include "Utilities/NameValidator.h"

#include <absl/strings/str_cat.h>
#include "Ssl/jwt.h"

#include <chrono>
#include <exception>

using namespace arangodb::application_features;
using namespace arangodb::httpclient;
using namespace arangodb::options;

namespace {
constexpr size_t DEFAULT_RETRIES = 2;
constexpr auto kJwtRenewalCheckInterval = std::chrono::seconds{1};
}  // anonymous namespace

namespace arangodb {

ClientFeature::ClientFeature(ApplicationServer& server)
    : ClientFeature{server, server.getFeature<CommunicationFeaturePhase>(),
                    typeid(HttpEndpointProvider), ClientFeatureOptions{}} {}

ClientFeature::ClientFeature(ApplicationServer& server,
                             ClientFeatureOptions options)
    : ClientFeature{server, server.getFeature<CommunicationFeaturePhase>(),
                    typeid(HttpEndpointProvider), std::move(options)} {}

ClientFeature::ClientFeature(ApplicationServer& server,
                             CommunicationFeaturePhase& comm,
                             std::type_index registration,
                             ClientFeatureOptions options)
    : HttpEndpointProvider(server, registration, name()),
      _options(std::move(options)),
      _comm{comm},
      _console{},
      _retries(DEFAULT_RETRIES),
      _warn(false),
      _warnConnect(true) {
  setOptional(true);

  if (server.hasFeature<ShellConsoleFeature>()) {
    _console = &server.getFeature<ShellConsoleFeature>();
  }

  startsAfter<CommunicationFeaturePhase>();
  startsAfter<GreetingsFeaturePhase>();

  if (auto res = DatabaseNameValidator::validateName(true, true,
                                                     _options.databaseName);
      res.fail()) {
    LOG_TOPIC("122a6", FATAL, arangodb::Logger::FIXME) << res.errorMessage();
    FATAL_ERROR_EXIT();
  }

  SimpleHttpClientParams::setDefaultMaxPacketSize(_options.maxPacketSize);
}

void ClientFeature::readPassword() {
  std::this_thread::sleep_for(std::chrono::milliseconds(10));

  if (_console && _console->isEnabled()) {
    _options.password = _console->readPassword("Please specify a password: ");
    return;
  }

  std::cout << "Please specify a password: " << std::flush;
  setPassword(ShellConsoleFeature::readPassword());
  std::cout << std::endl << std::flush;
}

void ClientFeature::readJwtToken() {
  std::this_thread::sleep_for(std::chrono::milliseconds(10));

  if (_console && _console->isEnabled()) {
    setJwtToken(_console->readPassword("Please specify a JWT token: "));
    return;
  }

  std::cout << "Please specify a JWT token: " << std::flush;
  setJwtToken(ShellConsoleFeature::readPassword());
  std::cout << std::endl << std::flush;
}

void ClientFeature::readJwtSecret() {
  std::this_thread::sleep_for(std::chrono::milliseconds(10));

  if (_console && _console->isEnabled()) {
    setJwtSecret(_console->readPassword("Please specify the JWT secret: "));
    return;
  }

  std::cout << "Please specify the JWT secret: " << std::flush;
  setJwtSecret(ShellConsoleFeature::readPassword());
  std::cout << std::endl << std::flush;
}

void ClientFeature::loadJwtSecretFile() {
  try {
    // Note that the secret is trimmed for whitespace, because whitespace
    // at the end of a file can easily happen. We do not base64-encode,
    // though, so the bytes count as given. Zero bytes might be a problem
    // here.
    setJwtSecret(basics::StringUtils::trim(
        basics::FileUtils::slurp(_options.jwtSecretFile), " \t\n\r"));
  } catch (std::exception const& ex) {
    LOG_TOPIC("aeaec", FATAL, Logger::STARTUP)
        << "unable to read content of jwt-secret file '"
        << _options.jwtSecretFile << "': " << ex.what()
        << ". please make sure the file/directory is readable for the "
           "arangod process and user";
    FATAL_ERROR_EXIT();
  }
}

void ClientFeature::prepare() {
  setDatabaseName(_options.databaseName);

  if (!isEnabled()) {
    return;
  }

  if (_options.askJwtSecret) {
    // ask for a jwt secret
    readJwtSecret();
  } else if (!_options.jwtSecretFile.empty()) {
    loadJwtSecretFile();
  } else if (_options.authentication && _options.haveServerPassword) {
    // ask for a password
    readPassword();
  } else if (!_options.jwtToken.empty() && _options.jwtToken == "-") {
    // if the jwt token is set to "-" we will ask for it
    readJwtToken();
  }

  if (!_options.jwtToken.empty()) {
    _renewingJwtToken = std::make_shared<RenewingJwtToken>(
        _options.jwtToken,
        [this](JwtToken const& token) { return renewJwtViaOpenAuth(token); },
        std::chrono::duration_cast<JwtClock::duration>(
            std::chrono::duration<double>{_options.jwtRenewalThreshold}),
        &JwtClock::now);
  }
}

void ClientFeature::start() {
  if (_renewingJwtToken != nullptr) {
    _jwtRenewal = std::make_unique<BackgroundJwtRenewal>(
        _renewingJwtToken, kJwtRenewalCheckInterval);
  }
}

void ClientFeature::stop() { _jwtRenewal.reset(); }

std::unique_ptr<SimpleHttpClient> ClientFeature::createHttpClient(
    size_t threadNumber, bool suppressError) const {
  std::string endpoint;
  {
    READ_LOCKER(locker, _settingsLock);
    endpoint = _options.endpoints[threadNumber % _options.endpoints.size()];
  }
  return createHttpClient(endpoint, suppressError);
}

std::unique_ptr<SimpleHttpClient> ClientFeature::createHttpClient(
    std::string const& definition, bool suppressError) const {
  return createHttpClient(definition, defaultHttpClientParams(), suppressError);
}

SimpleHttpClientParams ClientFeature::defaultHttpClientParams() const {
  READ_LOCKER(locker, _settingsLock);
  SimpleHttpClientParams params(_options.requestTimeout, _warn);
  params.setCompressRequestThreshold(
      _options.compressTransfer ? _options.compressRequestThreshold : 0);
  return params;
}

std::unique_ptr<httpclient::SimpleHttpClient>
ClientFeature::createBareHttpClient(std::string const& definition,
                                    SimpleHttpClientParams const& params,
                                    bool suppressError) const {
  std::unique_ptr<Endpoint> endpoint(Endpoint::clientFactory(definition));

  if (endpoint == nullptr) {
    if (definition != "none" && !suppressError) {
      LOG_TOPIC("2fac8", ERR, arangodb::Logger::FIXME)
          << "invalid value for --server.endpoint ('" << definition << "')";
    }
    THROW_ARANGO_EXCEPTION(TRI_ERROR_BAD_PARAMETER);
  }

  READ_LOCKER(locker, _settingsLock);

  std::unique_ptr<GeneralClientConnection> connection(
      GeneralClientConnection::factory(_comm, endpoint, _options.requestTimeout,
                                       _options.connectionTimeout, _retries,
                                       _options.sslProtocol));

  // takes over ownership for the connection object
  return std::make_unique<SimpleHttpClient>(connection, params);
}

std::unique_ptr<httpclient::SimpleHttpClient> ClientFeature::createHttpClient(
    std::string const& definition, SimpleHttpClientParams const& params,
    bool suppressError) const {
  auto httpClient = createBareHttpClient(definition, params, suppressError);

  READ_LOCKER(locker, _settingsLock);

  httpClient->params().setLocationRewriter(static_cast<void const*>(this),
                                           &ClientManager::rewriteLocation);
  httpClient->params().setUserNamePassword("/", _options.username,
                                           _options.password);
  if (_renewingJwtToken != nullptr) {
    // only installs the callable; it is invoked per request, never while
    // _settingsLock is held
    httpClient->params().setJwtProvider(
        [token = _renewingJwtToken] { return token->current(); });
  } else if (!_options.jwtToken.empty()) {
    httpClient->params().setJwt(_options.jwtToken);
  } else if (!_jwtSecret.empty()) {
    TRI_ASSERT(!_options.endpoints.empty());
    httpClient->params().setJwt(
        arangodb::rest::SslInterface::jwt::generateInternalToken(
            _jwtSecret, _options.endpoints[0]));
  }

  return httpClient;
}

RenewalOutcome ClientFeature::renewJwtViaOpenAuth(JwtToken const& token) const {
  try {
    auto const client =
        createBareHttpClient(endpoint(), defaultHttpClientParams(),
                             /*suppressError*/ false);
    std::unique_ptr<SimpleHttpResult> const response(client->request(
        rest::RequestType::POST, "/_open/auth/renew", nullptr, 0,
        {{StaticStrings::Authorization, absl::StrCat("bearer ", token)}}));
    if (response == nullptr || !response->isComplete()) {
      return RenewalOutcome::error(TRI_ERROR_SIMPLE_CLIENT_COULD_NOT_CONNECT,
                                   client->getErrorMessage());
    }
    return parseRenewalResponse(*response);
  } catch (std::exception const& ex) {
    return RenewalOutcome::error(TRI_ERROR_INTERNAL, ex.what());
  }
}

std::vector<std::string> ClientFeature::httpEndpoints() {
  std::vector<std::string> httpEndpoints;

  READ_LOCKER(locker, _settingsLock);
  std::for_each(_options.endpoints.begin(), _options.endpoints.end(),
                [&httpEndpoints](std::string const& endpoint) {
                  if (std::string http = Endpoint::uriForm(endpoint);
                      !http.empty()) {
                    httpEndpoints.emplace_back(std::move(http));
                  }
                });
  return httpEndpoints;
}

std::string ClientFeature::databaseName() const {
  READ_LOCKER(locker, _settingsLock);
  return _options.databaseName;
}

void ClientFeature::setDatabaseName(std::string_view databaseName) {
  if (auto res = DatabaseNameValidator::validateName(true, true, databaseName);
      res.fail()) {
    THROW_ARANGO_EXCEPTION(res);
  }

  WRITE_LOCKER(locker, _settingsLock);
  _options.databaseName = databaseName;
}

// get single endpoint. used by client tools that can handle only one endpoint
std::string ClientFeature::endpoint() const {
  READ_LOCKER(locker, _settingsLock);
  return _options.endpoints[0];
}

// set single endpoint
void ClientFeature::setEndpoint(std::string_view value) {
  WRITE_LOCKER(locker, _settingsLock);
  _options.endpoints[0] = value;
}

std::string ClientFeature::username() const {
  // a token authenticates as the user it was issued for, whatever
  // --server.username says
  if (auto const tokenUser =
          rest::SslInterface::jwt::extractPreferredUsername(jwtToken());
      tokenUser.has_value()) {
    return *tokenUser;
  }
  READ_LOCKER(locker, _settingsLock);
  return _options.username;
}

void ClientFeature::setUsername(std::string_view value) {
  WRITE_LOCKER(locker, _settingsLock);
  _options.username = value;
}

std::string ClientFeature::password() const {
  READ_LOCKER(locker, _settingsLock);
  return _options.password;
}

void ClientFeature::setPassword(std::string_view value) {
  WRITE_LOCKER(locker, _settingsLock);
  _options.password = value;
}

void ClientFeature::setJwtToken(std::string_view jwtToken) {
  WRITE_LOCKER(locker, _settingsLock);
  _options.jwtToken = jwtToken;
}

std::string ClientFeature::jwtSecret() const {
  READ_LOCKER(locker, _settingsLock);
  return _jwtSecret;
}

void ClientFeature::setJwtSecret(std::string_view jwtSecret) {
  WRITE_LOCKER(locker, _settingsLock);
  _jwtSecret = jwtSecret;
}

std::string ClientFeature::jwtToken() const {
  if (_renewingJwtToken != nullptr) {
    return _renewingJwtToken->current();
  }
  READ_LOCKER(locker, _settingsLock);
  return _options.jwtToken;
}

double ClientFeature::connectionTimeout() const noexcept {
  READ_LOCKER(locker, _settingsLock);
  return _options.connectionTimeout;
}

double ClientFeature::requestTimeout() const noexcept {
  READ_LOCKER(locker, _settingsLock);
  return _options.requestTimeout;
}

void ClientFeature::requestTimeout(double value) noexcept {
  WRITE_LOCKER(locker, _settingsLock);
  _options.requestTimeout = value;
}

uint64_t ClientFeature::maxPacketSize() const noexcept {
  READ_LOCKER(locker, _settingsLock);
  return _options.maxPacketSize;
}

uint64_t ClientFeature::sslProtocol() const noexcept {
  READ_LOCKER(locker, _settingsLock);
  return _options.sslProtocol;
}

bool ClientFeature::askJwtSecret() const noexcept {
  READ_LOCKER(locker, _settingsLock);
  return _options.askJwtSecret;
}

bool ClientFeature::forceJson() const noexcept {
  READ_LOCKER(locker, _settingsLock);
  return _options.forceJson;
}

void ClientFeature::setForceJson(bool value) noexcept {
  WRITE_LOCKER(locker, _settingsLock);
  _options.forceJson = value;
}

void ClientFeature::setRetries(size_t retries) noexcept {
  WRITE_LOCKER(locker, _settingsLock);
  _retries = retries;
}

void ClientFeature::setWarn(bool warn) noexcept {
  WRITE_LOCKER(locker, _settingsLock);
  _warn = warn;
}

bool ClientFeature::getWarn() const noexcept {
  READ_LOCKER(locker, _settingsLock);
  return _warn;
}

void ClientFeature::setWarnConnect(bool warnConnect) noexcept {
  WRITE_LOCKER(locker, _settingsLock);
  _warnConnect = warnConnect;
}

bool ClientFeature::getWarnConnect() const noexcept {
  READ_LOCKER(locker, _settingsLock);
  return _warnConnect;
}

bool ClientFeature::compressTransfer() const noexcept {
  READ_LOCKER(locker, _settingsLock);
  return _options.compressTransfer;
}

void ClientFeature::setCompressTransfer(bool value) noexcept {
  WRITE_LOCKER(locker, _settingsLock);
  _options.compressTransfer = value;
}

double ClientFeature::jwtRenewalThreshold() const noexcept {
  READ_LOCKER(locker, _settingsLock);
  return _options.jwtRenewalThreshold;
}

void ClientFeature::setJwtRenewalThreshold(double value) noexcept {
  WRITE_LOCKER(locker, _settingsLock);
  _options.jwtRenewalThreshold = value;
}

uint64_t ClientFeature::compressRequestThreshold() const noexcept {
  READ_LOCKER(locker, _settingsLock);
  return _options.compressRequestThreshold;
}

ApplicationServer& ClientFeature::server() const noexcept {
  return _comm.server();
}

std::string ClientFeature::buildConnectedMessage(
    std::string_view endpointSpecification, std::string_view version,
    std::string_view role, std::string_view mode, std::string_view databaseName,
    std::string_view user) {
  bool versionEmpty = (version.empty() || version == "arango");
  return absl::StrCat(
      "Connected to ArangoDB '", endpointSpecification,
      (versionEmpty ? "" : ", version: "), (versionEmpty ? "" : version), " [",
      (role.empty() ? "unknown" : role), ", ", mode, "], database: '",
      databaseName, "', username: '", user, "'");
}

int ClientFeature::runMain(
    int argc, char* argv[],
    std::function<int(int argc, char* argv[])> const& mainFunc) {
  try {
    return mainFunc(argc, argv);
  } catch (std::exception const& ex) {
    LOG_TOPIC("5b00f", ERR, arangodb::Logger::FIXME)
        << argv[0]
        << " terminated because of an unhandled exception: " << ex.what();
    return EXIT_FAILURE;
  } catch (...) {
    LOG_TOPIC("98466", ERR, arangodb::Logger::FIXME)
        << argv[0]
        << " terminated because of an unhandled exception of unknown type";
    return EXIT_FAILURE;
  }
}

}  // namespace arangodb
