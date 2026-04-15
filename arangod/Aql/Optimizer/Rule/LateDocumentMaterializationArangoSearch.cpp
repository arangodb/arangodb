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
/// @author Andrey Abramov
/// @author Vasiliy Nabatchikov
////////////////////////////////////////////////////////////////////////////////

#include "LateDocumentMaterializationArangoSearch.h"

#include "Aql/Ast.h"
#include "Aql/CalculationNodeVarFinder.h"
#include "Aql/ExecutionNode/CalculationNode.h"
#include "Aql/ExecutionNode/ExecutionNode.h"
#include "Aql/ExecutionNode/IResearchViewNode.h"
#include "Aql/ExecutionNode/LimitNode.h"
#include "Aql/ExecutionNode/MaterializeSearchNode.h"
#include "Aql/ExecutionNode/SortNode.h"
#include "Aql/ExecutionNode/SubqueryNode.h"
#include "Aql/ExecutionPlan.h"
#include "Aql/Optimizer.h"
#include "Aql/OptimizerRule.h"
#include "Basics/ScopeGuard.h"

namespace arangodb::iresearch {

void lateDocumentMaterializationArangoSearchRule(
    aql::Optimizer* opt, std::unique_ptr<aql::ExecutionPlan> plan,
    aql::OptimizerRule const& rule) {
  using EN = aql::ExecutionNode;

  auto modified = false;
  auto const addPlan =
      arangodb::scopeGuard([opt, &plan, &rule, &modified]() noexcept {
        opt->addPlan(std::move(plan), rule, modified);
      });
  // arangosearch view node supports late materialization
  // cppcheck-suppress accessMoved
  if (!plan->contains(EN::ENUMERATE_IRESEARCH_VIEW) ||
      // we need sort node  to be present  (without sort it will be just skip,
      // nothing to optimize)
      !plan->contains(EN::SORT) ||
      // limit node is needed as without limit all documents will be returned
      // anyway, nothing to optimize
      !plan->contains(EN::LIMIT)) {
    return;
  }

  aql::VarSet currentUsedVars;
  // nodes variables can be replaced
  containers::SmallVector<aql::CalculationNode*, 16> calcNodes;
  containers::SmallVector<EN*, 8> nodes;
  plan->findNodesOfType(nodes, EN::LIMIT, true);
  for (auto* limitNode : nodes) {
    auto* loop = const_cast<EN*>(limitNode->getLoop());
    if (loop != nullptr && EN::ENUMERATE_IRESEARCH_VIEW == loop->getType()) {
      auto& viewNode = *EN::castTo<IResearchViewNode*>(loop);
      if (viewNode.isNoMaterialization() || viewNode.isLateMaterialized() ||
          viewNode.isHeapSort()) {
        continue;  // loop is already optimized
      }
      auto* current = limitNode->getFirstDependency();
      TRI_ASSERT(current);
      EN* sortNode = nullptr;
      // examining plan. We are looking for SortNode closest to lowest LimitNode
      // without document body usage before that node.
      // this node could be appended with materializer
      auto stopSearch = false;
      auto stickToSortNode = false;
      auto const& var = viewNode.outVariable();
      calcNodes.clear();
      auto& viewNodeState = viewNode.state();
      while (current != loop) {
        auto const type = current->getType();
        switch (type) {
          case EN::SORT:
            if (sortNode == nullptr) {  // we need nearest to limit sort node,
                                        // so keep selected if any
              sortNode = current;
            }
            break;
          case EN::REMOTE:
            // REMOTE node is a blocker - we do not want to make materialization
            // calls across cluster! Moreover we pass raw collection pointer -
            // this must not cross process border!
            if (sortNode != nullptr) {
              stopSearch = true;
            } else {
              stickToSortNode = true;
            }
            break;
          case EN::LIMIT:
            // After sort-limit rule was modified we could encounter additional
            // limit nodes before Sort. Break search on them if still no sort
            // found. As we need the closest LIMIT to the Sort. If we encounter
            // additional LIMITs after we found a Sort node that is ok as it
            // makes no harm for the late materialization.
            if (sortNode == nullptr) {
              stopSearch = true;
            }
            break;
          default:
            break;
        }
        if (!stopSearch) {
          currentUsedVars.clear();
          current->getVariablesUsedHere(currentUsedVars);
          if (currentUsedVars.find(&var) != currentUsedVars.end()) {
            // currently only calculation nodes expected to use a loop variable
            // with attributes we successfully replace all references to the
            // loop variable
            auto valid = false;
            switch (type) {
              case EN::CALCULATION: {
                auto* calcNode = EN::castTo<aql::CalculationNode*>(current);
                TRI_ASSERT(calcNode);
                if (viewNodeState.canVariablesBeReplaced(calcNode)) {
                  calcNodes.emplace_back(calcNode);
                  valid = true;
                }
                break;
              }
              case EN::SUBQUERY: {
                auto& subqueryNode =
                    *EN::castTo<aql::SubqueryNode*>(current);
                auto* subquery = subqueryNode.getSubquery();
                TRI_ASSERT(subquery);
                containers::SmallVector<EN*, 8> subqueryCalcNodes;
                // find calculation nodes in the plan of a subquery
                aql::CalculationNodeVarFinder finder(&var, subqueryCalcNodes);
                valid = !subquery->walk(finder);
                if (valid) {  // if the finder did not stop
                  for (auto* scn : subqueryCalcNodes) {
                    TRI_ASSERT(scn && scn->getType() == EN::CALCULATION);
                    currentUsedVars.clear();
                    scn->getVariablesUsedHere(currentUsedVars);
                    if (currentUsedVars.find(&var) != currentUsedVars.end()) {
                      auto* calcNode =
                          EN::castTo<aql::CalculationNode*>(scn);
                      TRI_ASSERT(calcNode);
                      if (viewNodeState.canVariablesBeReplaced(calcNode)) {
                        calcNodes.emplace_back(calcNode);
                      } else {
                        valid = false;
                        break;
                      }
                    }
                  }
                }
                break;
              }
              default:
                break;
            }
            if (!valid) {
              if (sortNode != nullptr) {
                // we have a doc body used before selected SortNode
                // forget it, let`s look for better sort to use
                stopSearch = true;
              } else {
                // we are between limit and sort nodes
                // late materialization could still be applied but we must
                // insert MATERIALIZE node after sort not after limit
                stickToSortNode = true;
              }
            }
          }
        }
        if (stopSearch) {
          // this limit node affects only closest sort if this sort is invalid
          // we need to check other limit node
          sortNode = nullptr;
          break;
        }
        current = current->getFirstDependency();  // inspect next node
      }
      if (sortNode) {
        // we could apply late materialization
        // 1. Replace view variables in calculation node if need
        if (!calcNodes.empty()) {
          ::arangodb::containers::HashSet<EN*> toUnlink;
          auto viewVariables =
              viewNodeState.replaceViewVariables(calcNodes, toUnlink);
          viewNode.setViewVariables(viewVariables);
          if (!toUnlink.empty()) {
            plan->unlinkNodes(toUnlink);
          }
        }
        // 2. We need to notify view - it should not materialize documents, but
        // produce only localDocIds
        // 3. We need to add materializer after limit node to do materialization
        auto* ast = plan->getAst();
        TRI_ASSERT(ast);
        auto* localDocIdTmp = ast->variables()->createTemporaryVariable();
        TRI_ASSERT(localDocIdTmp);
        viewNode.setLateMaterialized(*localDocIdTmp);
        // insert a materialize node
        auto* materializeNode = plan->registerNode(
            std::make_unique<aql::materialize::MaterializeSearchNode>(
                plan.get(), plan->nextId(), *localDocIdTmp, var, var));
        TRI_ASSERT(materializeNode);

        auto* materializeDependency = stickToSortNode ? sortNode : limitNode;
        TRI_ASSERT(materializeDependency);
        auto* dependencyParent = materializeDependency->getFirstParent();
        TRI_ASSERT(dependencyParent);
        dependencyParent->replaceDependency(materializeDependency,
                                            materializeNode);
        materializeDependency->addParent(materializeNode);
        modified = true;
      }
    }
  }
}

}  // namespace arangodb::iresearch
