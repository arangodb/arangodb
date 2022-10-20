////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2014-2022 ArangoDB GmbH, Cologne, Germany
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
/// @author Heiko Kernbach
////////////////////////////////////////////////////////////////////////////////
#pragma once

#include "Aql/Optimizer2/Types/Types.h"

namespace arangodb::aql::optimizer2::nodes {

// Hint: aka. our ExecutionNode - think about renaming this.
struct BaseNode {
  AttributeTypes::NodeId id;
  AttributeTypes::NodeType type;
  AttributeTypes::Dependencies dependencies;
  AttributeTypes::Numeric estimatedCost;  // TODO: might be double. Check this.
  AttributeTypes::Numeric estimatedNrItems;
  std::optional<bool> canThrow;

  bool operator==(BaseNode const&) const = default;
};

template<class Inspector>
auto inspect(Inspector& f, BaseNode& v) {
  return f.object(v).fields(f.field("id", v.id), f.field("type", v.type),
                            f.field("dependencies", v.dependencies),
                            f.field("estimatedCost", v.estimatedCost),
                            f.field("estimatedNrItems", v.estimatedNrItems),
                            f.field("canThrow", v.canThrow));
}

}  // namespace arangodb::aql::optimizer2::nodes
