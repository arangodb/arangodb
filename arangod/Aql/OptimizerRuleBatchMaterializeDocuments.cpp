////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2014-2023 ArangoDB GmbH, Cologne, Germany
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

#include "Aql/AstHelper.h"
#include "Aql/AttributeNamePath.h"
#include "Aql/Collection.h"
#include "Aql/Condition.h"
#include "Aql/ConditionFinder.h"
#include "Aql/DocumentProducingNode.h"
#include "Aql/ExecutionEngine.h"
#include "Aql/ExecutionNode.h"
#include "Aql/ExecutionPlan.h"
#include "Aql/Expression.h"
#include "Aql/JoinNode.h"
#include "Aql/Optimizer.h"
#include "Aql/OptimizerUtils.h"
#include "Aql/Query.h"
#include "Aql/Variable.h"
#include "Aql/types.h"
#include "Containers/NodeHashMap.h"
#include "Containers/HashSet.h"
#include "Containers/SmallVector.h"
#include "Containers/ImmutableMap.h"
#include "Logger/LogMacros.h"
#include "OptimizerRules.h"

#include <boost/container_hash/hash.hpp>

#include "Aql/IndexNode.h"
#include "Aql/ExecutionNode.h"

using namespace arangodb;
using namespace arangodb::aql;
using namespace arangodb::containers;
using EN = arangodb::aql::ExecutionNode;

#define LOG_RULE LOG_DEVEL_IF(true)

namespace {}  // namespace

void arangodb::aql::batchMaterializeDocumentsRule(
    Optimizer* opt, std::unique_ptr<ExecutionPlan> plan,
    OptimizerRule const& rule) {
  bool modified = false;
  containers::SmallVector<ExecutionNode*, 8> indexes;
  plan->findNodesOfType(indexes, EN::INDEX, /* enterSubqueries */ true);

  for (auto node : indexes) {
    TRI_ASSERT(node->getType() == EN::INDEX);
    auto index = ExecutionNode::castTo<IndexNode*>(node);

    if (index->isLateMaterialized()) {
      LOG_RULE << "INDEX " << index->id() << " FAILED: "
               << "late materialized";
      continue;
    }
    if (index->getIndexes().size() != 1) {
      LOG_RULE << "INDEX " << index->id() << " FAILED: "
               << "not covered by one index";
      continue;
    }
    if (!index->projections().empty()) {
      LOG_RULE << "INDEX " << index->id() << " FAILED: "
               << "has projections";
      continue;
    }
    if (index->hasFilter()) {
      LOG_RULE << "INDEX " << index->id() << " FAILED: "
               << "has post filter";
      continue;
    }

    LOG_RULE << "FOUND INDEX NODE " << index->id();

    auto docIdVar = plan->getAst()->variables()->createTemporaryVariable();
    index->setLateMaterialized(docIdVar, index->getIndexes()[0]->id(), {});
    auto materialized = plan->createNode<materialize::MaterializeRocksDBNode>(
        plan.get(), plan->nextId(), index->collection(), *docIdVar,
        *index->outVariable());
    plan->insertAfter(index, materialized);
    modified = true;
  }

  opt->addPlan(std::move(plan), rule, modified);
}
