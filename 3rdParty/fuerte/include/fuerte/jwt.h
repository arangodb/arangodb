////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2018 ArangoDB GmbH, Cologne, Germany
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
/// @author Simon Grätzer
////////////////////////////////////////////////////////////////////////////////
#pragma once
#ifndef ARANGO_CXX_DRIVER_JWT
#define ARANGO_CXX_DRIVER_JWT

#include <string>

namespace arangodb {
namespace velocypack {
class Slice;
}

namespace fuerte { inline namespace v1 { namespace jwt {

/// Generate JWT token as used by internal arangodb communication
std::string generateInternalToken(std::string const& secret,
                                  std::string const& id);

/// Generate JWT token as used for 'users' in arangodb
std::string generateUserToken(std::string const& secret,
                              std::string const& username,
                              std::chrono::seconds validFor = std::chrono::seconds{0});

std::string generateRawJwt(std::string const& secret,
                           arangodb::velocypack::Slice const& body);

//////////////////////////////////////////////////////////////////////////
/// @brief Internals
//////////////////////////////////////////////////////////////////////////

enum Algorithm {
  ALGORITHM_SHA256 = 0,
  ALGORITHM_SHA1 = 1,
  ALGORITHM_MD5 = 2,
  ALGORITHM_SHA224 = 3,
  ALGORITHM_SHA384 = 4,
  ALGORITHM_SHA512 = 5
};

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
}}}  // namespace fuerte::v1::jwt
}  // namespace arangodb
#endif
