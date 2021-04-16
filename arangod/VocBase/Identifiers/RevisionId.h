////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2014-2021 ArangoDB GmbH, Cologne, Germany
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

#ifndef ARANGOD_VOCBASE_IDENTIFIERS_REVISION_ID_H
#define ARANGOD_VOCBASE_IDENTIFIERS_REVISION_ID_H 1

#include <velocypack/Slice.h>
#include <velocypack/Value.h>

#include "ApplicationFeatures/ApplicationServer.h"
#include "Basics/HybridLogicalClock.h"
#include "Basics/Identifier.h"

namespace arangodb {
class ClusterInfo;
class LocalDocumentId;

/// @brief server id type
class RevisionId : public arangodb::basics::Identifier {
 public:
  constexpr RevisionId() noexcept : Identifier() {}
  constexpr explicit RevisionId(BaseType id) noexcept : Identifier(id) {}
  explicit RevisionId(LocalDocumentId const& id);

  /// @brief whether or not the id is set (not 0)
  bool isSet() const noexcept;

  /// @brief whether or not the identifier is unset (equal to 0)
  bool empty() const noexcept;

  // @brief get the next revision id in sequence (this + 1)
  RevisionId next() const;

  // @brief get the previous revision id in sequence (this - 1)
  RevisionId previous() const;

  /// @brief Convert a revision ID to a string
  std::string toString() const;

  /// @brief Convert a revision ID to a string
  /// the buffer should be at least arangodb::basics::maxUInt64StringSize
  /// bytes long
  /// the length of the encoded value and the start position into
  /// the result buffer are returned
  std::pair<size_t, size_t> toString(char* buffer) const;

  /// @brief Convert revision ID to a string using the provided buffer,
  /// returning the result as a value pair for convenience
  /// the buffer should be at least arangodb::basics::maxUInt64StringSize
  /// bytes long
  arangodb::velocypack::ValuePair toValuePair(char* buffer) const;

  /// @brief Write revision ID to string for storage with correct endianness
  void toPersistent(std::string& buffer) const;

 public:
  /// @brief create a not-set revision id
  static constexpr RevisionId none() { return RevisionId{0}; }

  /// @brief create a maximum revision id
  static constexpr RevisionId max() {
    return RevisionId{std::numeric_limits<std::uint64_t>::max()};
  }

  /// @brief create a revision id using an HLC value
  static RevisionId create();

  /// @brief create a revision id which is guaranteed to be unique cluster-wide
  static RevisionId createClusterWideUnique(ClusterInfo& ci);

  /// @brief Convert a string into a revision ID, returns none() if invalid
  static RevisionId fromString(std::string const& ridStr);

  /// @brief Convert a string into a revision ID, returns none() if invalid
  static RevisionId fromString(std::string const& ridStr, bool& isOld, bool warn);

  /// @brief Convert a string into a revision ID, no check variant
  static RevisionId fromString(char const* p, size_t len, bool warn);

  /// @brief Convert a string into a revision ID, returns none() if invalid
  static RevisionId fromString(char const* p, size_t len, bool& isOld, bool warn);

  /// @brief extract revision from slice; expects either an integer or string,
  /// or an object with a string or integer _rev attribute
  static RevisionId fromSlice(velocypack::Slice const slice);

  /// @brief extract revision from persistent storage (proper endianness)
  static RevisionId fromPersistent(char const* data);
};

static_assert(sizeof(RevisionId) == sizeof(RevisionId::BaseType),
              "invalid size of RevisionId");
}  // namespace arangodb

DECLARE_HASH_FOR_IDENTIFIER(arangodb::RevisionId)
DECLARE_EQUAL_FOR_IDENTIFIER(arangodb::RevisionId)

#endif
