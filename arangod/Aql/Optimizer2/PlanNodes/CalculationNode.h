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
#include "Aql/Optimizer2/PlanNodeTypes/Function.h"

#include "Inspection/VPackInspection.h"

namespace arangodb::aql::optimizer2::nodes {

struct CalculationNode : optimizer2::nodes::BaseNode,
                         optimizer2::types::Function {
  optimizer2::types::Variable outVariable;
  optimizer2::types::Expression expression;
  AttributeTypes::String expressionType;
  // Important TODO: From what I am reading from the code, this is set during
  // serialization phase, but it seems that this value is never read.
  std::optional<std::vector<optimizer2::types::Function>> functions;
};

template<typename Inspector>
auto inspect(Inspector& f, CalculationNode& x) {
  return f.object(x).fields(
      f.embedFields(static_cast<optimizer2::nodes::BaseNode&>(x)),
      f.field("outVariable", x.outVariable),
      f.field("expression", x.expression),
      f.field("expressionType", x.expressionType),
      f.field("functions", x.functions));
}

}  // namespace arangodb::aql::optimizer2::nodes
