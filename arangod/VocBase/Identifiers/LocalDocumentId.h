////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2014-2016 ArangoDB GmbH, Cologne, Germany
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
/// @author Jan Steemann
/// @author Dan Larkin-York
////////////////////////////////////////////////////////////////////////////////

#ifndef ARANGOD_VOCBASE_IDENTIFIERS_LOCAL_DOCUMENT_ID_H
#define ARANGOD_VOCBASE_IDENTIFIERS_LOCAL_DOCUMENT_ID_H 1

#include "Basics/Identifier.h"
#include "VocBase/ticks.h"

namespace arangodb {
/// @brief a LocalDocumentId is an identifier for storing and retrieving
/// documents using a uint64_t value.
class LocalDocumentId : public basics::Identifier {
 public:
  constexpr LocalDocumentId() noexcept : Identifier() {}
  constexpr explicit LocalDocumentId(BaseType id) noexcept : Identifier(id) {}

  /// @brief whether or not the id is set (not 0)
  bool isSet() const noexcept;

  /// @brief whether or not the identifier is unset (equal to 0)
  bool empty() const noexcept;

 public:
  /// @brief create a not-set document id
  static constexpr LocalDocumentId none() { return LocalDocumentId(0); }

  /// @brief create a new document id
  static LocalDocumentId create() {
    return LocalDocumentId(TRI_HybridLogicalClock());
  }

  /// @brief create a document id from an existing id
  static constexpr LocalDocumentId create(BaseType id) {
    return LocalDocumentId(id);
  }

  /// @brief use to track an existing value in recovery to ensure no duplicates
  static void track(LocalDocumentId const& id) {
    TRI_HybridLogicalClock(id.id());
  }
};

// LocalDocumentId should not be bigger than the BaseType
static_assert(sizeof(LocalDocumentId) == sizeof(LocalDocumentId::BaseType),
              "invalid size of LocalDocumentId");
}  // namespace arangodb

DECLARE_HASH_FOR_IDENTIFIER(arangodb::LocalDocumentId)
DECLARE_EQUAL_FOR_IDENTIFIER(arangodb::LocalDocumentId)

#endif
