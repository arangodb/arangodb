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
#include "Aql/Index.h"
#include "Aql/SortCondition.h"
#include "Aql/Variable.h"
#include "Basics/Exceptions.h"
#include "Basics/json.h"
#include "Basics/JsonHelper.h"

using namespace triagens::aql;
using CompareResult = ConditionPartCompareResult;
        
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

ConditionPartCompareResult const ConditionPart::ResultsTable[3][7][7] = {
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

// -----------------------------------------------------------------------------
// --SECTION--                                              struct ConditionPart
// -----------------------------------------------------------------------------

ConditionPart::ConditionPart (Variable const* variable,
                              std::string const& attributeName,
                              AstNode const* operatorNode,
                              AttributeSideType side,
                              void* data)
  : variable(variable),
    attributeName(attributeName),
    operatorType(operatorNode->type),
    operatorNode(operatorNode),
    valueNode(nullptr),
    data(data),
    isExpanded(false) {

  if (side == ATTRIBUTE_LEFT) {
    valueNode = operatorNode->getMember(1);
  }
  else {
    valueNode = operatorNode->getMember(0);
    if (Ast::IsReversibleOperator(operatorType)) {
      operatorType = Ast::ReverseOperator(operatorType);
    }
  }

  isExpanded = (attributeName.find("[*]") != std::string::npos);
}

ConditionPart::ConditionPart (Variable const* variable,
                              std::vector<triagens::basics::AttributeName> const& attributeNames,
                              AstNode const* operatorNode,
                              AttributeSideType side,
                              void* data)
  : ConditionPart(variable, "", operatorNode, side, data) {

  TRI_AttributeNamesToString(attributeNames, attributeName, false);
}

ConditionPart::~ConditionPart () {

}

////////////////////////////////////////////////////////////////////////////////
/// @brief true if the condition is completely covered by the other condition
////////////////////////////////////////////////////////////////////////////////

bool ConditionPart::isCoveredBy (ConditionPart const& other) const {
  if (variable != other.variable ||
      attributeName != other.attributeName) {
    return false;
  }

  // special cases for IN...
  if (! isExpanded && 
      ! other.isExpanded &&
      other.operatorType == NODE_TYPE_OPERATOR_BINARY_IN &&
      other.valueNode->isConstant() &&
      other.valueNode->isArray()) {
    // compare an EQ with an IN
    if (operatorType == NODE_TYPE_OPERATOR_BINARY_EQ) {
      // TODO: currently this code will not fire
      // only activate it when it is confirmed that this code path is tested
      /*
      size_t const n = other.valueNode->numMembers();

      for (size_t i = 0; i < n; ++i) {
        auto v = other.valueNode->getMemberUnchecked(i);
  
        ConditionPartCompareResult res = ConditionPart::ResultsTable
          [CompareAstNodes(v, valueNode, false) + 1] 
          [0] // NODE_TYPE_OPERATOR_BINARY_EQ
          [whichCompareOperation()];
  
        if (res == CompareResult::OTHER_CONTAINED_IN_SELF ||
            res == CompareResult::CONVERT_EQUAL) { 
          return true;
        }
      }
      */
    }
    else if (operatorType == NODE_TYPE_OPERATOR_BINARY_IN &&
             valueNode->isConstant() &&
             valueNode->isArray()) {
      // compare IN with an IN
      // this has quadratic complexity
      size_t const n1 = valueNode->numMembers();
      size_t const n2 = other.valueNode->numMembers();

      // maximum number of comparisons that we will accept
      // otherwise the optimization will be aborted
      static size_t const MaxComparisons = 2048;

      if (n1 * n2 < MaxComparisons) {
        for (size_t i = 0; i < n1; ++i) {
          auto v = valueNode->getMemberUnchecked(i);
          for (size_t j = 0; j < n2; ++j) {
            auto w = other.valueNode->getMemberUnchecked(j);
    
            ConditionPartCompareResult res = ConditionPart::ResultsTable[CompareAstNodes(v, w, true) + 1][0][0];
    
            if (res != CompareResult::OTHER_CONTAINED_IN_SELF && 
                res != CompareResult::CONVERT_EQUAL && 
                res != CompareResult::IMPOSSIBLE) { 
              return false;
            }
          }
        }

        return true;
      }
    }
  }

  // Results are -1, 0, 1, move to 0, 1, 2 for the lookup:
  ConditionPartCompareResult res = ConditionPart::ResultsTable
    [CompareAstNodes(other.valueNode, valueNode, true) + 1] 
    [other.whichCompareOperation()]
    [whichCompareOperation()];

  if (res == CompareResult::OTHER_CONTAINED_IN_SELF ||
      res == CompareResult::CONVERT_EQUAL ||
      res == CompareResult::IMPOSSIBLE) { 
    return true;
  }

  return false;
}

// -----------------------------------------------------------------------------
// --SECTION--                                                   class Condition
// -----------------------------------------------------------------------------

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
/// return value is a pair indicating whether the index can be used for
/// filtering(first) and sorting(second)
////////////////////////////////////////////////////////////////////////////////

std::pair<bool, bool> Condition::findIndexes (EnumerateCollectionNode const* node, 
                                              std::vector<Index const*>& usedIndexes,
                                              SortCondition const* sortCondition) {
  TRI_ASSERT(usedIndexes.empty());
  Variable const* reference = node->outVariable();

  if (_root == nullptr) {
    // We do not have a condition. But we have a sort!
    if (! sortCondition->isEmpty() &&
        sortCondition->isOnlyAttributeAccess() &&
        sortCondition->isUnidirectional()) {
      size_t const itemsInIndex = node->collection()->count(); 
      double bestCost = 0.0;
      Index const* bestIndex = nullptr;

      std::vector<Index const*> indexes = node->collection()->getIndexes();

      for (auto const& idx : indexes) {
        if (idx->sparse) {
          // a sparse index may exclude some documents, so it can't be used to
          // get a sorted view of the ENTIRE collection
          continue;
        }
        double sortCost = 0.0;
        if (indexSupportsSort(idx, reference, sortCondition, itemsInIndex, sortCost)) {
          if (bestIndex == nullptr || sortCost < bestCost) {
            bestCost = sortCost;
            bestIndex = idx;
          }
        }
      }

      if (bestIndex != nullptr) {
        usedIndexes.emplace_back(bestIndex);
      }

      return std::make_pair(false, bestIndex != nullptr);
    }

    // No Index and no sort condition that
    // can be supported by an index.
    // Nothing to do here.
    return std::make_pair(false, false);
  }

  // We can only start after DNF transformation
  TRI_ASSERT(_root->type == NODE_TYPE_OPERATOR_NARY_OR);
  bool canUseForFilter = (_root->numMembers() > 0);
  bool canUseForSort   = false;

  for (size_t i = 0; i < _root->numMembers(); ++i) {
    auto canUseIndex = findIndexForAndNode(i, reference, node, usedIndexes, sortCondition);

    if (canUseIndex.second && ! canUseIndex.first) {
      // index can be used for sorting only
      // we need to abort further searching and only return one index
      TRI_ASSERT(! usedIndexes.empty());
      if (usedIndexes.size() > 1) {
        auto sortIndex = usedIndexes.back();

        usedIndexes.clear();
        usedIndexes.emplace_back(sortIndex);
      }

      return std::make_pair(false, true);
    }

    canUseForFilter &= canUseIndex.first;
    canUseForSort   |= canUseIndex.second;
  }

  // should always be true here. maybe not in the future in case a collection
  // has absolutely no indexes
  return std::make_pair(canUseForFilter, canUseForSort);
}

bool Condition::indexSupportsSort (Index const* idx,
                                   Variable const* reference,
                                   SortCondition const* sortCondition,
                                   size_t itemsInIndex,
                                   double& estimatedCost) const {
  if (idx->isSorted() &&
      idx->supportsSortCondition(sortCondition, reference, itemsInIndex, estimatedCost)) {
    // index supports the sort condition
    return true;
  }
    
  // index does not support the sort condition
  if (itemsInIndex > 0) {
    estimatedCost = itemsInIndex * std::log2(static_cast<double>(itemsInIndex));
  }
  else {
    estimatedCost = 0.0;
  }
  return false;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief finds the best index that can match this single node
////////////////////////////////////////////////////////////////////////////////

std::pair<bool, bool> Condition::findIndexForAndNode (size_t position,
                                                      Variable const* reference, 
                                                      EnumerateCollectionNode const* colNode, 
                                                      std::vector<Index const*>& usedIndexes,
                                                      SortCondition const* sortCondition) {
  // We can only iterate through a proper DNF
  auto node = _root->getMember(position);
  TRI_ASSERT(node->type == NODE_TYPE_OPERATOR_NARY_AND);
 
  size_t const itemsInIndex = colNode->collection()->count(); 

  Index const* bestIndex  = nullptr;
  double bestCost         = 0.0;
  bool bestSupportsFilter = false;
  bool bestSupportsSort   = false;

  std::vector<Index const*> indexes = colNode->collection()->getIndexes();

  for (auto const& idx : indexes) {
    double filterCost = 0.0;
    double sortCost   = 0.0;

    bool supportsFilter = false;
    bool supportsSort   = false;
    
    // check if the index supports the filter expression
    double estimatedCost;
    size_t estimatedItems;
    if (idx->supportsFilterCondition(node, reference, itemsInIndex, estimatedItems, estimatedCost)) {
      // index supports the filter condition
      filterCost = estimatedCost;
      supportsFilter = true;
    }
    else {
      // index does not support the filter condition
      filterCost = itemsInIndex * 1.5;
    }

    if (! sortCondition->isEmpty() &&
        sortCondition->isOnlyAttributeAccess() &&
        sortCondition->isUnidirectional()) {
      // only go in here if we actually have a sort condition and it can in
      // general be supported by an index. for this, a sort condition must not
      // be empty, must consist only of attribute access, and all attributes
      // must be sorted in the direction
      if (indexSupportsSort(idx, reference, sortCondition, itemsInIndex, sortCost)) {
        supportsSort = true;
      }
    }

    // std::cout << "INDEX: " << idx << ", FILTER COST: " << filterCost << ", SORT COST: " << sortCost << "\n";

    if (! supportsFilter && ! supportsSort) {
      continue;
    }

    double const totalCost = filterCost + sortCost;
    if (bestIndex == nullptr || totalCost < bestCost) {
      bestIndex          = idx;
      bestCost           = totalCost;
      bestSupportsFilter = supportsFilter;
      bestSupportsSort   = supportsSort;
    }
  }

  if (bestIndex == nullptr) {
    return std::make_pair(false, false);
  }

  _root->changeMember(position, bestIndex->specializeCondition(node, reference)); 
  _isSorted = sortOrs(reference);

  usedIndexes.emplace_back(bestIndex);

  return std::make_pair(bestSupportsFilter, bestSupportsSort);
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
  _root = fixRoot(_root, 0);

  optimize(plan);

#ifdef TRI_ENABLE_MAINTAINER_MODE
  if (_root != nullptr) {
    // _root->dump(0);
    validateAst(_root, 0);
  }
#endif
}

////////////////////////////////////////////////////////////////////////////////
/// @brief removes condition parts from another
////////////////////////////////////////////////////////////////////////////////

AstNode const* Condition::removeIndexCondition (Variable const* variable,
                                                AstNode const* other) {
  if (_root == nullptr || other == nullptr) {
    return _root;
  } 

  TRI_ASSERT(_root != nullptr);
  TRI_ASSERT(_root->type == NODE_TYPE_OPERATOR_NARY_OR);

  TRI_ASSERT(other != nullptr);
  TRI_ASSERT(other->type == NODE_TYPE_OPERATOR_NARY_OR);
  
  if (other->numMembers() != 1 && _root->numMembers() != 1) {
    return _root;
  }
    
  auto andNode = _root->getMemberUnchecked(0);
  TRI_ASSERT(andNode->type == NODE_TYPE_OPERATOR_NARY_AND);
  size_t const n = andNode->numMembers();

  std::unordered_set<size_t> toRemove;

  for (size_t i = 0; i < n; ++i) {
    auto operand = andNode->getMemberUnchecked(i);

    if (operand->isComparisonOperator()) {
      auto lhs = operand->getMember(0);
      auto rhs = operand->getMember(1);

      if (lhs->type == NODE_TYPE_ATTRIBUTE_ACCESS) {
        std::pair<Variable const*, std::vector<triagens::basics::AttributeName>> result;
          
        if (lhs->isAttributeAccessForVariable(result) &&
            rhs->isConstant()) {
          if (result.first != variable) {
            // attribute access for different variable
            continue;
          }
        
          ConditionPart current(variable, result.second, operand, ATTRIBUTE_LEFT, nullptr);

          if (canRemove(current, other)) {
            toRemove.emplace(i);
          }
        }
      }

      if (rhs->type == NODE_TYPE_ATTRIBUTE_ACCESS ||
          rhs->type == NODE_TYPE_EXPANSION) {
        std::pair<Variable const*, std::vector<triagens::basics::AttributeName>> result;
          
        if (rhs->isAttributeAccessForVariable(result) &&
            lhs->isConstant()) {
          if (result.first != variable) {
            // attribute access for different variable
            continue;
          }
          
          ConditionPart current(variable, result.second, operand, ATTRIBUTE_RIGHT, nullptr);

          if (canRemove(current, other)) {
            toRemove.emplace(i);
          }
        }
      }
    }
  }

  if (toRemove.empty()) {
    return _root;
  }

  // build a new AST condition
  AstNode* newNode = nullptr;

  for (size_t i = 0; i < n; ++i) {
    if (toRemove.find(i) == toRemove.end()) {
      auto what = andNode->getMemberUnchecked(i);

      if (newNode == nullptr) {
        // the only node so far
        newNode = what;
      }
      else {
        // AND-combine with existing node
        newNode = _ast->createNodeBinaryOperator(NODE_TYPE_OPERATOR_BINARY_AND, newNode, what);
      }
    }
  }

  return newNode;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief remove (now) invalid variables from the condition
////////////////////////////////////////////////////////////////////////////////

bool Condition::removeInvalidVariables (std::unordered_set<Variable const*> const& validVars) {
  if (_root == nullptr) {
    return false;
  }

  TRI_ASSERT(_root != nullptr);
  TRI_ASSERT(_root->type == NODE_TYPE_OPERATOR_NARY_OR);
 
  bool isEmpty = false;

  // handle sub nodes of top-level OR node
  size_t const n = _root->numMembers();
  std::unordered_set<Variable const*> varsUsed;

  for (size_t i = 0; i < n; ++i) {
    auto andNode = _root->getMemberUnchecked(i);
    TRI_ASSERT(andNode->type == NODE_TYPE_OPERATOR_NARY_AND);

    size_t nAnd = andNode->numMembers();
    for (size_t j = 0; j < nAnd; /* no hoisting */) {
      // check which variables are used in each AND
      varsUsed.clear();
      Ast::getReferencedVariables(andNode, varsUsed);

      bool invalid = false;
      for (auto& it : varsUsed) {
        if (validVars.find(it) == validVars.end()) {
          // found an invalid variable here...
          invalid = true;
          break;
        }
      }

      if (invalid) {
        andNode->removeMemberUnchecked(j);
        // repeat with some member index
        TRI_ASSERT(nAnd > 0);
        --nAnd;
        if (nAnd == 0) {
          isEmpty = true;
        }
      }
      else {
        ++j;
      }
    }
  }

  return isEmpty;
}

// -----------------------------------------------------------------------------
// --SECTION--                                                   private methods
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @brief sort ORs for the same attribute so they are in ascending value
/// order. this will only work if the condition is for a single attribute
////////////////////////////////////////////////////////////////////////////////

bool Condition::sortOrs (Variable const* variable) {
  if (_root == nullptr) {
    return true;
  }

  size_t const n = _root->numMembers();
  
  if (n < 2) {
    return true;
  }
    
  std::vector<ConditionPart> parts;
  parts.reserve(n);

  for (size_t i = 0; i < n; ++i) {
    // sort the conditions of each AND
    auto sub = _root->getMemberUnchecked(i);

    TRI_ASSERT(sub != nullptr && sub->type == NODE_TYPE_OPERATOR_NARY_AND);
    size_t const nAnd = sub->numMembers();

    if (nAnd != 1) {
      // we can't handle this one
      return false;
    }
    
    auto operand = sub->getMemberUnchecked(0);

    if (! operand->isComparisonOperator()) {
      return false;
    }
    if (operand->type == NODE_TYPE_OPERATOR_BINARY_NE ||
        operand->type == NODE_TYPE_OPERATOR_BINARY_NIN) {
      return false;
    }

    auto lhs = operand->getMember(0);
    auto rhs = operand->getMember(1);

    if (lhs->type == NODE_TYPE_ATTRIBUTE_ACCESS) {
      std::pair<Variable const*, std::vector<triagens::basics::AttributeName>> result;
          
      if (lhs->isAttributeAccessForVariable(result) &&
          rhs->isConstant()) {
        if (result.first == variable) { 
          parts.emplace_back(ConditionPart(result.first, result.second, operand, ATTRIBUTE_LEFT, sub));
        }
      }
    }

    if (rhs->type == NODE_TYPE_ATTRIBUTE_ACCESS ||
        rhs->type == NODE_TYPE_EXPANSION) {
      std::pair<Variable const*, std::vector<triagens::basics::AttributeName>> result;
          
      if (rhs->isAttributeAccessForVariable(result) &&
          lhs->isConstant()) {
          
        if (result.first == variable) { 
          parts.emplace_back(ConditionPart(result.first, result.second, operand, ATTRIBUTE_RIGHT, sub));
        }
      }
    }
  }

  if (parts.size() != _root->numMembers()) { 
    return false;
  }

  // now sort all conditions by variable name, attribute name, attribute value
  std::sort(parts.begin(), parts.end(), [] (ConditionPart const& lhs, ConditionPart const& rhs) -> bool {
    // compare variable names first
    auto res = lhs.variable->name.compare(rhs.variable->name);

    if (res != 0) {
      return res < 0;
    }

    // compare attribute names next
    res = lhs.attributeName.compare(rhs.attributeName);

    if (res != 0) {
      return res < 0;
    }

    // compare attribute values next
    auto ll = lhs.lowerBound();
    auto lr = rhs.lowerBound();

    if (ll == nullptr && lr != nullptr) {
      // left lower bound is not set but right
      return true;
    }
    else if (ll != nullptr && lr == nullptr) {
      // left lower bound is set but not right
      return false;
    }

    if (ll != nullptr && lr != nullptr) {
      // both lower bounds are set
      res = CompareAstNodes(ll, lr, true);

      if (res != 0) {
        return res < 0;
      }
    }

    if (lhs.isLowerInclusive() && ! rhs.isLowerInclusive()) {
      return true;
    }
    if (rhs.isLowerInclusive() && ! lhs.isLowerInclusive()) {
      return false;
    }

    // all things equal
    return false;
  });

/*
  auto l = 0;
  for (size_t r = 1; r < n; ++r) {
    auto& l = parts[l].data;
    auto& r = parts[r].data;

    if (l.higher > r.higher ||
        (l.higher == r.higher && (l.inclusive || ! r.inclusive)) {
      // r is contained in l => remove r (i.e. do nothing)
      r.data = nullptr;
    }
    else if (r.lower < l.higher || (r.lower == l.higher && (r.inclusive || l.inclusive))) {
      // r extends l => fuse l.lower & r.higher

      r.data = nullptr;
      newOrNode->getMember(newor
    }
    else {
      // disjoint ranges. simply add the node
      newOrNode->addMember(r);
    }
  }
  */

  for (size_t i = 0; i < n; ++i) {
    _root->changeMember(i, static_cast<AstNode*>(parts[i].data));
  }

  return true;
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

  // handle sub nodes of top-level OR node
  size_t n = _root->numMembers();
  size_t r = 0;

  while (r < n) { // foreach OR-Node
    bool retry = false;
    auto andNode = _root->getMemberUnchecked(r);
    TRI_ASSERT(andNode->type == NODE_TYPE_OPERATOR_NARY_AND);

restartThisOrItem:
    size_t andNumMembers = andNode->numMembers();
  
    // deduplicate and sort all IN arrays
    size_t inComparisons = 0;
    for (size_t j = 0; j < andNumMembers; ++j) {
      auto op = andNode->getMemberUnchecked(j);

      if (op->type == NODE_TYPE_OPERATOR_BINARY_IN) {
        ++inComparisons;
      }
      deduplicateInOperation(op);
    }
    andNumMembers = andNode->numMembers();

    if (andNumMembers <= 1) {
      // simple AND item with 0 or 1 members. nothing to do
      ++r;
      n = _root->numMembers();
      continue;
    }

    TRI_ASSERT(andNumMembers > 1);

    if (inComparisons > 0) {
      // move IN operations to the front to make comparison code below simpler
      std::vector<AstNode*> stack;
      size_t p = andNumMembers - 1;
      
      for (size_t j = p; ; --j) {
        auto op = andNode->getMemberUnchecked(j);

        if (op->type == NODE_TYPE_OPERATOR_BINARY_IN) {
          stack.push_back(op);
        }
        else {
          if (p != j) {
            andNode->changeMember(p, op);
          }
          --p;
        }
        if (j == 0) {
          break;
        }
      }

      p = 0;
      while (! stack.empty()) {
        auto it = stack.back();
        andNode->changeMember(p++, it);
        stack.pop_back();
      }
    }
    
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

        ConditionPart current(variable, attributeName, leftNode, positions[0].second, nullptr);

        if (! current.valueNode->isConstant()) {
          continue;
        }

        size_t j = 1;

        while (j < positions.size()) {
          TRI_ASSERT(j != 0);
          auto rightNode = andNode->getMemberUnchecked(positions[j].first);

          ConditionPart other(variable, attributeName, rightNode, positions[j].second, nullptr);

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

              // enumerate over IN list
              for (size_t k = 0; k < values->numMembers(); ++k) {
                auto value = values->getMemberUnchecked(k);
                ConditionPartCompareResult res = ConditionPart::ResultsTable
                  [CompareAstNodes(value, other.valueNode, true) + 1]
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
              leftNode->changeMember(1, inNode);
              // remove the other operator
              andNode->removeMemberUnchecked(positions[j].first);
              goto restartThisOrItem;
            }
          }
          // end of IN-merging

          // Results are -1, 0, 1, move to 0, 1, 2 for the lookup:
          ConditionPartCompareResult res = ConditionPart::ResultsTable
            [CompareAstNodes(current.valueNode, other.valueNode, true) + 1] 
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
              andNode->removeMemberUnchecked(positions.at(0).first);
              goto restartThisOrItem;
            }
            case CompareResult::OTHER_CONTAINED_IN_SELF: { 
              andNode->removeMemberUnchecked(positions.at(j).first);
              goto restartThisOrItem;
            }
            case CompareResult::CONVERT_EQUAL: { // both ok, now transform to a == x (== y)
              andNode->removeMemberUnchecked(positions.at(j).first);
              auto origNode = andNode->getMemberUnchecked(positions.at(0).first);
              auto newNode = plan->getAst()->createNode(NODE_TYPE_OPERATOR_BINARY_EQ);
              for (size_t iMemb = 0; iMemb < origNode->numMembers(); iMemb++) {
                newNode->addMember(origNode->getMemberUnchecked(iMemb));
              }

              andNode->changeMember(positions.at(0).first, newNode);
              goto restartThisOrItem;
            }
            case CompareResult::DISJOINT: {
              break;
            }
            case CompareResult::UNKNOWN: {
              break;
            }
          }

          ++j;
        }
      } // cross compare sub-and-nodes
    } // foreach sub-and-node

fastForwardToNextOrItem:
    if (! retry) {
      // root nodes hasn't changed. go to next sub-node!
      ++r;
    }
    // number of root sub-nodes has probably changed.
    // now recalculate the number and don't modify r!
    n = _root->numMembers();
  }
}

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
/// @brief validate the condition's AST
////////////////////////////////////////////////////////////////////////////////

#ifdef TRI_ENABLE_MAINTAINER_MODE
void Condition::validateAst (AstNode const* node, 
                             int level) {
  if (level == 0) {
    TRI_ASSERT(node->type == NODE_TYPE_OPERATOR_NARY_OR);
  }
    
  size_t const n = node->numMembers();
  for (size_t i = 0; i < n; ++i) {
    auto sub = node->getMemberUnchecked(i);
    if (level == 0) {
      TRI_ASSERT(sub->type == NODE_TYPE_OPERATOR_NARY_AND);
    }
    else {
      TRI_ASSERT(sub->type != NODE_TYPE_OPERATOR_NARY_OR &&
                 sub->type != NODE_TYPE_OPERATOR_NARY_AND);
    }

    validateAst(sub, level + 1);
  }
}
#endif

////////////////////////////////////////////////////////////////////////////////
/// @brief checks if the current condition is covered by the other
////////////////////////////////////////////////////////////////////////////////

bool Condition::canRemove (ConditionPart const& me,
                           triagens::aql::AstNode const* otherCondition) const {
  TRI_ASSERT(otherCondition != nullptr);
  TRI_ASSERT(otherCondition->type == NODE_TYPE_OPERATOR_NARY_OR);

  auto andNode = otherCondition->getMemberUnchecked(0);
  TRI_ASSERT(andNode->type == NODE_TYPE_OPERATOR_NARY_AND);
  size_t const n = andNode->numMembers();

  for (size_t i = 0; i < n; ++i) {
    auto operand = andNode->getMemberUnchecked(i);

    if (operand->isComparisonOperator()) {
      auto lhs = operand->getMember(0);
      auto rhs = operand->getMember(1);
     
      if (lhs->type == NODE_TYPE_ATTRIBUTE_ACCESS) {
        std::pair<Variable const*, std::vector<triagens::basics::AttributeName>> result;
          
        if (lhs->isAttributeAccessForVariable(result) &&
            rhs->isConstant()) {

          ConditionPart indexCondition(result.first, result.second, operand, ATTRIBUTE_LEFT, nullptr);

          if (me.isCoveredBy(indexCondition)) {
            return true;
          }
        }
      }
      
      if (rhs->type == NODE_TYPE_ATTRIBUTE_ACCESS ||
          rhs->type == NODE_TYPE_EXPANSION) {
        std::pair<Variable const*, std::vector<triagens::basics::AttributeName>> result;
          
        if (rhs->isAttributeAccessForVariable(result) &&
            lhs->isConstant()) {

          ConditionPart indexCondition(result.first, result.second, operand, ATTRIBUTE_RIGHT, nullptr);

          if (me.isCoveredBy(indexCondition)) {
            return true;
          }
        }
      }
    }
  }

  return false;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief deduplicate IN condition values (and sort them)
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
/// @brief merges the current node with the sub nodes of same type
////////////////////////////////////////////////////////////////////////////////

AstNode* Condition::collapse (AstNode const* node) {
  TRI_ASSERT(node->type == NODE_TYPE_OPERATOR_NARY_OR ||
             node->type == NODE_TYPE_OPERATOR_NARY_AND);

  auto newOperator = _ast->createNode(node->type);

  size_t const n = node->numMembers();

  for (size_t i = 0; i < n; ++i) {
    auto sub = node->getMemberUnchecked(i);

    if (sub->type == node->type) {
      // merge
      for (size_t j = 0; j < sub->numMembers(); ++j) {
        newOperator->addMember(sub->getMemberUnchecked(j));
      }
    }
    else {
      newOperator->addMember(sub);
    }
  }

  return newOperator;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief converts binary logical operators into n-ary operators
////////////////////////////////////////////////////////////////////////////////

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
    bool processChildren = false;
    bool mustCollapse = false;
    size_t const n = node->numMembers();

    for (size_t i = 0; i < n; ++i) {
      // process subnodes first
      auto sub = transformNode(node->getMemberUnchecked(i));
      node->changeMember(i, sub);

      if (sub->type == NODE_TYPE_OPERATOR_NARY_OR ||
          sub->type == NODE_TYPE_OPERATOR_BINARY_OR) {
        processChildren = true;
      }
      else if (sub->type == NODE_TYPE_OPERATOR_NARY_AND) {
        mustCollapse = true;
      }
    }
    
    if (processChildren) {
      // we found an AND with at least one OR child, e.g.
      //        AND
      //   OR          c
      // a    b 
      //
      // we need to move the OR to the top by converting the condition to:
      //         OR
      //   AND        AND
      //  a   c      b   c
      //
      auto newOperator = _ast->createNode(NODE_TYPE_OPERATOR_NARY_OR);

      std::vector<PermutationState> permutationStates;
      for (size_t i = 0; i < n; ++i) {
        auto sub = node->getMemberUnchecked(i);

        if (sub->type == NODE_TYPE_OPERATOR_NARY_OR) { // || sub->type == NODE_TYPE_OPERATOR_NARY_AND) {
          permutationStates.emplace_back(PermutationState(sub, sub->numMembers()));
        }
        else {
          permutationStates.emplace_back(PermutationState(sub, 1));
        }
      }
  
      size_t current = 0;
      bool done = false;
      size_t const numPermutations = permutationStates.size();

      while (! done) {
        auto andOperator = _ast->createNode(NODE_TYPE_OPERATOR_NARY_AND);

        for (size_t i = 0; i < numPermutations; ++i) {
          auto state = permutationStates[i];
          andOperator->addMember(state.getValue()->clone(_ast));
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

      node = newOperator;
    }

    if (mustCollapse) {
      node = collapse(node);
    }

    return node;
  }

  if (node->type == NODE_TYPE_OPERATOR_NARY_OR) {
    size_t const n = node->numMembers();
    bool mustCollapse = false;

    for (size_t i = 0; i < n; ++i) {
      auto sub = transformNode(node->getMemberUnchecked(i));
      node->changeMember(i, sub);

      if (sub->type == NODE_TYPE_OPERATOR_NARY_OR) {
        mustCollapse = true;
      }
    }

    if (mustCollapse) {
      node = collapse(node);
    }

    return node;
  }

  if (node->type == NODE_TYPE_OPERATOR_UNARY_NOT) {
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
  }
  
  return node;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief Creates a top-level OR node if it does not already exist, and make 
/// sure that all second level nodes are AND nodes. Additionally, this step will
/// remove all NOP nodes.
////////////////////////////////////////////////////////////////////////////////

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
