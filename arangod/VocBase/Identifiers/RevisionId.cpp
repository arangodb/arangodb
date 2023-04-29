////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2014-2023 ArangoDB GmbH, Cologne, Germany
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
#include "VocBase/Identifiers/LocalDocumentId.h"

namespace arangodb {

RevisionId::RevisionId(LocalDocumentId id) : Identifier(id.id()) {}

bool RevisionId::isSet() const noexcept { return id() != 0; }

bool RevisionId::empty() const noexcept { return !isSet(); }

// @brief get the next revision id in sequence (this + 1)
RevisionId RevisionId::next() const { return RevisionId{id() + 1}; }

// @brief get the previous revision id in sequence (this - 1)
RevisionId RevisionId::previous() const { return RevisionId{id() - 1}; }

std::string RevisionId::toString() const {
  if (id() <= tickLimit) {
    return arangodb::basics::StringUtils::itoa(id());
  }
  return toHLC();
}

/// encodes the uint64_t timestamp into the provided result buffer
/// the buffer should be at least arangodb::basics::maxUInt64StringSize
/// bytes long.
/// the length of the encoded value and the start position into
/// the result buffer are returned by the function
std::pair<size_t, size_t> RevisionId::toString(char* buffer) const {
  if (id() <= tickLimit) {
    std::pair<size_t, size_t> pos{0, 0};
    pos.second = basics::StringUtils::itoa(id(), buffer);
    return pos;
  }
  return basics::HybridLogicalClock::encodeTimeStamp(id(), buffer);
}

std::string RevisionId::toHLC() const {
  return basics::HybridLogicalClock::encodeTimeStamp(id());
}

/// encodes the uint64_t timestamp into a temporary velocypack ValuePair,
/// using the specific temporary result buffer
/// the result buffer should be at least
/// arangodb::basics::maxUInt64StringSize bytes long
arangodb::velocypack::ValuePair RevisionId::toValuePair(char* buffer) const {
  auto positions = toString(buffer);
  return arangodb::velocypack::ValuePair(&buffer[0] + positions.first,
                                         positions.second,
                                         velocypack::ValueType::String);
}

/// @brief create a revision id with a lower-bound HLC value
RevisionId RevisionId::lowerBound() {
  // "2021-01-01T00:00:00.000Z" => 1609459200000 milliseconds since the epoch
  RevisionId value{uint64_t(1609459200000ULL) << 20ULL};
  TRI_ASSERT(value.id() > (tickLimit << 20ULL));
  return value;
}

/// @brief create a revision id using an HLC value
RevisionId RevisionId::create() { return RevisionId{TRI_HybridLogicalClock()}; }

/// @brief create a revision id which is guaranteed to be unique cluster-wide
RevisionId RevisionId::createClusterWideUnique(ClusterInfo& ci) {
  return RevisionId{ci.uniqid()};
}

/// @brief Convert a string into a revision ID, returns 0 if format invalid
RevisionId RevisionId::fromString(std::string_view rid) {
  char const* p = rid.data();
  size_t len = rid.size();
  if (len > 0 && *p >= '1' && *p <= '9') {
    BaseType r = NumberUtils::atoi_positive_unchecked<BaseType>(p, p + len);
    return RevisionId{r};
  }
  return fromHLC(rid);
}

/// @brief Convert a HLC-encoded string into a revision ID, returns 0 if format
/// invalid
RevisionId RevisionId::fromHLC(std::string_view rid) {
  return RevisionId{basics::HybridLogicalClock::decodeTimeStamp(rid)};
}

/// @brief extract revision from slice; expects either an integer, or an object
/// with a string or integer _rev attribute
RevisionId RevisionId::fromSlice(velocypack::Slice slice) {
  slice = slice.resolveExternal();

  if (slice.isObject()) {
    slice = slice.get(StaticStrings::RevString);
  }
  if (slice.isInteger()) {
    return RevisionId{slice.getNumber<BaseType>()};
  }
  if (slice.isString()) {
    return fromString(slice.stringView());
  }

  return RevisionId::none();
}

}  // namespace arangodb
