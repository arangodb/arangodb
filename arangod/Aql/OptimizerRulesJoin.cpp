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

enum IndexUsage { LEFT, RIGHT, NONE };

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

bool constIndexConditionsMatches(
    IndexNode const* firstNode, IndexNode const* otherNode,
    std::map<Variable const*,
             std::vector<std::vector<arangodb::basics::AttributeName>>>
        constants) {
  bool eligible = true;

  auto buildOrderedIndexAttributes =
      [](IndexNode const* indexNode,
         std::map<Variable const*,
                  std::vector<std::vector<arangodb::basics::AttributeName>>>&
             constants) {
        std::vector<std::string_view> usedIndexFields;
        std::map<std::string_view, bool> orderedIndexAttributes;

        for (auto const& element : constants) {
          // TODO: We can get rid of this whole for-loop iteration later
          // probably.
          for (auto const& outerVector : element.second) {
            for (auto const& innerVector : outerVector) {
              usedIndexFields.push_back(innerVector.name);
            }
          }
        }
        for (auto const& field : indexNode->getIndexes()[0]->fields()) {
          // TODO: I think the field[0] access needs to be fixed here.
          // TODO: This is just a dummy-incomplete implementation right now.
          // TODO: This covers e.g. "x" but not "x.y" represented as
          // [...["x", "y"]...]
          auto it = std::find(usedIndexFields.begin(), usedIndexFields.end(),
                              field[0].name);
          if (it != usedIndexFields.end()) {
            orderedIndexAttributes.try_emplace(field[0].name, true);
            LOG_JOIN_OPTIMIZER_RULE << "-> " << field[0].name
                                    << ", value: true";
          } else {
            orderedIndexAttributes.try_emplace(field[0].name, false);
            LOG_JOIN_OPTIMIZER_RULE << "-> " << field[0].name
                                    << ", value: false";
          }
        }

        return orderedIndexAttributes;
      };

  LOG_JOIN_OPTIMIZER_RULE << "Index information for first node:";
  auto orderedIndexAttributesLHS =
      buildOrderedIndexAttributes(firstNode, constants);

  auto checkVarCoveredByIndex = [&](Variable const* var) {
    LOG_JOIN_OPTIMIZER_RULE << "Checking index valid for var: " << var->name;
    if (var->isEqualTo(*firstNode->outVariable())) {
      LOG_JOIN_OPTIMIZER_RULE << "-> Var fits to first node's index";
      return IndexUsage::LEFT;
    } else if (var->isEqualTo(*otherNode->outVariable())) {
      LOG_JOIN_OPTIMIZER_RULE << "-> Var fits to other node's index";
      return IndexUsage::RIGHT;
    }

    LOG_JOIN_OPTIMIZER_RULE
        << "Variable is not related to any index, we cannot optimize.";
    return IndexUsage::NONE;
  };

  auto checkVariableEligible =
      [&](Variable const* var,
          std::vector<basics::AttributeName> const& attrs) -> bool {
    LOG_JOIN_OPTIMIZER_RULE << "Calling checkVariableEligible: ";
    // 1.) Check if the variable is related to the index of the first node
    // or to the index of the second node.
    auto result = checkVarCoveredByIndex(var);

    if (result == IndexUsage::LEFT) {
      for (auto const& elem : orderedIndexAttributesLHS) {
        if (elem.first != var->name && !elem.second) {
          // We've found another attribute here, which is not used and does not
          // match our current variable to check. Therefore, we need to stop
          // optimizing here.
          LOG_JOIN_OPTIMIZER_RULE << "Variable: " << var->name
                                  << " is not eligible!";
          return false;
        } else {
          // Match found between variable and index attribute. We can continue.
          break;
        }
      }
      LOG_JOIN_OPTIMIZER_RULE << "Variable: " << var->name << " is eligible";
    } else if (result == IndexUsage::RIGHT) {
      LOG_JOIN_OPTIMIZER_RULE << "Variable: " << var->name << " is eligible";
      return true;
    } else {
      TRI_ASSERT(result == IndexUsage::NONE);
      LOG_JOIN_OPTIMIZER_RULE << "Variable: " << var->name
                              << " is not eligible, due to no IndexUsage.";
      return false;
    }

    return true;
  };

  auto* otherRoot = otherNode->condition()->root();
  if (otherRoot->type == NODE_TYPE_OPERATOR_NARY_OR) {
    auto member = otherRoot->getMember(0);
    TRI_ASSERT(member->numMembers() == 1);
    if (member->type == NODE_TYPE_OPERATOR_NARY_AND) {
      member = member->getMember(0);
      size_t numMembers = member->numMembers();

      if (member->type == NODE_TYPE_OPERATOR_BINARY_EQ) {
        TRI_ASSERT(numMembers == 2);

        auto lhsi = member->getMember(0);
        auto rhsi = member->getMember(1);

        std::pair<Variable const*, std::vector<basics::AttributeName>> lParts;
        std::pair<Variable const*, std::vector<basics::AttributeName>> rParts;

        if (lhsi->isAttributeAccessForVariable(lParts)) {
          LOG_JOIN_OPTIMIZER_RULE << "attr name: " << lParts.first->name;
          for (auto const& attr : lParts.second) {
            LOG_JOIN_OPTIMIZER_RULE << " -> inner attr: " << attr.name;
          }
          eligible = checkVariableEligible(lParts.first, lParts.second);
          if (!eligible) {
            return false;
          }
        }

        if (rhsi->isAttributeAccessForVariable(rParts)) {
          LOG_JOIN_OPTIMIZER_RULE << "attr name: " << rParts.first->name;
          for (auto const& attr : rParts.second) {
            LOG_JOIN_OPTIMIZER_RULE << " -> inner attr: " << attr.name;
          }
          eligible = checkVariableEligible(rParts.first, rParts.second);
          if (!eligible) {
            return false;
          }
        }
      }
    }
  }

  LOG_JOIN_OPTIMIZER_RULE << "################## END ###############";
  return eligible;
}

std::pair<bool, std::map<ExecutionNodeId, std::vector<AstNode*>>>
checkCandidatesEligible(
    ExecutionPlan& plan,
    containers::SmallVector<IndexNode*, 8> const& candidates) {
  // This flag is set to true if the first index node has a lookup condition
  // and all other indexes do match the order of conditions and the variables
  std::map<ExecutionNodeId, std::vector<AstNode*>> constantValues;
  std::map<ExecutionNodeId,
           std::map<Variable const*,
                    std::vector<std::vector<arangodb::basics::AttributeName>>>>
      outerCandidateConstantVariables = {};

  size_t candidatePosition = 0;
  for (auto* candidate : candidates) {
    if (candidatePosition == 0) {
      // first FOR loop. I may have a lookup condition. In case it has, it needs
      // further checks later if the lookup conditions do match with the
      // upcoming FOR loops.

      auto const* root = candidate->condition()->root();
      if (root != nullptr) {
        // TODO: candidate->getVarsValid() + "doc1 case"
        if (root->hasFlag(AstNodeFlagType::DETERMINED_CONSTANT)) {
          if (root->type == NODE_TYPE_OPERATOR_NARY_OR) {
            LOG_JOIN_OPTIMIZER_RULE << "FOUND NARY_OR";
            TRI_ASSERT(root->numMembers() == 1);
            auto member = root->getMember(0);
            if (member->type == NODE_TYPE_OPERATOR_NARY_AND) {
              LOG_JOIN_OPTIMIZER_RULE << "FOUND NARY_AND";
              TRI_ASSERT(member->numMembers() == 1);
              member = member->getMember(0);
              if (member->type == NODE_TYPE_OPERATOR_BINARY_EQ) {
                LOG_JOIN_OPTIMIZER_RULE << "FOUND BINARY_EQ";
                TRI_ASSERT(member->numMembers() == 2);

                auto lhsi = member->getMember(0);
                auto rhsi = member->getMember(1);
                // LATER Improvement TODO: Check "flag" isDeterministic <-- all
                // allowed here then
                if (lhsi->type == NODE_TYPE_VALUE) {
                  LOG_JOIN_OPTIMIZER_RULE << "Setting constant value to lhsi";
                  constantValues[candidate->id()].emplace_back(lhsi);
                } else if (rhsi->type == NODE_TYPE_VALUE) {
                  LOG_JOIN_OPTIMIZER_RULE << "Setting constant value to rhsi";
                  constantValues[candidate->id()].emplace_back(rhsi);
                }
              }
            }
          }
          if (constantValues.contains(candidate->id()) &&
              !constantValues[candidate->id()].empty()) {
            for (auto const* var : candidate->getVariablesSetHere()) {
              outerCandidateConstantVariables[candidate->id()].try_emplace(
                  var, candidate->condition()->getConstAttributes(var, false));
            }
          }
        } else {
          LOG_JOIN_OPTIMIZER_RULE << "Not eligible for index join: "
                                     "first index node has a lookup "
                                     "condition";
          return {false, {}};
        }
      }
    } else {
      // follow-up FOR loop. we expect it to have a lookup condition
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
        LOG_JOIN_OPTIMIZER_RULE
            << "II. (NARY_AND) Not eligible for index join: "
               "index node's lookup condition "
               "does not match";
        return {false, {}};
      }
      root = root->getMember(0);
      if (root == nullptr || root->type != NODE_TYPE_OPERATOR_BINARY_EQ ||
          root->numMembers() != 2) {
        LOG_JOIN_OPTIMIZER_RULE << "III. (BINARY_EQ) Not eligible for index "
                                   "join: index node's lookup "
                                   "condition does not match";
        return {false, {}};
      }

      if (!outerCandidateConstantVariables.empty()) {
        // TODO: Check if this condition above is actually correct.
        bool eligible = constIndexConditionsMatches(
            candidates[0], candidate,
            outerCandidateConstantVariables[candidates[0]->id()]);
        if (!eligible) {
          LOG_JOIN_OPTIMIZER_RULE
              << "IndexNode's outer loop has a condition, but inner loop "
                 "does not match, due to (constIndexConditionsMatches)";
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
    {
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
    }

    ++candidatePosition;
  }

  return {true, constantValues};
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
          LOG_JOIN_OPTIMIZER_RULE << "Found " << candidates.size()
                                  << " index nodes that qualify for joining";

          // First: Eligible bool, Second: Vector of constant expressions
          std::pair<bool, std::map<ExecutionNodeId, std::vector<AstNode*>>>
              candidatesResult = checkCandidatesEligible(*plan, candidates);
          bool eligible = candidatesResult.first;
          std::map<ExecutionNodeId, std::vector<AstNode*>>
              constExpressionNodes = std::move(candidatesResult.second);

          if (eligible) {
            // we will now replace all candidate IndexNodes with a JoinNode,
            // and remove the previous IndexNodes from the plan
            LOG_JOIN_OPTIMIZER_RULE << "Should be eligible for index join";
            std::vector<JoinNode::IndexInfo> indexInfos;
            indexInfos.reserve(candidates.size());

            for (auto* c : candidates) {
              std::vector<std::unique_ptr<Expression>> constExpressions{};
              std::vector<size_t> computedUseKeyFields{};
              std::vector<size_t> computedConstantFields{};

              if (!constExpressionNodes[c->id()].empty()) {
                for (auto const& constExprNode :
                     constExpressionNodes[c->id()]) {
                  auto* constExpr = constExprNode->clone(plan->getAst());
                  auto e =
                      std::make_unique<Expression>(plan->getAst(), constExpr);
                  constExpressions.push_back(std::move(e));
                }
                TRI_ASSERT(constExpressions.size() == 1);
                // TODO: Only one expression is supported right now
                // TODO: THIS NEEDS TO BE FIXED ASAP
                computedUseKeyFields = {1};
                computedConstantFields = {0};
              } else {
                computedUseKeyFields = {0};
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