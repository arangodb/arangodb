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
/// @author Jan Christoph Uhde
////////////////////////////////////////////////////////////////////////////////

#include "ReplaceLikeWithRange.h"

#include "Aql/AqlFunctionsInternalCache.h"
#include "Aql/Ast.h"
#include "Aql/AstNode.h"
#include "Aql/Collection.h"
#include "Aql/TypedAstNodes.h"
#include "Aql/ExecutionNode/CalculationNode.h"
#include "Aql/ExecutionNode/EnumerateCollectionNode.h"
#include "Aql/ExecutionNode/ExecutionNode.h"
#include "Aql/ExecutionPlan.h"
#include "Aql/Expression.h"
#include "Aql/Function.h"
#include "Aql/IndexHint.h"
#include "Aql/Optimizer.h"
#include "Aql/Optimizer/Utils/GetAstNode.h"
#include "Aql/Optimizer/Utils/GetFunction.h"
#include "Containers/SmallVector.h"
#include "Indexes/Index.h"
#include "VocBase/LogicalCollection.h"

namespace arangodb::aql {

using EN = ExecutionNode;

void replaceLikeWithRangeRule(Optimizer* opt,
                              std::unique_ptr<ExecutionPlan> plan,
                              OptimizerRule const& rule) {
  bool modified = false;

  containers::SmallVector<ExecutionNode*, 8> nodes;
  plan->findNodesOfType(nodes, ExecutionNode::CALCULATION, true);

  for (auto node : nodes) {
    auto visitor = [&modified, &plan](AstNode* node) {
      auto* func = getFunction(node);
      if (func != nullptr && func->name == "LIKE") {
        bool caseInsensitive = false;
        ast::FunctionCallNode likeFcall(node);
        auto args = likeFcall.getArguments().getElements();
        TRI_ASSERT(args.size() >= 2);
        if (args.size() >= 3) {
          caseInsensitive = true;
          auto caseArg = args[2];
          if (caseArg->isConstant()) {
            caseInsensitive = caseArg->isTrue();
          }
        }

        auto patternArg = args[1];

        if (!caseInsensitive && patternArg->isStringValue() &&
            args[0]->type == NODE_TYPE_ATTRIBUTE_ACCESS) {
          AstNode const* sub = args[0];
          while (sub != nullptr && sub->type == NODE_TYPE_ATTRIBUTE_ACCESS) {
            ast::AttributeAccessNode attrAccess(sub);
            sub = attrAccess.getObject();
          }
          if (sub == nullptr || sub->type != NODE_TYPE_REFERENCE) {
            return node;
          }
          ast::ReferenceNode ref(sub);
          auto setter = plan->getVarSetBy(ref.getVariable()->id);
          if (setter == nullptr ||
              setter->getType() != EN::ENUMERATE_COLLECTION) {
            return node;
          }
          auto cn =
              ExecutionNode::castTo<EnumerateCollectionNode const*>(setter);
          auto const& hint = cn->hint();
          if (hint.isDisabled()) {
            return node;
          }
          if (hint.isSimple()) {
            Collection const* c = cn->collection();

            for (auto const& name : hint.candidateIndexes()) {
              auto idx = c->getCollection()->lookupIndex(name);
              if (idx != nullptr &&
                  idx->type() == Index::TRI_IDX_TYPE_INVERTED_INDEX) {
                return node;
              }
            }
          }

          std::string unescapedPattern;
          auto [wildcardFound, wildcardIsLastChar] =
              AqlFunctionsInternalCache::inspectLikePattern(
                  unescapedPattern, patternArg->getStringView());

          if (!wildcardFound) {
            TRI_ASSERT(!wildcardIsLastChar);

            modified = true;
            Ast* ast = plan->getAst();

            char const* p = ast->resources().registerString(
                unescapedPattern.data(), unescapedPattern.size());
            AstNode* pattern =
                ast->createNodeValueString(p, unescapedPattern.size());

            return ast->createNodeBinaryOperator(NODE_TYPE_OPERATOR_BINARY_EQ,
                                                 args[0], pattern);
          }

          if (!unescapedPattern.empty()) {
            modified = true;
            Ast* ast = plan->getAst();

            char const* p = ast->resources().registerString(
                unescapedPattern.data(), unescapedPattern.size());
            AstNode* pattern =
                ast->createNodeValueString(p, unescapedPattern.size());
            AstNode* lhs = ast->createNodeBinaryOperator(
                NODE_TYPE_OPERATOR_BINARY_GE, args[0], pattern);

            constexpr std::string_view upper = "\xef\xbf\xbf";
            unescapedPattern.append(upper);
            p = ast->resources().registerString(unescapedPattern.data(),
                                                unescapedPattern.size());
            pattern = ast->createNodeValueString(p, unescapedPattern.size());
            AstNode* rhs = ast->createNodeBinaryOperator(
                NODE_TYPE_OPERATOR_BINARY_LT, args[0], pattern);

            AstNode* op = ast->createNodeBinaryOperator(
                NODE_TYPE_OPERATOR_BINARY_AND, lhs, rhs);
            return ast->createNodeBinaryOperator(NODE_TYPE_OPERATOR_BINARY_AND,
                                                 op, node);
          }
        }
      }

      return node;
    };

    CalculationNode* calc = ExecutionNode::castTo<CalculationNode*>(node);
    auto* original = getAstNode(calc);
    auto* replacement = Ast::traverseAndModify(original, visitor);

    if (replacement != original) {
      calc->expression()->replaceNode(replacement);
    }
  }

  opt->addPlan(std::move(plan), rule, modified);
}

}  // namespace arangodb::aql
