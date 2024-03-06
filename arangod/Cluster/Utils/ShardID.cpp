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
/// @author Michael Hackstein
////////////////////////////////////////////////////////////////////////////////

#include "ShardID.h"

#include "Basics/Exceptions.h"
#include "Basics/StringUtils.h"

#include <absl/strings/str_cat.h>

arangodb::ResultT<arangodb::ShardID> arangodb::ShardID::shardIdFromString(
    std::string_view s) {
  if (!s.starts_with('s')) {
    return Result{TRI_ERROR_BAD_PARAMETER,
                  "Expected ShardID to start with 's'"};
  }
  auto res = basics::StringUtils::try_uint64(s.data() + 1, s.length() - 1);
  if (res.fail()) {
    return std::move(res.result());
  }
  return ShardID{res.get()};
}
arangodb::ShardID arangodb::ShardID::invalidShard() noexcept {
  return ShardID{0};
}
arangodb::ShardID::ShardID(std::string_view id) {
  auto maybeShardID = shardIdFromString(id);
  if (maybeShardID.fail()) {
    THROW_ARANGO_EXCEPTION(maybeShardID.result());
  }
  *this = maybeShardID.get();
}
arangodb::ShardID::operator std::string() const {
  return absl::StrCat("s", _id);
}

// NOTE: This is not noexcept because of shardIdFromString
bool arangodb::ShardID::operator==(std::string_view other) const {
  if (auto otherShard = shardIdFromString(other); otherShard.ok()) {
    return *this == otherShard.get();
  }
  // If the other is not a valid Shard we cannot be equal to it.
  return false;
}

bool arangodb::ShardID::isValid() const noexcept {
  // We can never have ShardID 0. So we use it as invalid value.
  return _id != 0;
}

std::ostream& arangodb::operator<<(std::ostream& o,
                                   arangodb::ShardID const& r) {
  o << "s" << r.id();
  return o;
}
std::string operator+(std::string_view text, arangodb::ShardID const& s) {
  return absl::StrCat(text, "s", s.id());
}
std::string operator+(arangodb::ShardID const& s, std::string_view text) {
  return absl::StrCat("s", s.id(), text);
}

auto std::hash<arangodb::ShardID>::operator()(
    arangodb::ShardID const& v) const noexcept -> std::size_t {
  return std::hash<uint64_t>{}(v.id());
}
