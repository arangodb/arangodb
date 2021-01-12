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

#ifndef ARANGOD_VOCBASE_IDENTIFIERS_SERVER_ID_H
#define ARANGOD_VOCBASE_IDENTIFIERS_SERVER_ID_H 1

#include "Basics/Identifier.h"

namespace arangodb {

/// @brief server id type
class ServerId : public arangodb::basics::Identifier {
 public:
  constexpr ServerId() noexcept : Identifier() {}
  constexpr explicit ServerId(BaseType id) noexcept : Identifier(id) {}

  /// @brief whether or not the id is set (not 0)
  bool isSet() const noexcept;

  /// @brief whether or not the identifier is unset (equal to 0)
  bool empty() const noexcept;

 public:
  /// @brief create a not-set file id
  static constexpr ServerId none() { return ServerId{0}; }
};

static_assert(sizeof(ServerId) == sizeof(ServerId::BaseType),
              "invalid size of ServerId");
}  // namespace arangodb

DECLARE_HASH_FOR_IDENTIFIER(arangodb::ServerId)

#endif
