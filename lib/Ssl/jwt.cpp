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
/// @author Simon Grätzer
////////////////////////////////////////////////////////////////////////////////

#include "jwt.h"

#include "Ssl/SslInterface.h"

#include <absl/strings/escaping.h>
#include <absl/strings/internal/escaping.h>
#include <absl/strings/str_cat.h>
#include <velocypack/Builder.h>
#include <velocypack/Slice.h>
#include <openssl/evp.h>
#include <openssl/pem.h>

#include <chrono>

namespace arangodb::rest::SslInterface::jwt {

/// generate a JWT token for internal cluster communication
std::string generateInternalToken(std::string_view secret,
                                  std::string_view id) {
  std::chrono::seconds iss = std::chrono::duration_cast<std::chrono::seconds>(
      std::chrono::system_clock::now().time_since_epoch());

  velocypack::Builder bodyBuilder;
  bodyBuilder.openObject();
  bodyBuilder.add("server_id", velocypack::Value(id));
  bodyBuilder.add("iss", velocypack::Value("arangodb"));
  bodyBuilder.add("iat", velocypack::Value(static_cast<uint64_t>(iss.count())));
  bodyBuilder.close();
  return generateRawJwt(secret, bodyBuilder.slice());
}

/// Generate JWT token as used for 'users' in arangodb
std::string generateUserToken(std::string_view secret,
                              std::string_view username,
                              std::chrono::seconds validFor) {
  std::chrono::seconds iss = std::chrono::duration_cast<std::chrono::seconds>(
      std::chrono::system_clock::now().time_since_epoch());

  velocypack::Builder bodyBuilder;
  bodyBuilder.openObject(/*unindexed*/ true);
  bodyBuilder.add("preferred_username", velocypack::Value(username));
  bodyBuilder.add("iss", velocypack::Value("arangodb"));
  bodyBuilder.add("iat", velocypack::Value(static_cast<uint64_t>(iss.count())));
  if (validFor.count() > 0) {
    bodyBuilder.add("exp", velocypack::Value(static_cast<uint64_t>((iss + validFor).count())));
  }
  bodyBuilder.close();
  return generateRawJwt(secret, bodyBuilder.slice());
}

std::string generateRawJwt(std::string_view secret, velocypack::Slice bodySlice) {
  // Detect if the secret is an ES256 private key by trying to parse it as PEM
  bool isES256 = false;
  if (secret.size() > 26 && secret.find("-----BEGIN") != std::string_view::npos) {
    // Try to parse as EC private key
    BIO* keybio = BIO_new_mem_buf(secret.data(), static_cast<int>(secret.size()));
    if (keybio != nullptr) {
      EVP_PKEY* pKey = PEM_read_bio_PrivateKey(keybio, nullptr, nullptr, nullptr);
      if (pKey != nullptr) {
        if (EVP_PKEY_base_id(pKey) == EVP_PKEY_EC) {
          isES256 = true;
        }
        EVP_PKEY_free(pKey);
      }
      BIO_free_all(keybio);
    }
  }

  // Build the header with the appropriate algorithm
  velocypack::Builder headerBuilder;
  {
    velocypack::ObjectBuilder h(&headerBuilder);
    headerBuilder.add("alg", velocypack::Value(isES256 ? "ES256" : "HS256"));
    headerBuilder.add("typ", velocypack::Value("JWT"));
  }

  // https://tools.ietf.org/html/rfc7515#section-2 requires
  // JWT to use base64-encoding without trailing padding `=` chars

  auto header = headerBuilder.toJson();
  auto body = bodySlice.toJson();
  std::string headerBase64;
  std::string bodyBase64;

  absl::strings_internal::Base64EscapeInternal(
      reinterpret_cast<unsigned char const*>(header.data()), header.size(),
      &headerBase64, false, absl::strings_internal::kBase64Chars);
  absl::strings_internal::Base64EscapeInternal(
      reinterpret_cast<unsigned char const*>(body.data()), body.size(),
      &bodyBase64, false, absl::strings_internal::kBase64Chars);

  auto fullMessage = absl::StrCat(headerBase64, ".", bodyBase64);

  std::string signature;
  
  if (isES256) {
    // Use ES256 signing
    std::string error;
    if (arangodb::rest::SslInterface::signES256(secret, fullMessage, signature, error) != 0) {
      // If ES256 signing fails, return empty string
      return "";
    }
  } else {
    // Use HMAC-SHA256
    signature = arangodb::rest::SslInterface::sslHMAC(
        secret.data(), secret.size(), fullMessage.data(),
        fullMessage.size(), Algorithm::ALGORITHM_SHA256);
  }

  return absl::StrCat(fullMessage, ".", absl::WebSafeBase64Escape(signature));
}

}  // namespace arangodb::rest::SslInterface::jwt
