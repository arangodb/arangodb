////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2014-2016 ArangoDB GmbH, Cologne, Germany
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
/// @author Dr. Oreste Costa-Panaia
////////////////////////////////////////////////////////////////////////////////

#include "SslInterface.h"

#include "Basics/Exceptions.h"
#include "Basics/ScopeGuard.h"
#include "Basics/StringUtils.h"
#include "Basics/memory.h"
#include "Random/UniformCharacter.h"

#include <cstring>
#include <openssl/evp.h>
#include <openssl/hmac.h>
#include <openssl/md5.h>
#include <openssl/rand.h>
#include <openssl/sha.h>

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

namespace arangodb {
namespace rest {
namespace SslInterface {

// -----------------------------------------------------------------------------
// public methods
// -----------------------------------------------------------------------------

std::string sslMD5(std::string const& inputStr) {
  char hash[17];
  char* p = &hash[0];
  size_t length;

  sslMD5(inputStr.c_str(), inputStr.size(), p, length);

  char hex[33];
  p = &hex[0];

  SslInterface::sslHEX(hash, 16, p, length);

  return std::string(hex, 32);
}

void sslMD5(char const* inputStr, size_t length, char*& outputStr, size_t& outputLen) {
  if (outputStr == nullptr) {
    outputStr = new char[MD5_DIGEST_LENGTH];
    outputLen = MD5_DIGEST_LENGTH;
  }

  MD5((const unsigned char*)inputStr, length, (unsigned char*)outputStr);
}

void sslMD5(char const* inputStr, char*& outputStr, size_t& outputLen) {
  sslMD5(inputStr, strlen(inputStr), outputStr, outputLen);
}

void sslMD5(char const* input1, size_t length1, char const* input2,
            size_t length2, char*& outputStr, size_t& outputLen) {
  if (outputStr == nullptr) {
    outputStr = new char[MD5_DIGEST_LENGTH];
    outputLen = MD5_DIGEST_LENGTH;
  }

  MD5_CTX ctx;

  MD5_Init(&ctx);
  MD5_Update(&ctx, input1, length1);
  MD5_Update(&ctx, input2, length2);
  MD5_Final((unsigned char*)outputStr, &ctx);
}

void sslSHA1(char const* inputStr, size_t length, char*& outputStr, size_t& outputLen) {
  if (outputStr == nullptr) {
    outputStr = new char[SHA_DIGEST_LENGTH];
    outputLen = SHA_DIGEST_LENGTH;
  }

  SHA1((const unsigned char*)inputStr, length, (unsigned char*)outputStr);
}

void sslSHA1(char const* inputStr, char*& outputStr, size_t& outputLen) {
  sslSHA1(inputStr, strlen(inputStr), outputStr, outputLen);
}

void sslSHA224(char const* inputStr, size_t length, char*& outputStr, size_t& outputLen) {
  if (outputStr == nullptr) {
    outputStr = new char[SHA224_DIGEST_LENGTH];
    outputLen = SHA224_DIGEST_LENGTH;
  }

  SHA224((const unsigned char*)inputStr, length, (unsigned char*)outputStr);
}

void sslSHA224(char const* inputStr, char*& outputStr, size_t& outputLen) {
  sslSHA224(inputStr, strlen(inputStr), outputStr, outputLen);
}

void sslSHA256(char const* inputStr, size_t length, char*& outputStr, size_t& outputLen) {
  if (outputStr == nullptr) {
    outputStr = new char[SHA256_DIGEST_LENGTH];
    outputLen = SHA256_DIGEST_LENGTH;
  }

  SHA256((const unsigned char*)inputStr, length, (unsigned char*)outputStr);
}

void sslSHA256(char const* inputStr, char*& outputStr, size_t& outputLen) {
  sslSHA256(inputStr, strlen(inputStr), outputStr, outputLen);
}

void sslSHA384(char const* inputStr, size_t length, char*& outputStr, size_t& outputLen) {
  if (outputStr == nullptr) {
    outputStr = new char[SHA384_DIGEST_LENGTH];
    outputLen = SHA384_DIGEST_LENGTH;
  }

  SHA384((const unsigned char*)inputStr, length, (unsigned char*)outputStr);
}

void sslSHA384(char const* inputStr, char*& outputStr, size_t& outputLen) {
  sslSHA384(inputStr, strlen(inputStr), outputStr, outputLen);
}

void sslSHA512(char const* inputStr, size_t length, char*& outputStr, size_t& outputLen) {
  if (outputStr == nullptr) {
    outputStr = new char[SHA512_DIGEST_LENGTH];
    outputLen = SHA512_DIGEST_LENGTH;
  }

  SHA512((const unsigned char*)inputStr, length, (unsigned char*)outputStr);
}

void sslSHA512(char const* inputStr, char*& outputStr, size_t& outputLen) {
  sslSHA512(inputStr, strlen(inputStr), outputStr, outputLen);
}

void sslHEX(char const* inputStr, size_t length, char*& outputStr, size_t& outputLen) {
  static char const hexval[16] = {'0', '1', '2', '3', '4', '5', '6', '7',
                                  '8', '9', 'a', 'b', 'c', 'd', 'e', 'f'};

  if (outputStr == nullptr) {
    outputStr = new char[length * 2 + 1];
    outputLen = length * 2;
  }

  char const* e = inputStr + length;
  char* p = outputStr;

  for (char const* q = inputStr; q < e; ++q) {
    *p++ = hexval[(*q >> 4) & 0xF];
    *p++ = hexval[*q & 0x0F];
  }

  *p = '\0';
}

void sslHEX(char const* inputStr, char*& outputStr, size_t& outputLen) {
  sslHEX(inputStr, strlen(inputStr), outputStr, outputLen);
}

std::string sslPBKDF2HS1(char const* salt, size_t saltLength, char const* pass,
                         size_t passLength, int iter, int keyLength) {
  unsigned char* dk = (unsigned char*)TRI_Allocate(keyLength + 1);
  if (dk == nullptr) {
    THROW_ARANGO_EXCEPTION(TRI_ERROR_OUT_OF_MEMORY);
  }

  TRI_DEFER(TRI_Free(dk));

  PKCS5_PBKDF2_HMAC_SHA1(pass, (int)passLength, (const unsigned char*)salt,
                         (int)saltLength, iter, keyLength, dk);

  // return value as hex
  return StringUtils::encodeHex((char*)dk, keyLength);
}

std::string sslPBKDF2(char const* salt, size_t saltLength, char const* pass,
                      size_t passLength, int iter, int keyLength, Algorithm algorithm) {
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

  TRI_DEFER(TRI_Free(dk));

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

  TRI_DEFER(TRI_Free(md));
  unsigned int md_len;

  HMAC(evp_md, key, (int)keyLength, (const unsigned char*)message, messageLen, md, &md_len);

  return std::string((char*)md, md_len);
}

bool verifyHMAC(char const* challenge, size_t challengeLength,
                char const* secret, size_t secretLen, char const* response,
                size_t responseLen, Algorithm algorithm) {
  // challenge = key
  // secret, secretLen = message
  // result must == BASE64(response, responseLen)

  std::string s = sslHMAC(challenge, challengeLength, secret, secretLen, algorithm);

  if (s.length() == responseLen && s.compare(std::string(response, responseLen)) == 0) {
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

}  // namespace SslInterface
}  // namespace rest
}  // namespace arangodb
