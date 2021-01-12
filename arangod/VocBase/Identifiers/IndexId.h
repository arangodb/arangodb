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

#ifndef ARANGOD_VOCBASE_IDENTIFIERS_INDEX_ID_H
#define ARANGOD_VOCBASE_IDENTIFIERS_INDEX_ID_H 1

#include <limits>

#include "Basics/Identifier.h"

namespace arangodb {
/// @brief server id type
class IndexId : public arangodb::basics::Identifier {
 public:
  constexpr IndexId() noexcept : Identifier() {}
  constexpr explicit IndexId(BaseType id) noexcept : Identifier(id) {}

 public:
  /// @brief whether or not the id is set (not none())
  bool isSet() const noexcept;

  /// @brief whether or not the identifier is unset (equal to none())
  bool empty() const noexcept;

  bool isPrimary() const;
  bool isEdge() const;

 public:
  /// @brief create an invalid index id
  static constexpr IndexId none() {
    return IndexId{std::numeric_limits<BaseType>::max()};
  }

  /// @brief create an id for a primary index
  static constexpr IndexId primary() { return IndexId{0}; }

  /// @brief create an id for an edge _from index
  static constexpr IndexId edgeFrom() { return IndexId{1}; }

  /// @brief create an id for an edge _to index
  static constexpr IndexId edgeTo() { return IndexId{2}; }
};

static_assert(sizeof(IndexId) == sizeof(IndexId::BaseType),
              "invalid size of IndexId");
}  // namespace arangodb

DECLARE_HASH_FOR_IDENTIFIER(arangodb::IndexId)

#endif
