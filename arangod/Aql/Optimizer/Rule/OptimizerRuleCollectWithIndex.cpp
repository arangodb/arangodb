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
#include "Aql/ExecutionNode/IndexCollectNode.h"
#include "Aql/ExecutionNode/IndexNode.h"
#include "Aql/ExecutionPlan.h"
#include "Aql/Expression.h"
#include "Aql/IndexDistrinctScan.h"
#include "Aql/Optimizer.h"
#include "Aql/OptimizerRules.h"
#include "Indexes/Index.h"
#include "Logger/LogMacros.h"

using namespace arangodb;
using namespace arangodb::aql;
using namespace arangodb::containers;
using EN = arangodb::aql::ExecutionNode;

#define LOG_RULE LOG_DEVEL_IF(false)

namespace {

bool isCollectNodeEligible(CollectNode const& cn) {
  if (cn.hasOutVariable()) {
    LOG_RULE << "Collect " << cn.id()
             << " not eligible - it has an out variable";
    return false;
  }

  if (not cn.aggregateVariables().empty()) {
    LOG_RULE << "Collect " << cn.id() << " not eligible - it has aggregations";
    return false;
  }

  return true;
}

bool isIndexNodeEligible(IndexNode const& in) {
  if (in.hasFilter()) {
    LOG_RULE << "IndexNode " << in.id()
             << " not eligible - it has a post filter";
    return false;
  }
  if (in.isLateMaterialized()) {
    LOG_RULE << "IndexNode " << in.id()
             << " not eligible - it is in late materialize mode";
    return false;
  }
  if (in.getIndexes().size() != 1) {
    LOG_RULE << "IndexNode " << in.id()
             << " not eligible - it uses multiple indexes";
    return false;
  }
  auto index = in.getSingleIndex();
  if (index->type() != arangodb::Index::TRI_IDX_TYPE_PERSISTENT_INDEX) {
    LOG_RULE << "IndexNode " << in.id()
             << " not eligible - only persistent index supported";
    return false;
  }
  if (not in.projections().usesCoveringIndex()) {
    LOG_RULE << "IndexNode " << in.id()
             << " not eligible - index does not cover everything";
    return false;
  }
  if (not in.condition()->isEmpty()) {
    LOG_RULE << "IndexNode " << in.id()
             << " not eligible - index with condition is not yet supported";
    return false;
  }
  if (index->sparse()) {
    LOG_RULE << "IndexNode " << in.id()
             << " not eligible - sparse indexes not yet supported";
    return false;
  }

  // assume there are n documents in the collection and we have
  // k distinct features. A linear search is in O(n) while a distinct scan
  // requires O(k log n). So checking for k log n < n, it follows
  // k / n < 1 / log n, where k / n is precisely the selectivity estimate.

  double selectivity = index->selectivityEstimate();
  auto numberOfItems = in.estimateCost().estimatedNrItems;

  if (selectivity > 1. / log(numberOfItems)) {
    LOG_RULE << "IndexNode " << in.id()
             << " not eligible - selectivity is too high";
    return false;
  }

  return true;
}

bool isEligiblePair(ExecutionPlan const& plan, CollectNode const& cn,
                    IndexNode const& idx, IndexDistinctScanOptions& scanOptions,
                    std::vector<CalculationNode*>& calculationsToRemove,
                    IndexCollectGroups& groups) {
  auto const& coveredFields = idx.getSingleIndex()->coveredFields();

  // every group variable has to be a field of the index
  for (auto const& grp : cn.groupVariables()) {
    // get the producing node for the in variable, it should be a calculation
    auto producer = plan.getVarSetBy(grp.inVar->id);
    if (producer->getType() != EN::CALCULATION) {
      return false;
    }

    // check if it is just an attribute access for the out variable of the
    // index node.
    auto calculation = EN::castTo<CalculationNode*>(producer);
    std::pair<Variable const*, std::vector<arangodb::basics::AttributeName>>
        access;
    bool const isAttributeAccess =
        calculation->expression()->node()->isAttributeAccessForVariable(access);
    if (not isAttributeAccess || access.first->id != idx.outVariable()->id) {
      LOG_RULE
          << "Calculation " << calculation->id()
          << " not eligible - expression is not an attribute access on index ("
          << idx.id() << ") out variable";
      return false;
    }

    // check if this attribute is a covered field
    auto iter =
        std::find(coveredFields.begin(), coveredFields.end(), access.second);
    if (iter == coveredFields.end()) {
      LOG_RULE << "Calculation " << calculation->id()
               << " not eligible - accessed attribute " << access.second
               << " which is not covered by the index";
      return false;
    }

    std::size_t fieldIndex = std::distance(coveredFields.begin(), iter);
    scanOptions.distinctFields.push_back(fieldIndex);
    calculationsToRemove.push_back(calculation);
    groups.emplace_back(
        IndexCollectGroup{.indexField = fieldIndex, .outVariable = grp.outVar});
  }

  scanOptions.sorted = cn.getOptions().method !=
                       arangodb::aql::CollectOptions::CollectMethod::kHash;

  if (not idx.getSingleIndex()->supportsDistinctScan(scanOptions)) {
    LOG_RULE << "Index " << idx.id()
             << " not eligible - index does not support required distinct scan "
             << scanOptions;
    return false;
  }

  return true;
}

}  // namespace

void arangodb::aql::useIndexForCollect(Optimizer* opt,
                                       std::unique_ptr<ExecutionPlan> plan,
                                       OptimizerRule const& rule) {
  bool modified = false;

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

    auto singleton = indexNode->getFirstDependency();
    if (singleton->getType() != EN ::SINGLETON) {
      LOG_RULE << "Index node is not the first node after a singleton.";
      continue;
    }

    LOG_RULE << "Found collect " << collectNode->id() << " with index "
             << indexNode->id();

    IndexCollectGroups groups;
    IndexDistinctScanOptions scanOptions;
    std::vector<CalculationNode*> calculationsToRemove;

    if (not isEligiblePair(*plan, *collectNode, *indexNode, scanOptions,
                           calculationsToRemove, groups)) {
      continue;
    }

    auto indexCollectNode = plan->createNode<IndexCollectNode>(
        plan.get(), plan->nextId(), indexNode->collection(),
        indexNode->getSingleIndex(), indexNode->outVariable(),
        std::move(groups));

    plan->replaceNode(collectNode, indexCollectNode);
    plan->unlinkNode(indexNode);
    for (auto n : calculationsToRemove) {
      plan->unlinkNode(n);
    }
    modified = true;
  }

  opt->addPlan(std::move(plan), rule, modified);
}

std::ostream& arangodb::operator<<(std::ostream& os,
                                   IndexDistinctScanOptions const& opts) {
  return os << opts.distinctFields << (opts.sorted ? "sorted" : "unsorted");
}
