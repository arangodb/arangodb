////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2014-2025 ArangoDB GmbH, Cologne, Germany
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
////////////////////////////////////////////////////////////////////////////////
#include "Aql/Ast.h"
#include "Aql/Collection.h"
#include "Aql/Condition.h"
#include "Aql/ExecutionEngine.h"
#include "Aql/ExecutionNode/CalculationNode.h"
#include "Aql/ExecutionNode/CollectNode.h"
#include "Aql/ExecutionNode/ExecutionNode.h"
#include "Aql/Expression.h"
#include "Aql/ExecutionNode/IndexNode.h"
#include "Aql/ExecutionNode/IndexCollectNode.h"
#include "Aql/ExecutionPlan.h"
#include "Aql/Optimizer.h"
#include "Aql/OptimizerRules.h"
#include "Logger/LogMacros.h"

using namespace arangodb;
using namespace arangodb::aql;
using namespace arangodb::containers;
using EN = arangodb::aql::ExecutionNode;

#define LOG_RULE LOG_DEVEL_IF(true)

namespace {

bool isCollectNodeEligible(CollectNode const& cn) {
  if (cn.hasOutVariable()) {
    LOG_DEVEL << "Collect " << cn.id()
              << " not eligible - it has an out variable";
    return false;
  }

  if (not cn.aggregateVariables().empty()) {
    LOG_DEVEL << "Collect " << cn.id() << " not eligible - it has aggregations";
    return false;
  }

  return true;
}

bool isIndexNodeEligible(IndexNode const& idx) {
  if (idx.hasFilter()) {
    LOG_DEVEL << "IndexNode " << idx.id()
              << " not eligible - it has a post filter";
    return false;
  }
  if (idx.isLateMaterialized()) {
    LOG_DEVEL << "IndexNode " << idx.id()
              << " not eligible - it is in late materialize mode";
    return false;
  }
  if (not idx.isAllCoveredByOneIndex()) {
    LOG_DEVEL << "IndexNode " << idx.id()
              << " not eligible - it uses multiple indexes";
    return false;
  }
  if (not idx.projections().usesCoveringIndex()) {
    LOG_DEVEL << "IndexNode " << idx.id()
              << " not eligible - index does not cover everything";
    return false;
  }

  return true;
}

bool isEligiblePair(CollectNode const& cn, IndexNode const& idx) {
  return true;
}

}  // namespace

void arangodb::aql::useIndexForCollect(Optimizer* opt,
                                       std::unique_ptr<ExecutionPlan> plan,
                                       OptimizerRule const& rule) {
  bool modified = true;

  containers::SmallVector<ExecutionNode*, 8> collectNodes;
  plan->findNodesOfType(collectNodes, EN::COLLECT, /* enterSubqueries */ true);

  for (auto node : collectNodes) {
    TRI_ASSERT(node->getType() == EN::COLLECT);
    auto collectNode = ExecutionNode::castTo<CollectNode*>(node);
    LOG_RULE << "Found candidate collect node " << collectNode->id();

    if (not isCollectNodeEligible(*collectNode)) {
      continue;
    }

    // check is there is Index node as dependency
    auto dependency = collectNode->getFirstDependency();
    while (dependency != nullptr) {
      LOG_RULE << "DEPENDENCY " << dependency->id() << " "
               << dependency->getTypeString();
      if (dependency->getType() == EN::INDEX) {
        break;
      } else if (dependency->getType() == EN::CALCULATION) {
        auto calculation = EN::castTo<CalculationNode*>(dependency);
        LOG_RULE << "CALCULATION NODE " << calculation->id();
        dependency = dependency->getFirstDependency();
      } else {
        // maybe skip some nodes?
        dependency = nullptr;
      }
    }

    if (dependency == nullptr) {
      LOG_RULE << "No suitable dependency found";
      continue;
    }

    TRI_ASSERT(dependency->getType() == EN::INDEX);
    auto indexNode = ExecutionNode::castTo<IndexNode*>(dependency);
    LOG_RULE << "Found candidate index node " << indexNode->id();

    if (not isIndexNodeEligible(*indexNode)) {
      continue;
    }

    LOG_RULE << "Found collect " << collectNode->id() << " with index "
             << indexNode->id();

    if (not isEligiblePair(*collectNode, *indexNode)) {
      continue;
    }

    auto indexCollectNode = plan->createNode<IndexCollectNode>(
        plan.get(), plan->nextId(), indexNode->collection(),
        indexNode->getSingleIndex());

    plan->replaceNode(collectNode, indexCollectNode);
    plan->unlinkNode(indexNode);
    modified = true;
  }

  opt->addPlan(std::move(plan), rule, modified);
}
