////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2023-2023 ArangoDB GmbH, Cologne, Germany
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
/// @author Michael Hackstein
////////////////////////////////////////////////////////////////////////////////

#include "ShardID.h"

arangodb::ResultT<arangodb::ShardID> arangodb::ShardID::shardIdFromString(
    std::string_view s) {
  if (s.empty() || s.at(0) != 's') {
    return Result{TRI_ERROR_BAD_PARAMETER,
                  "Expected ShardID to start with 's'"};
  }
  auto res = basics::StringUtils::try_uint64(s.data() + 1, s.length() - 1);
  if (res.fail()) {
    return std::move(res.result());
  }
  return ShardID{res.get()};
}
arangodb::ShardID arangodb::ShardID::invalidShard() { return ShardID{0}; }
arangodb::ShardID::ShardID(std::string_view id) {
  auto maybeShardID = shardIdFromString(id);
  if (maybeShardID.fail()) {
    THROW_ARANGO_EXCEPTION(maybeShardID.result());
  }
  *this = maybeShardID.get();
}
arangodb::ShardID::operator std::string() const {
  return "s" + std::to_string(id);
}
arangodb::ShardID::operator arangodb::velocypack::Value() const {
  return arangodb::velocypack::Value("s" + std::to_string(id));
}
bool arangodb::ShardID::operator==(std::string_view other) const {
  return other == std::string{*this};
}
bool arangodb::ShardID::operator==(const std::string& other) const {
  return other == std::string{*this};
}
bool arangodb::ShardID::isValid() const noexcept {
  // We can never have ShardID 0. So we use it as invalid value.
  return id != 0;
}
std::ostream& arangodb::operator<<(std::ostream& o,
                                   const arangodb::ShardID& r) {
  o << "s" << r.id;
  return o;
}
std::string operator+(const std::string& text, const arangodb::ShardID& s) {
  return text + "s" + std::to_string(s.id);
}
std::string operator+(const arangodb::ShardID& s, const std::string& text) {
  return "s" + std::to_string(s.id) + text;
}

auto std::hash<arangodb::ShardID>::operator()(
    const arangodb::ShardID& v) const noexcept -> std::size_t {
  return std::hash<uint64_t>{}(v.id);
}
