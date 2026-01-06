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

#include "OptimizerRuleSpecializeCollect.h"

#include "Aql/Ast.h"
#include "Aql/ExecutionNode/CollectNode.h"
#include "Aql/ExecutionNode/ExecutionNode.h"
#include "Aql/ExecutionNode/SortNode.h"
#include "Aql/ExecutionPlan.h"
#include "Aql/Expression.h"
#include "Aql/Function.h"
#include "Aql/Optimizer.h"
#include "Aql/TypedAstNodes.h"
#include "Containers/SmallVector.h"

using namespace arangodb;
using namespace arangodb::aql;
using EN = arangodb::aql::ExecutionNode;

/// @brief determine the "right" type of CollectNode and
/// add a sort node for each COLLECT (note: the sort may be removed later)
/// this rule cannot be turned off (otherwise, the query result might be wrong!)
void arangodb::aql::specializeCollectRule(Optimizer* opt,
                                          std::unique_ptr<ExecutionPlan> plan,
                                          OptimizerRule const& rule) {
  containers::SmallVector<ExecutionNode*, 8> nodes;
  plan->findNodesOfType(nodes, EN::COLLECT, true);

  bool modified = false;

  for (auto const& n : nodes) {
    auto collectNode = ExecutionNode::castTo<CollectNode*>(n);

    if (collectNode->isFixedMethod()) {
      // already determined the COLLECT variant of this node.
      // it doesn't need to set again.
      continue;
    }

    auto const& groupVariables = collectNode->groupVariables();

    // test if we can use an alternative version of COLLECT with a hash table
    bool const canUseHashAggregation =
        (!groupVariables.empty() && collectNode->getOptions().canUseMethod(
                                        CollectOptions::CollectMethod::kHash));

    if (canUseHashAggregation) {
      bool preferHashCollect = collectNode->getOptions().shouldUseMethod(
          CollectOptions::CollectMethod::kHash);

      if (ExecutionNode const* loop = collectNode->getLoop();
          loop != nullptr && loop->getLoop() == nullptr &&
          (loop->getType() == EN::ENUMERATE_LIST ||
           loop->getType() == EN::TRAVERSAL ||
           loop->getType() == EN::ENUMERATE_PATHS ||
           loop->getType() == EN::SHORTEST_PATH)) {
        // if the COLLECT is contained inside a single loop, and the loop is an
        // enumeration over an array (in contrast to an enumeration over a
        // collection/view, then prefer the hashed collect variant. this is
        // because the loop output is unlikely to be sorted in any way.
        preferHashCollect = true;
      }

      if (preferHashCollect) {
        // user has explicitly asked for hash method
        // specialize existing the CollectNode so it will become a
        // HashedCollectBlock later. additionally, add a SortNode BEHIND the
        // CollectNode (to sort the final result).
        // this is an in-place modification of the plan.
        // we don't need to create an additional plan for this.
        collectNode->aggregationMethod(CollectOptions::CollectMethod::kHash);

        // add the post-SORT
        SortElementVector sortElements;
        for (auto const& v : collectNode->groupVariables()) {
          sortElements.push_back(SortElement::create(v.outVar, true));
        }

        auto sortNode = plan->createNode<SortNode>(plan.get(), plan->nextId(),
                                                   sortElements, false);

        TRI_ASSERT(collectNode->hasParent());
        auto parent = collectNode->getFirstParent();
        TRI_ASSERT(parent != nullptr);

        sortNode->addDependency(collectNode);
        parent->replaceDependency(collectNode, sortNode);

        modified = true;
        continue;
      }

      // are we allowed to generate additional plans?
      if (!opt->runOnlyRequiredRules()) {
        // create an additional plan with the adjusted COLLECT node
        std::unique_ptr<ExecutionPlan> newPlan(plan->clone());

        // use the cloned COLLECT node
        auto newCollectNode = ExecutionNode::castTo<CollectNode*>(
            newPlan->getNodeById(collectNode->id()));
        TRI_ASSERT(newCollectNode != nullptr);

        // specialize the CollectNode so it will become a HashedCollectBlock
        // later. additionally, add a SortNode BEHIND the CollectNode (to sort
        // the final result).
        newCollectNode->aggregationMethod(CollectOptions::CollectMethod::kHash);

        // add the post-SORT
        SortElementVector sortElements;
        for (auto const& v : newCollectNode->groupVariables()) {
          sortElements.push_back(SortElement::create(v.outVar, true));
        }

        auto sortNode = newPlan->createNode<SortNode>(
            newPlan.get(), newPlan->nextId(), std::move(sortElements), false);

        TRI_ASSERT(newCollectNode->hasParent());
        auto parent = newCollectNode->getFirstParent();
        TRI_ASSERT(parent != nullptr);

        sortNode->addDependency(newCollectNode);
        parent->replaceDependency(newCollectNode, sortNode);

        if (nodes.size() > 1) {
          // this will tell the optimizer to optimize the cloned plan with this
          // specific rule again
          opt->addPlanAndRerun(std::move(newPlan), rule, true);
        } else {
          // no need to run this specific rule again on the cloned plan
          opt->addPlan(std::move(newPlan), rule, true);
        }
      }
    } else if (groupVariables.empty() &&
               collectNode->hasOutVariable() == false &&
               collectNode->aggregateVariables().size() == 1 &&
               collectNode->aggregateVariables()[0].type == "LENGTH") {
      // we have no groups and only a single aggregator of type LENGTH, so we
      // can use the specialized count executor
      collectNode->aggregationMethod(CollectOptions::CollectMethod::kCount);
      modified = true;
      continue;
    }

    // finally, adjust the original plan and create a sorted version of COLLECT.
    collectNode->aggregationMethod(CollectOptions::CollectMethod::kSorted);

    // insert a SortNode IN FRONT OF the CollectNode, if we don't already have
    // one that can be used instead
    if (!groupVariables.empty()) {
      // check if our input is already sorted
      SortNode* sortNode = nullptr;
      auto* d = n->getFirstDependency();
      while (d) {
        switch (d->getType()) {
          case EN::SORT:
            sortNode = ExecutionNode::castTo<SortNode*>(d);
            d = nullptr;  // stop the loop
            break;

          case EN::FILTER:
          case EN::LIMIT:
          case EN::CALCULATION:
          case EN::SUBQUERY:
          case EN::INSERT:
          case EN::REMOVE:
          case EN::REPLACE:
          case EN::UPDATE:
          case EN::UPSERT:
            // these nodes do not affect our sort order,
            // so we can safely ignore them
            d = d->getFirstDependency();  // continue search
            break;

          default:
            d = nullptr;  // stop the loop
            break;
        }
      }

      bool needNewSortNode = true;
      if (sortNode != nullptr) {
        needNewSortNode = false;
        // we found a SORT node, now let's check if it covers all our group
        // variables
        auto& elems = sortNode->elements();
        for (auto const& v : groupVariables) {
          needNewSortNode |= std::find_if(elems.begin(), elems.end(),
                                          [&v](SortElement const& e) {
                                            return e.var == v.inVar;
                                          }) == elems.end();
        }
      }

      if (needNewSortNode) {
        SortElementVector sortElements;
        for (auto const& v : groupVariables) {
          sortElements.push_back(SortElement::create(v.inVar, true));
        }

        sortNode = plan->createNode<SortNode>(plan.get(), plan->nextId(),
                                              std::move(sortElements), true);

        TRI_ASSERT(collectNode->hasDependency());
        auto* dep = collectNode->getFirstDependency();
        TRI_ASSERT(dep != nullptr);
        sortNode->addDependency(dep);
        collectNode->replaceDependency(dep, sortNode);

        modified = true;
      }
    }
  }

  opt->addPlan(std::move(plan), rule, modified);
}