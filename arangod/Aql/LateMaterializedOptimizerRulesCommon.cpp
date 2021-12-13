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
/// @author Yuriy Popov
////////////////////////////////////////////////////////////////////////////////

#include "Aql/Ast.h"
#include "Aql/LateMaterializedOptimizerRulesCommon.h"
#include "Aql/IResearchViewNode.h"
#include "IResearch/IResearchViewSort.h"

using namespace arangodb::aql;

namespace {

// traverse the AST, using previsitor
void traverseReadOnly(AstNode* node, AstNode* parentNode, size_t childNumber,
                      std::function<bool(AstNode const*, AstNode*, size_t)> const& preVisitor) {
  if (node == nullptr) {
    return;
  }

  if (!preVisitor(node, parentNode, childNumber)) {
    return;
  }

  size_t const n = node->numMembers();

  for (size_t i = 0; i < n; ++i) {
    auto member = node->getMemberUnchecked(i);

    if (member != nullptr) {
      traverseReadOnly(member, node, i, preVisitor);
    }
  }
}

// traversal state
template<typename T>
struct TraversalState {
  Variable const* variable;
  T& nodeAttrs;
  bool optimize;
  bool wasAccess;
};

}

namespace arangodb {
namespace aql {
namespace latematerialized {

// determines attributes referenced in an expression for the specified out variable
template<typename T>
bool getReferencedAttributes(AstNode* node,
                             Variable const* variable,
                             T& nodeAttrs) {
  TraversalState<T> state{variable, nodeAttrs, true, false};

  auto preVisitor = [&state](AstNode const* node,
      AstNode* parentNode, size_t childNumber) {
    if (node == nullptr) {
      return false;
    }

    switch (node->type) {
      case NODE_TYPE_ATTRIBUTE_ACCESS:
        if (!state.wasAccess) {
          typename T::DataType afData;
          afData.parentNode = parentNode;
          afData.childNumber = childNumber;
          state.nodeAttrs.attrs.emplace_back(
            AttributeAndField<typename T::DataType>{std::vector<arangodb::basics::AttributeName>{
              {std::string(node->getStringValue(), node->getStringLength()), false}}, std::move(afData)});
          state.wasAccess = true;
        } else {
          state.nodeAttrs.attrs.back().attr.emplace_back(std::string(node->getStringValue(), node->getStringLength()), false);
        }
        return true;
      case NODE_TYPE_REFERENCE: {
        // reference to a variable
        auto v = static_cast<Variable const*>(node->getData());
        if (v == state.variable) {
          if (!state.wasAccess) {
            // we haven't seen an attribute access directly before
            state.optimize = false;

            return false;
          }
          std::reverse(state.nodeAttrs.attrs.back().attr.begin(), state.nodeAttrs.attrs.back().attr.end());
        } else if (state.wasAccess) {
          state.nodeAttrs.attrs.pop_back();
        }
        // finish an attribute path
        state.wasAccess = false;
        return true;
      }
      default:
        break;
    }

    if (state.wasAccess) {
      // not appropriate node type
      state.wasAccess = false;
      state.optimize = false;

      return false;
    }

    return true;
  };

  traverseReadOnly(node, nullptr, 0, preVisitor);

  return state.optimize;
}

template<bool indexDataOnly, typename Attrs>
bool attributesMatch(iresearch::IResearchViewSort const& primarySort,
                     iresearch::IResearchViewStoredValues const& storedValues,
                     Attrs& attrs,
                     std::vector<std::vector<ColumnVariant<indexDataOnly>>>& usedColumnsCounter,
                     size_t columnsCount) {
  TRI_ASSERT(columnsCount <= usedColumnsCounter.size());
  // check all node attributes to be in sort
  std::remove_reference_t<decltype(usedColumnsCounter)> tmpUsedColumnsCounter(columnsCount);
  typename ColumnVariant<indexDataOnly>::PostfixType postfix{};
  for (auto& nodeAttr : attrs) {
    auto found = false;
    nodeAttr.afData.field = nullptr;
    // try to find in the sort column
    size_t fieldNum = 0;
    TRI_ASSERT(!tmpUsedColumnsCounter.empty());
    auto& tmpSlot = tmpUsedColumnsCounter.front();
    for (auto const& field : primarySort.fields()) {
      if (latematerialized::isPrefix<indexDataOnly>(field, nodeAttr.attr, false, postfix)) {
        tmpSlot.emplace_back(&nodeAttr.afData, fieldNum, &field, std::move(postfix));
        if constexpr (std::is_arithmetic_v<decltype(postfix)>) {
          postfix = 0;
        }
        found = true;
        break;
      }
      ++fieldNum;
    }
    // try to find in other columns
    ptrdiff_t columnNum = 1;
    for (auto const& column : storedValues.columns()) {
      fieldNum = 0;
      TRI_ASSERT(static_cast<ptrdiff_t>(tmpUsedColumnsCounter.size()) >= columnNum);
      auto& tmpSlot = tmpUsedColumnsCounter[columnNum];
      for (auto const& field : column.fields) {
        if (latematerialized::isPrefix<indexDataOnly>(field.second, nodeAttr.attr, false, postfix)) {
          tmpSlot.emplace_back(&nodeAttr.afData, fieldNum, &field.second, std::move(postfix));
          if constexpr (std::is_arithmetic_v<decltype(postfix)>) {
            postfix = 0;
          }
          found = true;
          break;
        }
        ++fieldNum;
      }
      ++columnNum;
    }
    // not found value in columns
    if (!found) {
      return false;
    }
  }
  static_assert(std::is_move_constructible_v<ColumnVariant<indexDataOnly>>,
                "To efficiently move from temp variable we need working move for ColumnVariant");
  // store only on successful exit, otherwise pointers to afData will be invalidated as Node will be not stored!
  size_t current = 0;
  for (auto it = tmpUsedColumnsCounter.begin(); it != tmpUsedColumnsCounter.end(); ++it) {
    std::move(it->begin(), it->end(),  irs::irstd::back_emplacer(usedColumnsCounter[current++]));
  }
  return true;
}

template<bool indexDataOnly>
void setAttributesMaxMatchedColumns(std::vector<std::vector<ColumnVariant<indexDataOnly>>>& usedColumnsCounter,
                                    size_t columnsCount) {
  TRI_ASSERT(columnsCount <= usedColumnsCounter.size());
  std::vector<ptrdiff_t> idx(columnsCount);
  std::iota(idx.begin(), idx.end(), 0);
  // first is max size one
  std::sort(idx.begin(), idx.end(), [&usedColumnsCounter](auto const lhs, auto const rhs) {
    auto const& lhs_val = usedColumnsCounter[lhs];
    auto const& rhs_val = usedColumnsCounter[rhs];
    auto const lSize = lhs_val.size();
    auto const rSize = rhs_val.size();
    // column contains more fields or
    // columns sizes == 1 and postfix is less (less column size) or
    // less column number (sort column priority)
    if constexpr (indexDataOnly) {
      return lSize > rSize ||
          (lSize == rSize && ((lSize == 1 && lhs_val[0].postfix < rhs_val[0].postfix) ||
          lhs < rhs));
    } else {
      return lSize > rSize ||
          (lSize == rSize && ((lSize == 1 && lhs_val[0].postfix.size() < rhs_val[0].postfix.size()) ||
          lhs < rhs));
    }
  });
  // get values from columns which contain max number of appropriate values
  for (auto i : idx) {
    auto& it = usedColumnsCounter[i];
    for (auto& f : it) {
      TRI_ASSERT(f.afData);
      if (f.afData->field == nullptr) {
        f.afData->fieldNumber = f.fieldNum;
        f.afData->field = f.field;
        // if assertion below is violated consider adding proper i -> columnNum conversion
        // for filling f.afData->columnNumber
        static_assert((-1) == iresearch::IResearchViewNode::SortColumnNumber, "Value is no more valid for such implementation");
        f.afData->columnNumber = i-1;
        f.afData->postfix = std::move(f.postfix);
      }
    }
  }
}

template<bool indexDataOnly>
bool isPrefix(std::vector<arangodb::basics::AttributeName> const& prefix,
                                std::vector<arangodb::basics::AttributeName> const& attrs,
                                bool ignoreExpansionInLast,
                                typename ColumnVariant<indexDataOnly>::PostfixType& postfix) {
#ifdef ARANGODB_ENABLE_MAINTAINER_MODE
  if constexpr (!indexDataOnly) {
    TRI_ASSERT(postfix.empty());
  } else {
    TRI_ASSERT(postfix == 0);
  }
#endif
  if (prefix.size() > attrs.size()) {
    return false;
  }

  size_t i = 0;
  for (; i < prefix.size(); ++i) {
    if (prefix[i].name != attrs[i].name) {
      return false;
    }
    if (prefix[i].shouldExpand != attrs[i].shouldExpand) {
      if (!ignoreExpansionInLast) {
        return false;
      }
      if (i != prefix.size() - 1) {
        return false;
      }
    }
  }
  if (i < attrs.size()) {
    if constexpr (indexDataOnly) {
      postfix = attrs.size() - i;
    } else {
      postfix.reserve(attrs.size() - i);
      std::transform(attrs.cbegin() + static_cast<ptrdiff_t>(i),
                     attrs.cend(), std::back_inserter(postfix), [](auto const& attr) {
        return attr.name;
      });
    }
  }
  return true;
}

} // namespace arangodb::aql::latematerialized

template bool latematerialized::getReferencedAttributes(
  AstNode* node,
  Variable const* variable,
  NodeExpressionWithAttrs& nodeAttrs);

template bool latematerialized::getReferencedAttributes(
  AstNode* node,
  Variable const* variable,
  NodeWithAttrsColumn& nodeAttrs);

template
bool latematerialized::isPrefix<false>(
  std::vector<arangodb::basics::AttributeName> const& prefix,
  std::vector<arangodb::basics::AttributeName> const& attrs,
  bool ignoreExpansionInLast,
  typename ColumnVariant<false>::PostfixType& postfix);

template
void latematerialized::setAttributesMaxMatchedColumns<false>(
  std::vector<std::vector<latematerialized::ColumnVariant<false>>>& usedColumnsCounter,
  size_t columnsCount);

template
void latematerialized::setAttributesMaxMatchedColumns<true>(
  std::vector<std::vector<latematerialized::ColumnVariant<true>>>& usedColumnsCounter,
  size_t columnsCount);

template
bool latematerialized::attributesMatch<true,
                                       std::vector<
                                         latematerialized::AttributeAndField<
                                           latematerialized::IndexFieldData>>>  (
  iresearch::IResearchViewSort const& primarySort,
  iresearch::IResearchViewStoredValues const& storedValues,
  std::vector<latematerialized::AttributeAndField<latematerialized::IndexFieldData>>& attrs,
  std::vector<std::vector<ColumnVariant<true>>>& usedColumnsCounter,
  size_t columnsCount);

template
bool latematerialized::attributesMatch<false,
                                       std::vector<
                                         latematerialized::AttributeAndField<
                                         latematerialized::AstAndColumnFieldData>>>  (
  iresearch::IResearchViewSort const& primarySort,
  iresearch::IResearchViewStoredValues const& storedValues,
  std::vector<latematerialized::AttributeAndField<latematerialized::AstAndColumnFieldData>>& attrs,
  std::vector<std::vector<ColumnVariant<false>>>& usedColumnsCounter,
  size_t columnsCount);
