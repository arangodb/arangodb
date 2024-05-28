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
/// @author Yuriy Popov
////////////////////////////////////////////////////////////////////////////////

#pragma once

#include "IResearch/IResearchViewSort.h"

#include <memory>
#include <vector>
#include <string>

namespace arangodb {

namespace basics {
struct AttributeName;
}

namespace iresearch {
class IResearchViewSort;
class IResearchViewStoredValues;
}  // namespace iresearch

namespace aql {
struct AstNode;
class CalculationNode;
class Expression;
struct Variable;

namespace latematerialized {

struct AstAndFieldData {
  // ast node
  AstNode* parentNode{nullptr};
  size_t childNumber{0};

  // index data
  std::vector<arangodb::basics::AttributeName> const* field{nullptr};
  size_t fieldNumber{0};

  std::vector<std::string> postfix;
};

struct IndexFieldData {
  std::vector<arangodb::basics::AttributeName> const* field{nullptr};
  size_t fieldNumber{0};
  ptrdiff_t columnNumber{0};
  size_t postfix{0};
};

struct AstAndColumnFieldData : AstAndFieldData {
  ptrdiff_t columnNumber{0};
};

template<typename T>
struct AttributeAndField {
  std::vector<arangodb::basics::AttributeName> attr;
  T afData;
};

template<typename T>
struct NodeWithAttrs {
  using DataType = T;

  CalculationNode* node;
  std::vector<AttributeAndField<T>> attrs;
};

struct NodeExpressionWithAttrs : NodeWithAttrs<AstAndFieldData> {
  Expression* expression;
};

using NodeWithAttrsColumn = NodeWithAttrs<AstAndColumnFieldData>;

template<bool postfixLen>
struct ColumnVariant {
  using PostfixType =
      std::conditional_t<postfixLen, size_t, std::vector<std::string>>;
  using DataType =
      std::conditional_t<postfixLen, IndexFieldData, AstAndColumnFieldData>;

  DataType* afData;
  size_t fieldNum;
  std::vector<arangodb::basics::AttributeName> const* field;
  PostfixType postfix;

  ColumnVariant(DataType* afData, size_t fieldNum,
                std::vector<arangodb::basics::AttributeName> const* field,
                PostfixType&& postfix)
      : afData(afData),
        fieldNum(fieldNum),
        field(field),
        postfix(std::move(postfix)) {}
};

template<bool postfixLen, typename Attrs>
bool attributesMatch(
    iresearch::IResearchSortBase const& primarySort,
    iresearch::IResearchViewStoredValues const& storedValues, Attrs& attrs,
    std::vector<std::vector<ColumnVariant<postfixLen>>>& usedColumnsCounter,
    size_t columnsCount);

template<bool postfixLen>
void setAttributesMaxMatchedColumns(
    std::vector<std::vector<ColumnVariant<postfixLen>>>& usedColumnsCounter,
    size_t columnsCount);

template<typename T>
bool getReferencedAttributes(AstNode* node, Variable const* variable,
                             T& nodeAttrs);

template<bool postfixLen>
bool isPrefix(std::vector<arangodb::basics::AttributeName> const& prefix,
              std::vector<arangodb::basics::AttributeName> const& attrs,
              bool ignoreExpansionInLast,
              typename ColumnVariant<postfixLen>::PostfixType& postfix);

}  // namespace latematerialized
}  // namespace aql
}  // namespace arangodb
