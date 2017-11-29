////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2017 ArangoDB GmbH, Cologne, Germany
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
/// @author Vasiliy Nabatchikov
////////////////////////////////////////////////////////////////////////////////

#include "IResearchViewOptimizerRules.h"
#include "IResearchViewConditionFinder.h"
#include "Aql/ExecutionPlan.h"
#include "Aql/SortNode.h"
#include "Aql/Optimizer.h"

namespace arangodb {
namespace iresearch {

using namespace arangodb::aql;

/// @brief move filters and sort conditions into views
void handleViewsRule(
    Optimizer* opt,
    std::unique_ptr<ExecutionPlan> plan,
    OptimizerRule const* rule) {
  SmallVector<ExecutionNode*>::allocator_type::arena_type a;
  SmallVector<ExecutionNode*> nodes{a};
  std::unordered_map<size_t, ExecutionNode*> changes;

  auto cleanupChanges = [&changes](){
    for (auto& v : changes) {
      delete v.second;
    }
  };
  TRI_DEFER(cleanupChanges());

  // newly created view nodes (replacement)
  std::unordered_set<ExecutionNode const*> createdViewNodes;

  // try to find `EnumerateViewNode`s and push corresponding filters and sorts inside
  plan->findEndNodes(nodes, true);

  bool hasEmptyResult = false;
  for (auto const& n : nodes) {
    IResearchViewConditionFinder finder(plan.get(), &changes, &hasEmptyResult);
    n->walk(&finder);
  }

  createdViewNodes.reserve(changes.size());

  for (auto& it : changes) {
    auto*& node = it.second;

    if (!node || ExecutionNode::ENUMERATE_IRESEARCH_VIEW != node->getType()) {
      // filter out invalid nodes
      continue;
    }

    plan->registerNode(node);
    plan->replaceNode(plan->getNodeById(it.first), node);

    createdViewNodes.insert(node);

    // prevent double deletion by cleanupChanges()
    node = nullptr;
  }

  if (!changes.empty()) {
    std::unordered_set<ExecutionNode*> toUnlink;

    // remove filters covered by a view
    nodes.clear(); // ensure array is empty
    plan->findNodesOfType(nodes, ExecutionNode::FILTER, true);

    // `createdViewNodes` will not change
    auto const noMatch = createdViewNodes.end();

    for (auto* node : nodes) {
      // find the node with the filter expression
      auto inVar = static_cast<FilterNode const*>(node)->getVariablesUsedHere();
      TRI_ASSERT(inVar.size() == 1);

      auto setter = plan->getVarSetBy(inVar[0]->id);

      if (!setter || setter->getType() != ExecutionNode::CALCULATION) {
        continue;
      }

      auto const it = createdViewNodes.find(setter->getLoop());

      if (it != noMatch) {
        toUnlink.emplace(node);
        toUnlink.emplace(setter);
        static_cast<CalculationNode*>(setter)->canRemoveIfThrows(true);
      }
    }

    // remove sorts covered by a view
    nodes.clear(); // ensure array is empty
    plan->findNodesOfType(nodes, ExecutionNode::SORT, true);

    for (auto* node : nodes) {
      // find the node with the sort expression
      auto inVar = static_cast<aql::SortNode const*>(node)->getVariablesUsedHere();
      TRI_ASSERT(!inVar.empty());

      for (auto& var : inVar) {
        auto setter = plan->getVarSetBy(var->id);

        if (!setter || setter->getType() != ExecutionNode::CALCULATION) {
          continue;
        }

        auto const it = createdViewNodes.find(setter->getLoop());

        if (it != noMatch) {
          if (!(*it)->isInInnerLoop()) {
            toUnlink.emplace(node);
            toUnlink.emplace(setter);
          }
//FIXME uncomment when EnumerateViewNode can create variables
//        toUnlink.emplace(setter);
//        if (!(*it)->isInInnerLoop()) {
//          toUnlink.emplace(node);
//        }
          static_cast<CalculationNode*>(setter)->canRemoveIfThrows(true);
        }
      }
    }

    plan->unlinkNodes(toUnlink);
  }

  opt->addPlan(std::move(plan), rule, !changes.empty());
}

} // iresearch
} // arangodb
