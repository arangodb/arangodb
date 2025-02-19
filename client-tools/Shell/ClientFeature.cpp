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
/// @author Dr. Frank Celler
////////////////////////////////////////////////////////////////////////////////

#include "ClientFeature.h"

#include "ApplicationFeatures/ApplicationServer.h"
#include "ApplicationFeatures/CommunicationFeaturePhase.h"
#include "ApplicationFeatures/GreetingsFeaturePhase.h"
#include "Basics/FileUtils.h"
#include "Basics/ReadLocker.h"
#include "Basics/StaticStrings.h"
#include "Basics/Utf8Helper.h"
#include "Basics/WriteLocker.h"
#include "Basics/application-exit.h"
#include "Basics/files.h"
#include "Endpoint/Endpoint.h"
#include "Logger/Logger.h"
#include "Logger/LogMacros.h"
#include "ProgramOptions/Parameters.h"
#include "ProgramOptions/ProgramOptions.h"
#include "ProgramOptions/Section.h"
#include "Shell/ShellConsoleFeature.h"
#include "SimpleHttpClient/GeneralClientConnection.h"
#include "SimpleHttpClient/SimpleHttpClient.h"
#include "Ssl/ssl-helper.h"
#include "Utils/ClientManager.h"
#include "Utilities/NameValidator.h"

#include <absl/strings/str_cat.h>
#include <fuerte/jwt.h>

using namespace arangodb::application_features;
using namespace arangodb::httpclient;
using namespace arangodb::options;

namespace arangodb {

ClientFeature::ClientFeature(ApplicationServer& server,
                             CommunicationFeaturePhase& comm,
                             size_t registration, bool allowJwtSecret,
                             size_t maxNumEndpoints, double connectionTimeout,
                             double requestTimeout)
    : HttpEndpointProvider(server, registration, name()),
      _comm{comm},
      _console{},
      _endpoints{Endpoint::defaultEndpoint()},
      _maxNumEndpoints(maxNumEndpoints),
      _databaseName(StaticStrings::SystemDatabase),
      _username("root"),
      _connectionTimeout(connectionTimeout),
      _requestTimeout(requestTimeout),
      _maxPacketSize(1024 * 1024 * 1024),
      _compressRequestThreshold(0),
      _sslProtocol(TLS_V12),
      _retries(DEFAULT_RETRIES),
      _allowJwtSecret(allowJwtSecret),
      _authentication(true),
      _askJwtSecret(false),
      _warn(false),
      _warnConnect(true),
      _haveServerPassword(false),
      _forceJson(false),
      _compressTransfer(false) {
  setOptional(true);
}

void ClientFeature::collectOptions(std::shared_ptr<ProgramOptions> options) {
  options->addSection("server", "server connection");

  options->addOption("--server.database",
                     "The database name to use when connecting.",
                     new StringParameter(&_databaseName));

  options->addOption("--server.authentication",
                     "Require authentication credentials when connecting (does "
                     "not affect the server-side authentication settings).",
                     new BooleanParameter(&_authentication));

  options->addOption("--server.username",
                     "The username to use when connecting.",
                     new StringParameter(&_username));

  std::string basename = TRI_Basename(options->progname());
  bool isArangosh = basename == "arangosh";

  char const* endpointHelp;
  if (isArangosh) {
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

  auto& opt = options->addOption(
      "--server.endpoint", endpointHelp,
      new VectorParameter<StringParameter>(&_endpoints),
      arangodb::options::makeFlags(Flags::FlushOnFirst, Flags::Default));
  if (isArangosh) {
    opt.setLongDescription(R"(You can use `--server.endpoint none` to start
arangosh without connecting to a server.)");
  }

  options->addOption(
      "--server.password",
      "The password to use when connecting. If not specified and "
      "authentication is required, you are prompted for a password.\n"
      "In startup options, you can wrap the names of environment variables "
      "in at signs to use their value, like @ARANGO_PASSWORD@. This helps to "
      "expose the password less, like to the process list. "
      "Literal @ need to be escaped as @@.",
      new StringParameter(&_password));

  if (isArangosh) {
    // this option is only available in arangosh
    options->addOption("--server.force-json",
                       "Force to not use VelocyPack for easier debugging.",
                       new BooleanParameter(&_forceJson),
                       arangodb::options::makeDefaultFlags(
                           arangodb::options::Flags::Uncommon));
  }

  if (_allowJwtSecret) {
    // currently the option is only present for arangosh, but none
    // of the other client tools
    options->addOption(
        "--server.ask-jwt-secret",
        "If enabled, you are prompted for a JWT secret. This option is not "
        "compatible with --server.username and --server.password. "
        "If specified, it is used for all connections - even if a new "
        "connection to another server is created.",
        new BooleanParameter(&_askJwtSecret),
        arangodb::options::makeDefaultFlags(
            arangodb::options::Flags::Uncommon));

    options->addOption(
        "--server.jwt-secret-keyfile",
        "If enabled, the JWT secret is loaded from the given file. This option "
        "is not compatible with --server.ask-jwt-secret, --server.username and "
        "--server.password. If specified, it is used for all connections - "
        "even if a new connection to another server is created.",
        new StringParameter(&_jwtSecretFile),
        arangodb::options::makeDefaultFlags(
            arangodb::options::Flags::Uncommon));
  }

  options->addOption("--server.connection-timeout",
                     "The connection timeout (in seconds).",
                     new DoubleParameter(&_connectionTimeout));

  options->addOption("--server.request-timeout",
                     "The request timeout (in seconds).",
                     new DoubleParameter(&_requestTimeout));

  // note: the max-packet-size is used for all client tools that use the
  // SimpleHttpClient. fuerte does not use this
  options->addOption(
      "--server.max-packet-size",
      "The maximum packet size (in bytes) for client/server communication.",
      new UInt64Parameter(&_maxPacketSize),
      arangodb::options::makeDefaultFlags(arangodb::options::Flags::Uncommon));

  std::unordered_set<uint64_t> const sslProtocols = availableSslProtocols();

  options->addSection("ssl", "SSL communication");
  options->addOption("--ssl.protocol", availableSslProtocolsDescription(),
                     new DiscreteValuesParameter<UInt64Parameter>(
                         &_sslProtocol, sslProtocols));
  options
      ->addOption(
          "--compress-transfer",
          "Compress data for transport between " + basename + " and server.",
          new BooleanParameter(&_compressTransfer))
      .setIntroducedIn(31200)
      .setLongDescription(R"(This option enables transport compression for data
received by an ArangoDB server.)");

  options
      ->addOption("--compress-request-threshold",
                  "The HTTP request body size from which on requests are "
                  "transparently compressed when sending them to the server.",
                  new UInt64Parameter(&_compressRequestThreshold))
      .setIntroducedIn(31200)
      .setLongDescription(
          R"(Automatically compress outgoing HTTP requests 
with the deflate compression format. Compression will only happen for
HTTP/1.1 and HTTP/2 connections, if the size of the uncompressed request
body exceeds the threshold value controlled by this startup option,
and if the request body size after compression is less than the original
request body size.
Using the value 0 disables the automatic request compression.")");
}

void ClientFeature::validateOptions(std::shared_ptr<ProgramOptions> options) {
  if (_sslProtocol == SslProtocol::SSL_V2) {
    LOG_TOPIC("64f4f", FATAL, arangodb::Logger::SSL)
        << "SSLv2 is not supported any longer because of security "
           "vulnerabilities in this protocol";
    FATAL_ERROR_EXIT();
  }

  if (_endpoints.size() > _maxNumEndpoints) {
    // this is the case if we have more endpoints than allowed.
    // in versions before 3.9, it was allowed to specify `--server.endpoint`
    // multiple times, and if this was done, only the last provided endpoint
    // was used. to keep backward-compatibility, we now emulate this
    // behavior here.
    TRI_ASSERT(_maxNumEndpoints == 1);
    std::string selectedEndpoint = _endpoints.back();

    _endpoints = {std::move(selectedEndpoint)};
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

  if ((_askJwtSecret || hasJwtSecretFile) &&
      options->processingResult().touched("server.password")) {
    LOG_TOPIC("65475", FATAL, arangodb::Logger::FIXME)
        << "cannot specify both --server.password and jwt secret source";
    FATAL_ERROR_EXIT();
  }

  if ((_askJwtSecret || hasJwtSecretFile) &&
      options->processingResult().touched("server.username")) {
    LOG_TOPIC("9d886", FATAL, arangodb::Logger::FIXME)
        << "cannot specify both --server.username and jwt secret source";
    FATAL_ERROR_EXIT();
  }

  if (_askJwtSecret && hasJwtSecretFile) {
    LOG_TOPIC("aeaeb", FATAL, arangodb::Logger::FIXME)
        << "multiple jwt secret sources specified";
    FATAL_ERROR_EXIT();
  }

  if (!_endpoints.empty()) {
    std::for_each(
        _endpoints.begin(), _endpoints.end(), [](auto const& endpoint) {
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

  if (auto res = DatabaseNameValidator::validateName(true, true, _databaseName);
      res.fail()) {
    LOG_TOPIC("122a6", FATAL, arangodb::Logger::FIXME) << res.errorMessage();
    FATAL_ERROR_EXIT();
  }

  SimpleHttpClientParams::setDefaultMaxPacketSize(_maxPacketSize);
}

void ClientFeature::readPassword() {
  std::this_thread::sleep_for(std::chrono::milliseconds(10));

  if (_console && _console->isEnabled()) {
    _password = _console->readPassword("Please specify a password: ");
    return;
  }

  std::cout << "Please specify a password: " << std::flush;
  setPassword(ShellConsoleFeature::readPassword());
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
        basics::FileUtils::slurp(_jwtSecretFile), " \t\n\r"));
  } catch (std::exception const& ex) {
    LOG_TOPIC("aeaec", FATAL, Logger::STARTUP)
        << "unable to read content of jwt-secret file '" << _jwtSecretFile
        << "': " << ex.what()
        << ". please make sure the file/directory is readable for the "
           "arangod process and user";
    FATAL_ERROR_EXIT();
  }
}

void ClientFeature::prepare() {
  setDatabaseName(_databaseName);

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

std::unique_ptr<SimpleHttpClient> ClientFeature::createHttpClient(
    size_t threadNumber, bool suppressError) const {
  std::string endpoint;
  {
    READ_LOCKER(locker, _settingsLock);
    endpoint = _endpoints[threadNumber % _endpoints.size()];
  }
  return createHttpClient(endpoint, suppressError);
}

std::unique_ptr<SimpleHttpClient> ClientFeature::createHttpClient(
    std::string const& definition, bool suppressError) const {
  double requestTimeout;
  bool warn;
  {
    READ_LOCKER(locker, _settingsLock);
    requestTimeout = _requestTimeout;
    warn = _warn;
  }
  SimpleHttpClientParams params(requestTimeout, warn);
  params.setCompressRequestThreshold(
      compressTransfer() ? compressRequestThreshold() : 0);
  return createHttpClient(definition, std::move(params), suppressError);
}

std::unique_ptr<httpclient::SimpleHttpClient> ClientFeature::createHttpClient(
    std::string const& definition, SimpleHttpClientParams const& params,
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
      GeneralClientConnection::factory(_comm, endpoint, _requestTimeout,
                                       _connectionTimeout, _retries,
                                       _sslProtocol));

  // takes over ownership for the connection object
  auto httpClient = std::make_unique<SimpleHttpClient>(connection, params);
  // set client parameters
  httpClient->params().setLocationRewriter(static_cast<void const*>(this),
                                           &ClientManager::rewriteLocation);
  httpClient->params().setUserNamePassword("/", _username, _password);
  if (!_jwtSecret.empty()) {
    TRI_ASSERT(!_endpoints.empty());
    httpClient->params().setJwt(
        fuerte::jwt::generateInternalToken(_jwtSecret, _endpoints[0]));
  }

  return httpClient;
}

std::vector<std::string> ClientFeature::httpEndpoints() {
  std::vector<std::string> httpEndpoints;

  READ_LOCKER(locker, _settingsLock);
  std::for_each(_endpoints.begin(), _endpoints.end(),
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
  return _databaseName;
}

void ClientFeature::setDatabaseName(std::string_view databaseName) {
  if (auto res = DatabaseNameValidator::validateName(true, true, databaseName);
      res.fail()) {
    THROW_ARANGO_EXCEPTION(res);
  }

  WRITE_LOCKER(locker, _settingsLock);
  _databaseName = databaseName;
}

bool ClientFeature::authentication() const noexcept {
  READ_LOCKER(locker, _settingsLock);
  return _authentication;
}

// get single endpoint. used by client tools that can handle only one endpoint
std::string ClientFeature::endpoint() const {
  READ_LOCKER(locker, _settingsLock);
  return _endpoints[0];
}

// set single endpoint
void ClientFeature::setEndpoint(std::string_view value) {
  WRITE_LOCKER(locker, _settingsLock);
  _endpoints[0] = value;
}

std::string ClientFeature::username() const {
  READ_LOCKER(locker, _settingsLock);
  return _username;
}

void ClientFeature::setUsername(std::string_view value) {
  WRITE_LOCKER(locker, _settingsLock);
  _username = value;
}

std::string ClientFeature::password() const {
  READ_LOCKER(locker, _settingsLock);
  return _password;
}

void ClientFeature::setPassword(std::string_view value) {
  WRITE_LOCKER(locker, _settingsLock);
  _password = value;
}

std::string ClientFeature::jwtSecret() const {
  READ_LOCKER(locker, _settingsLock);
  return _jwtSecret;
}

void ClientFeature::setJwtSecret(std::string_view jwtSecret) {
  WRITE_LOCKER(locker, _settingsLock);
  _jwtSecret = jwtSecret;
}

double ClientFeature::connectionTimeout() const noexcept {
  READ_LOCKER(locker, _settingsLock);
  return _connectionTimeout;
}

double ClientFeature::requestTimeout() const noexcept {
  READ_LOCKER(locker, _settingsLock);
  return _requestTimeout;
}

void ClientFeature::requestTimeout(double value) noexcept {
  WRITE_LOCKER(locker, _settingsLock);
  _requestTimeout = value;
}

uint64_t ClientFeature::maxPacketSize() const noexcept {
  READ_LOCKER(locker, _settingsLock);
  return _maxPacketSize;
}

uint64_t ClientFeature::sslProtocol() const noexcept {
  READ_LOCKER(locker, _settingsLock);
  return _sslProtocol;
}

bool ClientFeature::askJwtSecret() const noexcept {
  READ_LOCKER(locker, _settingsLock);
  return _askJwtSecret;
}

bool ClientFeature::forceJson() const noexcept {
  READ_LOCKER(locker, _settingsLock);
  return _forceJson;
}

void ClientFeature::setForceJson(bool value) noexcept {
  WRITE_LOCKER(locker, _settingsLock);
  _forceJson = value;
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
  return _compressTransfer;
}

void ClientFeature::setCompressTransfer(bool value) noexcept {
  WRITE_LOCKER(locker, _settingsLock);
  _compressTransfer = value;
}

uint64_t ClientFeature::compressRequestThreshold() const noexcept {
  READ_LOCKER(locker, _settingsLock);
  return _compressRequestThreshold;
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
