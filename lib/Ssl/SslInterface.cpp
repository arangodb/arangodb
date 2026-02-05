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

#include "Logger/LogMacros.h"

#include <cstring>
#include <openssl/bn.h>
#include <openssl/ecdsa.h>
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

bool verifyES256Signature(std::string_view publicKeyPem,
                          std::string_view message,
                          std::string_view signature) {
  BIO* keybio = BIO_new_mem_buf(publicKeyPem.data(),
                                static_cast<int>(publicKeyPem.size()));
  if (keybio == nullptr) {
    return false;
  }
  auto cleanupBio = scopeGuard([&]() noexcept { BIO_free_all(keybio); });

  // Try to read as a public key first
  EVP_PKEY* pKey = PEM_read_bio_PUBKEY(keybio, nullptr, nullptr, nullptr);
  if (pKey == nullptr) {
    // If that fails, try to read as a private key (which also contains the
    // public key)
    BIO_reset(keybio);
    pKey = PEM_read_bio_PrivateKey(keybio, nullptr, nullptr, nullptr);
    if (pKey == nullptr) {
      return false;
    }
  }
  auto cleanupKey = scopeGuard([&]() noexcept { EVP_PKEY_free(pKey); });

  // Verify that this is an EC key
  if (EVP_PKEY_base_id(pKey) != EVP_PKEY_EC) {
    return false;
  }

  // JWT ES256 signatures are in raw R||S format (IEEE P1363), but OpenSSL
  // expects DER-encoded ECDSA-Sig-Value. For P-256, R and S are 32 bytes each.
  std::string derSignature;
  if (signature.size() == 64) {
    // Convert from raw R||S to DER format
    unsigned char const* sigBytes =
        reinterpret_cast<unsigned char const*>(signature.data());

    // Create ECDSA_SIG structure
    ECDSA_SIG* ecdsaSig = ECDSA_SIG_new();
    if (ecdsaSig == nullptr) {
      LOG_TOPIC("8f3a1", DEBUG, Logger::AUTHENTICATION)
          << "Failed to create ECDSA_SIG structure for ES256 signature "
             "verification";
      return false;
    }
    auto cleanupSig = scopeGuard([&]() noexcept { ECDSA_SIG_free(ecdsaSig); });

    // Set R and S from the raw signature
    BIGNUM* r = BN_bin2bn(sigBytes, 32, nullptr);
    BIGNUM* s = BN_bin2bn(sigBytes + 32, 32, nullptr);
    if (r == nullptr || s == nullptr) {
      LOG_TOPIC("8f3a2", DEBUG, Logger::AUTHENTICATION)
          << "Failed to convert R and S components to BIGNUM for ES256 "
             "signature";
      if (r != nullptr) BN_free(r);
      if (s != nullptr) BN_free(s);
      return false;
    }

    // ECDSA_SIG_set0 takes ownership of r and s
    if (ECDSA_SIG_set0(ecdsaSig, r, s) != 1) {
      LOG_TOPIC("8f3a3", DEBUG, Logger::AUTHENTICATION)
          << "Failed to set R and S in ECDSA_SIG structure";
      BN_free(r);
      BN_free(s);
      return false;
    }

    // Convert to DER format
    int derLen = i2d_ECDSA_SIG(ecdsaSig, nullptr);
    if (derLen <= 0) {
      LOG_TOPIC("8f3a4", DEBUG, Logger::AUTHENTICATION)
          << "Failed to get DER signature length for ES256";
      return false;
    }

    derSignature.resize(derLen);
    unsigned char* derPtr =
        reinterpret_cast<unsigned char*>(derSignature.data());
    if (i2d_ECDSA_SIG(ecdsaSig, &derPtr) != derLen) {
      LOG_TOPIC("8f3a5", DEBUG, Logger::AUTHENTICATION)
          << "Failed to encode ES256 signature to DER format";
      return false;
    }

    // Use the DER signature for verification
    signature = std::string_view(derSignature);
  }

  EVP_MD_CTX* mdctx = EVP_MD_CTX_new();
  if (mdctx == nullptr) {
    return false;
  }
  auto cleanupCtx = scopeGuard([&]() noexcept { EVP_MD_CTX_free(mdctx); });

  if (EVP_DigestVerifyInit(mdctx, nullptr, EVP_sha256(), nullptr, pKey) != 1) {
    return false;
  }

  if (EVP_DigestVerifyUpdate(mdctx, message.data(), message.size()) != 1) {
    return false;
  }

  int result = EVP_DigestVerifyFinal(
      mdctx, reinterpret_cast<unsigned char const*>(signature.data()),
      signature.size());

  if (result == -1) {
    LOG_TOPIC("8f3a6", TRACE, Logger::AUTHENTICATION)
        << "Error verifying ES256 signature: "
        << ERR_error_string(ERR_get_error(), nullptr);
  }
  return result == 1;
}

int signES256(std::string_view privateKeyPem, std::string_view message,
              std::string& signature, std::string& error) {
  BIO* keybio = BIO_new_mem_buf(privateKeyPem.data(),
                                static_cast<int>(privateKeyPem.size()));
  if (keybio == nullptr) {
    error.append("Failed to initialize keybio.");
    return 1;
  }
  auto cleanupBio = scopeGuard([&]() noexcept { BIO_free_all(keybio); });

  EVP_PKEY* pKey = PEM_read_bio_PrivateKey(keybio, nullptr, nullptr, nullptr);
  if (pKey == nullptr) {
    error.append("Failed to read private key from PEM: ")
        .append(ERR_error_string(ERR_get_error(), nullptr));
    return 1;
  }
  auto cleanupKey = scopeGuard([&]() noexcept { EVP_PKEY_free(pKey); });

  // Verify that this is an EC key
  if (EVP_PKEY_base_id(pKey) != EVP_PKEY_EC) {
    error.append("Key is not an EC key");
    return 1;
  }

  EVP_MD_CTX* mdctx = EVP_MD_CTX_new();
  if (mdctx == nullptr) {
    error.append("EVP_MD_CTX_new failed: ")
        .append(ERR_error_string(ERR_get_error(), nullptr));
    return 1;
  }
  auto cleanupCtx = scopeGuard([&]() noexcept { EVP_MD_CTX_free(mdctx); });

  if (EVP_DigestSignInit(mdctx, nullptr, EVP_sha256(), nullptr, pKey) != 1) {
    error.append("EVP_DigestSignInit failed: ")
        .append(ERR_error_string(ERR_get_error(), nullptr));
    return 1;
  }

  if (EVP_DigestSignUpdate(mdctx, message.data(), message.size()) != 1) {
    error.append("EVP_DigestSignUpdate failed: ")
        .append(ERR_error_string(ERR_get_error(), nullptr));
    return 1;
  }

  size_t derSignatureLen = 0;
  if (EVP_DigestSignFinal(mdctx, nullptr, &derSignatureLen) != 1) {
    error.append("EVP_DigestSignFinal (length) failed: ")
        .append(ERR_error_string(ERR_get_error(), nullptr));
    return 1;
  }

  std::string derSignature;
  derSignature.resize(derSignatureLen);
  if (EVP_DigestSignFinal(mdctx,
                          reinterpret_cast<unsigned char*>(derSignature.data()),
                          &derSignatureLen) != 1) {
    error.append("EVP_DigestSignFinal failed: ")
        .append(ERR_error_string(ERR_get_error(), nullptr));
    return 1;
  }
  derSignature.resize(derSignatureLen);

  // Convert DER signature to raw R||S format for JWT compatibility
  unsigned char const* derPtr =
      reinterpret_cast<unsigned char const*>(derSignature.data());
  ECDSA_SIG* ecdsaSig = d2i_ECDSA_SIG(nullptr, &derPtr, derSignature.size());
  if (ecdsaSig == nullptr) {
    error.append("Failed to parse DER signature: ")
        .append(ERR_error_string(ERR_get_error(), nullptr));
    return 1;
  }
  auto cleanupSig = scopeGuard([&]() noexcept { ECDSA_SIG_free(ecdsaSig); });

  BIGNUM const* r = nullptr;
  BIGNUM const* s = nullptr;
  ECDSA_SIG_get0(ecdsaSig, &r, &s);
  if (r == nullptr || s == nullptr) {
    error.append("Failed to get R and S from signature");
    return 1;
  }

  // For P-256, R and S should be 32 bytes each
  signature.resize(64);
  unsigned char* sigBytes = reinterpret_cast<unsigned char*>(signature.data());

  // Pad R to 32 bytes
  int rLen = BN_num_bytes(r);
  if (rLen > 32) {
    error.append("R component too large");
    return 1;
  }
  std::memset(sigBytes, 0, 32);
  BN_bn2bin(r, sigBytes + (32 - rLen));

  // Pad S to 32 bytes
  int sLen = BN_num_bytes(s);
  if (sLen > 32) {
    error.append("S component too large");
    return 1;
  }
  std::memset(sigBytes + 32, 0, 32);
  BN_bn2bin(s, sigBytes + 32 + (32 - sLen));

  return 0;
}

int extractPublicKeyFromPrivateKey(std::string_view privateKeyPem,
                                   std::string& publicKeyPem,
                                   std::string& error) {
  BIO* keybio = BIO_new_mem_buf(privateKeyPem.data(),
                                static_cast<int>(privateKeyPem.size()));
  if (keybio == nullptr) {
    error.append("Failed to initialize keybio.");
    return 1;
  }
  auto cleanupBio = scopeGuard([&]() noexcept { BIO_free_all(keybio); });

  EVP_PKEY* pKey = PEM_read_bio_PrivateKey(keybio, nullptr, nullptr, nullptr);
  if (pKey == nullptr) {
    error.append("Failed to read private key from PEM: ")
        .append(ERR_error_string(ERR_get_error(), nullptr));
    return 1;
  }
  auto cleanupKey = scopeGuard([&]() noexcept { EVP_PKEY_free(pKey); });

  // Verify that this is an EC key
  if (EVP_PKEY_base_id(pKey) != EVP_PKEY_EC) {
    error.append("Key is not an EC key");
    return 1;
  }

  // Extract the public key and write it to a string
  BIO* pubKeyBio = BIO_new(BIO_s_mem());
  if (pubKeyBio == nullptr) {
    error.append("Failed to create BIO for public key");
    return 1;
  }
  auto cleanupPubKeyBio =
      scopeGuard([&]() noexcept { BIO_free_all(pubKeyBio); });

  if (PEM_write_bio_PUBKEY(pubKeyBio, pKey) != 1) {
    error.append("Failed to write public key to BIO: ")
        .append(ERR_error_string(ERR_get_error(), nullptr));
    return 1;
  }

  // Read the public key PEM from the BIO
  char* pubKeyData = nullptr;
  long pubKeyLen = BIO_get_mem_data(pubKeyBio, &pubKeyData);
  if (pubKeyLen <= 0 || pubKeyData == nullptr) {
    error.append("Failed to get public key data from BIO");
    return 1;
  }

  publicKeyPem.assign(pubKeyData, pubKeyLen);
  return 0;
}

}  // namespace arangodb::rest::SslInterface
