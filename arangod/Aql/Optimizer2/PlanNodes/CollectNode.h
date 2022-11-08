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

namespace arangodb::aql::optimizer2::nodes {

namespace CollectNodeStructs {
// TODO: Move this into its own file and another place.

struct CollectNodeAggregate : optimizer2::types::Variable {
  std::optional<optimizer2::types::Variable> inVariable;
  std::optional<optimizer2::types::Variable> outVariable;
  AttributeTypes::String type;
};

template<typename Inspector>
auto inspect(Inspector& f, CollectNodeAggregate& x) {
  return f.object(x).fields(f.field("inVariable", x.inVariable),
                            f.field("outVariable", x.outVariable),
                            f.field("type", x.type));
}

struct CollectNodeVariable {
  optimizer2::types::Variable variable;

  bool operator==(CollectNodeVariable const&) const = default;
};

template<typename Inspector>
auto inspect(Inspector& f, CollectNodeVariable& x) {
  return f.object(x).fields(f.field("variable", x.variable));
}

struct CollectNodeGroupVariable {
  optimizer2::types::Variable outVariable;
  optimizer2::types::Variable inVariable;

  bool operator==(CollectNodeGroupVariable const&) const = default;
};

template<typename Inspector>
auto inspect(Inspector& f, CollectNodeGroupVariable& x) {
  return f.object(x).fields(f.field("inVariable", x.inVariable),
                            f.field("outVariable", x.outVariable));
}

struct CollectNodeAggregateVariable {
  optimizer2::types::Variable outVariable;
  std::optional<optimizer2::types::Variable> inVariable;
  AttributeTypes::String type;  // "functionName"

  bool operator==(CollectNodeAggregateVariable const&) const = default;
};

template<typename Inspector>
auto inspect(Inspector& f, CollectNodeAggregateVariable& x) {
  return f.object(x).fields(f.field("inVariable", x.inVariable),
                            f.field("outVariable", x.outVariable),
                            f.field("type", x.type));
}

enum class CollectMethod { UNDEFINED, HASH, SORTED, DISTINCT, COUNT };

template<class Inspector>
auto inspect(Inspector& f, CollectMethod& x) {
  return f.enumeration(x).values(
      CollectMethod::UNDEFINED,
      "undefined",  // TODO: current implementation throws in case of undefined
      CollectMethod::HASH, "hash", CollectMethod::SORTED, "sorted",
      CollectMethod::DISTINCT, "distinct", CollectMethod::COUNT, "count");
}

struct CollectOptions {
  CollectMethod method;

  bool operator==(CollectOptions const&) const = default;
};

template<typename Inspector>
auto inspect(Inspector& f, CollectOptions& x) {
  return f.object(x).fields(f.field("method", x.method));
}
}  // namespace CollectNodeStructs

struct CollectNode : optimizer2::nodes::BaseNode {
  optimizer2::nodes::CollectNodeStructs::CollectOptions collectOptions;
  std::vector<optimizer2::nodes::CollectNodeStructs::CollectNodeGroupVariable>
      groups;
  std::vector<
      optimizer2::nodes::CollectNodeStructs::CollectNodeAggregateVariable>
      aggregates;
  std::optional<optimizer2::types::Variable> expression;
  std::optional<optimizer2::types::Variable> outVariable;
  std::optional<
      std::vector<optimizer2::nodes::CollectNodeStructs::CollectNodeVariable>>
      keepVariables;
  bool isDistinctCommand;
  bool specialized;

  bool operator==(CollectNode const& other) const {
    return (this->groups == other.groups &&
            this->aggregates == other.aggregates &&
            this->expression == other.expression &&
            this->outVariable == other.outVariable &&
            this->keepVariables == other.keepVariables &&
            this->collectOptions == other.collectOptions &&
            this->isDistinctCommand == other.isDistinctCommand &&
            this->specialized == other.specialized);
  }
};

template<typename Inspector>
auto inspect(Inspector& f, CollectNode& x) {
  return f.object(x).fields(
      f.embedFields(static_cast<optimizer2::nodes::BaseNode&>(x)),
      f.field("collectOptions", x.collectOptions), f.field("groups", x.groups),
      f.field("aggregates", x.aggregates), f.field("expression", x.expression),
      f.field("outVariable", x.outVariable),
      f.field("keepVariables", x.keepVariables),
      f.field("isDistinctCommand", x.isDistinctCommand),
      f.field("specialized", x.specialized));
}

}  // namespace arangodb::aql::optimizer2::nodes
