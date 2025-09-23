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
/// @author Jure Bajic
/// @author Lars Maier
////////////////////////////////////////////////////////////////////////////////

#include "Aql/Ast.h"
#include "Aql/Collection.h"
#include "Aql/Condition.h"
#include "Aql/ExecutionNode/EnumerateNearVectorNode.h"
#include "Aql/Functions.h"
#include "Aql/ExecutionNode/EnumerateCollectionNode.h"
#include "Aql/Expression.h"
#include "Aql/ExecutionNode/CalculationNode.h"
#include "Aql/ExecutionNode/MaterializeRocksDBNode.h"
#include "Aql/ExecutionNode/LimitNode.h"
#include "Aql/ExecutionNode/FilterNode.h"
#include "Aql/ExecutionNode/SortNode.h"
#include "Aql/Optimizer.h"
#include "Aql/OptimizerRules.h"
#include "Aql/OptimizerUtils.h"
#include "Assertions/Assert.h"
#include "Indexes/Index.h"
#include "Indexes/VectorIndexDefinition.h"
#include "Inspection/VPack.h"
#include "Logger/LogMacros.h"
#include "RocksDBEngine/RocksDBVectorIndex.h"

using namespace arangodb;
using namespace arangodb::aql;
using EN = arangodb::aql::ExecutionNode;

#define LOG_RULE_ENABLED false
#define LOG_RULE_IF(cond) LOG_DEVEL_IF((LOG_RULE_ENABLED) && (cond))
#define LOG_RULE LOG_RULE_IF(true)

namespace {

bool checkFunctionNameMatchesIndexMetric(
    std::string_view const functionName,
    UserVectorIndexDefinition const& definition) {
  switch (definition.metric) {
    case SimilarityMetric::kL2: {
      return functionName == "APPROX_NEAR_L2";
    }
    case SimilarityMetric::kCosine: {
      return functionName == "APPROX_NEAR_COSINE";
    }
    case SimilarityMetric::kInnerProduct: {
      return functionName == "APPROX_NEAR_INNER_PRODUCT";
    }
  }
}

// Vector index can only have a single covered attribute
bool checkIfIndexedFieldIsSameAsSearched(
    auto const& vectorIndex,
    std::vector<basics::AttributeName>& attributeName) {
  // vector index can be only on single field
  TRI_ASSERT(vectorIndex->fields().size() == 1);
  auto const& indexedVectorField = vectorIndex->fields()[0];

  return attributeName == indexedVectorField;
}

bool checkApproxNearVariableInput(auto const& vectorIndex,
                                  auto const* approxFunctionParam,
                                  auto* outVariable) {
  std::pair<Variable const*, std::vector<arangodb::basics::AttributeName>>
      attributeAccessResult;
  if (approxFunctionParam == nullptr ||
      !approxFunctionParam->isAttributeAccessForVariable(attributeAccessResult,
                                                         false)) {
    return false;
  }
  // check if APPROX_NEAR function parameter is on indexed field
  if (!checkIfIndexedFieldIsSameAsSearched(vectorIndex,
                                           attributeAccessResult.second)) {
    return false;
  }

  // check if APPROX function parameter is the same one as being outputted by
  return outVariable == attributeAccessResult.first;
}

// We return nullptr for AstNode if the check has failed, in that case the bool
// is meaningless
std::pair<AstNode const*, bool> getApproxNearExpression(
    auto const* sortNode, std::unique_ptr<ExecutionPlan>& plan,
    std::shared_ptr<Index> const& vectorIndex) {
  auto const& sortFields = sortNode->elements();
  // since vector index can be created only on single attribute the check is
  // simple
  if (sortFields.size() != 1) {
    return {nullptr, false};
  }
  auto const& sortField = sortFields[0];
  bool ascending = sortField.ascending;

  // Check if the SORT node has a correct order:
  // L2: ASC
  // Cosine: DESC
  // InnerProduct: DESC
  switch (vectorIndex->getVectorIndexDefinition().metric) {
    // L2 metric can only be in ascending order
    case SimilarityMetric::kL2:
      if (!sortField.ascending) {
        return {nullptr, ascending};
      }
      break;
    // Cosine similarity can only be in descending order
    case SimilarityMetric::kCosine:
      if (sortField.ascending) {
        return {nullptr, ascending};
      }
      break;
    case SimilarityMetric::kInnerProduct:
      if (sortField.ascending) {
        return {nullptr, ascending};
      }
      break;
  }

  // check if SORT node contains APPROX function
  auto const* executionNode = plan->getVarSetBy(sortField.var->id);
  if (executionNode == nullptr || executionNode->getType() != EN::CALCULATION) {
    return {nullptr, ascending};
  }
  auto const* calculationNode =
      ExecutionNode::castTo<CalculationNode const*>(executionNode);
  auto const* calculationNodeExpression = calculationNode->expression();
  if (calculationNodeExpression == nullptr) {
    return {nullptr, ascending};
  }
  AstNode const* calculationNodeExpressionNode =
      calculationNodeExpression->node();
  if (calculationNodeExpressionNode == nullptr ||
      calculationNodeExpressionNode->type != AstNodeType::NODE_TYPE_FCALL) {
    return {nullptr, ascending};
  }
  if (auto const functionName =
          aql::functions::getFunctionName(*calculationNodeExpressionNode);
      !checkFunctionNameMatchesIndexMetric(
          functionName, vectorIndex->getVectorIndexDefinition())) {
    return {nullptr, ascending};
  }

  return {calculationNodeExpressionNode, ascending};
}

// Currently this only returns nProbe, in the future it might be possible to
// set other search parameters
SearchParameters getSearchParameters(
    auto const* calculationNodeExpressionNode) {
  auto const* approxFunctionParameters =
      calculationNodeExpressionNode->getMember(0);

  if (approxFunctionParameters->numMembers() == 3 &&
      approxFunctionParameters->getMemberUnchecked(2)->isObject()) {
    auto const searchParametersNode =
        approxFunctionParameters->getMemberUnchecked(2);

    SearchParameters searchParameters;
    VPackBuilder builder;
    searchParametersNode->toVelocyPackValue(builder);
    if (auto const res = velocypack::deserializeWithStatus(builder.slice(),
                                                           searchParameters);
        !res.ok()) {
      THROW_ARANGO_EXCEPTION_MESSAGE(
          TRI_ERROR_QUERY_PARSE,
          fmt::format("error parsing searchParameters: {}!", res.error()));
    }

    return searchParameters;
  }

  return {};
}

AstNode* getApproxNearAttributeExpression(
    auto const* calculationNodeExpressionNode,
    std::shared_ptr<Index> const& vectorIndex, const auto* outVariable) {
  // one of the params must be a documentField and the other a query point
  auto const* approxFunctionParameters =
      calculationNodeExpressionNode->getMember(0);
  TRI_ASSERT(approxFunctionParameters->numMembers() > 1 &&
             approxFunctionParameters->numMembers() < 4)
      << "There can be only two or three arguments to APPROX_NEAR"
      << ", currently there are "
      << calculationNodeExpressionNode->numMembers();

  auto* approxFunctionParamLeft =
      approxFunctionParameters->getMemberUnchecked(0);
  auto* approxFunctionParamRight =
      approxFunctionParameters->getMemberUnchecked(1);

  if (approxFunctionParamLeft->type ==
      arangodb::aql::NODE_TYPE_ATTRIBUTE_ACCESS) {
    if (checkApproxNearVariableInput(vectorIndex, approxFunctionParamLeft,
                                     outVariable)) {
      return approxFunctionParamRight;
    }
  }
  if (checkApproxNearVariableInput(vectorIndex, approxFunctionParamRight,
                                   outVariable)) {
    return approxFunctionParamLeft;
  }

  return nullptr;
}

}  // namespace

std::vector<std::shared_ptr<Index>> getVectorIndexes(
    auto& enumerateCollectionNode) {
  std::vector<std::shared_ptr<Index>> vectorIndexes;
  auto indexes = enumerateCollectionNode->collection()->indexes();
  std::ranges::copy_if(
      indexes, std::back_inserter(vectorIndexes), [](auto const& elem) {
        return elem->type() == Index::IndexType::TRI_IDX_TYPE_VECTOR_INDEX;
      });

  return vectorIndexes;
}

bool removeFilterAndCalculationNode(auto* maybeFilterNode, auto& plan,
                                    auto& filterExpression) {
  auto* maybeCalculationNode = maybeFilterNode->getFirstDependency();
  // We always expect a calculationNode in this scenario
  if (maybeCalculationNode == nullptr ||
      maybeCalculationNode->getType() != EN::CALCULATION) {
    return false;
  }
  auto const* calculationNode =
      ExecutionNode::castTo<CalculationNode const*>(maybeCalculationNode);
  TRI_ASSERT(calculationNode->expression() != nullptr);
  filterExpression = calculationNode->expression()->clone(plan->getAst());

  plan->unlinkNode(maybeFilterNode);
  plan->unlinkNode(maybeCalculationNode);

  return true;
}

// Vector Index Optimization Rule
//
// This rule optimizes queries that use vector similarity search with
// APPROX_NEAR_* functions. Similar to the use-index rule, it identifies
// opportunities to use vector indexes for efficient nearest neighbor searches
// by matching the vector attribute, APPROX_NEAR_* function, and the index's
// distance metric.
//
// Query Pattern:
// The rule detects queries with the pattern: SORT APPROX_NEAR_*(doc.vector,
// target) LIMIT topK It requires the presence of APPROX_NEAR_* functions used
// within a SORT operation followed by a LIMIT. The SORT node is removed (as the
// vector index handles ordering), while the LIMIT node is preserved to control
// result size.
//
// Filter Pushdown:
// When a filter expression is detected, the rule pushes it down to the
// EnumerateNearVectorNode for early evaluation during index traversal. This
// optimization removes both the FILTER node and its associated CALCULATION
// node. This transformation is safe because previous optimization rules have
// already eliminated trivial filters.
void arangodb::aql::useVectorIndexRule(Optimizer* opt,
                                       std::unique_ptr<ExecutionPlan> plan,
                                       OptimizerRule const& rule) {
  bool modified{false};

  containers::SmallVector<ExecutionNode*, 8> nodes;
  plan->findNodesOfType(nodes, EN::ENUMERATE_COLLECTION, true);
  for (ExecutionNode* node : nodes) {
    auto* enumerateCollectionNode =
        ExecutionNode::castTo<EnumerateCollectionNode*>(node);

    // check if there are vector indexes on collection
    auto const vectorIndexes = getVectorIndexes(enumerateCollectionNode);
    if (vectorIndexes.empty()) {
      continue;
    }

    // check that enumerateColNode has both sort and limit
    auto* currentNode = enumerateCollectionNode->getFirstParent();
    const auto skipOverCalculationNodes = [&currentNode] {
      while (currentNode != nullptr &&
             currentNode->getType() == EN::CALCULATION) {
        currentNode = currentNode->getFirstParent();
      }
    };
    skipOverCalculationNodes();

    // We tolerate post filtering
    ExecutionNode* maybeFilterNode{nullptr};
    if (currentNode != nullptr && currentNode->getType() == EN::FILTER) {
      maybeFilterNode = currentNode;
      currentNode = currentNode->getFirstParent();
    }
    skipOverCalculationNodes();

    if (currentNode == nullptr || currentNode->getType() != EN::SORT) {
      LOG_RULE << "DID NOT FIND SORT NODE, but instead "
               << (currentNode ? currentNode->getTypeString() : "nullptr");
      continue;
    }
    auto* sortNode = ExecutionNode::castTo<SortNode*>(currentNode);

    auto const* maybeLimitNode = sortNode->getFirstParent();
    if (!maybeLimitNode || maybeLimitNode->getType() != EN::LIMIT) {
      LOG_RULE << "DID NOT FIND LIMIT NODE, but instead "
               << (maybeLimitNode ? maybeLimitNode->getTypeString()
                                  : "nullptr");
      continue;
    }

    auto const* limitNode =
        ExecutionNode::castTo<LimitNode const*>(maybeLimitNode);
    // Offset cannot be handled, and there must be a limit which means topK
    if (limitNode->limit() == 0) {
      LOG_RULE << "Limit not set";
      continue;
    }

    for (auto const& index : vectorIndexes) {
      auto const [approxNearExpression, ascending] =
          getApproxNearExpression(sortNode, plan, index);
      if (approxNearExpression == nullptr) {
        LOG_RULE << "Query expression not valid";
        continue;
      }
      auto* approximatedAttributeExpression = getApproxNearAttributeExpression(
          approxNearExpression, index, enumerateCollectionNode->outVariable());
      if (approximatedAttributeExpression == nullptr) {
        LOG_RULE << "Function parameters not valid";
        continue;
      }

      auto searchParameters = getSearchParameters(approxNearExpression);

      // replace the collection enumeration with the enumerate near node
      // furthermore, we have to remove the calculation node
      const auto* documentVariable = enumerateCollectionNode->outVariable();

      auto const* distanceVariable = sortNode->elements()[0].var;
      auto const* oldDocumentVariable = enumerateCollectionNode->outVariable();
      auto const* documentIdVariable = oldDocumentVariable;

      auto limit = limitNode->limit();
      auto* inVariable = plan->getAst()->variables()->createTemporaryVariable();

      auto* queryPointCalculationNode = plan->createNode<CalculationNode>(
          plan.get(), plan->nextId(),
          std::make_unique<Expression>(plan->getAst(),
                                       approximatedAttributeExpression),
          inVariable);

      // If there is a FilterNode it comes with CalculationNode, we remove it
      // and handle it in EnumerateNearVectorNode
      // TODO Check if IndexNode always does this
      std::unique_ptr<Expression> filterExpression{nullptr};
      bool isCoveredByStoredValues{false};
      if (maybeFilterNode) {
        if (bool isRemoved = removeFilterAndCalculationNode(
                maybeFilterNode, plan, filterExpression);
            !isRemoved) {
          continue;
        }

        // check which variables are used by the node's post-filter
        std::vector<std::pair<VariableId, RegisterId>> filterVarsToRegs;
        VarSet inVars;
        filterExpression->variables(inVars);
        filterVarsToRegs.reserve(inVars.size());

        // Here we take all variables in the expression
        for (auto const& var : inVars) {
          TRI_ASSERT(var != nullptr);
          if (var->id == oldDocumentVariable->id) {
            continue;
          }
          // auto regId =
          // plan->getRegisterPlanForNode(maybeFilterNode)->variableToRegisterId(var);
          // TODO(jbajic) Why I cannot get this froim the executio plan and not
          // from the node?
          auto regId =
              maybeFilterNode->getRegisterPlan()->variableToRegisterId(var);

          filterVarsToRegs.emplace_back(var->id, regId);
        }

        // get stored fields from vec idx
        auto const* vecIdx = reinterpret_cast<RocksDBVectorIndex*>(index.get());
        if (vecIdx->hasStoredValues() && !filterVarsToRegs.empty()) {
          // check if all attribtues in filterExpression are covered
          // by storedValues

          // Get stored values from the vector index
          auto const& storedValues = vecIdx->storedValues();

          // Extract all attribute names from filter expression
          containers::FlatHashSet<aql::AttributeNamePath> filterAttributes;
          // auto* ast = engine.getQuery().ast();
          auto* ast = plan->getAst();

          for (auto const& varPair : filterVarsToRegs) {
            auto const* variable = ast->variables()->getVariable(varPair.first);
            if (variable != nullptr) {
              // Extract attribute names for this variable from the filter
              // expression
              if (!Ast::getReferencedAttributesRecursive(
                      filterExpression->node(), variable,
                      /*expectedAttribute*/ "", filterAttributes,
                      plan->getAst()->query().resourceMonitor())) {
                // If we can't extract attributes, we can't use stored values
                break;
              }
            }
          }

          // Check if all filter attributes are covered by stored values
          isCoveredByStoredValues = true;
          for (auto const& filterAttr : filterAttributes) {
            bool found = false;
            for (auto const& storedValue : storedValues) {
              // Convert stored value to AttributeNamePath for comparison
              aql::AttributeNamePath storedPath(
                  plan->getAst()->query().resourceMonitor());
              for (auto const& attrName : storedValue) {
                storedPath.add(attrName.name);
              }

              if (filterAttr == storedPath) {
                found = true;
                break;
              }
            }
            if (!found) {
              isCoveredByStoredValues = false;
              break;
            }
          }
        }
      }
      auto* enumerateNear = plan->createNode<EnumerateNearVectorNode>(
          plan.get(), plan->nextId(), inVariable, oldDocumentVariable,
          documentIdVariable, distanceVariable, limit, ascending,
          limitNode->offset(), std::move(searchParameters),
          enumerateCollectionNode->collection(), index,
          std::move(filterExpression), isCoveredByStoredValues);

      auto* materializer =
          plan->createNode<materialize::MaterializeRocksDBNode>(
              plan.get(), plan->nextId(), enumerateCollectionNode->collection(),
              *documentIdVariable, *documentVariable, *documentVariable);
      plan->excludeFromScatterGather(enumerateNear);

      plan->replaceNode(enumerateCollectionNode, enumerateNear);
      plan->insertBefore(enumerateNear, queryPointCalculationNode);
      plan->insertAfter(enumerateNear, materializer);

      // we don't need this sort node at all, because we produce sorted output
      plan->unlinkNode(sortNode);

      auto* distanceCalculationNode = plan->getVarSetBy(distanceVariable->id);
      plan->unlinkNode(distanceCalculationNode);

      modified = true;
      break;
    }
  }

  opt->addPlan(std::move(plan), rule, modified);
}
