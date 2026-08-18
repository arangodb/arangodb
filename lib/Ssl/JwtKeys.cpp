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

#include "Ssl/JwtKeys.h"
#include "Ssl/jwt.h"
#include "Basics/files.h"
#include "Basics/overload.h"

#include <format>

namespace arangodb::auth {
auto shaHashKey(AuthKey key) -> std::string {
  return std::visit(
      [](auto&& v) -> std::string {
        if (v._data.empty()) {
          return "";
        } else {
          auto sha = TRI_SHA256Functor();
          sha(v._data.data(), v._data.length());
          return sha.finalize();
        }
      },
      key);
}

auto authKeyInfo(AuthKey key) -> std::string {
  return std::visit(
      overload{[](ES256PublicKey& key) {
                 return std::format("ES256 Public Key (length: {})",
                                    key._data.size());
               },
               [](ES256PrivateKey& key) {
                 return std::format("ES256 Private Key (length: {})",
                                    key._data.size());
               },
               [](HS256Key& key) {
                 return std::format("HS256  Key (length: {})",
                                    key._data.size());
               }},
      key);
}

auto generateUserToken(AuthKey key, std::string_view username,
                       std::chrono::seconds validFor) -> std::string {
  auto keyData = std::visit([](auto&& key) { return key._data; }, key);
  return rest::SslInterface::jwt::generateUserToken(keyData, username,
                                                    validFor);
}

}  // namespace arangodb::auth
