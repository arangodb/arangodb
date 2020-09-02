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

#ifndef ARANGOD_VOCBASE_IDENTIFIERS_DATA_SOURCE_ID_H
#define ARANGOD_VOCBASE_IDENTIFIERS_DATA_SOURCE_ID_H 1

#include "Basics/Identifier.h"

namespace arangodb {

/// @brief server id type
class DataSourceId : public arangodb::basics::Identifier {
 public:
  constexpr DataSourceId() noexcept : Identifier() {}
  constexpr explicit DataSourceId(BaseType id) noexcept : Identifier(id) {}

  /// @brief whether or not the id is set (not 0)
  bool isSet() const noexcept;

  /// @brief whether or not the identifier is unset (equal to 0)
  bool empty() const noexcept;

 public:
  /// @brief create a not-set file id
  static constexpr DataSourceId none() { return DataSourceId{0}; }
};

static_assert(sizeof(DataSourceId) == sizeof(DataSourceId::BaseType),
              "invalid size of DataSourceId");
}  // namespace arangodb

DECLARE_HASH_FOR_IDENTIFIER(arangodb::DataSourceId)

#endif
