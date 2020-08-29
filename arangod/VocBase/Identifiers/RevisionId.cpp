////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2014-2020 ArangoDB GmbH, Cologne, Germany
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
/// @author Dan Larkin-York
////////////////////////////////////////////////////////////////////////////////

#include <cstdint>
#include <limits>

#include "VocBase/Identifiers/RevisionId.h"

#include "Basics/HybridLogicalClock.h"
#include "Basics/NumberUtils.h"
#include "Basics/StaticStrings.h"
#include "Basics/StringUtils.h"
#include "Basics/debugging.h"
#include "Cluster/ClusterInfo.h"
#include "Logger/LogMacros.h"
#include "RocksDBEngine/RocksDBFormat.h"
#include "VocBase/Identifiers/LocalDocumentId.h"

namespace {
/// @brief Convert a revision ID to a string
constexpr static arangodb::RevisionId::BaseType TickLimit =
    static_cast<arangodb::RevisionId::BaseType>(2016ULL - 1970ULL) * 1000ULL *
    60ULL * 60ULL * 24ULL * 365ULL;
}  // namespace

namespace arangodb {

RevisionId::RevisionId(LocalDocumentId const& id) : Identifier(id.id()) {}

bool RevisionId::isSet() const noexcept { return id() != 0; }

bool RevisionId::empty() const noexcept { return !isSet(); }

// @brief get the next revision id in sequence (this + 1)
RevisionId RevisionId::next() const { return RevisionId{id() + 1}; }

// @brief get the previous revision id in sequence (this - 1)
RevisionId RevisionId::previous() const { return RevisionId{id() - 1}; }

std::string RevisionId::toString() const {
  if (id() <= ::TickLimit) {
    return arangodb::basics::StringUtils::itoa(id());
  }
  return basics::HybridLogicalClock::encodeTimeStamp(id());
}

/// encodes the uint64_t timestamp into the provided result buffer
/// the buffer must be at least 11 chars long
/// the length of the encoded value and the start position into
/// the result buffer are returned by the function
std::pair<size_t, size_t> RevisionId::toString(char* buffer) const {
  if (id() <= ::TickLimit) {
    std::pair<size_t, size_t> pos{0, 0};
    pos.second = basics::StringUtils::itoa(id(), buffer);
    return pos;
  }
  return basics::HybridLogicalClock::encodeTimeStamp(id(), buffer);
}

/// encodes the uint64_t timestamp into a temporary velocypack ValuePair,
/// using the specific temporary result buffer
/// the result buffer must be at least 11 chars long
arangodb::velocypack::ValuePair RevisionId::toValuePair(char* buffer) const {
  auto positions = toString(buffer);
  return arangodb::velocypack::ValuePair(&buffer[0] + positions.first, positions.second,
                                         velocypack::ValueType::String);
}

/// @brief Write revision ID to string for storage with correct endianness
void RevisionId::toPersistent(std::string& buffer) const {
  rocksutils::uint64ToPersistent(buffer, id());
}

/// @brief create a revision id using an HLC value
RevisionId RevisionId::create() { return RevisionId{TRI_HybridLogicalClock()}; }

/// @brief create a revision id which is guaranteed to be unique cluster-wide
RevisionId RevisionId::createClusterWideUnique(ClusterInfo& ci) {
  return RevisionId{ci.uniqid()};
}

/// @brief Convert a string into a revision ID, no check variant
RevisionId RevisionId::fromString(char const* p, size_t len, bool warn) {
  [[maybe_unused]] bool isOld;
  return fromString(p, len, isOld, warn);
}

/// @brief Convert a string into a revision ID, returns 0 if format invalid
RevisionId RevisionId::fromString(std::string const& ridStr) {
  [[maybe_unused]] bool isOld;
  return fromString(ridStr.c_str(), ridStr.size(), isOld, false);
}

/// @brief Convert a string into a revision ID, returns 0 if format invalid
RevisionId RevisionId::fromString(std::string const& ridStr, bool& isOld, bool warn) {
  return fromString(ridStr.c_str(), ridStr.size(), isOld, warn);
}

/// @brief Convert a string into a revision ID, returns 0 if format invalid
RevisionId RevisionId::fromString(char const* p, size_t len, bool& isOld, bool warn) {
  if (len > 0 && *p >= '1' && *p <= '9') {
    BaseType r = NumberUtils::atoi_positive_unchecked<BaseType>(p, p + len);
    if (warn && r > ::TickLimit) {
      // An old tick value that could be confused with a time stamp
      LOG_TOPIC("66a3a", WARN, arangodb::Logger::FIXME)
          << "Saw old _rev value that could be confused with a time stamp!";
    }
    isOld = true;
    return RevisionId{r};
  }
  isOld = false;
  return RevisionId{basics::HybridLogicalClock::decodeTimeStamp(p, len)};
}

/// @brief extract revision from slice; expects either an integer, or an object
/// with a string or integer _rev attribute
RevisionId RevisionId::fromSlice(velocypack::Slice slice) {
  slice = slice.resolveExternal();

  if (slice.isInteger()) {
    return RevisionId{slice.getNumber<BaseType>()};
  } else if (slice.isString()) {
    velocypack::ValueLength l;
    char const* p = slice.getStringUnchecked(l);
    return fromString(p, l, false);
  } else if (slice.isObject()) {
    velocypack::Slice r(slice.get(StaticStrings::RevString));
    if (r.isString()) {
      velocypack::ValueLength l;
      char const* p = r.getStringUnchecked(l);
      return fromString(p, l, false);
    }
    if (r.isInteger()) {
      return RevisionId{r.getNumber<BaseType>()};
    }
  }

  return RevisionId::none();
}

RevisionId RevisionId::fromPersistent(char const* data) {
  return RevisionId{rocksutils::uint64FromPersistent(data)};
}

}  // namespace arangodb
