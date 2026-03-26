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
/// @author Tobias Gödderz
////////////////////////////////////////////////////////////////////////////////

#pragma once

#include "Basics/Identifier.h"

#include <limits>

namespace arangodb::aql {

class ExecutionNodeId : public basics::Identifier {
 public:
  using Identifier::BaseType;
  using Identifier::Identifier;

  /// @brief placeholder used for internal nodes
  static constexpr Identifier::BaseType InternalNode =
      std::numeric_limits<BaseType>::max();
};

}  // namespace arangodb::aql

DECLARE_HASH_FOR_IDENTIFIER(arangodb::aql::ExecutionNodeId)

template<>
struct std::formatter<::arangodb::aql::ExecutionNodeId>
    : std::formatter<::arangodb::basics::Identifier> {
  auto format(::arangodb::aql::ExecutionNodeId nodeId,
              std::format_context& fc) const {
    return std::formatter<typename ::arangodb::basics::Identifier>::format(
        nodeId, fc);
  }
};
