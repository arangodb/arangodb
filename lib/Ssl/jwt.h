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

#pragma once

#include <chrono>
#include <string>
#include <string_view>

namespace arangodb {
namespace velocypack {
class Slice;
}

namespace rest::SslInterface::jwt {

/// Generate JWT token as used by internal arangodb communication
std::string generateInternalToken(std::string_view secret, std::string_view id);

/// Generate JWT token as used for 'users' in arangodb
std::string generateUserToken(
    std::string_view secret, std::string_view username,
    std::chrono::seconds validFor = std::chrono::seconds{0});

std::string generateRawJwt(std::string_view secret, velocypack::Slice body);

}  // namespace rest::SslInterface::jwt
}  // namespace arangodb
