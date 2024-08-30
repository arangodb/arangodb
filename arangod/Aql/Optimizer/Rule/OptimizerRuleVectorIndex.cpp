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

#include "Aql/Collection.h"
#include "Aql/Condition.h"
#include "Aql/Functions.h"
#include "Aql/ExecutionNode/EnumerateCollectionNode.h"
#include "Aql/ExecutionNode/GatherNode.h"
#include "Aql/Expression.h"
#include "Aql/ExecutionNode/CalculationNode.h"
#include "Aql/ExecutionNode/IndexNode.h"
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

std::unique_ptr<Condition> buildVectorCondition(
    std::unique_ptr<ExecutionPlan>& plan) {
  Ast* ast = plan->getAst();
  auto cond = std::make_unique<Condition>(ast);

  cond->normalize(plan.get());
  return cond;
}

bool checkAqlFunction(std::string_view const functionName,
                      FullVectorIndexDefinition const& definition) {
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

bool isSortNodeValid(auto const* sortNode, std::unique_ptr<ExecutionPlan>& plan,
                     FullVectorIndexDefinition const& definition,
                     Variable const* enumerateNodeOutVar) {
  auto const& sortFields = sortNode->elements();
  // since vector index can be created only on single attribute the check is
  // simple
  if (sortFields.size() != 1) {
    return false;
  }
  auto const& sortField = sortFields[0];

  // Check if SORT node contains APPROX function
  auto const* executionNode = plan->getVarSetBy(sortField.var->id);
  if (executionNode == nullptr || executionNode->getType() != EN::CALCULATION) {
    return false;
  }
  auto const* calculationNode =
      ExecutionNode::castTo<CalculationNode const*>(executionNode);
  auto const* calculationNodeExpression = calculationNode->expression();
  if (calculationNodeExpression == nullptr) {
    return false;
  }
  AstNode const* calculationNodeExpressionNode =
      calculationNodeExpression->node();
  if (calculationNodeExpressionNode == nullptr ||
      calculationNodeExpressionNode->type != AstNodeType::NODE_TYPE_FCALL) {
    return false;
  }
  if (auto const functionName =
          aql::functions::getFunctionName(*calculationNodeExpressionNode);
      !checkAqlFunction(functionName, definition)) {
    return false;
  }

  // Check if APPROX function parameter is on indexed field
  TRI_ASSERT(calculationNodeExpressionNode->numMembers() > 0);
  auto const* approxFunctionParameters =
      calculationNodeExpressionNode->getMember(0);
  auto const* approxFunctionParam = approxFunctionParameters->getMember(0);
  std::pair<Variable const*, std::vector<arangodb::basics::AttributeName>>
      attributeAccessResult;
  if (approxFunctionParam == nullptr ||
      !approxFunctionParam->isAttributeAccessForVariable(attributeAccessResult,
                                                         false)) {
    LOG_DEVEL << __FUNCTION__ << ":" << __LINE__;
    return false;
  }
  // if variable whose attributes are being accessed is not the same as
  // indexed, we cannot apply the rule
  if (attributeAccessResult.first != enumerateNodeOutVar) {
    LOG_DEVEL << __FUNCTION__ << ":" << __LINE__;
    return false;
  }

  return true;
}

void arangodb::aql::useVectorIndexRule(Optimizer* opt,
                                       std::unique_ptr<ExecutionPlan> plan,
                                       OptimizerRule const& rule) {
  LOG_DEVEL << __FUNCTION__ << " START";
  bool modified{false};

  containers::SmallVector<ExecutionNode*, 8> nodes;
  // avoid subqueries for now
  plan->findNodesOfType(nodes, EN::ENUMERATE_COLLECTION, false);
  for (ExecutionNode* node : nodes) {
    auto const* enumerateColNode =
        ExecutionNode::castTo<EnumerateCollectionNode const*>(node);

    // check if there is a vector index on collection
    auto const index =
        std::invoke([&enumerateColNode]() -> std::shared_ptr<arangodb::Index> {
          auto indexes = enumerateColNode->collection()->indexes();
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
    auto* currentNode = enumerateColNode->getFirstParent();
    while (currentNode != nullptr &&
           currentNode->getType() == EN::CALCULATION) {
      currentNode = currentNode->getFirstParent();
    }
    if (currentNode == nullptr || currentNode->getType() != EN::SORT) {
      continue;
    }

    auto const* sortNode = ExecutionNode::castTo<SortNode const*>(currentNode);
    auto const* vectorIndex =
        dynamic_cast<RocksDBVectorIndex const*>(index.get());
    if (vectorIndex == nullptr) {
      continue;
    }
    if (!isSortNodeValid(sortNode, plan, vectorIndex->getDefinition(),
                         enumerateColNode->outVariable())) {
      continue;
    }
    LOG_DEVEL << "SORT CHECK PASSED";
    /*    for (const auto& elem : sortFields) {*/
    /*LOG_DEVEL << "ELEM " << elem.toString();*/
    /*}*/

    // Check LIMIT NODE
    auto const* maybeLimitNode = sortNode->getFirstParent();
    if (!maybeLimitNode || maybeLimitNode->getType() != EN::LIMIT) {
      continue;
    }
    auto const* limitNode =
        ExecutionNode::castTo<LimitNode const*>(maybeLimitNode);
    if (limitNode->offset() != 0) {
      continue;
    }
    LOG_DEVEL << "LIMIT CHECK PASSED";
    // auto const topK = limitNode->limit();
    //  now we have a sequence of ENUMERATE_COLLECTION -> SORT -> LIMIT

    // replace ENUMERATE_COLLECTION with INDEX if possible
    IndexIteratorOptions opts;
    opts.sorted = true;
    opts.ascending = true;
    opts.limit = 5;
    std::unique_ptr<Condition> condition = buildVectorCondition(plan);
    LOG_DEVEL << "CREATING INDEX NODE";
    auto indexNode = plan->createNode<IndexNode>(
        plan.get(), plan->nextId(), enumerateColNode->collection(),
        enumerateColNode->outVariable(),
        std::vector<transaction::Methods::IndexHandle>{
            transaction::Methods::IndexHandle{index}},
        false,  // here we are not using inverted index so
        // for sure no "whole" coverage
        std::move(condition), opts);

    plan->replaceNode(node, indexNode);
    modified = true;
  }

  opt->addPlan(std::move(plan), rule, modified);
}
