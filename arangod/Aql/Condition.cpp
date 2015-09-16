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
#include "Aql/Variable.h"
#include "Basics/Exceptions.h"
#include "Basics/json.h"
#include "Basics/JsonHelper.h"

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
      
CompareResult ConditionPart::compare (ConditionPart const& other) const {
  if (! valueNode->isConstant() || ! other.valueNode->isConstant()) {
    return CompareResult::UNKNOWN;
  }
  
  if (operatorType == NODE_TYPE_OPERATOR_BINARY_EQ) {
    if (other.operatorType == NODE_TYPE_OPERATOR_BINARY_EQ ||
        other.operatorType == NODE_TYPE_OPERATOR_BINARY_NE) {
      int cmp = CompareAstNodes(valueNode, other.valueNode, false);
      if ((other.operatorType == NODE_TYPE_OPERATOR_BINARY_EQ && cmp != 0) ||
          (other.operatorType == NODE_TYPE_OPERATOR_BINARY_NE && cmp == 0)) {
        // a == x && a == y
        // a == x && a != y
        return CompareResult::IMPOSSIBLE;
      }
      if (other.operatorType == NODE_TYPE_OPERATOR_BINARY_EQ) {
        return CompareResult::SELF_CONTAINED_IN_OTHER;
      }
      return CompareResult::DISJOINT;
    }
    if (other.operatorType == NODE_TYPE_OPERATOR_BINARY_GT ||
        other.operatorType == NODE_TYPE_OPERATOR_BINARY_GE) {
      int cmp = CompareAstNodes(valueNode, other.valueNode, true);
      if ((other.operatorType == NODE_TYPE_OPERATOR_BINARY_GT && cmp <= 0) ||
          (other.operatorType == NODE_TYPE_OPERATOR_BINARY_GE && cmp < 0)) {
        // a == x && a > y
        // a == x && a >= y
        return CompareResult::IMPOSSIBLE;
      }
      return CompareResult::OTHER_CONTAINED_IN_SELF;
    }
    if (other.operatorType == NODE_TYPE_OPERATOR_BINARY_LT ||
        other.operatorType == NODE_TYPE_OPERATOR_BINARY_LE) {
      int cmp = CompareAstNodes(valueNode, other.valueNode, true);
      if ((other.operatorType == NODE_TYPE_OPERATOR_BINARY_LT && cmp >= 0) ||
          (other.operatorType == NODE_TYPE_OPERATOR_BINARY_LE && cmp > 0)) {
        // a == x && a < y
        // a == x && a <= y
        return CompareResult::IMPOSSIBLE;
      }
      return CompareResult::OTHER_CONTAINED_IN_SELF;
    }
  }
  
  return CompareResult::UNKNOWN;
}

void ConditionPart::dump () const {
  std::cout << "VARIABLE NAME: " << variable->name << "." << attributeName << " " << triagens::basics::JsonHelper::toString(valueNode->toJson(TRI_UNKNOWN_MEM_ZONE, false)) << "\n";
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
/// @brief normalize the condition
/// this will convert the condition into its disjunctive normal form
////////////////////////////////////////////////////////////////////////////////

void Condition::normalize () {
  if (_isNormalized) {
    // already normalized
    return;
  }

  std::function<AstNode*(AstNode*)> transformNode = [this, &transformNode] (AstNode* node) -> AstNode* {
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
  };
  
  // collapse function. 
  // this will collapse nested logical AND/OR nodes 
  std::function<AstNode*(AstNode*)> collapseNesting = [this, &collapseNesting] (AstNode* node) -> AstNode* {
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
  };

  // finally create a top-level OR node if it does not already exist, and make sure that all second
  // level nodes are AND nodes
  // additionally, this processing step will remove all NOP nodes
  std::function<AstNode*(AstNode*, int)> fixRoot = [this, &fixRoot] (AstNode* node, int level) -> AstNode* {
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
  };

  _root = transformNode(_root);
  _root = collapseNesting(_root);
  _root = fixRoot(_root, 0);

  optimize();
/*
std::cout << "\n";
dump();
std::cout << "\n";
*/
  _isNormalized = true;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief optimize the condition's expression tree
////////////////////////////////////////////////////////////////////////////////

void Condition::optimize () {

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
        dst.emplace_back(std::make_pair(position, side));
      }
    }
  };
  

  TRI_ASSERT(_root != nullptr && _root->type == NODE_TYPE_OPERATOR_NARY_OR);

  // handle sub nodes or top-level OR node
  size_t const n = _root->numMembers();

  for (size_t i = 0; i < n; ++i) {
    auto andNode = _root->getMemberUnchecked(i);

    TRI_ASSERT(andNode->type == NODE_TYPE_OPERATOR_NARY_AND);

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
      for (auto const& it : variableUsage) {
        auto variable = it.first;

        for (auto const& it2 : it.second) {
          auto const& attributeName = it2.first;
          auto const& positions = it2.second;

          if (positions.size() <= 1) {
            // none or only one occurence of the attribute
            continue;
          }

          // multiple occurrences of the same attribute
          // std::cout << "ATTRIBUTE " << attributeName << " occurs in " << positions.size() << " positions\n";

          ConditionPart current(variable, attributeName, 0, andNode->getMemberUnchecked(positions[0].first), positions[0].second);
          // current.dump();
          size_t j = 1;

          while (j < positions.size()) {
            ConditionPart other(variable, attributeName, j, andNode->getMemberUnchecked(positions[j].first), positions[j].second);

            switch (current.compare(other)) {
              case CompareResult::IMPOSSIBLE:
                // std::cout << "IMPOSSIBLE WHERE\n";
                break;
              case CompareResult::SELF_CONTAINED_IN_OTHER:
                // std::cout << "SELF IS CONTAINED IN OTHER\n";
                break;
              case CompareResult::OTHER_CONTAINED_IN_SELF:
                // std::cout << "OTHER IS CONTAINED IN SELF\n";
                break;
              case CompareResult::DISJOINT:
                // std::cout << "DISJOINT\n";
                break;
              case CompareResult::UNKNOWN:
                // std::cout << "UNKNOWN\n";
                break;
            }

            ++j;
          }
        }
      }

    }
  }

}

////////////////////////////////////////////////////////////////////////////////
/// @brief dump a condition
////////////////////////////////////////////////////////////////////////////////

void Condition::dump () const {
  std::function<void(AstNode const*, int)> dumpNode;

  dumpNode = [&dumpNode] (AstNode const* node, int indent) {
    if (node == nullptr) {
      return;
    }

    for (int i = 0; i < indent * 2; ++i) {
      std::cout << " ";
    }

    std::cout << node->getTypeString();
    if (node->type == NODE_TYPE_VALUE) {
      std::cout << "  (value " << triagens::basics::JsonHelper::toString(node->toJsonValue(TRI_UNKNOWN_MEM_ZONE)) << ")";
    }
    else if (node->type == NODE_TYPE_ATTRIBUTE_ACCESS) {
      std::cout << "  (attribute " << node->getStringValue() << ")";
    }

    std::cout << "\n";
    for (size_t i = 0; i < node->numMembers(); ++i) {
      dumpNode(node->getMember(i), indent + 1);
    }
  };

  dumpNode(_root, 0);
}


// -----------------------------------------------------------------------------
// --SECTION--                                                       END-OF-FILE
// -----------------------------------------------------------------------------

// Local Variables:
// mode: outline-minor
// outline-regexp: "/// @brief\\|/// {@inheritDoc}\\|/// @page\\|// --SECTION--\\|/// @\\}"
// End:
