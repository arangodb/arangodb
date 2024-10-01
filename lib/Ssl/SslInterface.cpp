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
/// @author Dr. Oreste Costa-Panaia
////////////////////////////////////////////////////////////////////////////////

#include "SslInterface.h"

#include "Basics/Exceptions.h"
#include "Basics/ScopeGuard.h"
#include "Basics/StringUtils.h"
#include "Basics/memory.h"
#include "Random/UniformCharacter.h"

#include <cstring>
#include <openssl/err.h>
#include <openssl/evp.h>
#include <openssl/hmac.h>
#include <openssl/md5.h>
#include <openssl/pem.h>
#include <openssl/rand.h>
#include <openssl/sha.h>
#include <iostream>

#ifdef OPENSSL_NO_SSL2  // OpenSSL > 1.1.0 deprecates RAND_pseudo_bytes
#define RAND_BYTES RAND_bytes
#else
#define RAND_BYTES RAND_pseudo_bytes
#endif

using namespace arangodb;
using namespace arangodb::basics;

// -----------------------------------------------------------------------------
// helper
// -----------------------------------------------------------------------------

namespace {
static UniformCharacter SaltGenerator(
    "abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789!@#$%^&*(){}"
    "[]:;<>,.?/|");
}

namespace arangodb::rest::SslInterface {

// -----------------------------------------------------------------------------
// public methods
// -----------------------------------------------------------------------------

std::string sslMD5(std::string_view input) {
  char hash[16];
  sslMD5(input.data(), input.size(), &hash[0]);

  char hex[32];
  SslInterface::sslHEX(hash, 16, &hex[0]);

  return std::string(hex, 32);
}

void sslMD5(char const* inputStr, size_t length, char* outputStr) noexcept {
  TRI_ASSERT(inputStr != nullptr);
  TRI_ASSERT(outputStr != nullptr);
  MD5((const unsigned char*)inputStr, length, (unsigned char*)outputStr);
}

void sslSHA1(char const* inputStr, size_t length, char* outputStr) noexcept {
  SHA1((const unsigned char*)inputStr, length, (unsigned char*)outputStr);
}

void sslSHA224(char const* inputStr, size_t length, char* outputStr) noexcept {
  TRI_ASSERT(inputStr != nullptr);
  TRI_ASSERT(outputStr != nullptr);
  SHA224((const unsigned char*)inputStr, length, (unsigned char*)outputStr);
}

void sslSHA256(char const* inputStr, size_t length, char* outputStr) noexcept {
  TRI_ASSERT(inputStr != nullptr);
  TRI_ASSERT(outputStr != nullptr);
  SHA256((const unsigned char*)inputStr, length, (unsigned char*)outputStr);
}

void sslSHA384(char const* inputStr, size_t length, char* outputStr) noexcept {
  TRI_ASSERT(inputStr != nullptr);
  TRI_ASSERT(outputStr != nullptr);
  SHA384((const unsigned char*)inputStr, length, (unsigned char*)outputStr);
}

void sslSHA512(char const* inputStr, size_t length, char* outputStr) noexcept {
  TRI_ASSERT(inputStr != nullptr);
  TRI_ASSERT(outputStr != nullptr);
  SHA512((const unsigned char*)inputStr, length, (unsigned char*)outputStr);
}

void sslHEX(char const* inputStr, size_t length, char* outputStr) noexcept {
  TRI_ASSERT(inputStr != nullptr);
  TRI_ASSERT(outputStr != nullptr);

  constexpr char hexval[16] = {'0', '1', '2', '3', '4', '5', '6', '7',
                               '8', '9', 'a', 'b', 'c', 'd', 'e', 'f'};

  char const* e = inputStr + length;
  char* p = outputStr;

  for (char const* q = inputStr; q < e; ++q) {
    *p++ = hexval[(*q >> 4) & 0xF];
    *p++ = hexval[*q & 0x0F];
  }
  TRI_ASSERT(static_cast<size_t>(p - outputStr) == 2 * length);
}

std::string sslPBKDF2HS1(char const* salt, size_t saltLength, char const* pass,
                         size_t passLength, int iter, int keyLength) {
  unsigned char* dk = (unsigned char*)TRI_Allocate(keyLength + 1);
  if (dk == nullptr) {
    THROW_ARANGO_EXCEPTION(TRI_ERROR_OUT_OF_MEMORY);
  }

  auto sg = arangodb::scopeGuard([&]() noexcept { TRI_Free(dk); });

  PKCS5_PBKDF2_HMAC_SHA1(pass, (int)passLength, (const unsigned char*)salt,
                         (int)saltLength, iter, keyLength, dk);

  // return value as hex
  return StringUtils::encodeHex((char*)dk, keyLength);
}

std::string sslPBKDF2(char const* salt, size_t saltLength, char const* pass,
                      size_t passLength, int iter, int keyLength,
                      Algorithm algorithm) {
  EVP_MD* evp_md = nullptr;

  if (algorithm == Algorithm::ALGORITHM_SHA1) {
    evp_md = const_cast<EVP_MD*>(EVP_sha1());
  } else if (algorithm == Algorithm::ALGORITHM_SHA224) {
    evp_md = const_cast<EVP_MD*>(EVP_sha224());
  } else if (algorithm == Algorithm::ALGORITHM_MD5) {
    evp_md = const_cast<EVP_MD*>(EVP_md5());
  } else if (algorithm == Algorithm::ALGORITHM_SHA384) {
    evp_md = const_cast<EVP_MD*>(EVP_sha384());
  } else if (algorithm == Algorithm::ALGORITHM_SHA512) {
    evp_md = const_cast<EVP_MD*>(EVP_sha512());
  } else {
    // default
    evp_md = const_cast<EVP_MD*>(EVP_sha256());
  }

  unsigned char* dk = (unsigned char*)TRI_Allocate(keyLength + 1);
  if (dk == nullptr) {
    THROW_ARANGO_EXCEPTION(TRI_ERROR_OUT_OF_MEMORY);
  }

  auto sg = arangodb::scopeGuard([&]() noexcept { TRI_Free(dk); });

  PKCS5_PBKDF2_HMAC(pass, (int)passLength, (const unsigned char*)salt,
                    (int)saltLength, iter, evp_md, keyLength, dk);

  // return value as hex
  return StringUtils::encodeHex((char*)dk, keyLength);
}

std::string sslHMAC(char const* key, size_t keyLength, char const* message,
                    size_t messageLen, Algorithm algorithm) {
  EVP_MD* evp_md = nullptr;

  if (algorithm == Algorithm::ALGORITHM_SHA1) {
    evp_md = const_cast<EVP_MD*>(EVP_sha1());
  } else if (algorithm == Algorithm::ALGORITHM_SHA224) {
    evp_md = const_cast<EVP_MD*>(EVP_sha224());
  } else if (algorithm == Algorithm::ALGORITHM_MD5) {
    evp_md = const_cast<EVP_MD*>(EVP_md5());
  } else if (algorithm == Algorithm::ALGORITHM_SHA384) {
    evp_md = const_cast<EVP_MD*>(EVP_sha384());
  } else if (algorithm == Algorithm::ALGORITHM_SHA512) {
    evp_md = const_cast<EVP_MD*>(EVP_sha512());
  } else {
    // default
    evp_md = const_cast<EVP_MD*>(EVP_sha256());
  }

  unsigned char* md = (unsigned char*)TRI_Allocate(EVP_MAX_MD_SIZE + 1);
  if (md == nullptr) {
    THROW_ARANGO_EXCEPTION(TRI_ERROR_OUT_OF_MEMORY);
  }

  auto sg = arangodb::scopeGuard([&]() noexcept { TRI_Free(md); });
  unsigned int md_len;

  HMAC(evp_md, key, (int)keyLength, (const unsigned char*)message, messageLen,
       md, &md_len);

  return std::string((char*)md, md_len);
}

bool verifyHMAC(char const* challenge, size_t challengeLength,
                char const* secret, size_t secretLen, char const* response,
                size_t responseLen, Algorithm algorithm) {
  // challenge = key
  // secret, secretLen = message
  // result must == BASE64(response, responseLen)

  std::string s =
      sslHMAC(challenge, challengeLength, secret, secretLen, algorithm);

  if (s.length() == responseLen &&
      s.compare(std::string(response, responseLen)) == 0) {
    return true;
  }

  return false;
}

int sslRand(uint64_t* value) {
  if (!RAND_BYTES((unsigned char*)value, sizeof(uint64_t))) {
    return 1;
  }

  return 0;
}

int sslRand(int64_t* value) {
  if (!RAND_BYTES((unsigned char*)value, sizeof(int64_t))) {
    return 1;
  }

  return 0;
}

int sslRand(int32_t* value) {
  if (!RAND_BYTES((unsigned char*)value, sizeof(int32_t))) {
    return 1;
  }

  return 0;
}

int rsaPrivSign(EVP_MD_CTX* ctx, EVP_PKEY* pkey, std::string const& msg,
                std::string& sign, std::string& error) {
  size_t signLength;
  if (EVP_DigestSignInit(ctx, nullptr, EVP_sha256(), nullptr, pkey) == 0) {
    error.append("EVP_DigestSignInit failed: ")
        .append(ERR_error_string(ERR_get_error(), nullptr));
    return 1;
  }
  if (EVP_DigestSignUpdate(ctx, msg.c_str(), msg.size()) == 0) {
    error.append("EVP_DigestSignUpdate failed: ")
        .append(ERR_error_string(ERR_get_error(), nullptr));
    return 1;
  }
  if (EVP_DigestSignFinal(ctx, nullptr, &signLength) == 0) {
    error.append("EVP_DigestSignFinal failed: ")
        .append(ERR_error_string(ERR_get_error(), nullptr));
    return 1;
  }
  sign.resize(signLength);
  if (EVP_DigestSignFinal(ctx, (unsigned char*)sign.data(), &signLength) == 0) {
    error.append("EVP_DigestSignFinal failed, return code: ")
        .append(ERR_error_string(ERR_get_error(), nullptr));
    return 1;
  }
  return 0;
}

int rsaPrivSign(std::string const& pem, std::string const& msg,
                std::string& sign, std::string& error) {
  BIO* keybio = BIO_new_mem_buf(pem.c_str(), -1);
  if (keybio == nullptr) {
    error.append("Failed to initialize keybio.");
    return 1;
  }
  auto cleanupBio = scopeGuard([&]() noexcept { BIO_free_all(keybio); });

  RSA* rsa = RSA_new();
  if (rsa == nullptr) {
    error.append("Failed to initialize RSA algorithm.");
    return 1;
  }
  rsa = PEM_read_bio_RSAPrivateKey(keybio, &rsa, nullptr, nullptr);
  EVP_PKEY* pKey = EVP_PKEY_new();
  if (pKey == nullptr) {
    error.append("Failed to initialize public key.");
    return 1;
  }
  EVP_PKEY_assign_RSA(pKey, rsa);
  auto cleanupKeys = scopeGuard([&]() noexcept { EVP_PKEY_free(pKey); });

  auto* ctx = EVP_MD_CTX_new();
  auto cleanupContext = scopeGuard([&]() noexcept { EVP_MD_CTX_free(ctx); });
  if (ctx == nullptr) {
    error.append("EVP_MD_CTX_create failed,: ")
        .append(ERR_error_string(ERR_get_error(), nullptr));
    return 1;
  }

  return rsaPrivSign(ctx, pKey, msg, sign, error);
}

}  // namespace arangodb::rest::SslInterface
