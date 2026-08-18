////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2014-2026 ArangoDB GmbH, Cologne, Germany
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

#pragma once

#include <string>
#include <string_view>
#include <variant>
#include <chrono>

namespace arangodb::auth {

struct ES256PrivateKey {
  std::string _data;
};

struct ES256PublicKey {
  std::string _data;
};

struct HS256Key {
  static constexpr size_t kMinSecretLength = 32;
  static constexpr size_t kMaxSecretLength = 64;
  std::string _data;
};

using AuthKey = std::variant<ES256PrivateKey, ES256PublicKey, HS256Key>;

auto shaHashKey(AuthKey key) -> std::string;
auto authKeyInfo(AuthKey key) -> std::string;
auto generateUserToken(AuthKey key, std::string_view username,
                       std::chrono::seconds validFor = std::chrono::seconds{0})
    -> std::string;

}  // namespace arangodb::auth
