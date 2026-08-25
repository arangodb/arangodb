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

#include "Ssl/AuthInfo.h"
#include "Assertions/ProdAssert.h"
#include "Basics/FileUtils.h"
#include "Basics/files.h"
#include "Basics/StringUtils.h"
#include "Basics/overload.h"
#include "Logger/Logger.h"
#include "Logger/LogMacros.h"
#include "Random/RandomGenerator.h"
#include "Ssl/JwtKeys.h"
#include "Ssl/SslInterface.h"
#include "Ssl/jwt.h"
#include "Basics/ResultT.h"

#include <openssl/bio.h>
#include <openssl/evp.h>
#include <openssl/pem.h>

#include <string>
#include <filesystem>
#include <ranges>
#include <algorithm>

namespace arangodb::auth {

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

auto loadKeyString(std::string contents) -> ResultT<AuthKey> {
  // Check if this is an ES256 key (PEM format)
  if (contents.empty()) {
    LOG_DEVEL << "trying to load empty key";
    //    return Result(TRI_ERROR_BAD_PARAMETER, "empty key given");
  }
  if (isPemFormat(contents)) {
    // The active secret must be a private key for signing JWT tokens
    if (isEcPrivateKey(contents)) {
      return ES256PrivateKey(std::move(contents));
    } else if (isEcPublicKey(contents)) {
      return ES256PublicKey(std::move(contents));
    } else {
      return Result(TRI_ERROR_BAD_PARAMETER,
                    "PEM file detected but does not contain a valid EC key");
    }
  } else {
    // Non-PEM format, treat it as HS256
    if (contents.length() < HS256Key::kMinSecretLength) {
      LOG_DEVEL << "TOO SHORT KEY: length " << contents.length();
      //      return Result(TRI_ERROR_BAD_PARAMETER,
      //              "Given JWT secret too short. Minimal length is 32.");
    } else if (contents.length() > HS256Key::kMaxSecretLength) {
      return Result(TRI_ERROR_BAD_PARAMETER,
                    "Given JWT secret too long. Maximal length is 64.");
    }
    return HS256Key(std::move(contents));
  }
}

auto loadKeyfile(std::filesystem::path filename) -> ResultT<AuthKey> try {
  // Note that the secret is trimmed for whitespace, because whitespace
  // at the end of a file can easily happen. We do not base64-encode,
  // though, so the bytes count as given. Zero bytes might be a problem
  // here.
  auto contents = basics::FileUtils::slurp(filename);
  contents = basics::StringUtils::trim(contents, " \t\n\r");

  return loadKeyString(contents);
} catch (std::exception const& ex) {
  auto msg = std::format(
      "unable to read content of jwt-secret file '{}': {}. Please make sure "
      "the file is readable for the arangod process.",
      filename.string(), ex.what());
  return Result(TRI_ERROR_CANNOT_READ_FILE, std::move(msg));
}

}  // namespace

auto loadJwtSecretString(std::string key) -> ResultT<AuthInfo> {
  auto r = loadKeyString(key);
  if (!r.ok()) {
    return r.result();
  }
  return AuthInfo{.activeSecret = std::move(r.get()), .passiveSecrets = {}};
}

/// load JWT secret from file specified at startup
auto loadJwtSecretFile(std::filesystem::path filename) -> ResultT<AuthInfo> {
  auto r = loadKeyfile(filename);

  if (!r.ok()) {
    return r.result();
  }

  if (std::holds_alternative<ES256PublicKey>(r.get())) {
    return Result(TRI_ERROR_BAD_PARAMETER,
                  "JWT secret keyfile contains an ES256 public key, but a "
                  "private key is required for signing JWT tokens needed for "
                  "intra-cluster communication");
  }

  return AuthInfo{.activeSecret = std::move(r.get()), .passiveSecrets = {}};
}

/// load JWT secrets from folder
auto loadJwtSecretFolder(std::filesystem::path path) -> ResultT<AuthInfo> {
  TRI_ASSERT(!path.empty());

  LOG_TOPIC("4922f", INFO, arangodb::Logger::AUTHENTICATION)
      << "loading JWT secrets from folder " << path.string();

  auto list = basics::FileUtils::listFiles(path);
  std::ranges::sort(list);
  auto f =
      list |  //
      std::views::filter([](std::string const& file) {
        return not(file.empty() or file[0] == '.' or file.ends_with(".tmp"));
      });

  if (f.empty()) {
    return Result(TRI_ERROR_BAD_PARAMETER,
                  std::format("no viable JWT secrets found in directory: `{}`",
                              path.string()));
  }

  auto r =
      loadJwtSecretFile(basics::FileUtils::buildFilename(path, list.at(0)));
  if (!r.ok()) {
    return r.result();
  }
  auto result = r.get();

  std::vector<AuthKey> passiveSecrets;
  for (auto&& file : f | std::views::drop(1)) {
    auto r = loadKeyfile(basics::FileUtils::buildFilename(path, file));

    if (!r.ok()) {
      return r.result();
    }

    auto key = r.get();

    std::visit(
        overload{
            [&file](ES256PublicKey& key) {
              LOG_TOPIC("4922b", INFO, arangodb::Logger::AUTHENTICATION)
                  << "Adding ES256 public key to passive secrets for "
                     "verification: "
                  << file;
            },
            [&file](ES256PrivateKey& key) {
              LOG_TOPIC("4922c", INFO, arangodb::Logger::AUTHENTICATION)
                  << "Adding ES256 private key to passive secrets for "
                     "verification: "
                  << file;
            },
            [&file](HS256Key& key) {
              LOG_TOPIC("4922d", INFO, arangodb::Logger::AUTHENTICATION)
                  << "Adding HS256  key to passive secrets for verification: "
                  << file;
            }},
        key);
    result.passiveSecrets.push_back(std::move(key));
  }

  LOG_TOPIC("4a34f", INFO, arangodb::Logger::AUTHENTICATION)
      << "have " << result.passiveSecrets.size() << " passive JWT secrets";

  return result;
}

auto generateRandomHS256AuthInfo() -> AuthInfo {
  uint16_t m = 254;
  auto result = std::string();
  result.reserve(HS256Key::kMaxSecretLength);
  for (size_t i = 0; i < HS256Key::kMaxSecretLength; i++) {
    result += static_cast<char>(1 + RandomGenerator::interval(m));
  }
  return AuthInfo{.activeSecret = HS256Key{result}, .passiveSecrets = {}};
}

}  // namespace arangodb::auth
