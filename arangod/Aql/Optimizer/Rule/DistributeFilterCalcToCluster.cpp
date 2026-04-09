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

#include "DistributeFilterCalcToCluster.h"

#include "Aql/Ast.h"
#include "Aql/ExecutionNode/CalculationNode.h"
#include "Aql/ExecutionNode/ExecutionNode.h"
#include "Aql/ExecutionPlan.h"
#include "Aql/Expression.h"
#include "Aql/Optimizer.h"
#include "Aql/Query.h"
#include "Aql/types.h"
#include "Containers/SmallVector.h"
#include "VocBase/vocbase.h"

namespace arangodb::aql {
using EN = ExecutionNode;

/// @brief move filters up into the cluster distribution part of the plan
/// this rule modifies the plan in place
/// filters are moved as far up in the plan as possible to make result sets
/// as small as possible as early as possible
void distributeFilterCalcToClusterRule(Optimizer* opt,
                                       std::unique_ptr<ExecutionPlan> plan,
                                       OptimizerRule const& rule) {
  bool modified = false;

  containers::SmallVector<ExecutionNode*, 8> nodes;
  plan->findNodesOfType(nodes, EN::GATHER, true);

  VarSet varsSetHere;

  for (auto& n : nodes) {
    auto const& remoteNodeList = n->getDependencies();
    TRI_ASSERT(remoteNodeList.size() > 0);
    auto rn = remoteNodeList[0];

    if (!n->hasParent()) {
      continue;
    }

    bool allowOnlyFilterAndCalculation = false;

    varsSetHere.clear();
    auto parents = n->getParents();
    TRI_ASSERT(!parents.empty());

    while (true) {
      TRI_ASSERT(!parents.empty());
      bool stopSearching = false;
      auto inspectNode = parents[0];
      TRI_ASSERT(inspectNode != nullptr);

      auto type = inspectNode->getType();
      if (allowOnlyFilterAndCalculation && type != EN::FILTER &&
          type != EN::CALCULATION) {
        stopSearching = true;
        break;
      }

      switch (type) {
        case EN::ENUMERATE_LIST:
        case EN::SINGLETON:
        case EN::INSERT:
        case EN::REMOVE:
        case EN::REPLACE:
        case EN::UPDATE:
        case EN::UPSERT:
        case EN::SORT: {
          for (auto& v : inspectNode->getVariablesSetHere()) {
            varsSetHere.emplace(v);
          }
          parents = inspectNode->getParents();
          if (type == EN::SORT) {
            allowOnlyFilterAndCalculation = true;
          }
          continue;
        }

        case EN::COLLECT:
        case EN::RETURN:
        case EN::NORESULTS:
        case EN::SCATTER:
        case EN::DISTRIBUTE:
        case EN::GATHER:
        case EN::REMOTE:
        case EN::LIMIT:
        case EN::INDEX:
        case EN::ENUMERATE_COLLECTION:
        case EN::ENUMERATE_NEAR_VECTORS:
        case EN::TRAVERSAL:
        case EN::ENUMERATE_PATHS:
        case EN::SHORTEST_PATH:
        case EN::SUBQUERY:
        case EN::ENUMERATE_IRESEARCH_VIEW:
        case EN::WINDOW:
        case EN::MATERIALIZE:
          stopSearching = true;
          break;

        case EN::OFFSET_INFO_MATERIALIZE:
        case EN::CALCULATION:
        case EN::FILTER: {
          if (inspectNode->getType() == EN::CALCULATION) {
            TRI_vocbase_t& vocbase = plan->getAst()->query().vocbase();
            if (!ExecutionNode::castTo<CalculationNode const*>(inspectNode)
                     ->expression()
                     ->canRunOnDBServer(vocbase.isOneShard())) {
              stopSearching = true;
              break;
            }
          }

          TRI_ASSERT(inspectNode->getType() == EN::SUBQUERY ||
                     inspectNode->getType() == EN::CALCULATION ||
                     inspectNode->getType() == EN::FILTER);

          VarSet used;
          inspectNode->getVariablesUsedHere(used);
          for (auto& v : used) {
            if (varsSetHere.find(v) != varsSetHere.end()) {
              stopSearching = true;
              break;
            }
          }

          if (!stopSearching) {
            parents = inspectNode->getParents();
            plan->unlinkNode(inspectNode);
            plan->insertDependency(rn, inspectNode);

            modified = true;
          }
          break;
        }

        default: {
          TRI_ASSERT(false)
              << "Unhandled node type " << inspectNode->getTypeString();
        }
      }

      if (stopSearching) {
        break;
      }
    }
  }

  opt->addPlan(std::move(plan), rule, modified);
}

}  // namespace arangodb::aql
