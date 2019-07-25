////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2019 ArangoDB GmbH, Cologne, Germany
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
/// @author Tobias Gödderz
////////////////////////////////////////////////////////////////////////////////

#ifndef ARANGOD_CLUSTER_CLUSTERTYPES_H
#define ARANGOD_CLUSTER_CLUSTERTYPES_H

#include <limits>
#include <string>

namespace arangodb {

typedef std::string ServerID;         // ID of a server
typedef std::string DatabaseID;       // ID/name of a database
typedef std::string CollectionID;     // ID of a collection
typedef std::string ViewID;           // ID of a view
typedef std::string ShardID;          // ID of a shard
typedef uint32_t ServerShortID;       // Short ID of a server
typedef std::string ServerShortName;  // Short name of a server

class RebootId {
 public:
  explicit constexpr RebootId(uint64_t rebootId) noexcept : _value(rebootId) {}
  uint64_t value() const noexcept { return _value; }

  bool initialized() const noexcept { return value() != 0; }

  bool operator==(RebootId other) const noexcept {
    return value() == other.value();
  }
  bool operator!=(RebootId other) const noexcept {
    return value() != other.value();
  }
  bool operator<(RebootId other) const noexcept {
    return value() < other.value();
  }
  bool operator>(RebootId other) const noexcept {
    return value() > other.value();
  }
  bool operator<=(RebootId other) const noexcept {
    return value() <= other.value();
  }
  bool operator>=(RebootId other) const noexcept {
    return value() >= other.value();
  }

  static constexpr RebootId max() noexcept {
    return RebootId{std::numeric_limits<decltype(_value)>::max()};
  }

 private:
  uint64_t _value;
};

}  // namespace arangodb

#endif  // ARANGOD_CLUSTER_CLUSTERTYPES_H
