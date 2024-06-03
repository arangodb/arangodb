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
/// @author Lars Maier
////////////////////////////////////////////////////////////////////////////////

#include "Aql/Ast.h"
#include "Aql/Collection.h"
#include "Aql/Condition.h"
#include "Aql/ExecutionEngine.h"
#include "Aql/ExecutionNode/CollectNode.h"
#include "Aql/ExecutionNode/ExecutionNode.h"
#include "Aql/ExecutionNode/IndexNode.h"
#include "Aql/ExecutionNode/MaterializeRocksDBNode.h"
#include "Aql/ExecutionNode/JoinNode.h"
#include "Aql/ExecutionPlan.h"
#include "Aql/Optimizer.h"
#include "Aql/OptimizerRules.h"
#include "Logger/LogMacros.h"

using namespace arangodb;
using namespace arangodb::aql;
using namespace arangodb::containers;
using EN = arangodb::aql::ExecutionNode;

#define LOG_RULE LOG_DEVEL_IF(false)

namespace {

bool canUseIndex(std::shared_ptr<Index> const& indexHandle) {
  if (auto type = indexHandle->type();
      type == Index::TRI_IDX_TYPE_INVERTED_INDEX) {
    LOG_RULE << "INDEX " << indexHandle->id() << " FAILED: "
             << "index type explicitly excluded.";
    return false;
  }

  if (indexHandle->coveredFields().empty()) {
    LOG_RULE << "INDEX " << indexHandle->id() << " FAILED: "
             << "does not support covering call";
    return false;
  }

  return true;
}

}  // namespace

void arangodb::aql::batchMaterializeDocumentsRule(
    Optimizer* opt, std::unique_ptr<ExecutionPlan> plan,
    OptimizerRule const& rule) {
  bool modified = false;
  containers::SmallVector<ExecutionNode*, 8> indexes;
  plan->findNodesOfType(indexes, EN::INDEX, /* enterSubqueries */ true);

  for (auto node : indexes) {
    TRI_ASSERT(node->getType() == EN::INDEX);
    auto indexNode = ExecutionNode::castTo<IndexNode*>(node);

    if (indexNode->isLateMaterialized()) {
      LOG_RULE << "INDEX " << indexNode->id() << " FAILED: "
               << "already late materialized";
      continue;
    }

    const auto index = indexNode->getSingleIndex();
    if (index == nullptr || !canUseIndex(index)) {
      LOG_RULE << "INDEX " << indexNode->id() << " FAILED: "
               << "not a single index in use or index not usable";
      continue;
    }

    if (indexNode->projections().usesCoveringIndex()) {
      LOG_RULE << "INDEX " << indexNode->id() << " FAILED: "
               << "uses covering projections";
      continue;
    }

    if (indexNode->hasFilter() &&
        !indexNode->filterProjections().usesCoveringIndex()) {
      LOG_RULE << "INDEX " << indexNode->id() << " FAILED: "
               << "has post filter, which is not covered";
      continue;
    }

    if (!indexNode->canApplyLateDocumentMaterializationRule()) {
      LOG_RULE << "INDEX " << indexNode->id() << " FAILED: "
               << "no late materilize support";
      continue;
    }
    if (indexNode->canReadOwnWrites() == ReadOwnWrites::yes) {
      LOG_RULE << "INDEX " << indexNode->id() << " FAILED: "
               << "index has to read its own write - not supported";
      continue;
    }

    if (indexNode->estimateCost().estimatedNrItems < 100) {
      TRI_IF_FAILURE("batch-materialize-no-estimation") {
        // do nothing here
      }
      else {
        LOG_RULE << "INDEX " << indexNode->id() << " FAILED: "
                 << "estimated number of items too small ("
                 << indexNode->estimateCost().estimatedNrItems << ")";
        continue;
      }
    }

    LOG_RULE << "FOUND INDEX NODE " << indexNode->id();

    auto docIdVar = plan->getAst()->variables()->createTemporaryVariable();
    auto oldOutVariable = indexNode->outVariable();
    // a later optimizer rule will change the actual document output variable
    auto newOutVariable = oldOutVariable;

    auto materialized = plan->createNode<materialize::MaterializeRocksDBNode>(
        plan.get(), plan->nextId(), indexNode->collection(), *docIdVar,
        *newOutVariable, *oldOutVariable);

    plan->insertAfter(indexNode, materialized);

    materialized->setMaxProjections(indexNode->maxProjections());
    if (!indexNode->projections().empty()) {
      TRI_ASSERT(!indexNode->projections().usesCoveringIndex());
      TRI_ASSERT(!indexNode->projections().hasOutputRegisters());
      // move projections from index node into materialize node
      materialized->projections(std::move(indexNode->projections()));
    }
    indexNode->setLateMaterialized(docIdVar, indexNode->getIndexes()[0]->id(),
                                   {});
    modified = true;
  }

  opt->addPlan(std::move(plan), rule, modified);
}
