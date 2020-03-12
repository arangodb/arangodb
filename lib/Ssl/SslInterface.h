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

#ifndef ARANGODB_REST_SSL_INTERFACE_H
#define ARANGODB_REST_SSL_INTERFACE_H 1

#include <cstdlib>
#include <string>

#include "Basics/Common.h"

namespace arangodb {
namespace rest {
namespace SslInterface {

enum Algorithm {
  ALGORITHM_SHA256 = 0,
  ALGORITHM_SHA1 = 1,
  ALGORITHM_MD5 = 2,
  ALGORITHM_SHA224 = 3,
  ALGORITHM_SHA384 = 4,
  ALGORITHM_SHA512 = 5
};

//////////////////////////////////////////////////////////////////////////
/// @brief md5 hash
//////////////////////////////////////////////////////////////////////////

std::string sslMD5(std::string const&);

//////////////////////////////////////////////////////////////////////////
/// @brief md5 hash
//////////////////////////////////////////////////////////////////////////

void sslMD5(char const* inputStr, size_t length, char*& outputStr, size_t& outputLen);

//////////////////////////////////////////////////////////////////////////
/// @brief md5 hash
//////////////////////////////////////////////////////////////////////////

void sslMD5(char const* inputStr, char*& outputStr, size_t& outputLen);

//////////////////////////////////////////////////////////////////////////
/// @brief md5 hash
//////////////////////////////////////////////////////////////////////////

void sslMD5(char const* input1, size_t length1, char const* input2,
            size_t length2, char*& outputStr, size_t& outputLen);

//////////////////////////////////////////////////////////////////////////
/// @brief sha1 hash
//////////////////////////////////////////////////////////////////////////

void sslSHA1(char const* inputStr, size_t const length, char*& outputStr, size_t& outputLen);

//////////////////////////////////////////////////////////////////////////
/// @brief sha1 hash
//////////////////////////////////////////////////////////////////////////

void sslSHA1(char const* inputStr, char*& outputStr, size_t& outputLen);

//////////////////////////////////////////////////////////////////////////
/// @brief sha224 hash
//////////////////////////////////////////////////////////////////////////

void sslSHA224(char const* inputStr, size_t const length, char*& outputStr, size_t& outputLen);

//////////////////////////////////////////////////////////////////////////
/// @brief sha224 hash
//////////////////////////////////////////////////////////////////////////

void sslSHA224(char const* inputStr, char*& outputStr, size_t& outputLen);

//////////////////////////////////////////////////////////////////////////
/// @brief sha256 hash
//////////////////////////////////////////////////////////////////////////

void sslSHA256(char const* inputStr, size_t const length, char*& outputStr, size_t& outputLen);

//////////////////////////////////////////////////////////////////////////
/// @brief sha256 hash
//////////////////////////////////////////////////////////////////////////

void sslSHA256(char const* inputStr, char*& outputStr, size_t& outputLen);

//////////////////////////////////////////////////////////////////////////
/// @brief sha384 hash
//////////////////////////////////////////////////////////////////////////

void sslSHA384(char const* inputStr, size_t const length, char*& outputStr, size_t& outputLen);

//////////////////////////////////////////////////////////////////////////
/// @brief sha384 hash
//////////////////////////////////////////////////////////////////////////

void sslSHA384(char const* inputStr, char*& outputStr, size_t& outputLen);

//////////////////////////////////////////////////////////////////////////
/// @brief sha512 hash
//////////////////////////////////////////////////////////////////////////

void sslSHA512(char const* inputStr, size_t const length, char*& outputStr, size_t& outputLen);

//////////////////////////////////////////////////////////////////////////
/// @brief sha512 hash
//////////////////////////////////////////////////////////////////////////

void sslSHA512(char const* inputStr, char*& outputStr, size_t& outputLen);

//////////////////////////////////////////////////////////////////////////
/// @brief HEX
//////////////////////////////////////////////////////////////////////////

void sslHEX(char const* inputStr, size_t const length, char*& outputStr, size_t& outputLen);

//////////////////////////////////////////////////////////////////////////
/// @brief HEX
//////////////////////////////////////////////////////////////////////////

void sslHEX(char const* inputStr, char*& outputStr, size_t& outputLen);

//////////////////////////////////////////////////////////////////////////
/// @brief PBKDF2HS1
//////////////////////////////////////////////////////////////////////////

std::string sslPBKDF2HS1(char const* salt, size_t saltLength, char const* pass,
                         size_t passLength, int iter, int keyLength);

//////////////////////////////////////////////////////////////////////////
/// @brief PBKDF2
//////////////////////////////////////////////////////////////////////////

std::string sslPBKDF2(char const* salt, size_t saltLength, char const* pass,
                      size_t passLength, int iter, int keyLength, Algorithm algorithm);

//////////////////////////////////////////////////////////////////////////
/// @brief HMAC
//////////////////////////////////////////////////////////////////////////

std::string sslHMAC(char const* key, size_t keyLength, char const* message,
                    size_t messageLen, Algorithm algorithm);

//////////////////////////////////////////////////////////////////////////
/// @brief HMAC
//////////////////////////////////////////////////////////////////////////

bool verifyHMAC(char const* challenge, size_t challengeLength,
                char const* secret, size_t secretLen, char const* response,
                size_t responseLen, Algorithm algorithm);

//////////////////////////////////////////////////////////////////////////
/// @brief generate a random number using OpenSsl
///
/// will return 0 on success, and != 0 on error
//////////////////////////////////////////////////////////////////////////

int sslRand(uint64_t*);

//////////////////////////////////////////////////////////////////////////
/// @brief generate a random number using OpenSsl
///
/// will return 0 on success, and != 0 on error
//////////////////////////////////////////////////////////////////////////

int sslRand(int64_t*);

//////////////////////////////////////////////////////////////////////////
/// @brief generate a random number using OpenSsl
///
/// will return 0 on success, and != 0 on error
//////////////////////////////////////////////////////////////////////////

int sslRand(int32_t*);

}  // namespace SslInterface
}  // namespace rest
}  // namespace arangodb

#endif
