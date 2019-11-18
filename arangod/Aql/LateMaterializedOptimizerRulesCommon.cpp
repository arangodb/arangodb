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

#include "Aql/Ast.h"
#include "Aql/LateMaterializedOptimizerRulesCommon.h"

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
struct TraversalState {
  Variable const* variable;
  latematerialized::NodeWithAttrs& nodeAttrs;
  bool optimize;
  bool wasAccess;
};
}

// determines attributes referenced in an expression for the specified out variable
bool latematerialized::getReferencedAttributes(AstNode* node,
                                               Variable const* variable,
                                               NodeWithAttrs& nodeAttrs) {
  TraversalState state{variable, nodeAttrs, true, false};

  auto preVisitor = [&state](AstNode const* node,
      AstNode* parentNode, size_t childNumber) {
    if (node == nullptr) {
      return false;
    }

    switch (node->type) {
      case NODE_TYPE_ATTRIBUTE_ACCESS:
        if (!state.wasAccess) {
          state.nodeAttrs.attrs.emplace_back(
            NodeWithAttrs::AttributeAndField{std::vector<arangodb::basics::AttributeName>{
              {std::string(node->getStringValue(), node->getStringLength()), false}}, {parentNode, childNumber, nullptr, 0}});
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
