////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2019 ArangoDB GmbH, Cologne, Germany
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
struct Variable;

namespace latematerialized {

struct AstAndFieldData {
  // Ast node
  AstNode* parentNode;
  size_t childNumber;

  // index data
  std::vector<arangodb::basics::AttributeName> const* field;
  size_t fieldNumber;
};

struct AstAndColumnFieldData : AstAndFieldData {
  ptrdiff_t columnNumber;
  std::vector<std::string> postfix;
};

template<typename T>
struct NodeWithAttrs {
  struct AttributeAndField {
    std::vector<arangodb::basics::AttributeName> attr;
    T afData;
  };

  std::vector<AttributeAndField> attrs;
  CalculationNode* node;
};

template<typename T>
bool getReferencedAttributes(AstNode* node, Variable const* variable, NodeWithAttrs<T>& nodeAttrs);

}  // latematerialized
}  // namespace aql
}  // namespace arangodb

#endif  // ARANGOD_AQL_LATE_MATERIALIZATION_OPTIMIZER_RULES_COMMON_H
