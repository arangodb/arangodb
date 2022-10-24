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

#include "Aql/Optimizer2/PlanNodes/BaseNode.h"
#include "Aql/Optimizer2/PlanNodeTypes/Expression.h"
#include "Aql/Optimizer2/PlanNodeTypes/Variable.h"

#include "Inspection/VPackInspection.h"

namespace arangodb::aql::optimizer2::nodes {

struct FilterNodeElement : optimizer2::types::Variable {
  optimizer2::types::Variable inVariable;
  bool ascending;
};

template<typename Inspector>
auto inspect(Inspector& f, FilterNodeElement& x) {
  return f.object(x).fields(f.field("inVariable", x.inVariable),
                            f.field("ascending", x.ascending));
}

struct SortNode : optimizer2::nodes::BaseNode {
  std::vector<FilterNodeElement> elements;
  bool stable;
  AttributeTypes::Numeric limit;
  AttributeTypes::String strategy;
  std::optional<std::vector<AttributeTypes::String>> path;
};

template<typename Inspector>
auto inspect(Inspector& f, SortNode& x) {
  return f.object(x).fields(
      f.embedFields(static_cast<optimizer2::nodes::BaseNode&>(x)),
      f.field("stable", x.stable), f.field("elements", x.elements),
      f.field("limit", x.limit), f.field("strategy", x.strategy),
      f.field("path", x.path));
}

}  // namespace arangodb::aql::optimizer2::nodes
