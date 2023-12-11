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

#include "ApplicationFeatures/ApplicationServer.h"
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
#include "Aql/IndexNode.h"
#include "Aql/IndexStreamIterator.h"
#include "Aql/JoinNode.h"
#include "Aql/Optimizer.h"
#include "Aql/OptimizerUtils.h"
#include "Aql/Projections.h"
#include "Aql/Query.h"
#include "Aql/Variable.h"
#include "Aql/types.h"
#include "Basics/AttributeNameParser.h"
#include "Basics/NumberUtils.h"
#include "Basics/ScopeGuard.h"
#include "Basics/StaticStrings.h"
#include "Containers/FlatHashSet.h"
#include "Containers/HashSet.h"
#include "Containers/SmallUnorderedMap.h"
#include "Containers/SmallVector.h"
#include "Geo/GeoParams.h"
#include "Graph/ShortestPathOptions.h"
#include "Graph/TraverserOptions.h"
#include "Indexes/Index.h"
#include "Logger/LogMacros.h"
#include "OptimizerRules.h"
#include "Utils/CollectionNameResolver.h"
#include "VocBase/LogicalCollection.h"
#include "VocBase/Methods/Collections.h"

#include <bitset>
#include <span>
#include <tuple>

using namespace arangodb;
using namespace arangodb::aql;
using namespace arangodb::containers;
using EN = arangodb::aql::ExecutionNode;

#define LOG_JOIN_OPTIMIZER_RULE LOG_DEVEL_IF(true)

namespace {

enum IndexUsage { FIRST, OTHER, NONE };

struct IndexOffsets {
  std::vector<size_t> keyFields;
  std::vector<size_t> constantFields;
  std::vector<AstNode const*> constantValues;

  bool hasConstantValues() const { return !constantValues.empty(); }

  std::vector<size_t> const& getKeyFields() const { return keyFields; }

  std::vector<size_t> const& getConstantFields() const {
    return constantFields;
  }

  std::vector<AstNode const*> getConstantValues() const {
    return constantValues;
  }

  bool insertKeyField(size_t position) {
    if (std::find(keyFields.begin(), keyFields.end(), position) !=
        keyFields.end()) {
      return false;
    }
    keyFields.emplace_back(position);
    return true;
  }

  bool insertConstantField(size_t position) {
    if (std::find(constantFields.begin(), constantFields.end(), position) !=
        constantFields.end()) {
      LOG_JOIN_OPTIMIZER_RULE << "Could not insert constant field at position "
                              << position << " because it already exists.";
      return false;
    }
    constantFields.emplace_back(position);
    return true;
  }

  void insertConstantValue(AstNode const* node) {
    constantValues.emplace_back(node);
  }
};

using IndicesOffsets = std::map<ExecutionNodeId, IndexOffsets>;

}  // namespace

bool indexNodeQualifies(IndexNode const& indexNode) {
  // not yet supported:
  // - IndexIteratorOptions: sorted, ascending, evalFCalls, useCache,
  // waitForSync, limit, lookahead
  // - reverse iteration

  LOG_JOIN_OPTIMIZER_RULE << "Index node id: " << indexNode.id();
  if (indexNode.condition() == nullptr) {
    // IndexNode does not have an index lookup condition
    LOG_JOIN_OPTIMIZER_RULE << "IndexNode does not have an index lookup "
                               "condition, so we cannot join it";
    return false;
  }

  if (!indexNode.options().ascending) {
    // reverse sort not yet supported
    LOG_JOIN_OPTIMIZER_RULE << "IndexNode is not sorted ascending, so we "
                               "cannot join it";
    return false;
  }

  auto const& indexes = indexNode.getIndexes();
  if (indexes.size() != 1) {
    // must use exactly one index (otherwise this would be an OR condition)
    LOG_JOIN_OPTIMIZER_RULE << "IndexNode uses more than one index, so we "
                               "cannot join it";
    return false;
  }

  auto const& index = indexes[0];
  if (!index->isSorted()) {
    // must be a sorted index
    LOG_JOIN_OPTIMIZER_RULE << "IndexNode uses an unsorted index, so we "
                               "cannot join it";
    return false;
  }

  if (index->fields().empty()) {
    // index on more than one attribute
    LOG_JOIN_OPTIMIZER_RULE << "IndexNode uses an index on more than one "
                               "attribute, so we cannot join it";
    return false;
  }

  if (index->sparse()) {
    // index is a sparse index, we cannot use it. We would otherwise generated
    // incomplete results.
    LOG_JOIN_OPTIMIZER_RULE << "IndexNode uses an sparse index, "
                               "so we cannot join it";
    return false;
  }

  if (index->hasExpansion()) {
    // index uses expansion ([*]) operator
    LOG_JOIN_OPTIMIZER_RULE << "IndexNode uses an index with expansion, "
                               "so we cannot join it";
    return false;
  }

  LOG_JOIN_OPTIMIZER_RULE << "IndexNode qualifies for joining";
  return true;
}

bool joinConditionMatches(ExecutionPlan& plan, AstNode const* lhs,
                          AstNode const* rhs, IndexNode const* i1,
                          IndexNode const* i2) {
  std::pair<Variable const*, std::vector<arangodb::basics::AttributeName>>
      usedVar;
  LOG_JOIN_OPTIMIZER_RULE << "called with i1: " << i1->outVariable()->name
                          << ", i2: " << i2->outVariable()->name
                          << ", lhs: " << lhs->getTypeString()
                          << ", rhs: " << rhs->getTypeString();
  if (lhs->type == NODE_TYPE_REFERENCE) {
    Variable const* other = static_cast<Variable const*>(lhs->getData());
    LOG_JOIN_OPTIMIZER_RULE << "lhs is a reference to " << other->name
                            << ". looking for " << i1->outVariable()->name;
    auto setter = plan.getVarSetBy(other->id);
    if (setter != nullptr && (setter->getType() == EN::INDEX ||
                              setter->getType() == EN::ENUMERATE_COLLECTION)) {
      LOG_JOIN_OPTIMIZER_RULE << "lhs is set by index|enum";
      auto* documentNode =
          ExecutionNode::castTo<DocumentProducingNode*>(setter);
      auto const& p = documentNode->projections();
      for (size_t i = 0; i < p.size(); ++i) {
        if (p[i].path.get()[0] == i1->getIndexes()[0]->fields()[0][0].name) {
          usedVar.first = documentNode->outVariable();
          for (auto const& it : p[i].path.get()) {
            usedVar.second.emplace_back(it);
          }
          LOG_JOIN_OPTIMIZER_RULE << "lhs matched outvariable "
                                  << usedVar.first->name << ", "
                                  << p[i].path.get();
          break;
        }
      }
    }
  }

  if (usedVar.first == nullptr && !lhs->isAttributeAccessForVariable(
                                      usedVar, /*allowIndexedAccess*/ false)) {
    // lhs is not an attribute access
    return false;
  }
  if (usedVar.first != i1->outVariable() ||
      usedVar.second != i1->getIndexes()[0]->fields()[0]) {
    // lhs doesn't match i1's FOR loop index field
    return false;
  }
  // lhs matches i1's FOR loop index field

  usedVar.first = nullptr;
  usedVar.second.clear();

  if (rhs->type == NODE_TYPE_REFERENCE) {
    Variable const* other = static_cast<Variable const*>(rhs->getData());
    LOG_JOIN_OPTIMIZER_RULE << "rhs is a reference to " << other->name
                            << ". looking for " << i2->outVariable()->name;
    auto setter = plan.getVarSetBy(other->id);
    if (setter != nullptr && (setter->getType() == EN::INDEX ||
                              setter->getType() == EN::ENUMERATE_COLLECTION)) {
      LOG_JOIN_OPTIMIZER_RULE << "rhs is set by index|enum";
      auto* documentNode =
          ExecutionNode::castTo<DocumentProducingNode*>(setter);
      auto const& p = documentNode->projections();
      for (size_t i = 0; i < p.size(); ++i) {
        if (p[i].path.get()[0] == i2->getIndexes()[0]->fields()[0][0].name) {
          usedVar.first = documentNode->outVariable();
          for (auto const& it : p[i].path.get()) {
            usedVar.second.emplace_back(it);
          }
          LOG_JOIN_OPTIMIZER_RULE << "rhs matched outvariable "
                                  << usedVar.first->name << ", "
                                  << p[i].path.get();
          break;
        }
      }
    }
  }

  if (usedVar.first == nullptr && !rhs->isAttributeAccessForVariable(
                                      usedVar, /*allowIndexedAccess*/ false)) {
    // rhs is not an attribute access
    return false;
  }
  if (usedVar.first != i2->outVariable() ||
      usedVar.second != i2->getIndexes()[0]->fields()[0]) {
    // rhs doesn't match i2's FOR loop index field
    return false;
  }
  // rhs matches i2's FOR loop index field
  return true;
}

void removeUnnecessaryProjections(ExecutionPlan& plan, JoinNode* jn) {
  // remove any now-unused projections from the JoinNode.
  // these are projections that are not used after the JoinNode,
  // and that are not used by any of the JoinNodes post-filters.
  // we _can_ remove all projections that are only used inside
  // the JoinNode and that are part of the JoinNode's join
  // conditions.

  // we need to refresh the variable usage in the execution
  // plan first because we changed nodes around before.
  plan.findVarUsage();
  auto& indexInfos = jn->getIndexInfos();
  // for every index in the JoinNode, check if it is has
  // projections. if it has projections, check if the index
  // out variable is used after the JoinNode. if it is,
  // we do not attempt further optimizations.
  // if the index out variable is only used inside the
  // JoinNode itself, we check for every projection if the
  // project is not used in the following index lookup's
  // post-filter conditions. if it is not used there, it
  // is safe to remove the projection completely.

  containers::FlatHashSet<AttributeNamePath> projectionsUsedLater;
  // for every index in the JoinNode...
  for (size_t i = 0; i < indexInfos.size(); ++i) {
    // check if it has projections
    if (indexInfos[i].projections.empty() || indexInfos[i].isLateMaterialized) {
      // no projections => no projections to optimize away.
      // this check is important because further down we
      // rely on that projections have been present.
      continue;
    }
    Variable const* outVariable = indexInfos[i].outVariable;
    // output variable of current index in JoinNode is not
    // used after the JoinNode itself. now check if the projection
    // is used by any of the following indexes' filter conditions.
    // in that case we need to keep the projection, otherwise
    // we can remove it.
    projectionsUsedLater.clear();
    utils::findProjections(jn->getFirstParent(), outVariable, "", false,
                           projectionsUsedLater);

    // remove all projections for which the lambda returns true:
    TRI_ASSERT(!indexInfos[i].projections.empty());
    indexInfos[i].projections.erase([&](Projections::Projection& p) -> bool {
      containers::FlatHashSet<AttributeNamePath> attributes;
      // look into all following indexes in the JoinNode
      // (i.e. south of us)
      for (size_t j = i + 1; j < indexInfos.size(); ++j) {
        if (indexInfos[j].filter == nullptr) {
          // following index does not have a post filter
          // condition, and thus cannot use a projection
          continue;
        }
        // collect all used attribute names in the index (j)
        // post filter condition that refer to our (i) output
        // variable:
        if (!Ast::getReferencedAttributesRecursive(
                indexInfos[j].filter->node(), outVariable, "", attributes,
                plan.getAst()->query().resourceMonitor())) {
          // unable to determine referenced attributes safely.
          // we better do not remove the projection.
          return false;
        }
        if (std::find(attributes.begin(), attributes.end(), p.path) !=
            attributes.end()) {
          // projection of index (i) is used in post filter
          // condition of index (j). we cannot remove it
          return false;
        }
      }
      // projection of index (i) is not used in any of the
      // following indexes (i + 1..n).
      // now check if the projection is used later in the query
      for (auto const& path : projectionsUsedLater) {
        if (p.path.isPrefixOf(path)) {
          return false;
        }
      }

      // we return true so the superfluous projection will be
      // removed
      return true;
    });
    if (indexInfos[i].projections.empty() && indexInfos[i].producesOutput) {
      // if we had projections before and now removed all
      // projections, it means we removed all remaining projections
      // for the index. that means we also can make it not produce
      // any output.
      indexInfos[i].producesOutput = false;
    }
  }
}

void optimizeJoinNode(ExecutionPlan& plan, JoinNode* jn) {
  removeUnnecessaryProjections(plan, jn);
}

bool createSpanPositionsForIndex(
    AstNode const* attributeAccessNode,
    std::map<ExecutionNodeId,
             std::tuple<std::string, std::vector<size_t>, std::vector<size_t>>>&
        keyFieldsAndConstantFields,
    ExecutionNodeId candidateID,
    std::vector<std::vector<basics::AttributeName>> indexFields) {
  LOG_DEVEL << "XXXXXX problematic place XXXXXX";
  /*auto getAttributePosition =
      [](std::vector<std::string> const& vec,
         std::string const& target) -> std::optional<size_t> {
    auto it = std::find(vec.begin(), vec.end(), target);

    if (it != vec.end()) {
      return std::distance(vec.begin(), it);
    }
    return std::nullopt;
  };*/

  std::string attributeName = attributeAccessNode->getString();
  LOG_DEVEL << "Attribute name is: " << attributeName;
  if (!keyFieldsAndConstantFields.contains(candidateID)) {
    // initialize the entry for this candidateID
    LOG_DEVEL << "Stringy: " << attributeAccessNode->getString();
    keyFieldsAndConstantFields.try_emplace(
        candidateID,
        std::make_tuple<std::string, std::vector<size_t>, std::vector<size_t>>(
            std::string{attributeName}, {}, {}));
  }
  TRI_ASSERT(keyFieldsAndConstantFields.contains(candidateID));
  std::vector<std::string> idxAttrFields = {};
  for (auto iF : indexFields) {
    idxAttrFields.push_back(iF[0].name);
  }
  /*auto res = getAttributePosition(idxAttrFields, attributeName);
  if (!res.first) {
    // not found
    return false;
  }*/

  size_t keyFieldPosition = 0;  // now brokenx

  LOG_DEVEL << "===== YYYYYYYY ====== ";
  for (auto const& idxfield : indexFields) {
    LOG_DEVEL << "Field name is " << idxfield[0].name;
  }

  /*
   * If we're in our outer (first) IndexNode and access the outVariable of it,
   * we need to set the keyFieldPosition to the position the attribute
   * is standing in the own indexFields.
   */

  /*
   *
   * Let's assume the collection based on doc1 has indexed attributes on [x, y].
   * The collection based on doc2 has indexed attributes on [x, y, z].
   *
   * Cases:
   *
   * 1.) Constant Found (e.g. FILTER doc1.x == 1)
   * If a constant has been found, we need to properly set the constant field
   * position based on the used index (can be ours OR first node's index).
   *
   * 2.) Join Condition Found (e.g. FILTER doc1.x == doc2.y)
   * Detection: One side the outVariable of the first node, the other side the
   * outVariable of the other node.
   *
   */

  /*
   * If we're accessing our own outVariable, we need to set the keyFieldPosition
   * to the position the attribute is standing in the own indexFields
   * (currentCandidate)
   *
   * if we're accessing the other first node's outVariable, we need to set the
   * keyFieldPosition to the position the attribute is standing in the other
   * indexFields (firstCandidate)
   *
   * To calculate the offset, we can only do this if the other side of the eq
   * expression is constant. The value which need to be set is the position of
   * the attribute name in the indexFields.
   *
   */

  LOG_DEVEL << "Attribute pos for stringify: " << keyFieldPosition;
  if (keyFieldPosition != std::numeric_limits<size_t>::max()) {
    // found the key field
    if ((keyFieldPosition + 1) <= indexFields.size()) {
      LOG_DEVEL << "Will try to emplace back to cid: " << candidateID;
      // keyField

      size_t keyPos = keyFieldPosition + 1;
      size_t constantPos = keyFieldPosition;
      LOG_DEVEL << "keyFieldPosition: " << (keyFieldPosition + 1);
      LOG_DEVEL << "constant: " << (constantPos);
      std::get<1>(keyFieldsAndConstantFields[candidateID]).emplace_back(keyPos);
      // constantField
      std::get<2>(keyFieldsAndConstantFields[candidateID])
          .emplace_back(constantPos);
    } else {
      LOG_DEVEL << "Will not try to emplace back";
      return false;
    }
  }

  LOG_DEVEL << "===== ZZZZZZZZZ ====== ";
  LOG_DEVEL << "keyFieldsAndConstantFields.size(): "
            << keyFieldsAndConstantFields.size();
  for (auto const& [key, value] : keyFieldsAndConstantFields) {
    LOG_DEVEL << "key: " << key;
    LOG_DEVEL << "value: " << std::get<0>(value);
    auto keyOffsets = std::get<1>(value);
    auto constantOffsets = std::get<2>(value);

    for (auto const& kk : keyOffsets) {
      LOG_DEVEL << "-> key: " << kk;
    }
    for (auto const& cc : constantOffsets) {
      LOG_DEVEL << "-> constant: " << cc;
    }
  }

  // TODO: Implement verification of current keyFieldsAndConstantFields ( all
  // key and offset values). If one of them harms the allowance, we need to
  // return false here.

  return true;
}

[[nodiscard]] bool isVariableConstant(AstNode const* node,
                                      VarSet const& knownConstVariables) {
  LOG_JOIN_OPTIMIZER_RULE << "Checking if condition is constant ("
                          << node->toString() << ")";

  // TODO:  Quickly explain this section
  VarSet result;
  Ast::getReferencedVariables(node, result);

  for (auto const& var : result) {
    if (!knownConstVariables.contains(var)) {
      return false;
    }
  }

  return true;
}

void getIndexAttributes(IndexNode const* candidate,
                        std::string_view constantAttribute,
                        std::vector<basics::AttributeName>& resultVector) {
  arangodb::basics::TRI_ParseAttributeString(constantAttribute, resultVector,
                                             false);
}

std::pair<bool, size_t> doIndexAttributesMatch(
    IndexNode const* candidate,
    std::vector<basics::AttributeName>& resultVector) {
  LOG_DEVEL << "Do index attributes match?";
  LOG_DEVEL << "Checking attributes:";
  for (auto const& attr : resultVector) {
    LOG_DEVEL << "  - " << attr.name;
  }
  auto usedIndex = candidate->getIndexes()[0];

  LOG_DEVEL << "Primary index?: " << std::boolalpha
            << usedIndex->id().isPrimary();
  // TODO: NEXT - attributeMatchesWithPos does not work as expected. We need to
  // implement an own method for positions.
  return usedIndex->attributeMatchesWithPos(resultVector,
                                            usedIndex->id().isPrimary());
}

bool processConstantFinding(IndexNode const* currentCandidate,
                            IndicesOffsets& indicesOffsets,
                            AstNode const* constant,
                            AstNode const* constantValue) {
  TRI_ASSERT(constant->type == NODE_TYPE_ATTRIBUTE_ACCESS);

  std::vector<basics::AttributeName> resultVector;
  getIndexAttributes(currentCandidate, constant->getStringView(), resultVector);
  auto attributeMatchResult =
      doIndexAttributesMatch(currentCandidate, resultVector);

  if (attributeMatchResult.first) {
    // match found
    size_t constantPos = attributeMatchResult.second;
    TRI_ASSERT(constantPos >= 0);

    if (!indicesOffsets.contains(currentCandidate->id())) {
      IndexOffsets idxOffset{};
      indicesOffsets.try_emplace(currentCandidate->id(), std::move(idxOffset));
    }

    TRI_ASSERT(indicesOffsets.contains(currentCandidate->id()));
    auto& idxOffsetRef = indicesOffsets[currentCandidate->id()];
    if (idxOffsetRef.insertConstantField(constantPos)) {
      idxOffsetRef.insertConstantValue(constantValue);
      return true;
    }

    return false;
  }

  return false;
}

bool processJoinKeyFinding(IndexNode const* firstCandidate,
                           IndexNode const* currentCandidate,
                           IndicesOffsets& indicesOffsets,
                           AstNode const* node) {
  LOG_DEVEL << "aha here";
  node->dump(20);
  std::vector<basics::AttributeName> resultVectorFirst;
  std::vector<basics::AttributeName> resultVectorCurrent;
  getIndexAttributes(firstCandidate, node->getStringView(), resultVectorFirst);
  getIndexAttributes(currentCandidate, node->getStringView(),
                     resultVectorCurrent);

  auto attributeMatchResultFirst =
      doIndexAttributesMatch(currentCandidate, resultVectorFirst);
  auto attributeMatchResultCurrent =
      doIndexAttributesMatch(currentCandidate, resultVectorCurrent);

  if (attributeMatchResultFirst.first && attributeMatchResultCurrent.first) {
    // match found
    auto keyPosFirst = attributeMatchResultFirst.second;
    auto keyPosCurrent = attributeMatchResultCurrent.second;
    TRI_ASSERT(keyPosFirst >= 0);
    TRI_ASSERT(keyPosCurrent >= 0);
    LOG_DEVEL << "Found key pos for first: " << keyPosFirst;
    LOG_DEVEL << "Found key pos for current: " << keyPosCurrent;

    if (!indicesOffsets.contains(firstCandidate->id())) {
      IndexOffsets idxOffset{};
      indicesOffsets.try_emplace(firstCandidate->id(), std::move(idxOffset));
    }
    if (!indicesOffsets.contains(currentCandidate->id())) {
      IndexOffsets idxOffset{};
      indicesOffsets.try_emplace(currentCandidate->id(), std::move(idxOffset));
    }
    TRI_ASSERT(indicesOffsets.contains(firstCandidate->id()));
    TRI_ASSERT(indicesOffsets.contains(currentCandidate->id()));
    auto& idxOffsetFirstRef = indicesOffsets[firstCandidate->id()];
    auto& idxOffsetCurrentRef = indicesOffsets[firstCandidate->id()];
    if (idxOffsetFirstRef.insertKeyField(keyPosFirst) &&
        idxOffsetCurrentRef.insertKeyField(keyPosCurrent)) {
      LOG_DEVEL << "=====> Inserted both key Positions: " << keyPosFirst
                << " and " << keyPosCurrent;
      return true;
    }
  }

  return false;
}

bool isVarAccessToCandidateOutVariable(AstNode const* node,
                                       Variable const* outVariable) {
  if (node->type == NODE_TYPE_ATTRIBUTE_ACCESS) {
    TRI_ASSERT(node->numMembers() == 1);
    if (node->getMember(0)->type == NODE_TYPE_REFERENCE) {
      auto iNode = node->getMember(0);
      auto const* other = static_cast<Variable const*>(iNode->getData());
      TRI_ASSERT(other != nullptr);

      if (other->isEqualTo(*outVariable)) {
        return true;
      }
    }
  }
  return false;
}

[[nodiscard]] bool analyseBinaryEqMembers(IndexNode const* currentCandidate,
                                          IndexNode const* firstCandidate,
                                          AstNode const* lhs,
                                          AstNode const* rhs,
                                          IndicesOffsets& indicesOffsets,
                                          VarSet const& knownConstVariables) {
  // Note: This method will always be called twice to guarantee that both
  // directions of the binary eq are checked (lhs == rhs and rhs == lhs).

  // First, check if the variable is the outVariable of our current candidate
  if (isVarAccessToCandidateOutVariable(lhs, currentCandidate->outVariable())) {
    LOG_JOIN_OPTIMIZER_RULE
        << "This candidate (" << currentCandidate->id()
        << ") does make use of it's own outVariable. Condition: ("
        << lhs->toString() << ")";
    // `lhs` is using our candidates outVariable
    // Now we need to check if the other side is a constant condition.
    auto rhsConstantRes = isVariableConstant(rhs, knownConstVariables);
    if (rhsConstantRes) {
      LOG_JOIN_OPTIMIZER_RULE << "  => This condition is a constant condition.";
      // The other side (`rhs`) is a constant condition.
      // We now need to calculate the constant offset for this specific index
      // used here.
      if (processConstantFinding(currentCandidate, indicesOffsets, lhs, rhs)) {
        return true;
      }
    } else {
      LOG_JOIN_OPTIMIZER_RULE
          << "  => This condition is NOT a constant condition.";
      // The other side (`rhs`) is not a constant condition.
      // Therefore, we need to check if the other side is the outVariable of
      // the first candidate. If it is, we've found a join condition and need
      // to adjust the offsets for the candidates accordingly.
      if (firstCandidate == nullptr) {
        // Means our current candidate is the first `candidate[0]`.
        return false;
      }

      if (isVarAccessToCandidateOutVariable(rhs,
                                            firstCandidate->outVariable())) {
        LOG_JOIN_OPTIMIZER_RULE << "I. We've found a join condition for: "
                                << firstCandidate->outVariable()->name;
        // Now we need to first parse the condition, check the used attribute,
        // and then adjust the offsets accordingly.
        if (processJoinKeyFinding(firstCandidate, currentCandidate,
                                  indicesOffsets, rhs)) {
          return true;
        }
      }
      // Otherwise no valid candidate found. We cannot optimize this.
      // Will fall through to the end of the method and return false.
    }
  } else if (firstCandidate != nullptr &&
             isVarAccessToCandidateOutVariable(rhs,
                                               firstCandidate->outVariable())) {
    LOG_JOIN_OPTIMIZER_RULE << "II. We've found a join condition for: "
                            << firstCandidate->outVariable()->name;
    // Now we need to first parse the condition, check the used attribute,
    // and then adjust the offsets accordingly.
    if (processJoinKeyFinding(firstCandidate, currentCandidate, indicesOffsets,
                              rhs)) {
      return true;
    }
    // Otherwise no valid candidate found. We cannot optimize this.
    // Will fall through to the end of the method and return false.
  } else {
    LOG_JOIN_OPTIMIZER_RULE
        << "This candidate (" << currentCandidate->id()
        << ") does not make use of it's own outVariable. Condition: ("
        << lhs->toString() << ")";
  }

  return false;
}

[[nodiscard]] bool analyseBinaryEQ(IndexNode const* currentCandidate,
                                   IndexNode const* firstCandidate,
                                   AstNode const* eqRoot,
                                   IndicesOffsets& indicesOffsets,
                                   VarSet const& knownConstVariables) {
  TRI_ASSERT(eqRoot->numMembers() == 2);
  auto const* leftMember = eqRoot->getMember(0);
  auto const* rightMember = eqRoot->getMember(1);

  LOG_JOIN_OPTIMIZER_RULE << "Analysing binary condition: ("
                          << leftMember->toString() << " - "
                          << rightMember->toString() << ")";

  if (analyseBinaryEqMembers(currentCandidate, firstCandidate, leftMember,
                             rightMember, indicesOffsets,
                             knownConstVariables) ||
      analyseBinaryEqMembers(currentCandidate, firstCandidate, rightMember,
                             leftMember, indicesOffsets, knownConstVariables)) {
    // At least one of the members is constant and can be optimized.
    LOG_JOIN_OPTIMIZER_RULE << "Found at least one constant member which can "
                               "be optimized. Condition: ("
                            << leftMember->toString() << " - "
                            << rightMember->toString() << ")";
    return true;
  }
  return false;
}

[[nodiscard]] bool parseAndCheckNaryOr(
    IndexNode const* indexNode, AstNode const* root,
    std::map<ExecutionNodeId, std::vector<AstNode*>>& constantValues,
    ExecutionNodeId candidateId,
    std::map<ExecutionNodeId,
             std::tuple<std::string, std::vector<size_t>, std::vector<size_t>>>&
        keyFieldsAndConstantFields);

[[nodiscard]] bool parseAndCheckBinaryEQ(
    IndexNode const* indexNode, AstNode const* member,
    std::map<ExecutionNodeId, std::vector<AstNode*>>& constantValues,
    ExecutionNodeId candidateID,
    std::map<ExecutionNodeId,
             std::tuple<std::string, std::vector<size_t>, std::vector<size_t>>>&
        keyAndConstantFields) {
  LOG_JOIN_OPTIMIZER_RULE << "FOUND BINARY_EQ";
  TRI_ASSERT(member->numMembers() == 2);
  member->dump(20);
  auto lhsi = member->getMember(0);
  auto rhsi = member->getMember(1);

  // LATER Improvement TODO: Check "flag" isDeterministic <-- all
  // allowed here then
  if (lhsi->type == NODE_TYPE_VALUE || lhsi->type == NODE_TYPE_REFERENCE) {
    LOG_JOIN_OPTIMIZER_RULE << "Setting constant value to lhsi";
    if (rhsi->type == NODE_TYPE_ATTRIBUTE_ACCESS) {
      constantValues[candidateID].emplace_back(lhsi);
      bool valid =
          createSpanPositionsForIndex(rhsi, keyAndConstantFields, candidateID,
                                      indexNode->getIndexes()[0]->fields());
      LOG_DEVEL << "============================================== 1: "
                << std::boolalpha << valid;
      if (!valid) {
        return false;
      }
    }
  } else if (rhsi->type == NODE_TYPE_VALUE ||
             rhsi->type == NODE_TYPE_REFERENCE) {
    LOG_JOIN_OPTIMIZER_RULE << "Setting constant value to rhsi";
    if (lhsi->type == NODE_TYPE_ATTRIBUTE_ACCESS) {
      constantValues[candidateID].emplace_back(rhsi);
      bool valid =
          createSpanPositionsForIndex(lhsi, keyAndConstantFields, candidateID,
                                      indexNode->getIndexes()[0]->fields());
      LOG_DEVEL << "============================================== 2: "
                << std::boolalpha << valid;
      if (!valid) {
        return false;
      }
    }
  } else if (lhsi->type == NODE_TYPE_REFERENCE &&
             rhsi->type == NODE_TYPE_REFERENCE) {
    // potential join attribute if attr access equal
    // rhsi->

  } else {
    LOG_DEVEL "============================================== 3";
    LOG_DEVEL << "that strange TIMES issues";
    LOG_DEVEL << "Hm it is different here";
  }

  return true;
}

[[nodiscard]] bool parseAndCheckNaryOr(
    IndexNode const* indexNode, AstNode const* root,
    std::map<ExecutionNodeId, std::vector<AstNode*>>& constantValues,
    ExecutionNodeId candidateId,
    std::map<ExecutionNodeId,
             std::tuple<std::string, std::vector<size_t>, std::vector<size_t>>>&
        keyFieldsAndConstantFields) {
  if (root->type == NODE_TYPE_OPERATOR_NARY_OR) {
    LOG_JOIN_OPTIMIZER_RULE << "FOUND NARY_OR";
    TRI_ASSERT(root->numMembers() == 1);
    auto member = root->getMember(0);
    std::vector<bool> eligibles{};

    if (member->type == NODE_TYPE_OPERATOR_NARY_AND) {
      LOG_JOIN_OPTIMIZER_RULE << "FOUND NARY_AND";
      for (size_t innerPos = 0; innerPos < member->numMembers(); innerPos++) {
        auto innerMember = member->getMember(innerPos);
        if (innerMember->type == NODE_TYPE_OPERATOR_BINARY_EQ) {
          eligibles.push_back(
              parseAndCheckBinaryEQ(indexNode, innerMember, constantValues,
                                    candidateId, keyFieldsAndConstantFields));
        } else if (innerMember->type == NODE_TYPE_OPERATOR_NARY_OR) {
          LOG_DEVEL << "naryor checke - TODO: Think this case should NOT be "
                       "supported at all";
          TRI_ASSERT(false);
          eligibles.push_back(parseAndCheckNaryOr(indexNode, innerMember,
                                                  constantValues, candidateId,
                                                  keyFieldsAndConstantFields));
        } else {
          LOG_DEVEL << "some unexpected node here";
          eligibles.push_back(false);
        }
      }
      for (auto const& el : eligibles) {
        if (!el) {
          LOG_DEVEL << "Not all checks of all members are valid";
          return false;
        }
      }
    }
  }
  return true;
}

bool checkCandidateEligibleForAdvancedJoin(
    IndexNode* currentCandidate, IndexNode* firstCandidate,
    AstNode const* candidateConditionRoot, IndicesOffsets& indicesOffsets,
    VarSet const& knownConstVariables) {
  bool eligible = false;

  if (candidateConditionRoot->type == NODE_TYPE_OPERATOR_NARY_OR) {
    LOG_JOIN_OPTIMIZER_RULE << "FOUND NARY_OR";
    TRI_ASSERT(candidateConditionRoot->numMembers() == 1);
    auto member = candidateConditionRoot->getMember(0);
    std::vector<bool> binaryEqEligible{};

    if (member->type == NODE_TYPE_OPERATOR_NARY_AND) {
      LOG_JOIN_OPTIMIZER_RULE << "FOUND NARY_AND";
      for (size_t innerPos = 0; innerPos < member->numMembers(); innerPos++) {
        auto innerMember = member->getMember(innerPos);
        if (innerMember->type == NODE_TYPE_OPERATOR_BINARY_EQ) {
          bool binaryEqValid =
              analyseBinaryEQ(currentCandidate, firstCandidate, innerMember,
                              indicesOffsets, knownConstVariables);
          if (!binaryEqValid) {
            LOG_JOIN_OPTIMIZER_RULE << " => Candidate("
                                    << currentCandidate->id()
                                    << ") is not eligible, because the "
                                       "binary eq condition has not "
                                       "qualified for the index.";
            eligible = false;
            return eligible;
          }
          eligible = true;
        } else {
          LOG_JOIN_OPTIMIZER_RULE << " => Candidate(" << currentCandidate->id()
                                  << ") is not eligible";
          eligible = false;
          return eligible;
        }
      }
    }
  } else if (candidateConditionRoot->type == NODE_TYPE_OPERATOR_BINARY_EQ) {
    bool binaryEqValid = analyseBinaryEQ(currentCandidate, firstCandidate,
                                         candidateConditionRoot, indicesOffsets,
                                         knownConstVariables);
    if (!binaryEqValid) {
      LOG_JOIN_OPTIMIZER_RULE << " => Candidate(" << currentCandidate->id()
                              << ") is not eligible, because the "
                                 "binary eq condition has not "
                                 "qualified for the index.";
      eligible = false;
      return eligible;
    }
    eligible = true;
  }
  LOG_JOIN_OPTIMIZER_RULE
      << "checkCandidateEligibleForAdvancedJoin will return: " << std::boolalpha
      << eligible;

  return eligible;
}

bool checkAndBuildConstantValues(
    IndexNode const* indexNode, AstNode const* root,
    ExecutionNodeId candidateID,
    std::map<ExecutionNodeId, std::vector<AstNode*>>& constantValues,
    std::map<ExecutionNodeId,
             std::tuple<std::string, std::vector<size_t>, std::vector<size_t>>>&
        keyFieldsAndConstantFields) {
  return parseAndCheckNaryOr(indexNode, root, constantValues, candidateID,
                             keyFieldsAndConstantFields);
}

std::pair<bool, IndicesOffsets> checkCandidatesEligible(
    ExecutionPlan& plan,
    containers::SmallVector<IndexNode*, 8> const& candidates) {
  IndicesOffsets indicesOffsets = {};
  VarSet const* knownConstVariables;

  size_t candidatePosition = 0;
  for (auto* candidate : candidates) {
    LOG_JOIN_OPTIMIZER_RULE << "==> Checking candidate: " << candidatePosition
                            << "(" << candidate->id() << ")";
    if (candidatePosition == 0) {
      auto const* root = candidate->condition()->root();
      // auto const& firstCandidateID = candidate->id();
      if (root != nullptr) {
        // Build-up the known variables for the first index node.
        // This operation is only needed for the first index node, because
        // we're only interested in the variables which are known for the
        // first index node including the outVariable of the first index node.
        // Therefore, we call getVarsValid instead of getVariablesUsedHere.
        knownConstVariables = &candidate->getVarsValid();
        TRI_ASSERT(knownConstVariables != nullptr);
        LOG_JOIN_OPTIMIZER_RULE << "Variables which are known: ";
        for (auto kVar : *knownConstVariables) {
          LOG_DEVEL << " -> " << kVar->name;
        }

        LOG_JOIN_OPTIMIZER_RULE
            << "Calling CandidateEligible check for first candidate";
        bool eligible = checkCandidateEligibleForAdvancedJoin(
            candidate, nullptr, root, indicesOffsets, *knownConstVariables);

        if (!eligible) {
          LOG_JOIN_OPTIMIZER_RULE
              << "Not eligible for index join: "
                 "we've found a constant but the positions for key and "
                 "constant fields do not match";
          return {false, {}};
        }
        /*if (constantValues.contains(firstCandidateID) &&
            !constantValues[firstCandidateID].empty()) {
          for (auto const* var : candidate->getVariablesSetHere()) {
            firstCandidateConstantVariables.try_emplace(
                var, candidate->condition()->getConstAttributes(var, false));
          }
        }
        LOG_DEVEL << "=> Amount of elemeents in "
                     "outerCandidateConstantVariables for first index node: "
                  << firstCandidateConstantVariables.size();

        if (firstCandidateConstantVariables.empty()) {
          LOG_JOIN_OPTIMIZER_RULE
              << "Not eligible for index join: "
                 "first index node has a lookup "
                 "condition - and we've not found any constants.";
          return {false, {}};
        }*/
      }
    } else {
      // follow-up FOR loop(s). we expect it to have a lookup condition
      if (candidate->condition()->root() == nullptr) {
        LOG_JOIN_OPTIMIZER_RULE << "Not eligible for index join: "
                                   "index node does not have a "
                                   "lookup condition";
        return {false, {}};
      }
      auto const* root = candidate->condition()->root();
      if (root == nullptr || root->type != NODE_TYPE_OPERATOR_NARY_OR ||
          root->numMembers() != 1) {
        LOG_JOIN_OPTIMIZER_RULE << "I. (NARY_OR) Not eligible for index join: "
                                   "index node's lookup condition "
                                   "does not match";
        return {false, {}};
      }
      root = root->getMember(0);
      if (root == nullptr || root->type != NODE_TYPE_OPERATOR_NARY_AND ||
          root->numMembers() != 1) {
        if (indicesOffsets.empty()) {
          // TODO: Find a better place for this check.
          LOG_JOIN_OPTIMIZER_RULE
              << "II. (NARY_AND) Not eligible for index join: "
                 "index node's lookup condition "
                 "does not match";
          return {false, {}};
        }
      }
      root = root->getMember(0);
      if (root == nullptr || root->type != NODE_TYPE_OPERATOR_BINARY_EQ ||
          root->numMembers() != 2) {
        LOG_JOIN_OPTIMIZER_RULE << "III. (BINARY_EQ) Not eligible for index "
                                   "join: index node's lookup "
                                   "condition does not match";
        return {false, {}};
      }

      if (!indicesOffsets.empty()) {
        TRI_ASSERT(indicesOffsets.contains(candidates[0]->id()));
        // If we've found at least one condition in the first index node, we
        // need to check if the current index node matches the order of
        // conditions and the variables.
        bool eligible = checkCandidateEligibleForAdvancedJoin(
            candidate, &candidate[0], root, indicesOffsets,
            *knownConstVariables);

        if (!eligible) {
          LOG_JOIN_OPTIMIZER_RULE
              << "IndexNode's outer loop has a condition, but inner loop "
                 "does not match, due to "
                 "(checkCandidateEligibleForAdvancedJoin)";
          return {false, {}};
        }
      } else {
        auto* lhs = root->getMember(0);
        auto* rhs = root->getMember(1);
        if (!joinConditionMatches(plan, lhs, rhs, candidate, candidates[0]) &&
            !joinConditionMatches(plan, lhs, rhs, candidates[0], candidate)) {
          LOG_JOIN_OPTIMIZER_RULE << "IndexNode's lookup condition does not "
                                     "match due to (joinConditionMatches)";
          return {false, {}};
        }
      }
    }

    // if there is a post filter, make sure it only accesses variables
    // that are available before all index nodes
    if (candidate->hasFilter()) {
      VarSet vars;
      candidate->filter()->variables(vars);

      for (auto* other : candidates) {
        if (other != candidate && other->setsVariable(vars)) {
          LOG_JOIN_OPTIMIZER_RULE << "IndexNode's post filter "
                                     "accesses variables that are "
                                     "not available before all "
                                     "index nodes";
          return {false, {}};
        }
      }
    }

    // check if filter supports streaming interface
    {
      IndexStreamOptions opts;
      opts.usedKeyFields = {0};  // for now only 0 is supported
      if (candidate->projections().usesCoveringIndex()) {
        opts.projectedFields.reserve(candidate->projections().size());
        auto& proj = candidate->projections().projections();
        std::transform(proj.begin(), proj.end(),
                       std::back_inserter(opts.projectedFields),
                       [](auto const& p) { return p.coveringIndexPosition; });
      }
      if (!candidate->getIndexes()[0]->supportsStreamInterface(opts)) {
        LOG_JOIN_OPTIMIZER_RULE << "IndexNode's index does not "
                                   "support streaming interface";
        LOG_JOIN_OPTIMIZER_RULE
            << "-> Index name: " << candidate->getIndexes()[0]->name()
            << ", id: " << candidate->getIndexes()[0]->id();
        return {false, {}};
      }
    }

    // check if index does support constant values
    /*{
      if (!constantValues.empty()) {
        size_t amountOfFields = candidate->getIndexes()[0]->fields().size();
        if (amountOfFields == 1 || constantValues.size() >= amountOfFields) {
          constantValues.clear();
          continue;
        }
        IndexStreamOptions opts;
        opts.usedKeyFields = {0};  // for now only 0 is supported
        std::vector<size_t> constantFieldsForVerification{};
        // TODO: Adjust this whole area.
        for (auto const& cV : constantValues) {
          for (auto const& constants : constantValues[cV.first]) {
            std::ignore = constants;
            constantFieldsForVerification.push_back(
                {0});  // just used as a dummy here for verification
          }
        }
        if (!candidate->getIndexes()[0]->supportsStreamInterface(opts)) {
          LOG_JOIN_OPTIMIZER_RULE << "IndexNode's index does not "
                                     "support constant values";
          return {false, {}};
        }
      }
    }*/

    ++candidatePosition;
  }

  if (!indicesOffsets.empty()) {
    LOG_DEVEL << "==> Final Result: Can be optimized with constants!";
    return {true, std::move(indicesOffsets)};
  }
  LOG_DEVEL << "==> Final Result: Can be optimized!";
  return {true, {}};
}

void findCandidates(IndexNode* indexNode,
                    containers::SmallVector<IndexNode*, 8>& candidates,
                    containers::FlatHashSet<ExecutionNode const*>& handled) {
  containers::SmallVector<CalculationNode*, 8> calculations;

  while (true) {
    if (handled.contains(indexNode) || !indexNodeQualifies(*indexNode)) {
      break;
    }
    candidates.emplace_back(indexNode);
    auto* parent = indexNode->getFirstParent();
    while (true) {
      if (parent == nullptr) {
        return;
      } else if (parent->getType() == EN::CALCULATION) {
        // store this calculation and check later if and index depends on
        // it
        auto calc = ExecutionNode::castTo<CalculationNode*>(parent);
        calculations.push_back(calc);
        parent = parent->getFirstParent();
        continue;
      } else if (parent->getType() == EN::INDEX) {
        // check that this index node does not depend on previous
        // calculations

        indexNode = ExecutionNode::castTo<IndexNode*>(parent);
        VarSet usedVariables;
        indexNode->getVariablesUsedHere(usedVariables);
        for (auto* calc : calculations) {
          if (calc->setsVariable(usedVariables)) {
            // can not join past this calculation
            return;
          }
        }
        break;
      } else {
        return;
      }
    }
  }
}

Projections translateLMIndexVarsToProjections(
    ExecutionPlan* plan, IndexNode::IndexValuesVars const& indexVars,
    transaction::Methods::IndexHandle index) {
  // Translate the late materialize "projections" description
  // into the usual projections description
  auto& coveredFields = index->coveredFields();

  std::vector<AttributeNamePath> projectedAttributes;
  for (auto [var, fieldIndex] : indexVars.second) {
    auto& field = coveredFields[fieldIndex];
    std::vector<std::string> fieldCopy;
    fieldCopy.reserve(field.size());
    std::transform(field.begin(), field.end(), std::back_inserter(fieldCopy),
                   [&](auto const& attr) {
                     TRI_ASSERT(attr.shouldExpand == false);
                     return attr.name;
                   });
    projectedAttributes.emplace_back(std::move(fieldCopy),
                                     plan->getAst()->query().resourceMonitor());
  }

  Projections projections{std::move(projectedAttributes)};

  std::size_t i = 0;
  for (auto [var, fieldIndex] : indexVars.second) {
    auto& proj = projections[i++];
    proj.coveringIndexPosition = fieldIndex;
    proj.coveringIndexCutoff = fieldIndex;
    proj.variable = var;
    proj.levelsToClose = proj.startsAtLevel = 0;
    proj.type = proj.path.size() > 1 ? AttributeNamePath::Type::MultiAttribute
                                     : AttributeNamePath::Type::SingleAttribute;
  }

  projections.setCoveringContext(index->collection().id(), index);
  return projections;
}

void arangodb::aql::joinIndexNodesRule(Optimizer* opt,
                                       std::unique_ptr<ExecutionPlan> plan,
                                       OptimizerRule const& rule) {
  containers::SmallVector<ExecutionNode*, 8> nodes;
  plan->findNodesOfType(nodes, EN::INDEX, true);

  LOG_JOIN_OPTIMIZER_RULE << "Checking if we can join index nodes";

  bool modified = false;
  if (nodes.size() >= 2) {
    // IndexNodes we already handled
    containers::FlatHashSet<ExecutionNode const*> handled;

    // for each IndexNode we found in the found, iterate over a potential
    // chain of IndexNodes, from top to bottom
    for (auto* n : nodes) {
      auto* startNode = ExecutionNode::castTo<IndexNode*>(n);
      // go to the first IndexNode in the chain, so we can start at the
      // top.
      while (true) {
        auto* dep = startNode->getFirstDependency();
        if (dep == nullptr || dep->getType() != EN::INDEX) {
          break;
        }
        startNode = ExecutionNode::castTo<IndexNode*>(dep);
      }

      while (true) {
        containers::SmallVector<IndexNode*, 8> candidates;
        findCandidates(startNode, candidates, handled);

        if (candidates.size() >= 2) {
          LOG_JOIN_OPTIMIZER_RULE << "Found " << candidates.size()
                                  << " index nodes that qualify for joining";

          // First: Eligible bool
          // Second: Vector of constant expressions
          // Third: Vector of positions of keyFields and constantFields
          auto candidatesResult = checkCandidatesEligible(*plan, candidates);
          bool eligible = candidatesResult.first;
          auto indicesOffsets = candidatesResult.second;

          if (eligible) {
            // we will now replace all candidate IndexNodes with a JoinNode,
            // and remove the previous IndexNodes from the plan
            LOG_JOIN_OPTIMIZER_RULE << "Should be eligible for index join";
            std::vector<JoinNode::IndexInfo> indexInfos;
            indexInfos.reserve(candidates.size());

            size_t removeMe = 0;
            for (auto* c : candidates) {
              std::vector<std::unique_ptr<Expression>> constExpressions{};
              std::vector<size_t> computedUseKeyFields{};
              std::vector<size_t> computedConstantFields{};

              if (indicesOffsets.contains(c->id())) {
                auto const& idxOffset = indicesOffsets[c->id()];
                if (idxOffset.hasConstantValues()) {
                  for (auto const& constantValue :
                       idxOffset.getConstantValues()) {
                    auto* constExpr = constantValue->clone(plan->getAst());
                    auto e =
                        std::make_unique<Expression>(plan->getAst(), constExpr);
                    constExpressions.push_back(std::move(e));
                  }
                }

                computedUseKeyFields = std::move(idxOffset.getKeyFields());
                computedConstantFields =
                    std::move(idxOffset.getConstantFields());

                LOG_DEVEL << "DEBUG LOG FOR CANDIDATE: " << c->id();
                LOG_DEVEL << "Keys:";
                for (auto x : computedUseKeyFields) {
                  LOG_DEVEL << "{" << x << "}";
                }
                LOG_DEVEL << "Constants:";
                for (auto x : computedConstantFields) {
                  LOG_DEVEL << "{" << x << "}";
                }

              } else {
                // if no constants have been found, we'll stick to the
                // defaults.
                LOG_DEVEL << "No constants found for candidate: " << removeMe;
                computedUseKeyFields = {0};
                computedConstantFields = {};
              }

              auto info = JoinNode::IndexInfo{
                  .collection = c->collection(),
                  .outVariable = c->outVariable(),
                  .condition = c->condition()->clone(),
                  .filter = c->hasFilter() ? c->filter()->clone(plan->getAst())
                                           : nullptr,
                  .index = c->getIndexes()[0],
                  .projections = c->projections(),
                  .filterProjections = c->filterProjections(),
                  .usedAsSatellite = c->isUsedAsSatellite(),
                  .producesOutput = c->isProduceResult(),
                  .expressions = std::move(constExpressions),
                  .usedKeyFields = computedUseKeyFields,
                  .constantFields = computedConstantFields};

              if (c->isLateMaterialized()) {
                TRI_ASSERT(c->projections().empty());
                info.isLateMaterialized = true;

                auto [outVar, indexVars] = c->getLateMaterializedInfo();
                TRI_ASSERT(indexVars.first == c->getIndexes()[0]->id());
                info.outDocIdVariable = outVar;
                info.projections = translateLMIndexVarsToProjections(
                    plan.get(), indexVars, c->getIndexes()[0]);
              }

              indexInfos.emplace_back(std::move(info));
              handled.emplace(c);
            }
            JoinNode* jn = plan->createNode<JoinNode>(
                plan.get(), plan->nextId(), std::move(indexInfos),
                IndexIteratorOptions{});
            // Nodes we jumped over (like calculations) are left in place
            // and are now below the Join Node
            plan->replaceNode(candidates[0], jn);
            for (size_t i = 1; i < candidates.size(); ++i) {
              plan->unlinkNode(candidates[i]);
            }

            // do some post insertion optimizations
            optimizeJoinNode(*plan, jn);
            modified = true;
          } else {
            LOG_JOIN_OPTIMIZER_RULE << "Not eligible for index join due to: "
                                       "(checkCandidatesEligible)";
          }
        } else {
          LOG_JOIN_OPTIMIZER_RULE << "Not enough index nodes to join, size: "
                                  << candidates.size();
        }

        // try starting from next start node
        auto* parent = startNode->getFirstParent();
        if (parent == nullptr || parent->getType() != EN::INDEX) {
          break;
        }
        startNode = ExecutionNode::castTo<IndexNode*>(parent);
      }
    }
  }

  opt->addPlan(std::move(plan), rule, modified);
}