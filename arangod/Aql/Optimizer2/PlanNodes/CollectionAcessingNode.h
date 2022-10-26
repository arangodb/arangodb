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

#include "Aql/Optimizer2/PlanNodeTypes/Satellite.h"

namespace arangodb::aql::optimizer2::nodes {

struct CollectionAccessingNode {
  // optionals
  std::optional<AttributeTypes::String> prototype;
  std::optional<AttributeTypes::Numeric> numberOfShards;
  std::optional<AttributeTypes::String> restrictedTo;
  // specific
  AttributeTypes::String database;
  AttributeTypes::String collection;

  bool operator==(CollectionAccessingNode const&) const = default;
};

template<class Inspector>
auto inspect(Inspector& f, CollectionAccessingNode& v) {
  return f.object(v).fields(
      // optionals
      f.field("prototype", v.prototype),
      f.field("numberOfShards", v.numberOfShards),
      f.field("restrictedTo", v.restrictedTo),
      // specific
      f.field("database", v.database),
      f.field("collection", v.collection)
          .fallback(""));  // TODO: Currently we need a fallback here because
                           // TraversalNode inherits the CollectionAccessingNode
                           // but does not set a "collection" property
}

}  // namespace arangodb::aql::optimizer2::nodes
