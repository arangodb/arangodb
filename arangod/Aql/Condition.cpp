////////////////////////////////////////////////////////////////////////////////
/// @brief Aql, condition 
///
/// @file
///
/// DISCLAIMER
///
/// Copyright 2014 ArangoDB GmbH, Cologne, Germany
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
///
/// @author Jan Steemann
/// @author Copyright 2014, ArangoDB GmbH, Cologne, Germany
/// @author Copyright 2012-2013, triAGENS GmbH, Cologne, Germany
////////////////////////////////////////////////////////////////////////////////

#include "Condition.h"
#include "Aql/Ast.h"
#include "Aql/AstNode.h"
#include "Aql/ExecutionPlan.h"
#include "Aql/Variable.h"
#include "Basics/Exceptions.h"
#include "Basics/json.h"
#include "Basics/JsonHelper.h"

#include <iostream>

using namespace triagens::aql;
using CompareResult = ConditionPart::ConditionPartCompareResult;
        
static double const MaxSortCost   = 2.0;

struct PermutationState {
  PermutationState (triagens::aql::AstNode const* value, size_t n)
    : value(value),
      current(0),
      n(n) {
  }
    
  triagens::aql::AstNode const* getValue () const {
    if (value->type == triagens::aql::NODE_TYPE_OPERATOR_BINARY_AND || 
        value->type == triagens::aql::NODE_TYPE_OPERATOR_BINARY_OR || 
        value->type == triagens::aql::NODE_TYPE_OPERATOR_NARY_AND || 
        value->type == triagens::aql::NODE_TYPE_OPERATOR_NARY_OR) { 
      TRI_ASSERT(current < n);
      return value->getMember(current);
    }

    TRI_ASSERT(current == 0);
    return value;
  }

  triagens::aql::AstNode const* value;
  size_t                        current;
  size_t const                  n;
};

// -----------------------------------------------------------------------------
// --SECTION--                                              struct ConditionPart
// -----------------------------------------------------------------------------

ConditionPart::ConditionPart (Variable const* variable,
                              std::string const& attributeName,
                              size_t sourcePosition,
                              AstNode const* operatorNode,
                              AttributeSideType side)
  : variable(variable),
    attributeName(attributeName),
    sourcePosition(sourcePosition),
    operatorType(operatorNode->type),
    operatorNode(operatorNode),
    valueNode(nullptr) {

  if (side == ATTRIBUTE_LEFT) {
    valueNode = operatorNode->getMember(1);
  }
  else {
    valueNode = operatorNode->getMember(0);
    if (Ast::IsReversibleOperator(operatorType)) {
      operatorType = Ast::ReverseOperator(operatorType);
    }
  }
}

ConditionPart::~ConditionPart () {

}

#if 0
void ConditionPart::dump () const {
  std::cout << "VARIABLE NAME: " << variable->name << "." << attributeName << " " << triagens::basics::JsonHelper::toString(valueNode->toJson(TRI_UNKNOWN_MEM_ZONE, false)) << "\n";
}
#endif

// -----------------------------------------------------------------------------
// --SECTION--                                                   class Condition
// -----------------------------------------------------------------------------

// -----------------------------------------------------------------------------
// --SECTION--                                            static helper function
// -----------------------------------------------------------------------------

#if 0
static void dumpNode (AstNode const* node, int indent) {
  if (node == nullptr) {
    return;
  }

  for (int i = 0; i < indent * 2; ++i) {
    std::cout << " ";
  }

  std::cout << node->getTypeString();
  if (node->type == NODE_TYPE_VALUE) {
    TRI_json_t* jsonVal =  node->toJsonValue(TRI_UNKNOWN_MEM_ZONE);
    std::cout << "  (value " << triagens::basics::JsonHelper::toString(jsonVal) << ")";
    TRI_FreeJson(TRI_UNKNOWN_MEM_ZONE, jsonVal);
  }
  else if (node->type == NODE_TYPE_ATTRIBUTE_ACCESS) {
    std::cout << "  (attribute " << node->getStringValue() << ")";
  }
  else if (node->type == NODE_TYPE_REFERENCE) {
    std::cout << "  (variable name " << static_cast<Variable const*>(node->getData())->name << ")";
  }

  std::cout << "\n";
  for (size_t i = 0; i < node->numMembers(); ++i) {
    dumpNode(node->getMember(i), indent + 1);
  }
}
#endif

// -----------------------------------------------------------------------------
// --SECTION--                                        constructors / destructors
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @brief create the condition
////////////////////////////////////////////////////////////////////////////////

Condition::Condition (Ast* ast)
  : _ast(ast),
    _root(nullptr),
    _isNormalized(false) {

}

////////////////////////////////////////////////////////////////////////////////
/// @brief destroy the condition
////////////////////////////////////////////////////////////////////////////////

Condition::~Condition () {
  // memory for nodes is not owned and thus not freed by the condition
  // all nodes belong to the AST
}

// -----------------------------------------------------------------------------
// --SECTION--                                                  public functions
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @brief create a condition from JSON
////////////////////////////////////////////////////////////////////////////////
        
Condition* Condition::fromJson (ExecutionPlan* plan,
                                triagens::basics::Json const& json) {
  std::unique_ptr<Condition> condition(new Condition(plan->getAst()));

  std::unique_ptr<AstNode> node(new AstNode(plan->getAst(), json)); 
  condition->andCombine(node.get());
  node.release();

  condition->_isNormalized = true;
  //condition->normalize(plan);

  return condition.release();
}

////////////////////////////////////////////////////////////////////////////////
/// @brief clone the condition
////////////////////////////////////////////////////////////////////////////////

Condition* Condition::clone () const {
  std::unique_ptr<Condition> copy(new Condition(_ast));

  if (_root != nullptr) {
    copy->_root = _root->clone(_ast); 
  }

  copy->_isNormalized = _isNormalized;

  return copy.release();
}

////////////////////////////////////////////////////////////////////////////////
/// @brief add a sub-condition to the condition
/// the sub-condition will be AND-combined with the existing condition(s)
////////////////////////////////////////////////////////////////////////////////

void Condition::andCombine (AstNode const* node) {
  if (_isNormalized) {
    // already normalized
    THROW_ARANGO_EXCEPTION_MESSAGE(TRI_ERROR_INTERNAL, "cannot and-combine normalized condition");
  }

  if (_root == nullptr) {
    // condition was empty before
    _root = _ast->clone(node);
  }
  else {
    // condition was not empty before, now AND-merge
    _root = _ast->createNodeBinaryOperator(NODE_TYPE_OPERATOR_BINARY_AND, _root, _ast->clone(node));
  }

  TRI_ASSERT(_root != nullptr);
}

////////////////////////////////////////////////////////////////////////////////
/// @brief locate indexes for each condition
////////////////////////////////////////////////////////////////////////////////

bool Condition::findIndexes (EnumerateCollectionNode const* node, 
                             std::vector<Index const*>& usedIndexes,
                             SortCondition const& sortCondition) {
  TRI_ASSERT(usedIndexes.empty());
  Variable const* reference = node->outVariable();
  if (_root == nullptr) {
    // We do not have a condition. But we have a sort
    if (! sortCondition.isEmpty() &&
        sortCondition.isOnlyAttributeAccess() &&
        sortCondition.isUnidirectional()) {
      double bestCost = MaxSortCost;
      Index const* bestIndex = nullptr;
      std::vector<Index const*> indexes = node->collection()->getIndexes();
      for (auto const& idx : indexes) {
        double sortCost = 0.0;
        if (indexSupportsSort(idx, reference, sortCondition, sortCost) &&
            sortCost < bestCost) {
          bestCost = sortCost;
          bestIndex = idx;
        }
      }
      if (bestIndex != nullptr) {
        usedIndexes.emplace_back(bestIndex);
      }
    }
    else {
      // No Index and no sort condition that
      // can be supported by an index.
      // Nothing to do here.
      return false;
    }
  }
  else {
    // We can only start after DNF transformation
    TRI_ASSERT(_root->type == NODE_TYPE_OPERATOR_NARY_OR);


    for (size_t i = 0; i < _root->numMembers(); ++i) {
      if (! findIndexForAndNode(i, reference, node, usedIndexes, sortCondition)) {
        // We are not able to find an index for this AND block. Sorry we have to abort here
        return false;
      }
    }
  }

  // should always be true here. maybe not in the future in case a collection
  // has absolutely no indexes
  return ! usedIndexes.empty();
}

bool Condition::indexSupportsSort (Index const* idx,
                                   Variable const* reference,
                                   SortCondition const& sortCondition,
                                   double& estimatedCost) {
  if (idx->isSorted() &&
      idx->supportsSortCondition(&sortCondition, reference, estimatedCost)) {
    // index supports the sort condition
    return true;
  }
  else {
    // index does not support the sort condition
    estimatedCost = MaxSortCost;
  }
  return false;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief finds the best index that can match this single node
////////////////////////////////////////////////////////////////////////////////

bool Condition::findIndexForAndNode (size_t position,
                                     Variable const* reference, 
                                     EnumerateCollectionNode const* colNode, 
                                     std::vector<Index const*>& usedIndexes,
                                     SortCondition const& sortCondition) {
  // We can only iterate through a proper DNF
  auto node = _root->getMember(position);
  TRI_ASSERT(node->type == NODE_TYPE_OPERATOR_NARY_AND);
  
  static double const MaxFilterCost = 2.0;

  // This code is never responsible for the content of this pointer.
  Index const* bestIndex = nullptr;
  double bestCost = MaxFilterCost + MaxSortCost + std::numeric_limits<double>::epsilon(); 

  std::vector<Index const*> indexes = colNode->collection()->getIndexes();

  for (auto const& idx : indexes) {
    double filterCost = 0.0;
    double sortCost   = 0.0;

    bool supportsFilter = false;
    bool supportsSort   = false;
    
    // check if the index supports the filter expression
    double estimatedCost;
    if (idx->supportsFilterCondition(node, reference, estimatedCost)) {
      // index supports the filter condition
      filterCost = estimatedCost;
      supportsFilter = true;
    }
    else {
      // index does not support the filter condition
      filterCost = MaxFilterCost;
    }

    if (! sortCondition.isEmpty() &&
        sortCondition.isOnlyAttributeAccess() &&
        sortCondition.isUnidirectional()) {
      // only go in here if we actually have a sort condition and it can in
      // general be supported by an index. for this, a sort condition must not
      // be empty, must consist only of attribute access, and all attributes
      // must be sorted in the direction
      if (indexSupportsSort(idx, reference, sortCondition, sortCost)) {
        supportsSort = true;
      }
    }

    if (! supportsFilter && ! supportsSort) {
      continue;
    }

    // std::cout << "INDEX: " << triagens::basics::JsonHelper::toString(idx->getInternals()->toJson(TRI_UNKNOWN_MEM_ZONE, false).json()) << ", FILTER COST: " << filterCost << ", SORT COST: " << sortCost << "\n";
    double const totalCost = filterCost + sortCost;

    if (totalCost < bestCost) {
      bestIndex = idx;
      bestCost = totalCost;
    }
  }

  if (bestIndex == nullptr) {
    return false;
  }

  _root->changeMember(position, bestIndex->specializeCondition(node, reference)); 

  usedIndexes.emplace_back(bestIndex);

  return true;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief normalize the condition
/// this will convert the condition into its disjunctive normal form
////////////////////////////////////////////////////////////////////////////////

void Condition::normalize (ExecutionPlan* plan) {
  if (_isNormalized) {
    // already normalized
    return;
  }

  _root = transformNode(_root);
  _root = collapseNesting(_root);
  _root = fixRoot(_root, 0);

  optimize(plan);

#if 0
  std::cout << "\n";
  dump();
  std::cout << "\n";
  _isNormalized = true;
#endif  
}

////////////////////////////////////////////////////////////////////////////////
/// @brief optimize the condition's expression tree
////////////////////////////////////////////////////////////////////////////////

//        |         | a == y | a != y | a <  y | a <= y | a >= y | a > y  
// -------|------------------|--------|--------|--------|--------|--------
// x  < y |         |   IMP  |   OIS  |   OIS  |  OIS   |   IMP  |  IMP   
// x == y |  a == x |   OIS  |   IMP  |   IMP  |  OIS   |   OIS  |  IMP   
// x  > y |         |   IMP  |   OIS  |   IMP  |  IMP   |   OIS  |  OIS   
// -------|------------------|--------|--------|--------|--------|--------
// x  < y |         |   SIO  |   DIJ  |   DIJ  |  DIJ   |   SIO  |  SIO   
// x == y |  a != x |   IMP  |   OIS  |   SIO  |  DIJ   |   DIJ  |  SIO   
// x  > y |         |   SIO  |   DIJ  |   SIO  |  SIO   |   DIJ  |  DIJ   
// -------|------------------|--------|--------|--------|--------|--------
// x  < y |         |   IMP  |   OIS  |   OIS  |  OIS   |   IMP  |  IMP   
// x == y |  a <  x |   IMP  |   OIS  |   OIS  |  OIS   |   IMP  |  IMP   
// x  > y |         |   SIO  |   DIJ  |   SIO  |  SIO   |   DIJ  |  DIJ   
// -------|------------------|--------|--------|--------|--------|--------
// x  < y |         |   IMP  |   OIS  |   OIS  |  OIS   |   IMP  |  IMP   
// x == y |  a <= x |   SIO  |   DIJ  |   SIO  |  OIS   |   CEQ  |  IMP   
// x  > y |         |   SIO  |   DIJ  |   SIO  |  SIO   |   DIJ  |  DIJ   
// -------|------------------|--------|--------|--------|--------|--------
// x  < y |         |   SIO  |   DIJ  |   DIJ  |  DIJ   |   SIO  |  SIO   
// x == y |  a >= x |   SIO  |   DIJ  |   IMP  |  CEQ   |   OIS  |  SIO   
// x  > y |         |   IMP  |   OIS  |   IMP  |  IMP   |   OIS  |  OIS   
// -------|------------------|--------|--------|--------|--------|--------
// x  < y |         |   SIO  |   DIJ  |   DIJ  |   DIJ  |   SIO  |  SIO   
// x == y |  a >  x |   IMP  |   OIS  |   IMP  |   IMP  |   OIS  |  OIS   
// x  > y |         |   IMP  |   OIS  |   IMP  |   IMP  |   OIS  |  OIS   
// the 7th column is here as fallback if the operation is not in the table above.
// IMP -> IMPOSSIBLE -> empty result -> the complete AND set of conditions can be dropped.
// CEQ -> CONVERT_EQUAL -> both conditions can be combined to a equals x.
// DIJ -> DISJOINT -> neither condition is a consequence of the other -> both have to stay in place.
// SIO -> SELF_CONTAINED_IN_OTHER -> the left condition is a consequence of the right condition
// OIS -> OTHER_CONTAINED_IN_SELF -> the right condition is a consequence of the left condition
// If a condition (A) is a consequence of another (B), the solution set of A is larger than that of B
//  -> A can be dropped.

ConditionPart::ConditionPartCompareResult const ConditionPart::ResultsTable[3][7][7] = {
  { // X < Y
    {IMPOSSIBLE, OTHER_CONTAINED_IN_SELF, OTHER_CONTAINED_IN_SELF, OTHER_CONTAINED_IN_SELF, IMPOSSIBLE, IMPOSSIBLE, DISJOINT},
    {SELF_CONTAINED_IN_OTHER, DISJOINT, DISJOINT, DISJOINT, SELF_CONTAINED_IN_OTHER, SELF_CONTAINED_IN_OTHER, DISJOINT},
    {IMPOSSIBLE, OTHER_CONTAINED_IN_SELF, OTHER_CONTAINED_IN_SELF, OTHER_CONTAINED_IN_SELF, IMPOSSIBLE, IMPOSSIBLE, DISJOINT},
    {IMPOSSIBLE, OTHER_CONTAINED_IN_SELF, OTHER_CONTAINED_IN_SELF, OTHER_CONTAINED_IN_SELF, IMPOSSIBLE, IMPOSSIBLE, DISJOINT},
    {SELF_CONTAINED_IN_OTHER, DISJOINT, DISJOINT, DISJOINT, SELF_CONTAINED_IN_OTHER, SELF_CONTAINED_IN_OTHER, DISJOINT},
    {SELF_CONTAINED_IN_OTHER, DISJOINT, DISJOINT, DISJOINT, SELF_CONTAINED_IN_OTHER, SELF_CONTAINED_IN_OTHER, DISJOINT},
    {DISJOINT, DISJOINT, DISJOINT, DISJOINT, DISJOINT, DISJOINT, DISJOINT}
  },
  { // X == Y
    {OTHER_CONTAINED_IN_SELF, IMPOSSIBLE, IMPOSSIBLE, OTHER_CONTAINED_IN_SELF, OTHER_CONTAINED_IN_SELF, IMPOSSIBLE, DISJOINT},
    {IMPOSSIBLE, OTHER_CONTAINED_IN_SELF, SELF_CONTAINED_IN_OTHER, DISJOINT, DISJOINT, SELF_CONTAINED_IN_OTHER, DISJOINT},
    {IMPOSSIBLE, OTHER_CONTAINED_IN_SELF, OTHER_CONTAINED_IN_SELF, OTHER_CONTAINED_IN_SELF, IMPOSSIBLE, IMPOSSIBLE, DISJOINT},
    {SELF_CONTAINED_IN_OTHER, DISJOINT, SELF_CONTAINED_IN_OTHER, OTHER_CONTAINED_IN_SELF, CONVERT_EQUAL, IMPOSSIBLE, DISJOINT},
    {SELF_CONTAINED_IN_OTHER, DISJOINT, IMPOSSIBLE, CONVERT_EQUAL, OTHER_CONTAINED_IN_SELF, SELF_CONTAINED_IN_OTHER, DISJOINT},
    {IMPOSSIBLE, OTHER_CONTAINED_IN_SELF, IMPOSSIBLE, IMPOSSIBLE, OTHER_CONTAINED_IN_SELF, OTHER_CONTAINED_IN_SELF, DISJOINT},
    {DISJOINT, DISJOINT, DISJOINT, DISJOINT, DISJOINT, DISJOINT, DISJOINT}
  },
  { // X > Y
    {IMPOSSIBLE, OTHER_CONTAINED_IN_SELF, IMPOSSIBLE, IMPOSSIBLE, OTHER_CONTAINED_IN_SELF, OTHER_CONTAINED_IN_SELF, DISJOINT},
    {SELF_CONTAINED_IN_OTHER, DISJOINT, SELF_CONTAINED_IN_OTHER, SELF_CONTAINED_IN_OTHER, DISJOINT, DISJOINT, DISJOINT},
    {SELF_CONTAINED_IN_OTHER, DISJOINT, SELF_CONTAINED_IN_OTHER, SELF_CONTAINED_IN_OTHER, DISJOINT, DISJOINT, DISJOINT},
    {SELF_CONTAINED_IN_OTHER, DISJOINT, SELF_CONTAINED_IN_OTHER, SELF_CONTAINED_IN_OTHER, DISJOINT, DISJOINT, DISJOINT},
    {IMPOSSIBLE, OTHER_CONTAINED_IN_SELF, IMPOSSIBLE, IMPOSSIBLE, OTHER_CONTAINED_IN_SELF, OTHER_CONTAINED_IN_SELF, DISJOINT},
    {IMPOSSIBLE, OTHER_CONTAINED_IN_SELF, IMPOSSIBLE, IMPOSSIBLE, OTHER_CONTAINED_IN_SELF, OTHER_CONTAINED_IN_SELF, DISJOINT},
    {DISJOINT, DISJOINT, DISJOINT, DISJOINT, DISJOINT, DISJOINT, DISJOINT}
  }
};

////////////////////////////////////////////////////////////////////////////////
/// @brief registers an attribute access for a particular (collection) variable
////////////////////////////////////////////////////////////////////////////////

void Condition::storeAttributeAccess (VariableUsageType& variableUsage, 
                                      AstNode const* node, 
                                      size_t position, 
                                      AttributeSideType side) {

  std::pair<Variable const*, std::vector<triagens::basics::AttributeName>> result;
          
  if (! node->isAttributeAccessForVariable(result)) {
    return;
  }
  auto variable = result.first;

  if (variable != nullptr) {
    auto it = variableUsage.find(variable);

    if (it == variableUsage.end()) {
      // nothing recorded yet for variable
      it = variableUsage.emplace(variable, AttributeUsageType()).first;
    }

    std::string attributeName;
    TRI_AttributeNamesToString(result.second, attributeName, false);

    auto it2 = (*it).second.find(attributeName);
        
    if (it2 == (*it).second.end()) {
      // nothing recorded yet for attribute name in this variable
      it2 = (*it).second.emplace(attributeName, UsagePositionType()).first;
    }
          
    auto& dst = (*it2).second;

    if (! dst.empty() && dst.back().first == position) {
      // already have this attribute for this variable. can happen in case a condition refers to itself (e.g. a.x == a.x)
      // in this case, we won't optimize it
      dst.erase(dst.begin() + dst.size() - 1);
    }
    else {
      dst.emplace_back(position, side);
    }
  }
}

////////////////////////////////////////////////////////////////////////////////
/// @brief optimize the condition expression tree
////////////////////////////////////////////////////////////////////////////////

void Condition::optimize (ExecutionPlan* plan) {
  if (_root == nullptr) {
    return;
  } 

  TRI_ASSERT(_root != nullptr);
  TRI_ASSERT(_root->type == NODE_TYPE_OPERATOR_NARY_OR);

  // handle sub nodes or top-level OR node
  size_t n = _root->numMembers();
  size_t r = 0;

  while (r < n) { // foreach OR-Node
    bool retry = false;
    auto andNode = _root->getMemberUnchecked(r);
    TRI_ASSERT(andNode->type == NODE_TYPE_OPERATOR_NARY_AND);

restartThisOrItem:
    size_t const andNumMembers = andNode->numMembers();
  
    // deduplicate all IN arrays
    for (size_t j = 0; j < andNumMembers; ++j) {
      deduplicateInOperation(andNode->getMemberUnchecked(j));
    }

    if (andNumMembers <= 1) {
      // simple AND item with 0 or 1 members. nothing to do
      ++r;
      continue;
    }

    TRI_ASSERT(andNumMembers > 1);

    // move IN operation to the front to make comparison code below simpler
    andNode->sortMembers([] (AstNode const* lhs, AstNode const* rhs) -> bool {
      if (lhs->type == NODE_TYPE_OPERATOR_BINARY_IN) {
        if (rhs->type != NODE_TYPE_OPERATOR_BINARY_IN) {
          return true; // IN < other
        }
        // fallthrough 
      }
      else if (rhs->type == NODE_TYPE_OPERATOR_BINARY_IN) {
        return false; // other > IN
      }
      return (lhs < rhs); // compare pointers to have a deterministic order
    });

    // optimization is only necessary if an AND node has multiple members
    VariableUsageType variableUsage;
 
    for (size_t j = 0; j < andNumMembers; ++j) {
      auto operand = andNode->getMemberUnchecked(j);

      if (operand->isComparisonOperator()) {
        auto lhs = operand->getMember(0);
        auto rhs = operand->getMember(1);

        if (lhs->type == NODE_TYPE_ATTRIBUTE_ACCESS) {
          storeAttributeAccess(variableUsage, lhs, j, ATTRIBUTE_LEFT);
        }
        if (rhs->type == NODE_TYPE_ATTRIBUTE_ACCESS ||
            rhs->type == NODE_TYPE_EXPANSION) {
          storeAttributeAccess(variableUsage, rhs, j, ATTRIBUTE_RIGHT);
        }
      }
    }

    // now find the variables and attributes for which there are multiple conditions
    for (auto const& it : variableUsage) { // foreach sub-and-node
      auto variable = it.first;

      for (auto const& it2 : it.second) { // cross compare sub-and-nodes
        auto const& attributeName = it2.first;
        auto const& positions = it2.second;

        if (positions.size() <= 1) {
          // none or only one occurence of the attribute
          continue;
        }

        // multiple occurrences of the same attribute
        auto leftNode = andNode->getMemberUnchecked(positions[0].first);

        ConditionPart current(variable, attributeName, 0, leftNode, positions[0].second);

        if (! current.valueNode->isConstant()) {
          continue;
        }

        // current.dump();
        size_t j = 1;

        while (j < positions.size()) {
          auto rightNode = andNode->getMemberUnchecked(positions[j].first);

          ConditionPart other(variable, attributeName, j, rightNode, positions[j].second);

          if (! other.valueNode->isConstant()) {
            ++j;
            continue;
          }

          // IN-merging 
          if (leftNode->type == NODE_TYPE_OPERATOR_BINARY_IN &&
              leftNode->getMemberUnchecked(1)->isConstant()) {
            TRI_ASSERT(leftNode->numMembers() == 2);

            if (rightNode->type == NODE_TYPE_OPERATOR_BINARY_IN &&
                rightNode->getMemberUnchecked(1)->isConstant()) {
              // merge IN with IN on same attribute
              TRI_ASSERT(rightNode->numMembers() == 2);

              auto merged = _ast->createNodeBinaryOperator(
                  NODE_TYPE_OPERATOR_BINARY_IN, 
                  leftNode->getMemberUnchecked(0), 
                  mergeInOperations(leftNode, rightNode)
                  );
              andNode->removeMemberUnchecked(positions[j].first);
              andNode->changeMember(positions[0].first, merged);
              goto restartThisOrItem;
            }
            else if (rightNode->isSimpleComparisonOperator()) {
              // merge other comparison operator with IN
              TRI_ASSERT(rightNode->numMembers() == 2);

              auto inNode = _ast->createNodeArray();
              auto values = leftNode->getMemberUnchecked(1);

              for (size_t k = 0; k < values->numMembers(); ++k) {
                auto value = values->getMemberUnchecked(k);
                ConditionPart::ConditionPartCompareResult res = ConditionPart::ResultsTable
                  [CompareAstNodes(value, other.valueNode, false) + 1]
                  [0 /*NODE_TYPE_OPERATOR_BINARY_EQ*/]
                  [other.whichCompareOperation()];

                bool const keep = (res == CompareResult::OTHER_CONTAINED_IN_SELF || res == CompareResult::CONVERT_EQUAL);
                if (keep) {
                  inNode->addMember(value);
                }
              }

              if (inNode->numMembers() == 0) {
                // no values left after merging -> IMPOSSIBLE
                _root->removeMemberUnchecked(r);
                retry = true;
                goto fastForwardToNextOrItem;
              }

              // use the new array of values
              andNode->getMemberUnchecked(positions[0].first)->changeMember(1, inNode);
              // remove the other operator
              andNode->removeMemberUnchecked(positions[j].first);
              goto restartThisOrItem;
            }
          }
          // end of IN-merging

          // Results are -1, 0, 1, move to 0, 1, 2 for the lookup:
          ConditionPart::ConditionPartCompareResult res = ConditionPart::ResultsTable
            [CompareAstNodes(current.valueNode, other.valueNode, false) + 1] 
            [current.whichCompareOperation()]
            [other.whichCompareOperation()];

          switch (res) {
            case CompareResult::IMPOSSIBLE: {
              // impossible condition
              // j = positions.size(); 
              // we remove this one, so fast forward the loops to their end:
              _root->removeMemberUnchecked(r);
              retry = true;
              goto fastForwardToNextOrItem;
            }
            case CompareResult::SELF_CONTAINED_IN_OTHER: {
              std::cout << "SELF IS CONTAINED IN OTHER\n";
              andNode->removeMemberUnchecked(positions[0].first);
              goto restartThisOrItem;
            }
            case CompareResult::OTHER_CONTAINED_IN_SELF: { 
              std::cout << "OTHER IS CONTAINED IN SELF\n";
              andNode->removeMemberUnchecked(positions[j].first);
              goto restartThisOrItem;
            }
            case CompareResult::CONVERT_EQUAL: { /// beide gehen, werden umgeformt zu a == x (== y)
              andNode->removeMemberUnchecked(positions[j].first);
              auto origNode = andNode->getMemberUnchecked(positions[0].first);
              auto newNode = plan->getAst()->createNode(NODE_TYPE_OPERATOR_BINARY_EQ);
              for (size_t iMemb = 0; iMemb < origNode->numMembers(); iMemb++) {
                newNode->addMember(origNode->getMemberUnchecked(iMemb));
              }

              andNode->changeMember(positions[0].first, newNode);
              std::cout << "RESULT equals X/Y\n";
              break;
            }
            case CompareResult::DISJOINT: {
              break;
            }
            case CompareResult::UNKNOWN: {
              std::cout << "UNKNOWN\n";
              break;
            }
          }

          ++j;
        }
      } // cross compare sub-and-nodes
    } // foreach sub-and-node

fastForwardToNextOrItem:
    if (retry) {
      // number of root sub-nodes has probably changed.
      // now recalculate the number and don't modify r!
      n = _root->numMembers();
    }
    else {
      // root nodes hasn't changed. go to next sub-node!
      ++r;
    }
  }
}

////////////////////////////////////////////////////////////////////////////////
/// @brief deduplicate IN condition values
/// this may modify the node in place
////////////////////////////////////////////////////////////////////////////////

void Condition::deduplicateInOperation (AstNode* operation) {
  if (operation->type != NODE_TYPE_OPERATOR_BINARY_IN) {
    return;
  }

  // found an IN
  TRI_ASSERT(operation->numMembers() == 2);

  auto rhs = operation->getMemberUnchecked(1);

  if (! rhs->isArray() || ! rhs->isConstant()) {
    return;
  }

  auto deduplicated = _ast->deduplicateArray(rhs);

  if (deduplicated != rhs) {
    // there were duplicates
    operation->changeMember(1, const_cast<AstNode*>(deduplicated));
  }
}

////////////////////////////////////////////////////////////////////////////////
/// @brief merge the values from two IN operations
////////////////////////////////////////////////////////////////////////////////
        
AstNode* Condition::mergeInOperations (AstNode const* lhs, 
                                       AstNode const* rhs) {
  TRI_ASSERT(lhs->type == NODE_TYPE_OPERATOR_BINARY_IN);
  TRI_ASSERT(rhs->type == NODE_TYPE_OPERATOR_BINARY_IN);
  
  auto lValue = lhs->getMemberUnchecked(1);
  auto rValue = rhs->getMemberUnchecked(1);

  TRI_ASSERT(lValue->isArray() && lValue->isConstant());
  TRI_ASSERT(rValue->isArray() && rValue->isConstant());

  return _ast->createNodeMergedArray(lValue, rValue);
}

////////////////////////////////////////////////////////////////////////////////
/// @brief dump the condition for debug purposes
////////////////////////////////////////////////////////////////////////////////

#if 0
void Condition::dump () const {
  dumpNode(_root, 0);
}
#endif

AstNode* Condition::transformNode (AstNode* node) {
  if (node == nullptr) {
    return nullptr;
  }

  if (node->type == NODE_TYPE_OPERATOR_BINARY_AND ||
      node->type == NODE_TYPE_OPERATOR_BINARY_OR) {
    // convert binary AND/OR into n-ary AND/OR
    TRI_ASSERT(node->numMembers() == 2);
    auto old = node;

    // create a new n-ary node 
    node = _ast->createNode(Ast::NaryOperatorType(old->type));
    node->addMember(old->getMember(0));
    node->addMember(old->getMember(1));
  }

  TRI_ASSERT(node->type != NODE_TYPE_OPERATOR_BINARY_AND &&
             node->type != NODE_TYPE_OPERATOR_BINARY_OR);

  if (node->type == NODE_TYPE_OPERATOR_NARY_AND) {
    // first recurse into subnodes
    bool processChildren = false;
    size_t const n = node->numMembers();

    for (size_t i = 0; i < n; ++i) {
      node->changeMember(i, transformNode(node->getMemberUnchecked(i)));

      if (node->type == NODE_TYPE_OPERATOR_NARY_OR ||
          node->type == NODE_TYPE_OPERATOR_BINARY_OR) {
        processChildren = true;
      }
    }
      
    if (processChildren) {
      auto newOperator = _ast->createNode(NODE_TYPE_OPERATOR_NARY_OR);

      std::vector<PermutationState> permutationStates;
      for (size_t i = 0; i < n; ++i) {
        permutationStates.emplace_back(PermutationState(node->getMemberUnchecked(i), node->numMembers()));
      }
  
      size_t current = 0;
      bool done = false;
      size_t const numPermutations = permutationStates.size();

      while (! done) {
        auto andOperator = _ast->createNode(NODE_TYPE_OPERATOR_NARY_AND);

        for (size_t i = 0; i < numPermutations; ++i) {
          auto state = permutationStates[i];
          andOperator->addMember(state.getValue());
        }

        newOperator->addMember(andOperator);

        // now permute
        while (true) {
          if (++permutationStates[current].current < permutationStates[current].n) {
            current = 0;
            // abort inner iteration
            break;
          }

          permutationStates[current].current = 0;

          if (++current >= n) {
            done = true;
            break;
          }
          // next inner iteration
        }
      }

      return newOperator;
    }
    // fallthrough intentional
  }
  else if (node->type == NODE_TYPE_OPERATOR_NARY_OR) {
    // recurse into subnodes
    size_t const n = node->numMembers();

    for (size_t i = 0; i < n; ++i) {
      node->changeMember(i, transformNode(node->getMemberUnchecked(i)));
    }
    // fallthrough intentional
  }
  else if (node->type == NODE_TYPE_OPERATOR_UNARY_NOT) {
    // push down logical negations
    auto sub = node->getMemberUnchecked(0);

    if (sub->type == NODE_TYPE_OPERATOR_NARY_AND ||
        sub->type == NODE_TYPE_OPERATOR_BINARY_AND ||
        sub->type == NODE_TYPE_OPERATOR_NARY_OR ||
        sub->type == NODE_TYPE_OPERATOR_BINARY_OR) {
      size_t const n = sub->numMembers();
    
      AstNode* newOperator = nullptr;
      if (sub->type == NODE_TYPE_OPERATOR_NARY_AND ||
          sub->type == NODE_TYPE_OPERATOR_BINARY_AND) {
        // ! (a && b)  =>  (! a) || (! b)
        newOperator = _ast->createNode(NODE_TYPE_OPERATOR_NARY_OR);
      }
      else {
        // ! (a || b)  =>  (! a) && (! b)
        newOperator = _ast->createNode(NODE_TYPE_OPERATOR_NARY_AND);
      }

      for (size_t i = 0; i < n; ++i) {
        auto negated = transformNode(_ast->createNodeUnaryOperator(NODE_TYPE_OPERATOR_UNARY_NOT, sub->getMemberUnchecked(i)));
        auto optimized = _ast->optimizeNotExpression(negated);

        newOperator->addMember(optimized);
      }

      return newOperator;
    }

    node->changeMember(0, transformNode(sub));
    // fallthrough intentional
  }
  
  return node;
}

AstNode* Condition::collapseNesting (AstNode* node) {
  if (node == nullptr) {
    return nullptr;
  }

  if (node->type == NODE_TYPE_OPERATOR_NARY_AND || 
      node->type == NODE_TYPE_OPERATOR_NARY_OR) {
    // first recurse into subnodes
    size_t const n = node->numMembers();

    for (size_t i = 0; i < n; ++i) {
      auto sub = collapseNesting(node->getMemberUnchecked(i));

      if (sub->type == node->type) {
        // sub-node has the same type as parent node
        // now merge the sub-nodes of the sub-node into the parent node
        size_t const subNumMembers = sub->numMembers();

        for (size_t j = 0; j < subNumMembers; ++j) {
          node->addMember(sub->getMemberUnchecked(j));
        }
        // invalidate the child node which we just expanded
        node->changeMember(i, _ast->createNodeNop());
      }
      else { 
        // different type
        node->changeMember(i, sub);
      }
    }
  }

  return node;
}

AstNode* Condition::fixRoot (AstNode* node, int level) {
  if (node == nullptr) {
    return nullptr;
  }

  AstNodeType type;

  if (level == 0) {
    type = NODE_TYPE_OPERATOR_NARY_OR;
  }
  else {
    type = NODE_TYPE_OPERATOR_NARY_AND;
  }
  // check if first-level node is an OR node
  if (node->type != type) {
    // create new root node
    node = _ast->createNodeNaryOperator(type, node);
  }

  size_t const n = node->numMembers();
  size_t j = 0;

  for (size_t i = 0; i < n; ++i) {
    auto sub = node->getMemberUnchecked(i);

    if (sub->type == NODE_TYPE_NOP) {
      // ignore this node
      continue;
    }

    if (level == 0) {
      // recurse into next level
      node->changeMember(j, fixRoot(sub, level + 1));
    }
    else if (i != j) {
      node->changeMember(j, sub);
    }
    ++j;
  }

  if (j != n) {
    // adjust number of members (because of the NOP nodes removes)
    node->reduceMembers(j);
  }

  return node;
}

// -----------------------------------------------------------------------------
// --SECTION--                                                       END-OF-FILE
// -----------------------------------------------------------------------------

// Local Variables:
// mode: outline-minor
// outline-regexp: "/// @brief\\|/// {@inheritDoc}\\|/// @page\\|// --SECTION--\\|/// @\\}"
// End:
