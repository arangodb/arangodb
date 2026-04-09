////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2014-2026 ArangoDB GmbH, Cologne, Germany
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
/// @author Max Neunhoeffer
/// @author Jan Steemann
////////////////////////////////////////////////////////////////////////////////

#include "RemoveUnnecessaryCalculations.h"

#include "Aql/Ast.h"
#include "Aql/ExecutionNode/CalculationNode.h"
#include "Aql/ExecutionNode/CollectNode.h"
#include "Aql/ExecutionNode/ExecutionNode.h"
#include "Aql/ExecutionNode/SubqueryNode.h"
#include "Aql/ExecutionPlan.h"
#include "Aql/Expression.h"
#include "Aql/Optimizer.h"
#include "Aql/Optimizer/Utils/VariableReplacer.h"
#include "Aql/TypedAstNodes.h"
#include "Aql/Variable.h"
#include "Containers/SmallVector.h"

namespace arangodb::aql {
using EN = ExecutionNode;

namespace {
static constexpr std::initializer_list<ExecutionNode::NodeType>
    removeUnnecessaryCalculationsNodeTypes{ExecutionNode::CALCULATION,
                                           ExecutionNode::SUBQUERY};
}  // namespace

void removeUnnecessaryCalculationsRule(Optimizer* opt,
                                       std::unique_ptr<ExecutionPlan> plan,
                                       OptimizerRule const& rule) {
  containers::SmallVector<ExecutionNode*, 8> nodes;
  plan->findNodesOfType(nodes, removeUnnecessaryCalculationsNodeTypes, true);

  ::arangodb::containers::HashSet<ExecutionNode*> toUnlink;

  bool modified = false;

  for (auto const& n : nodes) {
    Variable const* outVariable = nullptr;

    if (n->getType() == EN::CALCULATION) {
      auto nn = ExecutionNode::castTo<CalculationNode*>(n);

      if (!nn->isDeterministic()) {
        continue;
      }

      outVariable = nn->outVariable();
    } else if (n->getType() == EN::SUBQUERY) {
      auto nn = ExecutionNode::castTo<SubqueryNode*>(n);

      if (!nn->isDeterministic()) {
        continue;
      }

      if (nn->isModificationNode()) {
        continue;
      }
      outVariable = nn->outVariable();
    } else {
      TRI_ASSERT(false);
      continue;
    }

    TRI_ASSERT(outVariable != nullptr);

    if (!n->isVarUsedLater(outVariable)) {
      toUnlink.emplace(n);
    } else if (n->getType() == EN::CALCULATION) {
      CalculationNode* calcNode = ExecutionNode::castTo<CalculationNode*>(n);

      if (!calcNode->expression()->isDeterministic()) {
        continue;
      }

      AstNode const* rootNode = calcNode->expression()->node();

      if (rootNode->type == NODE_TYPE_REFERENCE) {
        bool hasCollectWithOutVariable = false;
        auto current = n->getFirstParent();

        while (current != nullptr) {
          if (current->getType() == EN::COLLECT) {
            CollectNode const* collectNode =
                ExecutionNode::castTo<CollectNode const*>(current);
            if (collectNode->hasOutVariable() &&
                !collectNode->hasExpressionVariable()) {
              hasCollectWithOutVariable = true;
              break;
            }
          }
          current = current->getFirstParent();
        }

        if (!hasCollectWithOutVariable) {
          std::unordered_map<VariableId, Variable const*> replacements;
          replacements.try_emplace(outVariable->id,
                                   ast::ReferenceNode(rootNode).getVariable());

          VariableReplacer finder(replacements);
          plan->root()->walk(finder);
          toUnlink.emplace(n);
          continue;
        }
      } else if (rootNode->type == NODE_TYPE_ATTRIBUTE_ACCESS) {
        bool eligible = true;
        auto current = n->getFirstParent();

        VarSet vars;
        std::vector<CalculationNode*> found;

        while (current != nullptr) {
          vars.clear();
          current->getVariablesUsedHere(vars);
          if (current->getType() != EN::CALCULATION) {
            if (vars.contains(outVariable)) {
              eligible = false;
              break;
            }
          } else {
            if (vars.contains(outVariable)) {
              found.emplace_back(
                  ExecutionNode::castTo<CalculationNode*>(current));
            }
          }

          if (current->getType() == EN::COLLECT) {
            CollectNode const* collectNode =
                ExecutionNode::castTo<CollectNode const*>(current);
            if (collectNode->hasOutVariable() &&
                !collectNode->hasExpressionVariable()) {
              eligible = false;
              break;
            }
          }
          current = current->getFirstParent();
        }

        if (eligible) {
          auto visitor = [&](AstNode* node) {
            if (node->type == NODE_TYPE_REFERENCE &&
                static_cast<Variable const*>(node->getData()) ==
                    calcNode->outVariable()) {
              return const_cast<AstNode*>(rootNode);
            }
            return node;
          };
          for (auto const& it : found) {
            AstNode* simplified = plan->getAst()->traverseAndModify(
                it->expression()->nodeForModification(), visitor);
            it->expression()->replaceNode(simplified);
          }
          toUnlink.emplace(n);
          continue;
        }
      }

      VarSet vars;

      size_t usageCount = 0;
      CalculationNode* other = nullptr;
      auto current = n->getFirstParent();

      while (current != nullptr) {
        current->getVariablesUsedHere(vars);
        if (vars.contains(outVariable)) {
          if (current->getType() == EN::COLLECT) {
            if (ExecutionNode::castTo<CollectNode const*>(current)
                    ->hasOutVariable()) {
              usageCount = 0;
              break;
            }
          }
          if (current->getType() != EN::CALCULATION) {
            usageCount = 0;
            break;
          }

          ++usageCount;
          other = ExecutionNode::castTo<CalculationNode*>(current);
        }

        if (usageCount > 1) {
          break;
        }

        current = current->getFirstParent();
        vars.clear();
      }

      if (usageCount == 1) {
        auto otherExpression = other->expression();

        if (rootNode->type != NODE_TYPE_ATTRIBUTE_ACCESS &&
            Ast::countReferences(otherExpression->node(), outVariable) > 1) {
          continue;
        }

        if (rootNode->isSimple() != otherExpression->node()->isSimple()) {
          continue;
        }

        auto otherLoop = other->getLoop();

        if (otherLoop != nullptr && rootNode->callsFunction()) {
          auto nLoop = n->getLoop();

          if (nLoop != otherLoop) {
            continue;
          }
          VarSet outer = nLoop->getVarsValid();
          VarSet used;
          Ast::getReferencedVariables(rootNode, used);
          bool doOptimize = true;
          for (auto& it : used) {
            if (outer.find(it) == outer.end()) {
              doOptimize = false;
              break;
            }
          }
          if (!doOptimize) {
            continue;
          }
        }

        TRI_ASSERT(other != nullptr);
        otherExpression->replaceVariableReference(outVariable, rootNode);

        toUnlink.emplace(n);
      }
    }
  }

  if (!toUnlink.empty()) {
    plan->unlinkNodes(toUnlink);
    TRI_ASSERT(nodes.size() >= toUnlink.size());
    modified = true;
    if (nodes.size() - toUnlink.size() > 0) {
      opt->addPlanAndRerun(std::move(plan), rule, modified);
    } else {
      opt->addPlan(std::move(plan), rule, modified);
    }
  } else {
    opt->addPlan(std::move(plan), rule, modified);
  }
}
}  // namespace arangodb::aql