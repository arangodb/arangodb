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

#include "ApplicationFeatures/ApplicationServer.h"
#include "Aql/Ast.h"
#include "Aql/AstHelper.h"
#include "Aql/AttributeNamePath.h"
#include "Aql/Collection.h"
#include "Aql/Condition.h"
#include "Aql/ConditionFinder.h"
#include "Aql/ExecutionEngine.h"
#include "Aql/ExecutionNode/CalculationNode.h"
#include "Aql/ExecutionNode/DocumentProducingNode.h"
#include "Aql/ExecutionNode/ExecutionNode.h"
#include "Aql/ExecutionNode/IndexNode.h"
#include "Aql/ExecutionNode/JoinNode.h"
#include "Aql/ExecutionPlan.h"
#include "Aql/Expression.h"
#include "Aql/IndexStreamIterator.h"
#include "Aql/Optimizer.h"
#include "Aql/OptimizerRules.h"
#include "Aql/OptimizerUtils.h"
#include "Aql/Projections.h"
#include "Aql/Query.h"
#include "Aql/Variable.h"
#include "Aql/types.h"
#include "Basics/AttributeNameParser.h"
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
#include "Utils/CollectionNameResolver.h"
#include "VocBase/LogicalCollection.h"
#include "VocBase/Methods/Collections.h"

#include <span>
#include <tuple>

using namespace arangodb;
using namespace arangodb::aql;
using namespace arangodb::containers;
using EN = arangodb::aql::ExecutionNode;

#define LOG_JOIN_OPTIMIZER_RULE LOG_DEVEL_IF(false)
#define LOG_JOIN_OPTIMIZER_RULE_CONSTANTS LOG_DEVEL_IF(false)
#define LOG_JOIN_OPTIMIZER_RULE_KEYS LOG_DEVEL_IF(false)
#define LOG_JOIN_OPTIMIZER_RULE_OFFSETS LOG_DEVEL_IF(false)
#define LOG_JOIN_OPTIMIZER_FIND_CAN LOG_DEVEL_IF(false)

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
      return true;
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

[[nodiscard]] bool isVariableConstant(AstNode const* node,
                                      VarSet const* knownConstVariables,
                                      bool isVarAccessToOthersSideOutVariable) {
  TRI_ASSERT(node != nullptr);
  LOG_JOIN_OPTIMIZER_RULE << "Checking if condition is constant ("
                          << node->toString() << ")";

  if (isVarAccessToOthersSideOutVariable) {
    // Only constant if we're sure that both (lhs + rhs) are not used outVars
    // of our first candidate.
    return false;
  }

  // TODO:  Quickly explain this section
  VarSet result;
  Ast::getReferencedVariables(node, result);

  if (result.empty()) {
    return true;
  }

  // Print names of the known constant variables
  LOG_JOIN_OPTIMIZER_RULE << "Known constant variables:";
  if (knownConstVariables != nullptr) {
    for (auto const& var : *knownConstVariables) {
      LOG_JOIN_OPTIMIZER_RULE << "  - " << var->name;
    }
    for (auto const& var : result) {
      if (!knownConstVariables->contains(var)) {
        return false;
      }
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
  LOG_JOIN_OPTIMIZER_RULE_KEYS << "Attributes: ";
  for (auto const& d : resultVector) {
    LOG_JOIN_OPTIMIZER_RULE_KEYS << "- " << d.name;
  }
  auto usedIndex = candidate->getIndexes()[0];
  return usedIndex->attributeMatchesWithPos(resultVector,
                                            usedIndex->id().isPrimary());
}

bool processConstantFinding(IndexNode const* currentCandidate,
                            IndicesOffsets& indicesOffsets,
                            AstNode const* constant,
                            AstNode const* constantValue) {
  TRI_ASSERT(constant->type == NODE_TYPE_ATTRIBUTE_ACCESS);
  LOG_JOIN_OPTIMIZER_RULE_CONSTANTS << "Calling processConstantFinding";

  std::vector<basics::AttributeName> resultVector;
  getIndexAttributes(currentCandidate, constant->getStringView(), resultVector);
  auto attributeMatchResult =
      doIndexAttributesMatch(currentCandidate, resultVector);

  if (attributeMatchResult.first) {
    // match found
    size_t constantPos = attributeMatchResult.second;
    if (!indicesOffsets.contains(currentCandidate->id())) {
      IndexOffsets idxOffset{};
      indicesOffsets.try_emplace(currentCandidate->id(), std::move(idxOffset));
    }

    TRI_ASSERT(indicesOffsets.contains(currentCandidate->id()));
    LOG_JOIN_OPTIMIZER_RULE_CONSTANTS << "Inserting constant of: ("
                                      << constantPos
                                      << ") to CID: " << currentCandidate->id();
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
                           IndicesOffsets& indicesOffsets, AstNode const* lhs,
                           AstNode const* rhs) {
  LOG_JOIN_OPTIMIZER_RULE_KEYS << "Calling processJoinKeyFinding based on ("
                               << lhs->toString() << ") and ("
                               << rhs->toString() << ")";
  std::vector<basics::AttributeName> resultVectorFirst;
  std::vector<basics::AttributeName> resultVectorCurrent;
  getIndexAttributes(firstCandidate, lhs->getStringView(), resultVectorFirst);
  getIndexAttributes(currentCandidate, rhs->getStringView(),
                     resultVectorCurrent);

  LOG_JOIN_OPTIMIZER_RULE_KEYS << "Get first index information";
  auto attributeMatchResultFirst =
      doIndexAttributesMatch(firstCandidate, resultVectorFirst);
  LOG_JOIN_OPTIMIZER_RULE_KEYS << "Get current index information";
  auto attributeMatchResultCurrent =
      doIndexAttributesMatch(currentCandidate, resultVectorCurrent);

  if (attributeMatchResultFirst.first && attributeMatchResultCurrent.first) {
    // match found
    auto keyPosFirst = attributeMatchResultFirst.second;
    auto keyPosCurrent = attributeMatchResultCurrent.second;
    LOG_JOIN_OPTIMIZER_RULE_KEYS << "Pos for first: " << keyPosFirst;
    LOG_JOIN_OPTIMIZER_RULE_KEYS << "Pos for current: " << keyPosCurrent;

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
    auto& idxOffsetCurrentRef = indicesOffsets[currentCandidate->id()];
    LOG_JOIN_OPTIMIZER_RULE_KEYS << "Inserting first of (" << keyPosFirst
                                 << ")";
    bool first = idxOffsetFirstRef.insertKeyField(keyPosFirst);
    LOG_JOIN_OPTIMIZER_RULE_KEYS << "Inserting current of (" << keyPosCurrent
                                 << ")";
    bool current = idxOffsetCurrentRef.insertKeyField(keyPosCurrent);
    if (first && current) {
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
                                          VarSet const* knownConstVariables) {
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

    bool isVarAccessToOthersSideOutVariable = false;
    if (firstCandidate != nullptr) {
      isVarAccessToOthersSideOutVariable =
          isVarAccessToCandidateOutVariable(rhs, firstCandidate->outVariable());
    }
    auto isConstantRes = isVariableConstant(rhs, knownConstVariables,
                                            isVarAccessToOthersSideOutVariable);
    if (isConstantRes) {
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
        LOG_JOIN_OPTIMIZER_RULE << "First is nullptr. Does not qualify : ("
                                << lhs->toString() << " - " << rhs->toString()
                                << ")";
        return false;
      }

      if (isVarAccessToCandidateOutVariable(rhs,
                                            firstCandidate->outVariable())) {
        LOG_JOIN_OPTIMIZER_RULE << "I. We've found a join condition for: "
                                << firstCandidate->outVariable()->name;
        // Now we need to first parse the condition, check the used attribute,
        // and then adjust the offsets accordingly.
        if (processJoinKeyFinding(firstCandidate, currentCandidate,
                                  indicesOffsets, rhs, lhs)) {
          return true;
        }
      }
      // Otherwise no valid candidate found. We cannot optimize this.
      // Will fall through to the end of the method and return false.
    }
  } else if (firstCandidate != nullptr &&
             isVarAccessToCandidateOutVariable(lhs,
                                               firstCandidate->outVariable())) {
    LOG_JOIN_OPTIMIZER_RULE << "II. We've found a join condition for: "
                            << firstCandidate->outVariable()->name;
    // Now we need to first parse the condition, check the used attribute,
    // and then adjust the offsets accordingly.
    if (processJoinKeyFinding(firstCandidate, currentCandidate, indicesOffsets,
                              lhs, rhs)) {
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

  LOG_JOIN_OPTIMIZER_RULE << "Does not qualify : (" << lhs->toString() << " - "
                          << rhs->toString() << ")";
  return false;
}

[[nodiscard]] bool analyseBinaryEQ(IndexNode const* currentCandidate,
                                   IndexNode const* firstCandidate,
                                   AstNode const* eqRoot,
                                   IndicesOffsets& indicesOffsets,
                                   VarSet const* knownConstVariables) {
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
    LOG_JOIN_OPTIMIZER_RULE
        << "Found at least one constant member or join which can "
           "be optimized. Condition: ("
        << leftMember->toString() << " - " << rightMember->toString() << ")";
    return true;
  }
  return false;
}

bool analyseNodeTypeOperatorNaryAnd(IndexNode const* currentCandidate,
                                    IndexNode const* firstCandidate,
                                    AstNode const* member,
                                    IndicesOffsets& indicesOffsets,
                                    VarSet const* knownConstVariables) {
  TRI_ASSERT(member->type == NODE_TYPE_OPERATOR_NARY_AND);
  LOG_JOIN_OPTIMIZER_RULE << "FOUND: NODE_TYPE_OPERATOR_NARY_AND";
  bool eligible = false;

  for (size_t innerPos = 0; innerPos < member->numMembers(); innerPos++) {
    auto innerMember = member->getMember(innerPos);
    if (innerMember->type == NODE_TYPE_OPERATOR_BINARY_EQ) {
      bool binaryEqValid =
          analyseBinaryEQ(currentCandidate, firstCandidate, innerMember,
                          indicesOffsets, knownConstVariables);
      if (!binaryEqValid) {
        LOG_JOIN_OPTIMIZER_RULE << " => Candidate(" << currentCandidate->id()
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

  return eligible;
}

bool checkCandidateEligibleForAdvancedJoin(
    IndexNode* currentCandidate, IndexNode* firstCandidate,
    AstNode const* candidateConditionRoot, IndicesOffsets& indicesOffsets,
    VarSet const* knownConstVariables) {
  bool eligible = false;

  if (candidateConditionRoot->type == NODE_TYPE_OPERATOR_NARY_OR) {
    LOG_JOIN_OPTIMIZER_RULE << "FOUND: NODE_TYPE_OPERATOR_NARY_OR";
    TRI_ASSERT(candidateConditionRoot->numMembers() == 1);
    auto member = candidateConditionRoot->getMember(0);

    if (member->type == NODE_TYPE_OPERATOR_NARY_AND) {
      eligible = analyseNodeTypeOperatorNaryAnd(
          currentCandidate, firstCandidate, member, indicesOffsets,
          knownConstVariables);
    }
  } else if (candidateConditionRoot->type == NODE_TYPE_OPERATOR_NARY_AND) {
    eligible = analyseNodeTypeOperatorNaryAnd(
        currentCandidate, firstCandidate, candidateConditionRoot,
        indicesOffsets, knownConstVariables);
  } else if (candidateConditionRoot->type == NODE_TYPE_OPERATOR_BINARY_EQ) {
    LOG_JOIN_OPTIMIZER_RULE << "FOUND: NODE_TYPE_OPERATOR_BINARY_EQ";
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
  } else {
    LOG_JOIN_OPTIMIZER_RULE << "FOUND: something else: ";
  }
  LOG_JOIN_OPTIMIZER_RULE
      << "checkCandidateEligibleForAdvancedJoin will return: " << std::boolalpha
      << eligible;

  return eligible;
}

std::tuple<bool, IndicesOffsets> checkCandidatesEligible(
    ExecutionPlan& plan, std::span<IndexNode*> candidates) {
  IndicesOffsets indicesOffsets = {};
  VarSet const* knownConstVariables = nullptr;

  for (auto* candidate : candidates) {
    LOG_JOIN_OPTIMIZER_RULE << "==> Checking candidate: (" << candidate->id()
                            << ")";
    if (candidate == candidates.front()) {
      auto const* root = candidate->condition()->root();

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
          LOG_JOIN_OPTIMIZER_RULE << " -> " << kVar->name;
        }

        LOG_JOIN_OPTIMIZER_RULE
            << "Calling CandidateEligible check for first candidate";
        bool eligible = checkCandidateEligibleForAdvancedJoin(
            candidate, nullptr, root, indicesOffsets, knownConstVariables);

        if (!eligible) {
          LOG_JOIN_OPTIMIZER_RULE
              << "Not eligible for index join: "
                 "we've found a constant but the positions for key and "
                 "constant fields do not match";
          return {false, {}};
        }
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
      LOG_JOIN_OPTIMIZER_RULE
          << "Calling CandidateEligible check for other candidate ("
          << candidate->id() << ")";
      bool eligible = checkCandidateEligibleForAdvancedJoin(
          candidate, candidates[0], candidate->condition()->root(),
          indicesOffsets, knownConstVariables);

      if (!eligible) {
        LOG_JOIN_OPTIMIZER_RULE
            << "IndexNode's outer loop has a condition, but inner loop "
               "does not match, due to "
               "(checkCandidateEligibleForAdvancedJoin)";
        return {false, {}};
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
  }

  if (!indicesOffsets.empty()) {
    // Indices offsets are fully computed after all candidates have been
    // processed. Therefore, we cannot use the upper supportsStreamInterface
    // check and need to re-check here after all candidates have been processed.
    LOG_JOIN_OPTIMIZER_RULE_OFFSETS << "Indices Offsets Debug Output, Size: "
                                    << indicesOffsets.size();

    bool allCandidatesSupportStreamInterface = true;
    for (auto const& [candidateId, indexOffset] : indicesOffsets) {
      // if constants found, we need to use the computed offsets
      auto& idxOffsetRef = indexOffset;
      LOG_JOIN_OPTIMIZER_RULE_OFFSETS << "==> Candidate: " << candidateId
                                      << " <==";
      IndexStreamOptions opts;
      opts.usedKeyFields = idxOffsetRef.getKeyFields();
      LOG_JOIN_OPTIMIZER_RULE_OFFSETS << "Keys";
      if (opts.usedKeyFields.empty()) {
        LOG_JOIN_OPTIMIZER_RULE << "-> { EMPTY }";
      }
      for (auto const& kk : opts.usedKeyFields) {
        LOG_JOIN_OPTIMIZER_RULE_OFFSETS << "-> {" << kk << "}";
      }
      opts.constantFields = idxOffsetRef.getConstantFields();
      LOG_JOIN_OPTIMIZER_RULE_OFFSETS << "Constants";
      for (auto const& oo : opts.constantFields) {
        LOG_JOIN_OPTIMIZER_RULE_OFFSETS << "-> {" << oo << "}";
      }
      if (opts.constantFields.empty()) {
        LOG_JOIN_OPTIMIZER_RULE << "-> { EMPTY }";
      }

      for (auto cIndexNode : candidates) {
        if (cIndexNode->id() == candidateId) {
          if (!cIndexNode->getIndexes()[0]->supportsStreamInterface(opts)) {
            allCandidatesSupportStreamInterface = false;
            LOG_JOIN_OPTIMIZER_RULE << "IndexNode's index does not "
                                       "support streaming interface";
            LOG_JOIN_OPTIMIZER_RULE
                << "-> Index name: " << cIndexNode->getIndexes()[0]->name()
                << ", id: " << cIndexNode->getIndexes()[0]->id();
          }
        }
      }
    }

    if (!allCandidatesSupportStreamInterface) {
      return {false, {}};
    }

    LOG_JOIN_OPTIMIZER_RULE
        << "=> Final Result: Can be optimized with constants!";
    return {true, std::move(indicesOffsets)};
  }
  LOG_JOIN_OPTIMIZER_RULE << "=> Final Result: Can be optimized!";
  return {true, {}};
}

std::tuple<bool, IndicesOffsets, containers::SmallVector<IndexNode*, 8>>
checkCandidatePermutationsEligible(ExecutionPlan& plan,
                                   std::span<IndexNode*> candidates) {
  if (candidates.size() == 2) {
    return std::tuple_cat(
        checkCandidatesEligible(plan, candidates),
        std::make_tuple(containers::SmallVector<IndexNode*, 8>{
            candidates.begin(), candidates.end()}));
  } else if (candidates.size() == 3) {
    {
      auto [eligible, indexes] = checkCandidatesEligible(plan, candidates);
      if (eligible) {
        return std::make_tuple(true, std::move(indexes),
                               containers::SmallVector<IndexNode*, 8>{
                                   candidates.begin(), candidates.end()});
      }
    }

    // test all 2 subsets
    {
      std::array<IndexNode*, 2> selected = {candidates[0], candidates[1]};
      auto [eligible, indexes] = checkCandidatesEligible(plan, selected);
      if (eligible) {
        return std::make_tuple(true, std::move(indexes),
                               containers::SmallVector<IndexNode*, 8>{
                                   selected.begin(), selected.end()});
      }
    }
    {
      std::array<IndexNode*, 2> selected = {candidates[0], candidates[2]};
      auto [eligible, indexes] = checkCandidatesEligible(plan, selected);
      if (eligible) {
        return std::make_tuple(true, std::move(indexes),
                               containers::SmallVector<IndexNode*, 8>{
                                   selected.begin(), selected.end()});
      }
    }
  } else if (candidates.size() == 4) {
    {
      auto [eligible, indexes] = checkCandidatesEligible(plan, candidates);
      if (eligible) {
        return std::make_tuple(true, std::move(indexes),
                               containers::SmallVector<IndexNode*, 8>{
                                   candidates.begin(), candidates.end()});
      }
    }

    // test all 3 subsets
    {
      std::array<IndexNode*, 3> selected = {candidates[0], candidates[1],
                                            candidates[2]};
      auto [eligible, indexes] = checkCandidatesEligible(plan, selected);
      if (eligible) {
        return std::make_tuple(true, std::move(indexes),
                               containers::SmallVector<IndexNode*, 8>{
                                   selected.begin(), selected.end()});
      }
    }
    {
      std::array<IndexNode*, 3> selected = {candidates[0], candidates[2],
                                            candidates[3]};
      auto [eligible, indexes] = checkCandidatesEligible(plan, selected);
      if (eligible) {
        return std::make_tuple(true, std::move(indexes),
                               containers::SmallVector<IndexNode*, 8>{
                                   selected.begin(), selected.end()});
      }
    }
    {
      std::array<IndexNode*, 3> selected = {candidates[0], candidates[1],
                                            candidates[3]};
      auto [eligible, indexes] = checkCandidatesEligible(plan, selected);
      if (eligible) {
        return std::make_tuple(true, std::move(indexes),
                               containers::SmallVector<IndexNode*, 8>{
                                   selected.begin(), selected.end()});
      }
    }

    // test all 2 subsets
    {
      std::array<IndexNode*, 2> selected = {candidates[0], candidates[1]};
      auto [eligible, indexes] = checkCandidatesEligible(plan, selected);
      if (eligible) {
        return std::make_tuple(true, std::move(indexes),
                               containers::SmallVector<IndexNode*, 8>{
                                   selected.begin(), selected.end()});
      }
    }
    {
      std::array<IndexNode*, 2> selected = {candidates[0], candidates[2]};
      auto [eligible, indexes] = checkCandidatesEligible(plan, selected);
      if (eligible) {
        return std::make_tuple(true, std::move(indexes),
                               containers::SmallVector<IndexNode*, 8>{
                                   selected.begin(), selected.end()});
      }
    }
    {
      std::array<IndexNode*, 2> selected = {candidates[0], candidates[3]};
      auto [eligible, indexes] = checkCandidatesEligible(plan, selected);
      if (eligible) {
        return std::make_tuple(true, std::move(indexes),
                               containers::SmallVector<IndexNode*, 8>{
                                   selected.begin(), selected.end()});
      }
    }
  }

  return {false, {}, {}};
}

void findCandidates(IndexNode* indexNode,
                    containers::SmallVector<IndexNode*, 8>& candidates,
                    containers::FlatHashSet<ExecutionNode const*>& handled) {
  containers::SmallVector<CalculationNode*, 8> calculations;

  auto dependsOnPrevCalc = [&](ExecutionNode* node) {
    VarSet usedVariables;
    node->getVariablesUsedHere(usedVariables);
    return std::any_of(
        calculations.begin(), calculations.end(),
        [&](auto const& calc) { return calc->setsVariable(usedVariables); });
  };

  while (true) {
    LOG_JOIN_OPTIMIZER_FIND_CAN << "AT " << indexNode->id() << " "
                                << indexNode->getTypeString();
    if (handled.contains(indexNode)) {
      break;
    }
    // it is ok to ignore some index nodes if they don't qualify
    // Enumerations always commute.
    if (indexNodeQualifies(*indexNode)) {
      LOG_JOIN_OPTIMIZER_FIND_CAN << "QUALIFIED";
      candidates.emplace_back(indexNode);
    } else {
      LOG_JOIN_OPTIMIZER_FIND_CAN << "IGNORED";
    }
    auto* parent = indexNode->getFirstParent();
    while (true) {
      if (parent == nullptr) {
        return;
      } else if (parent->getType() == EN::CALCULATION) {
        // store this calculation and check later if and index depends on
        // it
        auto calc = ExecutionNode::castTo<CalculationNode*>(parent);
        calculations.push_back(calc);

        LOG_JOIN_OPTIMIZER_FIND_CAN << "FOUND CALCULATION ";
        parent = parent->getFirstParent();
        continue;
      } else if (parent->getType() == EN::MATERIALIZE) {
        // we can always move past materialize nodes
        parent = parent->getFirstParent();
        LOG_JOIN_OPTIMIZER_FIND_CAN << "FOUND MATERIALIZE";
        continue;
      } else if (parent->getType() == EN::ENUMERATE_COLLECTION) {
        // we can move past enumerate collections if their filters
        // do not depend on any calculations
        if (dependsOnPrevCalc(parent)) {
          LOG_JOIN_OPTIMIZER_FIND_CAN << "HAS DEPENDENT VAR";
          return;
        }

        LOG_JOIN_OPTIMIZER_FIND_CAN << "JOINING PAST EC";
        parent = parent->getFirstParent();
        continue;
      } else if (parent->getType() == EN::INDEX) {
        // check that this index node does not depend on previous
        // calculations
        if (dependsOnPrevCalc(parent)) {
          LOG_JOIN_OPTIMIZER_FIND_CAN << "HAS DEPENDENT VAR";
          return;
        }

        LOG_JOIN_OPTIMIZER_FIND_CAN << "JOINING PAST INDEX";
        indexNode = ExecutionNode::castTo<IndexNode*>(parent);
        break;
      } else {
        LOG_JOIN_OPTIMIZER_FIND_CAN << "UNKNOWN NODE exit";
        return;
      }
    }
  }
}

}  // namespace
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
          LOG_JOIN_OPTIMIZER_RULE << "========================================="
                                     "=================================";
          LOG_JOIN_OPTIMIZER_RULE << "Found " << candidates.size()
                                  << " index nodes that qualify for joining";

          auto [eligible, indicesOffsets, selectedCandidates] =
              checkCandidatePermutationsEligible(*plan, candidates);
          if (eligible) {
            // we will now replace all candidate IndexNodes with a JoinNode,
            // and remove the previous IndexNodes from the plan
            LOG_JOIN_OPTIMIZER_RULE << "Should be eligible for index join";
            std::vector<JoinNode::IndexInfo> indexInfos;
            indexInfos.reserve(selectedCandidates.size());
            TRI_ASSERT(selectedCandidates.size() >= 2);
            TRI_ASSERT(selectedCandidates[0] == candidates[0]);

            for (auto* c : selectedCandidates) {
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
              } else {
                // if no constants have been found, we'll stick to the
                // defaults.
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
                info.isLateMaterialized = true;

                info.outDocIdVariable = c->getLateMaterializedDocIdOutVar();
                info.projections = c->projections();
              }

              indexInfos.emplace_back(std::move(info));
              handled.emplace(c);
            }
            JoinNode* jn = plan->createNode<JoinNode>(
                plan.get(), plan->nextId(), std::move(indexInfos),
                IndexIteratorOptions{});
            // Nodes we jumped over (like calculations) are left in place
            // and are now below the Join Node
            plan->replaceNode(selectedCandidates[0], jn);
            for (size_t i = 1; i < selectedCandidates.size(); ++i) {
              plan->unlinkNode(selectedCandidates[i]);
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
