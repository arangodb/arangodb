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
/// @author Julia Puget
/// @author Koichi Nakata
////////////////////////////////////////////////////////////////////////////////

#include "OptimizerRuleRemoveCollectVariables.h"

#include "Aql/Ast.h"
#include "Aql/AstHelper.h"
#include "Aql/ExecutionNode/CalculationNode.h"
#include "Aql/ExecutionNode/CollectNode.h"
#include "Aql/ExecutionNode/ExecutionNode.h"
#include "Aql/ExecutionPlan.h"
#include "Aql/Expression.h"
#include "Aql/Function.h"
#include "Aql/Optimizer.h"
#include "Aql/TypedAstNodes.h"
#include "Aql/Variable.h"
#include "Containers/SmallVector.h"

using namespace arangodb;
using namespace arangodb::aql;
using EN = arangodb::aql::ExecutionNode;

/// @brief remove INTO of a COLLECT if not used
/// additionally remove all unused aggregate calculations from a COLLECT
void arangodb::aql::removeCollectVariablesRule(
    Optimizer* opt, std::unique_ptr<ExecutionPlan> plan,
    OptimizerRule const& rule) {
  containers::SmallVector<ExecutionNode*, 8> nodes;
  plan->findNodesOfType(nodes, EN::COLLECT, true);

  bool modified = false;

  for (ExecutionNode* n : nodes) {
    auto collectNode = ExecutionNode::castTo<CollectNode*>(n);
    TRI_ASSERT(collectNode != nullptr);

    auto const& varsUsedLater = collectNode->getVarsUsedLater();
    auto outVariable = collectNode->outVariable();

    if (outVariable != nullptr &&
        varsUsedLater.find(outVariable) == varsUsedLater.end()) {
      // outVariable not used later
      collectNode->clearOutVariable();
      collectNode->clearKeepVariables();
      modified = true;
    } else if (outVariable != nullptr &&
               !collectNode->hasExpressionVariable()) {
      // outVariable used later, no INTO expression, no KEEP
      // e.g. COLLECT something INTO g
      // we will now check how many parts of "g" are used later

      auto keepAttributes = containers::HashSet<std::string>();

      bool doOptimize = true;
      auto planNode = collectNode->getFirstParent();
      while (planNode != nullptr && doOptimize) {
        if (planNode->getType() == EN::CALCULATION) {
          auto cc = ExecutionNode::castTo<CalculationNode const*>(planNode);
          Expression const* exp = cc->expression();
          if (exp->node() != nullptr) {
            bool isSafeForOptimization;
            auto usedThere = ast::getReferencedAttributesForKeep(
                exp->node(), outVariable, isSafeForOptimization);
            if (isSafeForOptimization) {
              for (auto const& it : usedThere) {
                keepAttributes.emplace(it);
              }
            } else {
              doOptimize = false;
              break;
            }

          }  // end - expression exists
        } else {
          auto here = planNode->getVariableIdsUsedHere();
          if (here.find(outVariable->id) != here.end()) {
            // the outVariable of the last collect should not be used by any
            // following node directly
            doOptimize = false;
            break;
          }
          if (planNode->getType() == EN::COLLECT) {
            break;
          }
        }

        planNode = planNode->getFirstParent();

      }  // end - inspection of nodes below the found collect node - while valid
         // planNode

      if (doOptimize) {
        auto keepVariables = containers::HashSet<Variable const*>();
        // we are allowed to do the optimization
        auto current = n->getFirstDependency();
        while (current != nullptr) {
          for (auto const& var : current->getVariablesSetHere()) {
            for (auto it = keepAttributes.begin(); it != keepAttributes.end();
                 /* no hoisting */) {
              if ((*it) == var->name) {
                keepVariables.emplace(var);
                it = keepAttributes.erase(it);
              } else {
                ++it;
              }
            }
          }
          if (keepAttributes.empty()) {
            // done
            break;
          }
          current = current->getFirstDependency();
        }  // while current

        if (keepAttributes.empty() && !keepVariables.empty()) {
          collectNode->restrictKeepVariables(keepVariables);
          modified = true;
        }
      }  // end - if doOptimize
    }    // end - if collectNode has outVariable

    size_t numGroupVariables = collectNode->groupVariables().size();
    size_t numAggregateVariables = collectNode->aggregateVariables().size();

    collectNode->clearAggregates(
        [&varsUsedLater, &numGroupVariables, &numAggregateVariables,
         &modified](AggregateVarInfo const& aggregate) -> bool {
          // it is ok to remove unused aggregations if we have at least one
          // aggregate variable remaining, or if we have a group variable left.
          // it is not ok to have 0 aggregate variables and 0 group variables
          // left, because the different COLLECT executors require some
          // variables to be present.
          if (varsUsedLater.find(aggregate.outVar) == varsUsedLater.end()) {
            // result of aggregate function not used later
            if (numGroupVariables > 0 || numAggregateVariables > 1) {
              --numAggregateVariables;
              modified = true;
              return true;
            }
          }
          return false;
        });

    TRI_ASSERT(!collectNode->groupVariables().empty() ||
               !collectNode->aggregateVariables().empty());

  }  // for node in nodes
  opt->addPlan(std::move(plan), rule, modified);
}