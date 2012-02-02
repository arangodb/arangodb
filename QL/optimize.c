////////////////////////////////////////////////////////////////////////////////
/// @brief AST optimization functions
///
/// @file
///
/// DISCLAIMER
///
/// Copyright 2010-2012 triagens GmbH, Cologne, Germany
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
/// Copyright holder is triAGENS GmbH, Cologne, Germany
///
/// @author Jan Steemann
/// @author Copyright 2012, triagens GmbH, Cologne, Germany
////////////////////////////////////////////////////////////////////////////////

#include "ast-node.h"
#include "optimize.h"

// -----------------------------------------------------------------------------
// --SECTION--                                                 private functions
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @addtogroup QL
/// @{
////////////////////////////////////////////////////////////////////////////////


////////////////////////////////////////////////////////////////////////////////
/// @brief check whether a node is optimizable as an arithmetic operand
////////////////////////////////////////////////////////////////////////////////

bool QLOptimizeCanBeUsedAsArithmeticOperand (const QL_ast_node_t const *node) {
  switch (node->_type) {
    case QLNodeValueNumberDouble:
    case QLNodeValueNumberDoubleString:
    case QLNodeValueBool:
    case QLNodeValueNull:  // NULL is equal to 0 in this case, i.e. NULL + 1 == 1, NULL -1 == -1 etc.
      return true;
    default:
      return false;
  }
}

////////////////////////////////////////////////////////////////////////////////
/// @brief check whether a node is optimizable as a relational operand
////////////////////////////////////////////////////////////////////////////////

bool QLOptimizeCanBeUsedAsRelationalOperand (const QL_ast_node_t const *node) {
  switch (node->_type) {
    case QLNodeValueNumberDouble:
    case QLNodeValueNumberDoubleString:
    case QLNodeValueBool:
      return true;
    default:
      return false;
  }
}


////////////////////////////////////////////////////////////////////////////////
/// @brief check whether a node is optimizable as a logical operand
////////////////////////////////////////////////////////////////////////////////

bool QLOptimizeCanBeUsedAsLogicalOperand (const QL_ast_node_t const *node) {
  switch (node->_type) {
    case QLNodeValueNumberDouble:
    case QLNodeValueNumberDoubleString:
    case QLNodeValueBool:
    case QLNodeValueNull:
      return true;
    default:
      return false;
  }
}


////////////////////////////////////////////////////////////////////////////////
/// @brief return a node value, converted to a bool
////////////////////////////////////////////////////////////////////////////////

bool QLOptimizeGetBool (const QL_ast_node_t const *node) {
  double d;

  if (node->_type == QLNodeValueNumberDouble) {
    return (node->_value._doubleValue != 0.0 ? true : false);
  }

  if (node->_type == QLNodeValueNumberDoubleString) {
    d = TRI_DoubleString(node->_value._stringValue);
    if (TRI_errno() != TRI_ERROR_NO_ERROR && d != 0.0) {
      return true;
    }
    return (d != 0.0);
  }

  if (node->_type == QLNodeValueNumberInt) {
    return (node->_value._intValue != 0 ? true : false);
  }

  if (node->_type == QLNodeValueBool) {
    return (node->_value._boolValue ? true : false);
  }
  
  if (node->_type == QLNodeValueNull) {
    return false;
  }

  return false;
}


////////////////////////////////////////////////////////////////////////////////
/// @brief return a node value, converted to a double
////////////////////////////////////////////////////////////////////////////////

double QLOptimizeGetDouble (const QL_ast_node_t const *node) { 
  if (node->_type == QLNodeValueNumberDouble) {
    return node->_value._doubleValue;
  }

  if (node->_type == QLNodeValueNumberDoubleString) {
    return TRI_DoubleString(node->_value._stringValue);
  }

  if (node->_type == QLNodeValueNumberInt) {
    return (double) node->_value._intValue;
  }

  if (node->_type == QLNodeValueBool) {
    return (node->_value._boolValue ? 1.0 : 0.0);
  }

  if (node->_type == QLNodeValueNull) {
    return 0.0;
  }

  return 0.0;
}


////////////////////////////////////////////////////////////////////////////////
/// @brief convert a node to a null value node
////////////////////////////////////////////////////////////////////////////////

void QLOptimizeMakeValueNull (QL_ast_node_t *node) {
  node->_type               = QLNodeValueNull;
  node->_lhs                = 0;
  node->_rhs                = 0;
}


////////////////////////////////////////////////////////////////////////////////
/// @brief convert a node to a bool value node
////////////////////////////////////////////////////////////////////////////////

void QLOptimizeMakeValueBool (QL_ast_node_t *node, bool value) {
  node->_type               = QLNodeValueBool;
  node->_value._boolValue   = value;
  node->_lhs                = 0;
  node->_rhs                = 0;
}


////////////////////////////////////////////////////////////////////////////////
/// @brief convert a node to a double value node
////////////////////////////////////////////////////////////////////////////////

void QLOptimizeMakeValueNumberDouble (QL_ast_node_t *node, double value) {
  node->_type               = QLNodeValueNumberDouble;
  node->_value._doubleValue = value;
  node->_lhs                = 0;
  node->_rhs                = 0;
}


////////////////////////////////////////////////////////////////////////////////
/// @brief make a node a copy of another node
////////////////////////////////////////////////////////////////////////////////

void QLOptimizeClone (QL_ast_node_t *target, QL_ast_node_t *source) {
  target->_type    = source->_type;
  target->_value   = source->_value;
  target->_lhs     = source->_lhs;
  target->_rhs     = source->_rhs;
}

////////////////////////////////////////////////////////////////////////////////
/// @}
////////////////////////////////////////////////////////////////////////////////


// -----------------------------------------------------------------------------
// --SECTION--                                                  public functions
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @addtogroup QL
/// @{
////////////////////////////////////////////////////////////////////////////////


////////////////////////////////////////////////////////////////////////////////
/// @brief optimization function for unary operators
////////////////////////////////////////////////////////////////////////////////

void QLOptimizeUnaryOperator (QL_ast_node_t *node) {
  QL_ast_node_t *lhs;
  QL_ast_node_type_e type;

  lhs = node->_lhs;
  if (lhs == 0) {
    // node has no child 
    return;
  }
  
  type = node->_type;
  if (type != QLNodeUnaryOperatorMinus && 
      type != QLNodeUnaryOperatorPlus && 
      type != QLNodeUnaryOperatorNot) {
    return;
  }

  if (!QLOptimizeCanBeUsedAsLogicalOperand(lhs)) {
    // child node is not suitable for optimization
    return;
  }

  if (type == QLNodeUnaryOperatorPlus) {
    // unary plus. This will make the result a numeric value 
    QLOptimizeMakeValueNumberDouble(node, QLOptimizeGetDouble(lhs));
  } 
  else if (type == QLNodeUnaryOperatorMinus) {  
    // unary minus. This will make the result a numeric value
    node->_type = QLNodeValueNumberDouble;
    QLOptimizeMakeValueNumberDouble(node, 0.0 - QLOptimizeGetDouble(lhs));
  } 
  else if (type == QLNodeUnaryOperatorNot) {
    QLOptimizeMakeValueBool(node, !QLOptimizeGetBool(lhs));
  }
}


////////////////////////////////////////////////////////////////////////////////
/// @brief optimize an arithmetic operation
////////////////////////////////////////////////////////////////////////////////

void QLOptimizeArithmeticOperator (QL_ast_node_t *node) {
  double lhsValue, rhsValue;
  QL_ast_node_t *lhs, *rhs;
  QL_ast_node_type_e type;

  type = node->_type;
  lhs = node->_lhs;
  rhs = node->_rhs;

  if (QLOptimizeCanBeUsedAsArithmeticOperand(lhs) && QLOptimizeCanBeUsedAsArithmeticOperand(rhs)) {
    // both operands are constants and can be merged into one result
    lhsValue = QLOptimizeGetDouble(lhs);
    rhsValue = QLOptimizeGetDouble(rhs);

    if (type == QLNodeBinaryOperatorAdd) {
      // const + const ==> merge
      QLOptimizeMakeValueNumberDouble(node, lhsValue + rhsValue);
    } 
    else if (type == QLNodeBinaryOperatorSubtract) {
      // const - const ==> merge
      QLOptimizeMakeValueNumberDouble(node, lhsValue - rhsValue);
    } 
    else if (type == QLNodeBinaryOperatorMultiply) {
      // const * const ==> merge
      QLOptimizeMakeValueNumberDouble(node, lhsValue * rhsValue);
    } 
    else if (type == QLNodeBinaryOperatorDivide && rhsValue != 0.0) { 
      // ignore division by zero. div0 will be handled in JS
      // const / const ==> merge
      QLOptimizeMakeValueNumberDouble(node, lhsValue / rhsValue);
    } 
    else if (type == QLNodeBinaryOperatorModulus && rhsValue != 0.0) { 
      // ignore division by zero. div0 will be handled in JS
      // const % const ==> merge
      QLOptimizeMakeValueNumberDouble(node, fmod(lhsValue, rhsValue));
    } 
  }
  else if (QLOptimizeCanBeUsedAsArithmeticOperand(lhs)) {
    // only left operand is a constant
    lhsValue = QLOptimizeGetDouble(lhs);
    
    if (type == QLNodeBinaryOperatorAdd && lhsValue == 0.0) {
      // 0 + x ==> x
      // TODO: by adding 0, the result would become a double. Just copying over rhs is not enough!
      // QLOptimizeClone(node, rhs);
    }
    else if (type == QLNodeBinaryOperatorMultiply && lhsValue == 0.0) {
      // 0 * x ==> 0
      QLOptimizeMakeValueNumberDouble(node, 0.0);
    } 
    else if (type == QLNodeBinaryOperatorMultiply && lhsValue == 1.0) {
      // 1 * x ==> x
      // TODO: by adding 0, the result would become a double. Just copying over rhs is not enough!
      // QLOptimizeClone(node, rhs);
    } 
  }
  else if (QLOptimizeCanBeUsedAsArithmeticOperand(rhs)) {
    // only right operand is a constant
    rhsValue = QLOptimizeGetDouble(rhs);
    
    if (type == QLNodeBinaryOperatorAdd && rhsValue == 0.0) {
      // x + 0 ==> x
      // TODO: by adding 0, the result would become a double. Just copying over lhs is not enough!
      QLOptimizeClone(node, lhs);
    }
    else if (type == QLNodeBinaryOperatorSubtract && rhsValue == 0.0) {
      // x - 0 ==> x
      // TODO: by adding 0, the result would become a double. Just copying over lhs is not enough!
      QLOptimizeClone(node, lhs);
    }
    else if (type == QLNodeBinaryOperatorMultiply && rhsValue == 0.0) {
      // x * 0 ==> 0
      QLOptimizeMakeValueNumberDouble(node, 0.0);
    } 
    else if (type == QLNodeBinaryOperatorMultiply && rhsValue == 1.0) {
      // x * 1 ==> x
      // TODO: by adding 0, the result would become a double. Just copying over lhs is not enough!
      QLOptimizeClone(node, lhs);
    } 
  }
}


////////////////////////////////////////////////////////////////////////////////
/// @brief optimize a logical operation
////////////////////////////////////////////////////////////////////////////////

void QLOptimizeLogicalOperator (QL_ast_node_t *node) {
  bool lhsValue;
  QL_ast_node_t *lhs, *rhs;
  QL_ast_node_type_e type;

  type = node->_type;
  lhs  = node->_lhs;
  rhs  = node->_rhs;

  if (type == QLNodeBinaryOperatorAnd) {
    // logical and

    if (lhs->_type == QLNodeValueNull) {
      // NULL && r ==> NULL
      QLOptimizeMakeValueNull(node);
    }
    else if (QLOptimizeCanBeUsedAsLogicalOperand(lhs)) {
      lhsValue = QLOptimizeGetBool(lhs);
      if (lhsValue) { 
        // true && r ==> r
        QLOptimizeClone(node, rhs);
      } 
      else {
        // false && r ==> l (and l evals to false)
        QLOptimizeClone(node, lhs);
      }
    }
  }
  else if (type == QLNodeBinaryOperatorOr) {
    // logical or

    if (lhs->_type == QLNodeValueNull) {
      // NULL || r ==> r
      QLOptimizeClone(node, rhs);
    }
    else if (QLOptimizeCanBeUsedAsLogicalOperand(lhs)) {
      lhsValue = QLOptimizeGetBool(lhs);
      if (lhsValue) { 
        // true || r ==> true
        QLOptimizeMakeValueBool(node, true);
      }
      else {
        // false || r ==> r
        QLOptimizeClone(node, rhs);
      }
    }
  }
}


////////////////////////////////////////////////////////////////////////////////
/// @brief optimize a relational operation
////////////////////////////////////////////////////////////////////////////////

void QLOptimizeRelationalOperator (QL_ast_node_t *node) {
  double lhsValue, rhsValue;
  QL_ast_node_t *lhs, *rhs;
  QL_ast_node_type_e type;

  type = node->_type;
  lhs = node->_lhs;
  rhs = node->_rhs;

  if (QLOptimizeCanBeUsedAsRelationalOperand(lhs) && QLOptimizeCanBeUsedAsRelationalOperand(rhs)) {
    // both operands are constants and can be merged into one result
    lhsValue = QLOptimizeGetDouble(lhs);
    rhsValue = QLOptimizeGetDouble(rhs);

    if (type == QLNodeBinaryOperatorIdentical) { 
      QLOptimizeMakeValueBool(node, (lhsValue == rhsValue) && (lhs->_type == rhs->_type));
    } 
    else if (type == QLNodeBinaryOperatorUnidentical) { 
      QLOptimizeMakeValueBool(node, (lhsValue != rhsValue) || (lhs->_type != rhs->_type));
    } 
    else if (type == QLNodeBinaryOperatorEqual) {
      QLOptimizeMakeValueBool(node, lhsValue == rhsValue);
    } 
    else if (type == QLNodeBinaryOperatorUnequal) {
      QLOptimizeMakeValueBool(node, lhsValue != rhsValue);
    } 
    else if (type == QLNodeBinaryOperatorLess) {
      QLOptimizeMakeValueBool(node, lhsValue < rhsValue);
    } 
    else if (type == QLNodeBinaryOperatorGreater) {
      QLOptimizeMakeValueBool(node, lhsValue > rhsValue);
    } 
    else if (type == QLNodeBinaryOperatorLessEqual) {
      QLOptimizeMakeValueBool(node, lhsValue <= rhsValue);
    } 
    else if (type == QLNodeBinaryOperatorGreaterEqual) {
      QLOptimizeMakeValueBool(node, lhsValue >= rhsValue);
    } 
  }
}


////////////////////////////////////////////////////////////////////////////////
/// @brief optimization function for binary operators
////////////////////////////////////////////////////////////////////////////////

void QLOptimizeBinaryOperator (QL_ast_node_t *node) {
  if (QLAstNodeIsArithmeticOperator(node)) {
    // optimize arithmetic operation
    QLOptimizeArithmeticOperator(node);
  } 
  else if (QLAstNodeIsLogicalOperator(node)) {
    // optimize logical operation
    QLOptimizeLogicalOperator(node);
  } 
  else if (QLAstNodeIsRelationalOperator(node)) {
  // optimize relational operation
    QLOptimizeRelationalOperator(node);
  }
}


////////////////////////////////////////////////////////////////////////////////
/// @brief optimization function for the ternary operator
////////////////////////////////////////////////////////////////////////////////

void QLOptimizeTernaryOperator (QL_ast_node_t *node) {
  QL_ast_node_t *lhs, *rhs;
  bool lhsValue;

  // condition part
  lhs = node->_lhs;
   
  if (QLOptimizeCanBeUsedAsLogicalOperand(lhs)) {
    lhsValue = QLOptimizeGetBool(lhs);
    // true and false parts
    rhs = node->_rhs;
    if (lhsValue) {
      QLOptimizeClone(node, rhs->_lhs);
    }
    else {
      QLOptimizeClone(node, rhs->_rhs);
    }
  }
}


////////////////////////////////////////////////////////////////////////////////
/// @brief recursively optimize order by
////////////////////////////////////////////////////////////////////////////////

void QLOptimizeOrder (QL_ast_node_t *node) {
  QL_ast_node_t *next;
  
  next = node->_next;
  while (next != 0) {
    // lhs contains the order expression, rhs contains the sort order
    QLOptimizeExpression(next->_lhs);
    next = next->_next;
  }
}


////////////////////////////////////////////////////////////////////////////////
/// @brief recursively optimize nodes in an expression AST
////////////////////////////////////////////////////////////////////////////////

void QLOptimizeExpression (QL_ast_node_t *node) {
  QL_ast_node_type_e type;
  QL_ast_node_t *lhs, *rhs, *next;

  if (node == 0) {
    return;
  }
  
  type = node->_type;
  if (type == QLNodeContainerList) {
    next = node->_next;
    while (next) {
      if (!QLAstNodeIsValueNode(node)) {
        // no need to optimize value nodes
        QLOptimizeExpression(next);
      }
      next = next->_next;
    }
  }

  if (QLAstNodeIsValueNode(node)) {
    // exit early, no need to optimize value nodes
    return;
  }

  lhs = node->_lhs;
  if (lhs != 0) {
    QLOptimizeExpression(lhs);
  }

  rhs = node->_rhs;
  if (rhs != 0) {
    QLOptimizeExpression(rhs);
  }

  if (QLAstNodeIsUnaryOperator(node)) {
    QLOptimizeUnaryOperator(node);
  }
  else if (QLAstNodeIsBinaryOperator(node)) {
    QLOptimizeBinaryOperator(node);
  }
  else if (QLAstNodeIsTernaryOperator(node)) {
    QLOptimizeTernaryOperator(node);
  }
}


////////////////////////////////////////////////////////////////////////////////
/// @brief get the type of a query's SELECT part
////////////////////////////////////////////////////////////////////////////////

QL_ast_query_select_type_e QLOptimizeGetSelectType (const QL_ast_query_t *query) {
  QL_ast_node_t *selectNode = query->_select._base;

  if (selectNode == 0) {
    return QLQuerySelectTypeUndefined; 
  }

  if (selectNode->_type == QLNodeValueIdentifier && selectNode->_value._stringValue != 0) {
    char *alias = QLAstQueryGetPrimaryAlias(query);
    
    if (alias !=0 && strcmp(alias, selectNode->_value._stringValue) == 0) {
      // primary document alias specified as (only) SELECT part
      return QLQuerySelectTypeSimple;
    }
  } 

  // SELECT part must be evaluated for all rows
  return QLQuerySelectTypeEvaluated;
}


////////////////////////////////////////////////////////////////////////////////
/// @brief get the type of a query's WHERE condition
////////////////////////////////////////////////////////////////////////////////

QL_ast_query_where_type_e QLOptimizeGetWhereType (const QL_ast_query_t *query) {
  QL_ast_node_t *whereNode = query->_where._base;

  if (whereNode == 0) {
    // query does not have a WHERE part 
    return QLQueryWhereTypeAlwaysTrue;
  }
  
  if (QLAstNodeIsBooleanizable(whereNode)) {
    // WHERE part is constant
    if (QLOptimizeGetBool(whereNode)) {
      // WHERE is always true
      return QLQueryWhereTypeAlwaysTrue; 
    } 
      // WHERE is always false
    return QLQueryWhereTypeAlwaysFalse; 
  }

  // WHERE must be checked for all records
  return QLQueryWhereTypeMustEvaluate;
}


////////////////////////////////////////////////////////////////////////////////
/// @}
////////////////////////////////////////////////////////////////////////////////

// Local Variables:
// mode: outline-minor
// outline-regexp: "^\\(/// @brief\\|/// {@inheritDoc}\\|/// @addtogroup\\|// --SECTION--\\|/// @\\}\\)"
// End:

