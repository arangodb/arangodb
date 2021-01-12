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
/// @author Tobias Gödderz
////////////////////////////////////////////////////////////////////////////////

#ifndef ARANGOD_AQL_EXECUTIONNODEID_H
#define ARANGOD_AQL_EXECUTIONNODEID_H

#include "Basics/Identifier.h"

#include <limits>

namespace arangodb::aql {

class ExecutionNodeId : public basics::Identifier {
 public:
  using Identifier::Identifier;
  using Identifier::BaseType;

  /// @brief placeholder used for internal nodes
  static constexpr Identifier::BaseType InternalNode = std::numeric_limits<BaseType>::max();
};

}  // namespace arangodb::aql

DECLARE_HASH_FOR_IDENTIFIER(arangodb::aql::ExecutionNodeId)

#endif  // ARANGOD_AQL_EXECUTIONNODEID_H
