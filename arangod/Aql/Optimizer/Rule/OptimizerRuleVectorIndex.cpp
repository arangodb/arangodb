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
    std::unique_ptr<ExecutionPlan>& plan, AstNode const* functionCallNode) {
  Ast* ast = plan->getAst();
  auto cond = std::make_unique<Condition>(ast);
  cond->andCombine(functionCallNode);
  cond->normalize();

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

AstNode const* isSortNodeValid(auto const* sortNode,
                               std::unique_ptr<ExecutionPlan>& plan,
                               FullVectorIndexDefinition const& definition,
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

  // Check if SORT node contains APPROX function
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
      !checkAqlFunction(functionName, definition)) {
    return nullptr;
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
    return nullptr;
  }
  // if variable whose attributes are being accessed is not the same as
  // indexed, we cannot apply the rule
  if (attributeAccessResult.first != enumerateNodeOutVar) {
    return nullptr;
  }

  return calculationNodeExpressionNode;
}

void arangodb::aql::useVectorIndexRule(Optimizer* opt,
                                       std::unique_ptr<ExecutionPlan> plan,
                                       OptimizerRule const& rule) {
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
    auto const* functionCallNode =
        isSortNodeValid(sortNode, plan, vectorIndex->getDefinition(),
                        enumerateColNode->outVariable());
    if (functionCallNode == nullptr) {
      continue;
    }

    // Check LIMIT NODE
    auto const* maybeLimitNode = sortNode->getFirstParent();
    if (!maybeLimitNode || maybeLimitNode->getType() != EN::LIMIT) {
      continue;
    }
    auto const* limitNode =
        ExecutionNode::castTo<LimitNode const*>(maybeLimitNode);
    // Offset cannot be handled, and there must be a limit which means topK
    if (limitNode->offset() != 0 || limitNode->limit() == 0) {
      continue;
    }
    //  now we have a sequence of ENUMERATE_COLLECTION -> SORT -> LIMIT

    // replace ENUMERATE_COLLECTION with INDEX if possible
    IndexIteratorOptions opts;
    opts.sorted = true;
    opts.ascending = true;
    opts.limit = limitNode->limit();
    std::unique_ptr<Condition> condition =
        buildVectorCondition(plan, functionCallNode);
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
