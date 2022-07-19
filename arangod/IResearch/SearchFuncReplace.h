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
/// @author Andrey Abramov
////////////////////////////////////////////////////////////////////////////////

#pragma once

#include "IResearch/SearchFunc.h"

namespace arangodb::iresearch {

class IResearchViewNode;

template<typename RefExtractor>
void replaceSearchFunc(aql::CalculationNode& node, DedupSearchFuncs& dedup,
                       RefExtractor&& refExtractor) {
  if (!node.expression()) {
    return;
  }

  auto& expr = *node.expression();
  auto* ast = expr.ast();

  if (!expr.ast()) {
    // ast is not set
    return;
  }

  auto* exprNode = expr.nodeForModification();

  if (!exprNode) {
    // node is not set
    return;
  }

  auto replaceFunc = [ast, &dedup,
                      &refExtractor](aql::AstNode* node) -> aql::AstNode* {
    TRI_ASSERT(node);  // ensured by 'Ast::traverseAndModify(...)'

    auto* ref = refExtractor(*node);

    if (!ref) {
      // not a valid search function
      return node;
    }

    HashedSearchFunc const key{ref, node};

    auto it = dedup.find(key);

    if (it == dedup.end()) {
      // create variable
      auto* var = ast->variables()->createTemporaryVariable();

      it = dedup.try_emplace(key, var).first;
    }

    return ast->createNodeReference(it->second);
  };

  // Try to modify root node of the expression
  auto newNode = replaceFunc(exprNode);

  if (exprNode != newNode) {
    // simple expression, e.g LET x = BM25(d)
    expr.replaceNode(newNode);
  } else if (const bool hasFunc = !arangodb::iresearch::visit<true>(
                 *exprNode,
                 [&refExtractor](aql::AstNode const& node) {
                   return nullptr == refExtractor(node);
                 });
             hasFunc) {
    auto* exprClone = exprNode->clone(ast);
    aql::Ast::traverseAndModify(exprClone, replaceFunc);
    expr.replaceNode(exprClone);
  }
}

void extractSearchFunc(IResearchViewNode const& viewNode,
                       DedupSearchFuncs& dedup, std::vector<SearchFunc>& funcs);

}  // namespace arangodb::iresearch
