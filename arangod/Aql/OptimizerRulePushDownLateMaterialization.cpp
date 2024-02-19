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
#include "Aql/Query.h"
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

bool usesOutVariable(materialize::MaterializeNode const* matNode,
                     ExecutionNode const* other) {
  return other->getVariableIdsUsedHere().contains(matNode->outVariable().id);
}

bool checkCalculation(ExecutionPlan* plan,
                      materialize::MaterializeNode const* matNode,
                      ExecutionNode* other) {
  LOG_RULE << "ATTEMPT TO MOVE PAST CALCULATION";
  if (!usesOutVariable(matNode, other)) {
    return true;
  }

  TRI_ASSERT(other->getType() == EN::CALCULATION);
  auto* calc = ExecutionNode::castTo<CalculationNode*>(other);

  auto* matOriginNode =
      matNode->plan()->getVarSetBy(matNode->docIdVariable().id);
  if (matOriginNode->getType() != EN::INDEX) {
    return false;
  }

  auto* indexNode = ExecutionNode::castTo<IndexNode*>(matOriginNode);
  TRI_ASSERT(indexNode->isLateMaterialized());
  auto index = indexNode->getSingleIndex();
  if (index == nullptr) {
    return false;
  }

  containers::FlatHashSet<aql::AttributeNamePath> attributes;
  Ast::getReferencedAttributesRecursive(
      calc->expression()->node(), &matNode->outVariable(), "", attributes,
      matNode->plan()->getAst()->query().resourceMonitor());
  LOG_RULE << "CALC ATTRIBUTES: ";
  for (auto const& attr : attributes) {
    LOG_RULE << attr;
  }

  auto proj = indexNode->projections();  // copy intentional
  proj.addPaths(attributes);

  if (index->covers(proj)) {
    LOG_RULE << "INDEX COVERS ATTRIBTUES";
    calc->replaceVariables(
        {{matNode->outVariable().id, indexNode->outVariable()}});
    indexNode->recalculateProjections(plan);
    return true;
  }

  return false;
}

bool canMovePastNode(ExecutionPlan* plan,
                     materialize::MaterializeNode const* matNode,
                     ExecutionNode* other) {
  switch (other->getType()) {
    case EN::LIMIT:
    case EN::MATERIALIZE:
      return true;
    case EN::SORT:
    case EN::FILTER:
      return !usesOutVariable(matNode, other);
    case EN::CALCULATION:
      // TODO This is very pessimistic. In fact we could move the materialize
      // node down, if the index supports
      //      projecting the required attribute.
      return checkCalculation(plan, matNode, other);
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
      if (!canMovePastNode(plan.get(), matNode, insertBefore)) {
        LOG_RULE << "NODE " << matNode->id() << " can not move past "
                 << insertBefore->id() << " " << insertBefore->getTypeString();
        break;
      }
      insertBefore = insertBefore->getFirstParent();
    }

    if (insertBefore != matNode->getFirstParent()) {
      plan->unlinkNode(matNode);
      plan->insertBefore(insertBefore, matNode);
    }

    // create a new output variable for the materialized document.
    // this happens after the join rule. Otherwise, joins are not detected.
    // A separate variable comes in handy when optimizing projections, because
    // it allows to distinguish where the projections belongs to (Index or Mat).
    auto newOutVariable =
        plan->getAst()->variables()->createTemporaryVariable();
    matNode->setDocOutVariable(*newOutVariable);

    // replace every occurrence with this new variable
    for (auto* n = matNode->getFirstParent(); n != nullptr;
         n = n->getFirstParent()) {
      n->replaceVariables({{matNode->oldDocVariable().id, newOutVariable}});
    }
    modified = true;
  }

  opt->addPlan(std::move(plan), rule, modified);
}
