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

#include "Aql/Ast.h"
#include "Aql/Collection.h"
#include "Aql/Condition.h"
#include "Aql/ExecutionEngine.h"
#include "Aql/ExecutionNode/CalculationNode.h"
#include "Aql/ExecutionNode/ExecutionNode.h"
#include "Aql/ExecutionNode/IndexNode.h"
#include "Aql/ExecutionNode/JoinNode.h"
#include "Aql/ExecutionNode/MaterializeRocksDBNode.h"
#include "Aql/ExecutionNode/SubqueryNode.h"
#include "Aql/ExecutionPlan.h"
#include "Aql/Expression.h"
#include "Aql/Optimizer.h"
#include "Aql/OptimizerRules.h"
#include "Aql/OptimizerUtils.h"
#include "Aql/Query.h"
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

void replaceVariableDownwards(
    ExecutionNode* first,
    std::unordered_map<VariableId, Variable const*> const& replacement) {
  for (auto* n = first; n != nullptr; n = n->getFirstParent()) {
    if (n->getType() == ExecutionNode::SUBQUERY) {
      auto* subquery = ExecutionNode::castTo<SubqueryNode*>(n);
      replaceVariableDownwards(subquery->getSubquery()->getSingleton(),
                               replacement);
    } else {
      n->replaceVariables(replacement);
    }
  }
}

bool extractRelevantAttributes(
    ExecutionNode* node, aql::Variable const& var,
    containers::FlatHashSet<aql::AttributeNamePath>& attributes) {
  switch (node->getType()) {
    case arangodb::aql::ExecutionNode::CALCULATION: {
      auto* calc = ExecutionNode::castTo<CalculationNode*>(node);
      return Ast::getReferencedAttributesRecursive(
          calc->expression()->node(), &var, "", attributes,
          node->plan()->getAst()->query().resourceMonitor());
    }

    case arangodb::aql::ExecutionNode::SUBQUERY: {
      auto* sub = ExecutionNode::castTo<SubqueryNode*>(node);
      return arangodb::aql::utils::findProjections(
          sub->getSubquery()->getSingleton(), &var, "", false, attributes);
    }

    default:
      return false;
  }
}

bool checkPossibleProjection(ExecutionPlan* plan,
                             materialize::MaterializeNode const* matNode,
                             ExecutionNode* other) {
  LOG_RULE << "ATTEMPT TO MOVE PAST CALCULATION";
  if (!usesOutVariable(matNode, other)) {
    return true;
  }

  containers::FlatHashSet<aql::AttributeNamePath> attributes;
  bool notUsingFullDocument =
      extractRelevantAttributes(other, matNode->outVariable(), attributes);
  if (!notUsingFullDocument) {
    LOG_RULE << "CALCULATION USES FULL DOCUMENT";
    return false;
  }
  LOG_RULE << "CALC ATTRIBUTES: ";
  for (auto const& attr : attributes) {
    LOG_RULE << attr;
  }

  auto* matOriginNode =
      matNode->plan()->getVarSetBy(matNode->docIdVariable().id);
  if (matOriginNode->getType() == EN::INDEX) {
    LOG_RULE << "ATTEMPT TO ADD PROJECTIONS TO INDEX NODE";
    auto* indexNode = ExecutionNode::castTo<IndexNode*>(matOriginNode);
    TRI_ASSERT(indexNode->isLateMaterialized());
    auto index = indexNode->getSingleIndex();
    if (index == nullptr) {
      return false;
    }

    auto proj = indexNode->projections();  // copy intentional
    TRI_ASSERT(!proj.hasOutputRegisters())
        << "projection output registers shouldn't be set at this point";
    proj.addPaths(attributes);

    if (proj.size() <= indexNode->maxProjections()) {
      if (index->covers(proj)) {
        LOG_RULE << "INDEX COVERS ATTRIBTUES";
        indexNode->projections() = std::move(proj);
        indexNode->projections().setCoveringContext(
            indexNode->collection()->id(), index);
        TRI_ASSERT(indexNode->projections().usesCoveringIndex());
        return true;
      } else {
        LOG_RULE << "INDEX DOES NOT COVER " << proj;
      }
    } else {
      LOG_RULE << "maxProjections for index reached (" << proj.size()
               << " <= " << indexNode->maxProjections() << ")";
    }
  } else if (matOriginNode->getType() == EN::JOIN) {
    LOG_RULE << "ATTEMPT TO ADD PROJECTIONS TO JOIN NODE";
    auto* joinNode = ExecutionNode::castTo<JoinNode*>(matOriginNode);
    auto& indexes = joinNode->getIndexInfos();

    // find the index with the out variable of the materializer
    auto idx =
        std::find_if(indexes.begin(), indexes.end(), [&](auto const& idx) {
          return idx.outVariable->id == matNode->outVariable().id;
        });

    // this node is the out variable, there has to be an index that produces it
    TRI_ASSERT(idx != indexes.end());
    TRI_ASSERT(idx->isLateMaterialized);

    auto proj = idx->projections;  // copy intentional
    TRI_ASSERT(!proj.hasOutputRegisters())
        << "projection output registers shouldn't be set at this point";
    proj.addPaths(attributes);

    if (idx->index->covers(proj)) {
      LOG_RULE << "INDEX COVERS ATTRIBTUES";
      idx->projections = std::move(proj);
      idx->projections.setCoveringContext(idx->collection->id(), idx->index);
      TRI_ASSERT(idx->projections.usesCoveringIndex());
      return true;
    } else {
      LOG_RULE << "INDEX DOES NOT COVER " << proj;
    }
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
    case EN::SUBQUERY:
      return checkPossibleProjection(plan, matNode, other);
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

  // this rule depends crucially on the optimize-projections rule
  if (!plan->isDisabledRule(
          static_cast<int>(OptimizerRule::optimizeProjectionsRule))) {
    containers::SmallVector<ExecutionNode*, 8> indexes;
    plan->findNodesOfType(indexes, EN::MATERIALIZE, /* enterSubqueries */ true);

    for (auto node : indexes) {
      TRI_ASSERT(node->getType() == EN::MATERIALIZE);
      auto matNode = dynamic_cast<materialize::MaterializeRocksDBNode*>(node);
      if (matNode == nullptr) {
        continue;  // search materialize requires more work
      }

      ExecutionNode* insertBefore = matNode->getFirstParent();

      while (insertBefore != nullptr) {
        LOG_RULE << "ATTEMPT MOVING PAST " << insertBefore->getTypeString()
                 << "[" << insertBefore->id() << "]";
        if (!canMovePastNode(plan.get(), matNode, insertBefore)) {
          LOG_RULE << "NODE " << matNode->id() << " can not move past "
                   << insertBefore->id() << " "
                   << insertBefore->getTypeString();
          break;
        }
        insertBefore = insertBefore->getFirstParent();
      }

      if (insertBefore != matNode->getFirstParent()) {
        plan->unlinkNode(matNode);
        plan->insertBefore(insertBefore, matNode);
      }

      modified = true;
    }
  }

  opt->addPlan(std::move(plan), rule, modified);
}

void arangodb::aql::materializeIntoSeparateVariable(
    Optimizer* opt, std::unique_ptr<ExecutionPlan> plan,
    OptimizerRule const& rule) {
  bool modified = false;

  // this rule depends crucially on the optimize-projections rule
  if (!plan->isDisabledRule(
          static_cast<int>(OptimizerRule::optimizeProjectionsRule))) {
    containers::SmallVector<ExecutionNode*, 8> indexes;
    plan->findNodesOfType(indexes, EN::MATERIALIZE, /* enterSubqueries */ true);

    for (auto node : indexes) {
      TRI_ASSERT(node->getType() == EN::MATERIALIZE);
      auto matNode = dynamic_cast<materialize::MaterializeRocksDBNode*>(node);
      if (matNode == nullptr) {
        continue;  // search materialize requires more work
      }

      // create a new output variable for the materialized document.
      // this happens after the join rule. Otherwise, joins are not detected.
      // A separate variable comes in handy when optimizing projections, because
      // it allows to distinguish where the projections belongs to (Index or
      // Mat).
      auto newOutVariable =
          plan->getAst()->variables()->createTemporaryVariable();
      matNode->setDocOutVariable(*newOutVariable);

      // replace every occurrence with this new variable
      replaceVariableDownwards(
          matNode->getFirstParent(),
          {{matNode->oldDocVariable().id, newOutVariable}});
      modified = true;
    }
  }

  opt->addPlan(std::move(plan), rule, modified);
}
