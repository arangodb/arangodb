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
/// @author Yuriy Popov
////////////////////////////////////////////////////////////////////////////////

#ifndef ARANGOD_AQL_LATE_MATERIALIZATION_OPTIMIZER_RULES_COMMON_H
#define ARANGOD_AQL_LATE_MATERIALIZATION_OPTIMIZER_RULES_COMMON_H 1

#include <memory>
#include <vector>

namespace arangodb {

namespace basics {
struct AttributeName;
}

namespace aql {
struct AstNode;
class CalculationNode;
class Expression;
struct Variable;

namespace latematerialized {

struct AstAndFieldData {
  // ast node
  AstNode* parentNode;
  size_t childNumber;

  // index data
  std::vector<arangodb::basics::AttributeName> const* field;
  size_t fieldNumber;

  std::vector<std::string> postfix;
};

struct AstAndColumnFieldData : AstAndFieldData {
  ptrdiff_t columnNumber;
};

template<typename T>
struct NodeWithAttrs {
  using DataType = T;

  struct AttributeAndField {
    std::vector<arangodb::basics::AttributeName> attr;
    T afData;
  };

  CalculationNode* node;
  std::vector<AttributeAndField> attrs;
};

struct NodeExpressionWithAttrs : NodeWithAttrs<AstAndFieldData> {
  Expression* expression;
};

using NodeWithAttrsColumn = latematerialized::NodeWithAttrs<latematerialized::AstAndColumnFieldData>;

template<typename T>
bool getReferencedAttributes(AstNode* node, Variable const* variable, T& nodeAttrs);

bool isPrefix(std::vector<arangodb::basics::AttributeName> const& prefix,
              std::vector<arangodb::basics::AttributeName> const& attrs,
              bool ignoreExpansionInLast,
              std::vector<std::string>& postfix);

}  // namespace latematerialized
}  // namespace aql
}  // namespace arangodb

#endif  // ARANGOD_AQL_LATE_MATERIALIZATION_OPTIMIZER_RULES_COMMON_H
