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
////////////////////////////////////////////////////////////////////////////////

#include <functional>

#include "Aql/Ast.h"
#include "Aql/Collection.h"
#include "Aql/Condition.h"
#include "Aql/Functions.h"
#include "Aql/ExecutionNode/EnumerateCollectionNode.h"
#include "Aql/Expression.h"
#include "Aql/ExecutionNode/CalculationNode.h"
#include "Aql/ExecutionNode/EnumerateNearVectors.h"
#include "Aql/ExecutionNode/MaterializeRocksDBNode.h"
#include "Aql/ExecutionNode/LimitNode.h"
#include "Aql/ExecutionNode/SortNode.h"
#include "Aql/Optimizer.h"
#include "Aql/OptimizerRules.h"
#include "Aql/OptimizerUtils.h"
#include "Indexes/Index.h"
#include "Indexes/VectorIndexDefinition.h"
#include "Logger/LogMacros.h"
#include "RocksDBEngine/RocksDBVectorIndex.h"

using namespace arangodb;
using namespace arangodb::aql;
using EN = arangodb::aql::ExecutionNode;

#define LOG_RULE_ENABLED true
#define LOG_RULE_IF(cond) LOG_DEVEL_IF((LOG_RULE_ENABLED) && (cond))
#define LOG_RULE LOG_RULE_IF(true)

namespace {

bool checkFunctionNameMatchesIndexMetric(
    std::string_view const functionName,
    UserVectorIndexDefinition const& definition) {
  switch (definition.metric) {
    case SimilarityMetric::kL1: {
      return functionName == "APPROX_NEAR_L1";
    }
    case SimilarityMetric::kL2: {
      return functionName == "APPROX_NEAR_L2";
    }
    case SimilarityMetric::kCosine: {
      return functionName == "APPROX_NEAR_COSINE";
    }
  }
}

AstNode* isSortNodeValid(auto const* sortNode,
                         std::unique_ptr<ExecutionPlan>& plan,
                         UserVectorIndexDefinition const& definition,
                         Variable const* enumerateNodeOutVar) {
  auto const& sortFields = sortNode->elements();
  // since vector index can be created only on single attribute the check is
  // simple
  if (sortFields.size() != 1) {
    return nullptr;
  }
  auto const& sortField = sortFields[0];
  // Cannot be descending
  if (!sortField.ascending) {
    return nullptr;
  }

  // check if SORT node contains APPROX function
  auto const* executionNode = plan->getVarSetBy(sortField.var->id);
  if (executionNode == nullptr || executionNode->getType() != EN::CALCULATION) {
    return nullptr;
  }
  auto const* calculationNode =
      ExecutionNode::castTo<CalculationNode const*>(executionNode);
  auto const* calculationNodeExpression = calculationNode->expression();
  if (calculationNodeExpression == nullptr) {
    return nullptr;
  }
  AstNode const* calculationNodeExpressionNode =
      calculationNodeExpression->node();
  if (calculationNodeExpressionNode == nullptr ||
      calculationNodeExpressionNode->type != AstNodeType::NODE_TYPE_FCALL) {
    return nullptr;
  }
  if (auto const functionName =
          aql::functions::getFunctionName(*calculationNodeExpressionNode);
      !checkFunctionNameMatchesIndexMetric(functionName, definition)) {
    return nullptr;
  }

  // check if APPROX function parameter is on indexed field
  TRI_ASSERT(calculationNodeExpressionNode->numMembers() > 0);
  auto const* approxFunctionParameters =
      calculationNodeExpressionNode->getMember(0);
  auto const* approxFunctionParam = approxFunctionParameters->getMember(0);
  // TODO check both parameters, because those functions are commutative
  std::pair<Variable const*, std::vector<arangodb::basics::AttributeName>>
      attributeAccessResult;
  if (approxFunctionParam == nullptr ||
      !approxFunctionParam->isAttributeAccessForVariable(attributeAccessResult,
                                                         false)) {
    return nullptr;
  }
  // if variable whose attributes are being accessed is not the same as
  // indexed, we cannot apply the rule
  if (attributeAccessResult.first != enumerateNodeOutVar) {
    return nullptr;
  }

  return approxFunctionParameters->getMember(1);
}

}  // namespace

void arangodb::aql::useVectorIndexRule(Optimizer* opt,
                                       std::unique_ptr<ExecutionPlan> plan,
                                       OptimizerRule const& rule) {
  bool modified{false};

  containers::SmallVector<ExecutionNode*, 8> nodes;
  // avoid subqueries for now
  plan->findNodesOfType(nodes, EN::ENUMERATE_COLLECTION, true);
  for (ExecutionNode* node : nodes) {
    auto enumerateCollectionNode =
        ExecutionNode::castTo<EnumerateCollectionNode*>(node);

    // check if there is a vector index on collection
    auto const index = std::invoke(
        [&enumerateCollectionNode]() -> std::shared_ptr<arangodb::Index> {
          auto indexes = enumerateCollectionNode->collection()->indexes();
          for (auto const& index : indexes) {
            if (index->type() == Index::IndexType::TRI_IDX_TYPE_VECTOR_INDEX) {
              return index;
            }
          }
          return nullptr;
        });
    if (!index) {
      continue;
    }

    // check that enumerateColNode has both sort and limit
    auto* currentNode = enumerateCollectionNode->getFirstParent();

    // skip over some calculation nodes
    while (currentNode != nullptr &&
           currentNode->getType() == EN::CALCULATION) {
      currentNode = currentNode->getFirstParent();
    }

    if (currentNode == nullptr || currentNode->getType() != EN::SORT) {
      LOG_RULE << "DID NOT FIND SORT NODE, but instead "
               << (currentNode ? currentNode->getTypeString() : "nullptr");
      continue;
    }
    auto sortNode = ExecutionNode::castTo<SortNode*>(currentNode);

    auto const* maybeLimitNode = sortNode->getFirstParent();
    if (!maybeLimitNode || maybeLimitNode->getType() != EN::LIMIT) {
      LOG_RULE << "DID NOT FIND LIMIT NODE, but instead "
               << (maybeLimitNode ? maybeLimitNode->getTypeString()
                                  : "nullptr");
      continue;
    }

    auto queryExpression =
        isSortNodeValid(sortNode, plan, index->getVectorIndexDefinition(),
                        enumerateCollectionNode->outVariable());
    if (queryExpression == nullptr) {
      LOG_RULE << "Query expression not valid";
      continue;
    }

    // check LIMIT NODE
    auto const* limitNode =
        ExecutionNode::castTo<LimitNode const*>(maybeLimitNode);
    // Offset cannot be handled, and there must be a limit which means topK
    if (limitNode->offset() != 0 || limitNode->limit() == 0) {
      continue;
    }
    //  now we have a sequence of ENUMERATE_COLLECTION -> SORT -> LIMIT

    // replace the collection enumeration with the enumerate near node
    // furthermore, we have to remove the calculation node
    auto documentVariable = enumerateCollectionNode->outVariable();

    auto distanceVariable = sortNode->elements()[0].var;
    auto oldDocumentVariable = enumerateCollectionNode->outVariable();

    // actually we want this to be late materialized.
    // But this is too complicated for now. A later optimizer rule should
    // but the late materialization into place.
    auto documentIdVariable = oldDocumentVariable;
    // plan->getAst()->variables()->createTemporaryVariable();

    auto limit = limitNode->limit();
    auto inVariable = plan->getAst()->variables()->createTemporaryVariable();

    auto queryPointCalculationNode = plan->createNode<CalculationNode>(
        plan.get(), plan->nextId(),
        std::make_unique<Expression>(plan->getAst(), queryExpression),
        inVariable);

    LOG_RULE << "Enumerate Near in " << inVariable->id
             << " out = " << documentVariable->id
             << " distance = " << distanceVariable->id << " limit = " << limit;

    auto enumerateNear = plan->createNode<EnumerateNearVectors>(
        plan.get(), plan->nextId(), inVariable, oldDocumentVariable,
        documentIdVariable, distanceVariable, limit,
        enumerateCollectionNode->collection(), index);

    plan->replaceNode(enumerateCollectionNode, enumerateNear);
    plan->insertBefore(enumerateNear, queryPointCalculationNode);
    // plan->insertAfter(enumerateNear, materializer);

    // we don't need this sort node at all, because we produce sorted output
    plan->unlinkNode(sortNode);

    auto distanceCalculationNode = plan->getVarSetBy(distanceVariable->id);
    plan->unlinkNode(distanceCalculationNode);

    modified = true;
  }

  opt->addPlan(std::move(plan), rule, modified);
}
