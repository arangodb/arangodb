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

void ConditionPart::dump () const {
  std::cout << "VARIABLE NAME: " << variable->name << "." << attributeName << " " << triagens::basics::JsonHelper::toString(valueNode->toJson(TRI_UNKNOWN_MEM_ZONE, false)) << "\n";
}

// -----------------------------------------------------------------------------
// --SECTION--                                                   class Condition
// -----------------------------------------------------------------------------

// -----------------------------------------------------------------------------
// --SECTION--                                            static helper function
// -----------------------------------------------------------------------------

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
    std::cout << "  (name " << static_cast<Variable const*>(node->getData())->name << ")";
  }

  std::cout << "\n";
  for (size_t i = 0; i < node->numMembers(); ++i) {
    dumpNode(node->getMember(i), indent + 1);
  }
}

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
  // We can only start after DNF transformation
  TRI_ASSERT(_root->type == NODE_TYPE_OPERATOR_NARY_OR);

  Variable const* reference = node->outVariable();

  for (size_t i = 0; i < _root->numMembers(); ++i) {
    if (! findIndexForAndNode(_root->getMember(i), reference, node, usedIndexes, sortCondition)) {
      // We are not able to find an index for this AND block. Sorry we have to abort here
      return false;
    }
  }

  // should always be true here. maybe not in the future in case a collection
  // has absolutely no indexes
  return ! usedIndexes.empty();
}

////////////////////////////////////////////////////////////////////////////////
/// @brief finds the best index that can match this single node
////////////////////////////////////////////////////////////////////////////////

bool Condition::findIndexForAndNode (AstNode const* node, 
                                     Variable const* reference, 
                                     EnumerateCollectionNode const* colNode, 
                                     std::vector<Index const*>& usedIndexes,
                                     SortCondition const& sortCondition) {
  // We can only iterate through a proper DNF
  TRI_ASSERT(node->type == NODE_TYPE_OPERATOR_NARY_AND);
  
  static double const MaxFilterCost = 2.0;
  static double const MaxSortCost   = 2.0;

  // This code is never responsible for the content of this pointer.
  Index const* bestIndex = nullptr;
  double bestCost        = MaxFilterCost + MaxSortCost + std::numeric_limits<double>::epsilon(); 

  std::vector<Index const*> indexes = colNode->collection()->getIndexes();

  for (auto& idx : indexes) {
    double filterCost = 0.0;
    double sortCost   = 0.0;
    
    // check if the index supports the filter expression
    double estimatedCost;
    if (idx->supportsFilterCondition(node, reference, estimatedCost)) {
      // index supports the filter condition
      filterCost = estimatedCost;
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
      double estimatedCost;
      if (idx->isSorted() &&
          idx->supportsSortCondition(&sortCondition, reference, estimatedCost)) {
        // index supports the sort condition
        sortCost = estimatedCost;
      }
      else {
        // index does not support the sort condition
        sortCost = MaxSortCost;
      }
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

  std::cout << "\n";
  dump();
  std::cout << "\n";
  _isNormalized = true;
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

ConditionPart::ConditionPartCompareResult ConditionPart::ResultsTable[3][7][7] = {
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

void Condition::optimize (ExecutionPlan* plan) {
//  normalize();
  typedef std::vector<std::pair<size_t, AttributeSideType>> UsagePositionType;
  typedef std::unordered_map<std::string, UsagePositionType> AttributeUsageType;
  typedef std::unordered_map<Variable const*, AttributeUsageType> VariableUsageType;
      
  auto storeAttributeAccess = [] (VariableUsageType& variableUsage, AstNode const* node, size_t position, AttributeSideType side) {
    auto&& attributeAccess = Ast::extractAttributeAccess(node);
    auto variable = attributeAccess.first;

    if (variable != nullptr) {
      auto it = variableUsage.find(variable);
      auto const& attributeName = attributeAccess.second;

      if (it == variableUsage.end()) {
        // nothing recorded yet for variable
        it = variableUsage.emplace(variable, AttributeUsageType()).first;
      }

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
  };
 
  if (_root == nullptr) {
    return;
  } 

  TRI_ASSERT(_root != nullptr);
  TRI_ASSERT(_root->type == NODE_TYPE_OPERATOR_NARY_OR);

  // handle sub nodes or top-level OR node
  size_t const n = _root->numMembers();

  for (size_t i = 0; i < n; ++i) { // foreach OR-Node
    auto andNode = _root->getMemberUnchecked(i);
    TRI_ASSERT(andNode->type == NODE_TYPE_OPERATOR_NARY_AND);

  restartThisOrItem:
    size_t const andNumMembers = andNode->numMembers();

    if (andNumMembers > 1) {
      // optimization is only necessary if an AND node has members
      VariableUsageType variableUsage;
 
      for (size_t j = 0; j < andNumMembers; ++j) {
        auto operand = andNode->getMemberUnchecked(j);

        if (operand->isComparisonOperator()) {
          auto lhs = operand->getMember(0);
          auto rhs = operand->getMember(1);

          if (lhs->type == NODE_TYPE_ATTRIBUTE_ACCESS) {
            storeAttributeAccess(variableUsage, lhs, j, ATTRIBUTE_LEFT);
          }
          if (rhs->type == NODE_TYPE_ATTRIBUTE_ACCESS) {
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
          std::cout << "ATTRIBUTE " << attributeName << " occurs in " << positions.size() << " positions\n";

          ConditionPart current(variable, attributeName, 0, andNode->getMemberUnchecked(positions[0].first), positions[0].second);
          // current.dump();
          size_t j = 1;

          while (j < positions.size()) {

            ConditionPart other(variable, attributeName, j, andNode->getMemberUnchecked(positions[j].first), positions[j].second);

            if (! current.valueNode->isConstant() || ! other.valueNode->isConstant()) {
              continue;
            }
            
            // Results are -1, 0, 1, move to 0,1,2 for the lookup:
            ConditionPart::ConditionPartCompareResult res = ConditionPart::ResultsTable
              [CompareAstNodes(current.valueNode, other.valueNode, false) + 1] 
              [current.whichCompareOperation()]
              [other.whichCompareOperation()];

            switch (res) {
              case CompareResult::IMPOSSIBLE: {
                // impossible condition
                // j = positions.size(); 
                // we remove this one, so fast forward the loops to their end:
                _root->removeMemberUnchecked(i);
                /// i -= 1; <- wenn wir das ohne goto machen...
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
                for (uint32_t iMemb = 0; iMemb < origNode->numMembers(); iMemb++) {
                  newNode->addMember(origNode->getMemberUnchecked(iMemb));
                }

                andNode->changeMember(positions[0].first, newNode);
                std::cout << "RESULT equals X/Y\n";
                break;
              }
              case CompareResult::DISJOINT: {
                std::cout << "DISJOINT\n";
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
      continue;
    }
  }

}

////////////////////////////////////////////////////////////////////////////////
/// @brief dump a condition
////////////////////////////////////////////////////////////////////////////////

void Condition::dump () const {
  dumpNode(_root, 0);
}

AstNode* Condition::transformNode (AstNode* node) {
  if (node == nullptr) {
    return nullptr;
  }

  if (node->type == NODE_TYPE_OPERATOR_BINARY_AND ||
      node->type == NODE_TYPE_OPERATOR_BINARY_OR) {
    // convert binary AND/OR into n-ary AND/OR
    auto lhs = node->getMember(0);
    auto rhs = node->getMember(1);
    node = _ast->createNodeBinaryOperator(Ast::NaryOperatorType(node->type), lhs, rhs);
  }

  TRI_ASSERT(node->type != NODE_TYPE_OPERATOR_BINARY_AND &&
      node->type != NODE_TYPE_OPERATOR_BINARY_OR);

  if (node->type == NODE_TYPE_OPERATOR_NARY_AND) {
    // first recurse into subnodes
    node->changeMember(0, transformNode(node->getMember(0)));
    node->changeMember(1, transformNode(node->getMember(1)));

    auto lhs = node->getMember(0);
    auto rhs = node->getMember(1);

    if (lhs->type == NODE_TYPE_OPERATOR_NARY_OR &&
        rhs->type == NODE_TYPE_OPERATOR_NARY_OR) {
      auto and1 = transformNode(_ast->createNodeBinaryOperator(NODE_TYPE_OPERATOR_NARY_AND, lhs->getMember(0), rhs->getMember(0)));
      auto and2 = transformNode(_ast->createNodeBinaryOperator(NODE_TYPE_OPERATOR_NARY_AND, lhs->getMember(0), rhs->getMember(1)));
      auto or1  = _ast->createNodeBinaryOperator(NODE_TYPE_OPERATOR_NARY_OR, and1, and2);

      auto and3 = transformNode(_ast->createNodeBinaryOperator(NODE_TYPE_OPERATOR_NARY_AND, lhs->getMember(1), rhs->getMember(0)));
      auto and4 = transformNode(_ast->createNodeBinaryOperator(NODE_TYPE_OPERATOR_NARY_AND, lhs->getMember(1), rhs->getMember(1)));
      auto or2  = _ast->createNodeBinaryOperator(NODE_TYPE_OPERATOR_NARY_OR, and3, and4);

      return _ast->createNodeBinaryOperator(NODE_TYPE_OPERATOR_NARY_OR, or1, or2);
    }
    else if (lhs->type == NODE_TYPE_OPERATOR_NARY_OR) {
      auto and1 = transformNode(_ast->createNodeBinaryOperator(NODE_TYPE_OPERATOR_NARY_AND, rhs, lhs->getMember(0)));
      auto and2 = transformNode(_ast->createNodeBinaryOperator(NODE_TYPE_OPERATOR_NARY_AND, rhs, lhs->getMember(1)));

      return _ast->createNodeBinaryOperator(NODE_TYPE_OPERATOR_NARY_OR, and1, and2);
    }
    else if (rhs->type == NODE_TYPE_OPERATOR_NARY_OR) {
      auto and1 = transformNode(_ast->createNodeBinaryOperator(NODE_TYPE_OPERATOR_NARY_AND, lhs, rhs->getMember(0)));
      auto and2 = transformNode(_ast->createNodeBinaryOperator(NODE_TYPE_OPERATOR_NARY_AND, lhs, rhs->getMember(1)));

      return _ast->createNodeBinaryOperator(NODE_TYPE_OPERATOR_NARY_OR, and1, and2);
    }
  }
  else if (node->type == NODE_TYPE_OPERATOR_NARY_OR) {
    // recurse into subnodes
    node->changeMember(0, transformNode(node->getMember(0)));
    node->changeMember(1, transformNode(node->getMember(1)));
  }
  else if (node->type == NODE_TYPE_OPERATOR_UNARY_NOT) {
    // push down logical negations
    auto sub = node->getMemberUnchecked(0);

    if (sub->type == NODE_TYPE_OPERATOR_NARY_AND ||
        sub->type == NODE_TYPE_OPERATOR_BINARY_AND) {
      // ! (a && b)  =>  (! a) || (! b)
      auto neg1 = transformNode(_ast->createNodeUnaryOperator(NODE_TYPE_OPERATOR_UNARY_NOT, sub->getMember(0)));
      auto neg2 = transformNode(_ast->createNodeUnaryOperator(NODE_TYPE_OPERATOR_UNARY_NOT, sub->getMember(1)));

      neg1 = _ast->optimizeNotExpression(neg1);
      neg2 = _ast->optimizeNotExpression(neg2);

      return _ast->createNodeBinaryOperator(NODE_TYPE_OPERATOR_NARY_OR, neg1, neg2);
    }
    else if (sub->type == NODE_TYPE_OPERATOR_NARY_OR ||
        sub->type == NODE_TYPE_OPERATOR_BINARY_OR) {
      // ! (a || b)  =>  (! a) && (! b)
      auto neg1 = transformNode(_ast->createNodeUnaryOperator(NODE_TYPE_OPERATOR_UNARY_NOT, sub->getMember(0)));
      auto neg2 = transformNode(_ast->createNodeUnaryOperator(NODE_TYPE_OPERATOR_UNARY_NOT, sub->getMember(1)));

      neg1 = _ast->optimizeNotExpression(neg1);
      neg2 = _ast->optimizeNotExpression(neg2);

      return _ast->createNodeBinaryOperator(NODE_TYPE_OPERATOR_NARY_AND, neg1, neg2);
    }

    node->changeMember(0, transformNode(sub));
    return node;
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
