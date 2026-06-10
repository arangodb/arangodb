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
/// @author Andreas Streichardt <andreas@arangodb.com>
////////////////////////////////////////////////////////////////////////////////

#include "AuthenticationFeature.h"
#include "AuthenticationOptionsProvider.h"

#include "ApplicationFeatures/ApplicationServer.h"
#include "FeaturePhases/BasicFeaturePhaseServer.h"
#include "Auth/Handler.h"
#include "Auth/TokenCache.h"
#include "Auth/UserManagerImpl.h"
#include "Basics/FileUtils.h"
#include "Basics/StringUtils.h"
#include "Basics/application-exit.h"
#include "Cluster/ServerState.h"
#include "Logger/LogMacros.h"
#include "Logger/Logger.h"
#include "Logger/LoggerStream.h"
#include "ProgramOptions/Parameters.h"
#include "ProgramOptions/ProgramOptions.h"
#include "Random/RandomGenerator.h"
#include "RestServer/QueryRegistryFeature.h"

#include <limits>

#include <openssl/bio.h>
#include <openssl/evp.h>
#include <openssl/pem.h>
#include <filesystem>

using namespace arangodb::options;

namespace arangodb {

namespace {
/// @brief Check if a string contains a PEM-formatted key
bool isPemFormat(std::string const& content) {
  return content.find("-----BEGIN") != std::string::npos &&
         content.find("-----END") != std::string::npos;
}

/// @brief Check if a PEM file contains an EC private key
bool isEcPrivateKey(std::string const& pemContent) {
  BIO* bio =
      BIO_new_mem_buf(pemContent.c_str(), static_cast<int>(pemContent.size()));
  if (!bio) {
    return false;
  }

  EVP_PKEY* pkey = PEM_read_bio_PrivateKey(bio, nullptr, nullptr, nullptr);
  BIO_free(bio);

  if (!pkey) {
    return false;
  }

  bool isEc = EVP_PKEY_base_id(pkey) == EVP_PKEY_EC;
  EVP_PKEY_free(pkey);

  return isEc;
}

/// @brief Check if a PEM file contains an EC public key
bool isEcPublicKey(std::string const& pemContent) {
  BIO* bio =
      BIO_new_mem_buf(pemContent.c_str(), static_cast<int>(pemContent.size()));
  if (!bio) {
    return false;
  }

  EVP_PKEY* pkey = PEM_read_bio_PUBKEY(bio, nullptr, nullptr, nullptr);
  BIO_free(bio);

  if (!pkey) {
    return false;
  }

  bool isEc = EVP_PKEY_base_id(pkey) == EVP_PKEY_EC;
  EVP_PKEY_free(pkey);

  return isEc;
}
}  // namespace

std::atomic<AuthenticationFeature*> AuthenticationFeature::INSTANCE = nullptr;

AuthenticationFeature::AuthenticationFeature(
    application_features::ApplicationServer& server)
    : ApplicationFeature{server, *this},
      _userManager(nullptr),
      _authCache(nullptr) {
  setOptional(false);
  startsAfter<application_features::BasicFeaturePhaseServer>();
}

AuthenticationFeature::~AuthenticationFeature() = default;

void AuthenticationFeature::collectOptions(
    std::shared_ptr<ProgramOptions> options) {
  AuthenticationOptionsProvider provider;
  provider.declareOptions(options, _options);
}

void AuthenticationFeature::validateOptions(
    std::shared_ptr<ProgramOptions> options) {
  if (!_options.jwtSecretKeyfileProgramOption.empty() &&
      !_options.jwtSecretFolderProgramOption.empty()) {
    LOG_TOPIC("d3515", FATAL, Logger::STARTUP)
        << "please specify either '--server.jwt-"
           "secret-keyfile' or '--server.jwt-secret-folder' but not both.";
    FATAL_ERROR_EXIT();
  }

  if (!_options.jwtSecretKeyfileProgramOption.empty() ||
      !_options.jwtSecretFolderProgramOption.empty()) {
    Result res = loadJwtSecretsFromFile();
    if (res.fail()) {
      LOG_TOPIC("d3617", FATAL, Logger::STARTUP) << res.errorMessage();
      FATAL_ERROR_EXIT();
    }
  }
  if (!_options.jwtSecretProgramOption.empty()) {
    // Only check length for non-PEM (HS256) secrets
    // ES256 keys in PEM format can be longer
    if (!_options.jwtSecretIsES256 &&
        _options.jwtSecretProgramOption.length() > kMaxSecretLength) {
      LOG_TOPIC("9abfc", FATAL, arangodb::Logger::STARTUP)
          << "Given JWT secret too long. Max length is " << kMaxSecretLength
          << " have " << _options.jwtSecretProgramOption.length();
      FATAL_ERROR_EXIT();
    }
  }

  if (options->processingResult().touched("server.jwt-secret")) {
    LOG_TOPIC("1aaae", WARN, arangodb::Logger::AUTHENTICATION)
        << "--server.jwt-secret is insecure. Use --server.jwt-secret-keyfile "
           "instead.";
  }

  AuthenticationOptionsProvider provider;
  provider.validateOptions(options, _options);
}

void AuthenticationFeature::prepare() {
  TRI_ASSERT(isEnabled());
  TRI_ASSERT(_userManager == nullptr);

  ServerState::RoleEnum role = ServerState::instance()->getRole();
  TRI_ASSERT(role != ServerState::RoleEnum::ROLE_UNDEFINED);
  if (ServerState::isSingleServer(role) || ServerState::isCoordinator(role)) {
    if (_userManager == nullptr) {
      _userManager = std::make_unique<auth::UserManagerImpl>(server());
    }

    TRI_ASSERT(_userManager != nullptr);
  } else {
    LOG_TOPIC("713c0", DEBUG, Logger::AUTHENTICATION)
        << "Not creating user manager";
  }

  TRI_ASSERT(_authCache == nullptr);
  _authCache = std::make_unique<auth::TokenCache>(
      _userManager.get(), _options.authenticationTimeout);

  if (_options.jwtSecretProgramOption.empty()) {
    LOG_TOPIC("43396", INFO, Logger::AUTHENTICATION)
        << "Jwt secret not specified, generating...";
    uint16_t m = 254;
    for (size_t i = 0; i < kMaxSecretLength; i++) {
      _options.jwtSecretProgramOption +=
          static_cast<char>(1 + RandomGenerator::interval(m));
    }
    _options.jwtSecretIsES256 = false;  // generated secrets are always HS256
  }

  _authCache->setJwtSecrets(_options.jwtSecretProgramOption,
                            _options.jwtPassiveSecrets,
                            _options.jwtSecretIsES256);

  INSTANCE.store(this, std::memory_order_release);
}

void AuthenticationFeature::start() {
  TRI_ASSERT(isEnabled());
  std::ostringstream out;

  out << "Authentication is turned " << (_options.active ? "on" : "off");

  if (_options.active && _options.authenticationSystemOnly) {
    out << " (system only)";
  }

#ifdef ARANGODB_HAVE_DOMAIN_SOCKETS
  out << ", authentication for unix sockets is turned "
      << (_options.authenticationUnixSockets ? "on" : "off");
#endif

  LOG_TOPIC("3844e", INFO, arangodb::Logger::AUTHENTICATION) << out.str();
}

void AuthenticationFeature::stop() {
  if (_userManager) {
    _userManager->shutdown();
  }
}

void AuthenticationFeature::unprepare() {
  INSTANCE.store(nullptr, std::memory_order_relaxed);
}

AuthenticationFeature* AuthenticationFeature::instance() noexcept {
  return INSTANCE.load(std::memory_order_acquire);
}

bool AuthenticationFeature::isActive() const noexcept {
  return _options.active && isEnabled();
}

bool AuthenticationFeature::authenticationUnixSockets() const noexcept {
  return _options.authenticationUnixSockets;
}

bool AuthenticationFeature::authenticationSystemOnly() const noexcept {
  return _options.authenticationSystemOnly;
}

std::string_view AuthenticationFeature::externalRBACservice() const noexcept {
  return _options.externalRBACservice;
}

/// @return Cache to deal with authentication tokens
auth::TokenCache& AuthenticationFeature::tokenCache() const noexcept {
  TRI_ASSERT(_authCache);
  return *_authCache.get();
}

/// @brief user manager may be null on DBServers and Agency
/// @return user manager singleton
auth::UserManager* AuthenticationFeature::userManager() const noexcept {
  return _userManager.get();
}

bool AuthenticationFeature::hasUserdefinedJwt() const {
  std::lock_guard<std::mutex> guard(_jwtSecretsLock);
  return !_options.jwtSecretProgramOption.empty();
}

/// verification only secrets
std::tuple<std::string, std::vector<std::string>, bool>
AuthenticationFeature::jwtSecrets() const {
  std::lock_guard<std::mutex> guard(_jwtSecretsLock);
  return {_options.jwtSecretProgramOption, _options.jwtPassiveSecrets,
          _options.jwtSecretIsES256};
}

Result AuthenticationFeature::loadJwtSecretsFromFile() {
  std::lock_guard<std::mutex> guard(_jwtSecretsLock);
  if (!_options.jwtSecretFolderProgramOption.empty()) {
    return loadJwtSecretFolder();
  } else if (!_options.jwtSecretKeyfileProgramOption.empty()) {
    return loadJwtSecretKeyfile();
  }
  return Result(TRI_ERROR_BAD_PARAMETER, "no JWT secret file was specified");
}

#ifdef ARANGODB_USE_GOOGLE_TESTS
void AuthenticationFeature::setUserManager(
    std::unique_ptr<auth::UserManager> um) {
  _userManager.swap(um);
}
#endif  // ARANGODB_USE_GOOGLE_TESTS

/// load JWT secret from file specified at startup
Result AuthenticationFeature::loadJwtSecretKeyfile() {
  try {
    // Note that the secret is trimmed for whitespace, because whitespace
    // at the end of a file can easily happen. We do not base64-encode,
    // though, so the bytes count as given. Zero bytes might be a problem
    // here.
    std::string contents =
        basics::FileUtils::slurp(_options.jwtSecretKeyfileProgramOption);
    _options.jwtSecretProgramOption =
        basics::StringUtils::trim(contents, " \t\n\r");

    // Check if this is an ES256 key (PEM format)
    if (isPemFormat(_options.jwtSecretProgramOption)) {
      // The active secret must be a private key for signing JWT tokens
      if (isEcPrivateKey(_options.jwtSecretProgramOption)) {
        _options.jwtSecretIsES256 = true;
        LOG_TOPIC("4923e", INFO, arangodb::Logger::AUTHENTICATION)
            << "Detected ES256 private key in JWT secret keyfile (for signing "
               "and "
               "verification)";
      } else if (isEcPublicKey(_options.jwtSecretProgramOption)) {
        return Result(
            TRI_ERROR_BAD_PARAMETER,
            "JWT secret keyfile contains an ES256 public key, but a "
            "private key is required for signing JWT tokens needed for "
            "intra-cluster communication");
      } else {
        return Result(TRI_ERROR_BAD_PARAMETER,
                      "PEM file detected but does not contain a valid EC key");
      }
    } else {
      // Non-PEM format, must be HS256
      _options.jwtSecretIsES256 = false;
      // Check length limit for HS256 secrets
      if (_options.jwtSecretProgramOption.length() > kMaxSecretLength) {
        return Result(TRI_ERROR_BAD_PARAMETER,
                      "Given JWT secret too long. Max length is 64");
      }
    }
  } catch (std::exception const& ex) {
    std::string msg("unable to read content of jwt-secret file '");
    msg.append(_options.jwtSecretKeyfileProgramOption)
        .append("': ")
        .append(ex.what())
        .append(". please make sure the file/directory is readable for the ")
        .append("arangod process and user");
    return Result(TRI_ERROR_CANNOT_READ_FILE, std::move(msg));
  }
  return Result();
}

/// load JWT secrets from folder
Result AuthenticationFeature::loadJwtSecretFolder() try {
  TRI_ASSERT(!_options.jwtSecretFolderProgramOption.empty());

  LOG_TOPIC("4922f", INFO, arangodb::Logger::AUTHENTICATION)
      << "loading JWT secrets from folder "
      << _options.jwtSecretFolderProgramOption;

  auto list =
      basics::FileUtils::listFiles(_options.jwtSecretFolderProgramOption);

  // filter out empty filenames, hidden files, tmp files and symlinks
  list.erase(std::remove_if(
                 list.begin(), list.end(),
                 [this](std::string const& file) {
                   if (file.empty() || file[0] == '.') {
                     return true;
                   }
                   if (file.ends_with(".tmp")) {
                     return true;
                   }
                   auto p =
                       std::filesystem::path(basics::FileUtils::buildFilename(
                           _options.jwtSecretFolderProgramOption, file));
                   if (std::filesystem::is_symlink(p)) {
                     return true;
                   }
                   return false;
                 }),
             list.end());

  if (list.empty()) {
    return Result(TRI_ERROR_BAD_PARAMETER, "empty JWT secrets directory");
  }

  auto slurpy = [&](std::string const& file) {
    auto p = basics::FileUtils::buildFilename(
        _options.jwtSecretFolderProgramOption, file);
    std::string contents = basics::FileUtils::slurp(p);
    return basics::StringUtils::trim(contents, " \t\n\r");
  };

  std::sort(std::begin(list), std::end(list));
  std::string activeSecret = slurpy(list[0]);

  // Check if the active secret is in PEM format (ES256) or raw bytes (HS256)
  bool isES256 = false;
  if (isPemFormat(activeSecret)) {
    // The active secret must be a private key for signing JWT tokens
    if (isEcPrivateKey(activeSecret)) {
      isES256 = true;
      LOG_TOPIC("4922e", INFO, arangodb::Logger::AUTHENTICATION)
          << "Detected ES256 private key in JWT secret file (for signing and "
             "verification)";
    } else if (isEcPublicKey(activeSecret)) {
      return Result(
          TRI_ERROR_BAD_PARAMETER,
          "First JWT secret file (active secret) contains an ES256 "
          "public key, but a private key is required for signing JWT "
          "tokens needed for intra-cluster communication. Public keys "
          "can only be used as passive secrets for verification.");
    } else {
      return Result(TRI_ERROR_BAD_PARAMETER,
                    "PEM file detected but does not contain a valid EC key");
    }
  }

  const std::string msg = "Given JWT secret too long. Max length is 64";
  if (!isES256 && activeSecret.length() > kMaxSecretLength) {
    return Result(TRI_ERROR_BAD_PARAMETER, msg);
  }

  _options.jwtSecretIsES256 = isES256;

  std::vector<std::string> passiveSecrets;
  if (list.size() > 1) {
    list.erase(list.begin());
    for (auto const& file : list) {
      std::string secret = slurpy(file);

      if (secret.empty()) {
        continue;  // ignore empty files
      }

      // Check if this is a PEM file
      if (isPemFormat(secret)) {
        // Accept both ES256 private and public keys for verification
        // Private keys can now be used for signature verification too
        if (isEcPrivateKey(secret)) {
          LOG_TOPIC("4922c", INFO, arangodb::Logger::AUTHENTICATION)
              << "Adding ES256 private key to passive secrets for "
                 "verification: "
              << file;
          passiveSecrets.push_back(std::move(secret));
          continue;
        }

        // Accept ES256 public keys for verification
        if (isEcPublicKey(secret)) {
          LOG_TOPIC("4922b", INFO, arangodb::Logger::AUTHENTICATION)
              << "Adding ES256 public key to passive secrets for verification: "
              << file;
          passiveSecrets.push_back(std::move(secret));
          continue;
        }

        // PEM file but not a valid EC key
        LOG_TOPIC("4922a", WARN, arangodb::Logger::AUTHENTICATION)
            << "Ignoring PEM file that does not contain a valid EC key: "
            << file;
        continue;
      }

      // For non-PEM (HS256) secrets, check the length limit
      if (secret.length() > kMaxSecretLength) {
        return Result(TRI_ERROR_BAD_PARAMETER, msg);
      }

      passiveSecrets.push_back(std::move(secret));
    }
  }
  _options.jwtPassiveSecrets = std::move(passiveSecrets);

  LOG_TOPIC("4a34f", INFO, arangodb::Logger::AUTHENTICATION)
      << "have " << _options.jwtPassiveSecrets.size() << " passive JWT secrets";

  _options.jwtSecretProgramOption = std::move(activeSecret);

  return Result();
} catch (basics::Exception const& ex) {
  std::string msg("unable to read content of jwt-secret-folder '");
  msg.append(_options.jwtSecretFolderProgramOption)
      .append("': ")
      .append(ex.what())
      .append(". please make sure the file/directory is readable for the ")
      .append("arangod process and user");
  return Result(TRI_ERROR_CANNOT_READ_FILE, std::move(msg));
}

}  // namespace arangodb
