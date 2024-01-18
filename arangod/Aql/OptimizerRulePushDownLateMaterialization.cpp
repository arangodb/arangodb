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

#include "Aql/Collection.h"
#include "Aql/Condition.h"
#include "Aql/ExecutionEngine.h"
#include "Aql/ExecutionNode.h"
#include "Aql/ExecutionPlan.h"
#include "Aql/Expression.h"
#include "Aql/IndexNode.h"
#include "Aql/JoinNode.h"
#include "Aql/Optimizer.h"
#include "Aql/OptimizerRules.h"
#include "Logger/LogMacros.h"

using namespace arangodb;
using namespace arangodb::aql;
using namespace arangodb::containers;
using EN = arangodb::aql::ExecutionNode;

#define LOG_RULE LOG_DEVEL_IF(true)

namespace {

bool usesOutVariable(materialize::MaterializeNode const* matNode,
                     ExecutionNode const* other) {
  return other->getVariableIdsUsedHere().contains(matNode->outVariable().id);
}

bool canMovePastNode(materialize::MaterializeNode const* matNode,
                     ExecutionNode const* other) {
  switch (other->getType()) {
    case EN::LIMIT:
    case EN::MATERIALIZE:
      return true;
    case EN::SORT:
    case EN::FILTER:
    case EN::CALCULATION:
      // TODO This is very pessimistic. In fact we could move the materialize
      // node down, if the index supports
      //      projecting the required attribute.
      return !usesOutVariable(matNode, other);
    default:
      break;
  }
  return false;
}

}  // namespace

void arangodb::aql::pushDownLateMaterializationRule(
    Optimizer* opt, std::unique_ptr<ExecutionPlan> plan,
    OptimizerRule const& rule) {
  bool modified = false;

  containers::SmallVector<ExecutionNode*, 8> indexes;
  plan->findNodesOfType(indexes, EN::MATERIALIZE, /* enterSubqueries */ true);

  for (auto node : indexes) {
    TRI_ASSERT(node->getType() == EN::MATERIALIZE);
    auto matNode = ExecutionNode::castTo<materialize::MaterializeNode*>(node);

    ExecutionNode* insertBefore = matNode->getFirstParent();

    while (insertBefore != nullptr) {
      if (!canMovePastNode(matNode, insertBefore)) {
        LOG_RULE << "NODE " << matNode->id() << " can not move past "
                 << insertBefore->id() << " " << insertBefore->getTypeString();
        break;
      }
      insertBefore = insertBefore->getFirstParent();
    }

    if (insertBefore != matNode->getFirstParent()) {
      plan->unlinkNode(matNode);
      plan->insertBefore(insertBefore, matNode);
      modified = true;
    }
  }

  opt->addPlan(std::move(plan), rule, modified);
}
