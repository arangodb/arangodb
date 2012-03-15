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

#include <BasicsC/logging.h>

#include "QL/optimize.h"

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

static bool QLOptimizeCanBeUsedAsArithmeticOperand (const TRI_query_node_t* const node) {
  switch (node->_type) {
    case TRI_QueryNodeValueNumberDouble:
    case TRI_QueryNodeValueNumberDoubleString:
    case TRI_QueryNodeValueBool:
    case TRI_QueryNodeValueNull:  // NULL is equal to 0 in this case, i.e. NULL + 1 == 1, NULL -1 == -1 etc.
      return true;
    default:
      return false;
  }
}

////////////////////////////////////////////////////////////////////////////////
/// @brief check whether a node is optimizable as a relational operand
////////////////////////////////////////////////////////////////////////////////

static bool QLOptimizeCanBeUsedAsRelationalOperand (const TRI_query_node_t* const node) {
  switch (node->_type) {
    case TRI_QueryNodeValueNumberDouble:
    case TRI_QueryNodeValueNumberDoubleString:
    case TRI_QueryNodeValueBool:
      return true;
    default:
      return false;
  }
}

////////////////////////////////////////////////////////////////////////////////
/// @brief check whether a node is optimizable as a logical operand
////////////////////////////////////////////////////////////////////////////////

static bool QLOptimizeCanBeUsedAsLogicalOperand (const TRI_query_node_t* const node) {
  switch (node->_type) {
    case TRI_QueryNodeValueNumberDouble:
    case TRI_QueryNodeValueNumberDoubleString:
    case TRI_QueryNodeValueBool:
    case TRI_QueryNodeValueNull:
      return true;
    default:
      return false;
  }
}

////////////////////////////////////////////////////////////////////////////////
/// @brief return a node value, converted to a bool
////////////////////////////////////////////////////////////////////////////////

static bool QLOptimizeGetBool (const TRI_query_node_t* const node) {
  double d;

  if (node->_type == TRI_QueryNodeValueNumberDouble) {
    return (node->_value._doubleValue != 0.0 ? true : false);
  }

  if (node->_type == TRI_QueryNodeValueNumberDoubleString) {
    d = TRI_DoubleString(node->_value._stringValue);
    if (TRI_errno() != TRI_ERROR_NO_ERROR && d != 0.0) {
      return true;
    }
    return (d != 0.0);
  }

  if (node->_type == TRI_QueryNodeValueNumberInt) {
    return (node->_value._intValue != 0 ? true : false);
  }

  if (node->_type == TRI_QueryNodeValueBool) {
    return (node->_value._boolValue ? true : false);
  }
  
  if (node->_type == TRI_QueryNodeValueNull) {
    return false;
  }

  return false;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief return a node value, converted to a double
////////////////////////////////////////////////////////////////////////////////

static double QLOptimizeGetDouble (const TRI_query_node_t* const node) { 
  if (node->_type == TRI_QueryNodeValueNumberDouble) {
    return node->_value._doubleValue;
  }

  if (node->_type == TRI_QueryNodeValueNumberDoubleString) {
    return TRI_DoubleString(node->_value._stringValue);
  }

  if (node->_type == TRI_QueryNodeValueNumberInt) {
    return (double) node->_value._intValue;
  }

  if (node->_type == TRI_QueryNodeValueBool) {
    return (node->_value._boolValue ? 1.0 : 0.0);
  }

  if (node->_type == TRI_QueryNodeValueNull) {
    return 0.0;
  }

  return 0.0;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief check if a document declaration is static or dynamic
////////////////////////////////////////////////////////////////////////////////

static bool QLOptimizeIsStaticDocument (const TRI_query_node_t* node) {
  bool result;

  if (node->_next) {
    while (node->_next) {
      result = QLOptimizeIsStaticDocument(node->_next);
      if (!result) {
        return false;
      }
      node = node->_next;
    }
    return true;
  }

  if (node->_lhs) {
    result = QLOptimizeIsStaticDocument(node->_lhs);
    if (!result) {
      return false;
    }
  }
  if (node->_rhs) {
    result = QLOptimizeIsStaticDocument(node->_rhs);
    if (!result) {
      return false;
    }
  }
  if (node->_type == TRI_QueryNodeReferenceCollectionAlias ||
      node->_type == TRI_QueryNodeControlFunctionCall ||
      node->_type == TRI_QueryNodeControlTernary ||
      node->_type == TRI_QueryNodeContainerMemberAccess) {
    return false;
  }

  return true;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief convert a node to a bool value node
////////////////////////////////////////////////////////////////////////////////

static void QLOptimizeMakeValueBool (TRI_query_node_t* node, 
                                     const bool value) {
  node->_type               = TRI_QueryNodeValueBool;
  node->_value._boolValue   = value;
  node->_lhs                = NULL;
  node->_rhs                = NULL;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief convert a node to a double value node
////////////////////////////////////////////////////////////////////////////////

static void QLOptimizeMakeValueNumberDouble (TRI_query_node_t* node, 
                                             const double value) {
  node->_type               = TRI_QueryNodeValueNumberDouble;
  node->_value._doubleValue = value;
  node->_lhs                = NULL;
  node->_rhs                = NULL;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief make a node a copy of another node
////////////////////////////////////////////////////////////////////////////////

static void QLOptimizeClone (TRI_query_node_t* target, 
                             const TRI_query_node_t* const source) {
  target->_type    = source->_type;
  target->_value   = source->_value;
  target->_lhs     = source->_lhs;
  target->_rhs     = source->_rhs;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief optimization function for unary operators
///
/// this function will optimize unary plus and unary minus operators that are
/// used together with constant values, e.g. it will merge the two nodes "-" and
/// "1" to a "-1".
////////////////////////////////////////////////////////////////////////////////

void QLOptimizeUnaryOperator (TRI_query_node_t* node) {
  TRI_query_node_t* lhs;
  TRI_query_node_type_e type;

  lhs = node->_lhs;
  if (!lhs) {
    // node has no child 
    return;
  }
  
  type = node->_type;
  if (type != TRI_QueryNodeUnaryOperatorMinus && 
      type != TRI_QueryNodeUnaryOperatorPlus && 
      type != TRI_QueryNodeUnaryOperatorNot) {
    return;
  }

  if (!QLOptimizeCanBeUsedAsLogicalOperand(lhs)) {
    // child node is not suitable for optimization
    return;
  }

  if (type == TRI_QueryNodeUnaryOperatorPlus) {
    // unary plus. This will make the result a numeric value 
    QLOptimizeMakeValueNumberDouble(node, QLOptimizeGetDouble(lhs));
  } 
  else if (type == TRI_QueryNodeUnaryOperatorMinus) {  
    // unary minus. This will make the result a numeric value
    QLOptimizeMakeValueNumberDouble(node, 0.0 - QLOptimizeGetDouble(lhs));
  } 
  else if (type == TRI_QueryNodeUnaryOperatorNot) {
    // logical !
    QLOptimizeMakeValueBool(node, !QLOptimizeGetBool(lhs));
  }
}

////////////////////////////////////////////////////////////////////////////////
/// @brief optimize an arithmetic operation
////////////////////////////////////////////////////////////////////////////////

void QLOptimizeArithmeticOperator (TRI_query_node_t* node) {
  double lhsValue, rhsValue;
  TRI_query_node_t *lhs, *rhs;
  TRI_query_node_type_e type;

  type = node->_type;
  lhs = node->_lhs;
  rhs = node->_rhs;

  if (QLOptimizeCanBeUsedAsArithmeticOperand(lhs) && 
      QLOptimizeCanBeUsedAsArithmeticOperand(rhs)) {
    // both operands are constants and can be merged into one result
    lhsValue = QLOptimizeGetDouble(lhs);
    rhsValue = QLOptimizeGetDouble(rhs);

    if (type == TRI_QueryNodeBinaryOperatorAdd) {
      // const + const ==> merge
      QLOptimizeMakeValueNumberDouble(node, lhsValue + rhsValue);
    } 
    else if (type == TRI_QueryNodeBinaryOperatorSubtract) {
      // const - const ==> merge
      QLOptimizeMakeValueNumberDouble(node, lhsValue - rhsValue);
    } 
    else if (type == TRI_QueryNodeBinaryOperatorMultiply) {
      // const * const ==> merge
      QLOptimizeMakeValueNumberDouble(node, lhsValue * rhsValue);
    } 
    else if (type == TRI_QueryNodeBinaryOperatorDivide && rhsValue != 0.0) { 
      // ignore division by zero. div0 will be handled in JS
      // const / const ==> merge
      QLOptimizeMakeValueNumberDouble(node, lhsValue / rhsValue);
    } 
    else if (type == TRI_QueryNodeBinaryOperatorModulus && rhsValue != 0.0) { 
      // ignore division by zero. div0 will be handled in JS
      // const % const ==> merge
      QLOptimizeMakeValueNumberDouble(node, fmod(lhsValue, rhsValue));
    } 
  }
  else if (QLOptimizeCanBeUsedAsArithmeticOperand(lhs)) {
    // only left operand is a constant
    lhsValue = QLOptimizeGetDouble(lhs);
    
    if (type == TRI_QueryNodeBinaryOperatorAdd && lhsValue == 0.0) {
      // 0 + x ==> x
      // TODO: by adding 0, the result would become a double. Just copying over rhs is not enough!
      // QLOptimizeClone(node, rhs);
    }
    else if (type == TRI_QueryNodeBinaryOperatorMultiply && lhsValue == 0.0) {
      // 0 * x ==> 0
      QLOptimizeMakeValueNumberDouble(node, 0.0);
    } 
    else if (type == TRI_QueryNodeBinaryOperatorMultiply && lhsValue == 1.0) {
      // 1 * x ==> x
      // TODO: by adding 0, the result would become a double. Just copying over rhs is not enough!
      // QLOptimizeClone(node, rhs);
    } 
  }
  else if (QLOptimizeCanBeUsedAsArithmeticOperand(rhs)) {
    // only right operand is a constant
    rhsValue = QLOptimizeGetDouble(rhs);
    
    if (type == TRI_QueryNodeBinaryOperatorAdd && rhsValue == 0.0) {
      // x + 0 ==> x
      // TODO: by adding 0, the result would become a double. Just copying over lhs is not enough!
      QLOptimizeClone(node, lhs);
    }
    else if (type == TRI_QueryNodeBinaryOperatorSubtract && rhsValue == 0.0) {
      // x - 0 ==> x
      // TODO: by adding 0, the result would become a double. Just copying over lhs is not enough!
      QLOptimizeClone(node, lhs);
    }
    else if (type == TRI_QueryNodeBinaryOperatorMultiply && rhsValue == 0.0) {
      // x * 0 ==> 0
      QLOptimizeMakeValueNumberDouble(node, 0.0);
    } 
    else if (type == TRI_QueryNodeBinaryOperatorMultiply && rhsValue == 1.0) {
      // x * 1 ==> x
      // TODO: by adding 0, the result would become a double. Just copying over lhs is not enough!
      QLOptimizeClone(node, lhs);
    } 
  }
}

////////////////////////////////////////////////////////////////////////////////
/// @brief optimize a logical operation
////////////////////////////////////////////////////////////////////////////////

void QLOptimizeLogicalOperator (TRI_query_node_t* node) {
  bool lhsValue;
  TRI_query_node_t *lhs, *rhs;
  TRI_query_node_type_e type;

  type = node->_type;
  lhs  = node->_lhs;
  rhs  = node->_rhs;

  if (type == TRI_QueryNodeBinaryOperatorAnd) {
    // logical and

    if (QLOptimizeCanBeUsedAsLogicalOperand(lhs)) {
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
  else if (type == TRI_QueryNodeBinaryOperatorOr) {
    // logical or

    if (QLOptimizeCanBeUsedAsLogicalOperand(lhs)) {
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
/// @brief optimize a constant string comparison
////////////////////////////////////////////////////////////////////////////////

static bool QLOptimizeStringComparison (TRI_query_node_t* node) {
  TRI_query_node_t *lhs, *rhs;
  TRI_query_node_type_e type;
  int compareResult;

  lhs = node->_lhs;
  rhs = node->_rhs;

  compareResult = strcmp(lhs->_value._stringValue, rhs->_value._stringValue);
  type = node->_type;

  if (type == TRI_QueryNodeBinaryOperatorIdentical || type == TRI_QueryNodeBinaryOperatorEqual) {
    QLOptimizeMakeValueBool(node, compareResult == 0);
    return true;
  }
  if (type == TRI_QueryNodeBinaryOperatorUnidentical || type == TRI_QueryNodeBinaryOperatorUnequal) {
    QLOptimizeMakeValueBool(node, compareResult != 0);
    return true;
  } 
  if (type == TRI_QueryNodeBinaryOperatorGreater) {
    QLOptimizeMakeValueBool(node, compareResult > 0);
    return true;
  } 
  if (type == TRI_QueryNodeBinaryOperatorGreaterEqual) {
    QLOptimizeMakeValueBool(node, compareResult >= 0);
    return true;
  } 
  if (type == TRI_QueryNodeBinaryOperatorLess) {
    QLOptimizeMakeValueBool(node, compareResult < 0);
    return true;
  }
  if (type == TRI_QueryNodeBinaryOperatorLessEqual) {
    QLOptimizeMakeValueBool(node, compareResult <= 0);
    return true;
  }
  return false;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief optimize a member comparison 
////////////////////////////////////////////////////////////////////////////////

static bool QLOptimizeMemberComparison (TRI_query_node_t* node) {
  TRI_query_node_t *lhs, *rhs;
  TRI_query_node_type_e type;
  bool isSameMember; 

  lhs = node->_lhs;
  rhs = node->_rhs;
  type = node->_type;

  isSameMember = (QLAstQueryGetMemberNameHash(lhs) == QLAstQueryGetMemberNameHash(rhs));
  if (isSameMember) {
    if (type == TRI_QueryNodeBinaryOperatorIdentical || 
        type == TRI_QueryNodeBinaryOperatorEqual || 
        type == TRI_QueryNodeBinaryOperatorGreaterEqual ||
        type == TRI_QueryNodeBinaryOperatorLessEqual) {
      QLOptimizeMakeValueBool(node, true);
      return true;
    }
    if (type == TRI_QueryNodeBinaryOperatorUnidentical || 
        type == TRI_QueryNodeBinaryOperatorUnequal || 
        type == TRI_QueryNodeBinaryOperatorGreater ||
        type == TRI_QueryNodeBinaryOperatorLess) {
      QLOptimizeMakeValueBool(node, false);
      return true;
    }
  }

  // caller function must handle this
  return false;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief optimize a relational operation
////////////////////////////////////////////////////////////////////////////////

void QLOptimizeRelationalOperator (TRI_query_node_t* node) {
  double lhsValue, rhsValue;
  TRI_query_node_t *lhs, *rhs;
  TRI_query_node_type_e type;
  
  lhs = node->_lhs;
  rhs = node->_rhs;
  type = node->_type;

  if (lhs->_type == TRI_QueryNodeValueString && rhs->_type == TRI_QueryNodeValueString) {
    // both operands are constant strings
    if (QLOptimizeStringComparison(node)) {
      return;
    }
  }

  if (lhs->_type == TRI_QueryNodeContainerMemberAccess && 
      rhs->_type == TRI_QueryNodeContainerMemberAccess) {
    // both operands are collections members (document properties)
    if (QLOptimizeMemberComparison(node)) {
      return;
    }
  }

  if (QLOptimizeCanBeUsedAsRelationalOperand(lhs) && 
      QLOptimizeCanBeUsedAsRelationalOperand(rhs)) {
    // both operands are constants and can be merged into one result
    lhsValue = QLOptimizeGetDouble(lhs);
    rhsValue = QLOptimizeGetDouble(rhs);

    if (type == TRI_QueryNodeBinaryOperatorIdentical) { 
      QLOptimizeMakeValueBool(node, (lhsValue == rhsValue) && (lhs->_type == rhs->_type));
    } 
    else if (type == TRI_QueryNodeBinaryOperatorUnidentical) { 
      QLOptimizeMakeValueBool(node, (lhsValue != rhsValue) || (lhs->_type != rhs->_type));
    } 
    else if (type == TRI_QueryNodeBinaryOperatorEqual) {
      QLOptimizeMakeValueBool(node, lhsValue == rhsValue);
    } 
    else if (type == TRI_QueryNodeBinaryOperatorUnequal) {
      QLOptimizeMakeValueBool(node, lhsValue != rhsValue);
    } 
    else if (type == TRI_QueryNodeBinaryOperatorLess) {
      QLOptimizeMakeValueBool(node, lhsValue < rhsValue);
    } 
    else if (type == TRI_QueryNodeBinaryOperatorGreater) {
      QLOptimizeMakeValueBool(node, lhsValue > rhsValue);
    } 
    else if (type == TRI_QueryNodeBinaryOperatorLessEqual) {
      QLOptimizeMakeValueBool(node, lhsValue <= rhsValue);
    } 
    else if (type == TRI_QueryNodeBinaryOperatorGreaterEqual) {
      QLOptimizeMakeValueBool(node, lhsValue >= rhsValue);
    } 
  }
}

////////////////////////////////////////////////////////////////////////////////
/// @brief optimization function for binary operators
///
/// this function will optimize arithmetic, relational and logical operators 
/// that are used together with constant values. It will replace the binary 
/// operator with the result of the optimization. 
/// The node will therefore change its type from a binary operator to a value
/// type node. The former sub nodes (lhs and rhs) of the binary operator will 
/// be unlinked, but not be freed here.
/// Freeing memory is done later when the whole query structure is deallocated.
////////////////////////////////////////////////////////////////////////////////

void QLOptimizeBinaryOperator (TRI_query_node_t* node) {
  if (TRI_QueryNodeIsArithmeticOperator(node)) {
    // optimize arithmetic operation
    QLOptimizeArithmeticOperator(node);
  } 
  else if (TRI_QueryNodeIsLogicalOperator(node)) {
    // optimize logical operation
    QLOptimizeLogicalOperator(node);
  } 
  else if (TRI_QueryNodeIsRelationalOperator(node)) {
    // optimize relational operation
    QLOptimizeRelationalOperator(node);
  }
}

////////////////////////////////////////////////////////////////////////////////
/// @brief optimization function for the ternary operator
///
/// this function will optimize the ternary operator if the conditional part is
/// reducible to a constant. It will substitute the condition with the true 
/// part if the condition is true, and with the false part if the condition is
/// false.
////////////////////////////////////////////////////////////////////////////////

void QLOptimizeTernaryOperator (TRI_query_node_t* node) {
  TRI_query_node_t *lhs, *rhs;
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

// -----------------------------------------------------------------------------
// --SECTION--                                                  public functions
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @brief check whether a query part uses bind parameters
////////////////////////////////////////////////////////////////////////////////

bool QLOptimizeUsesBindParameters (const TRI_query_node_t* const node) {
  TRI_query_node_t* next;

  if (!node) {
    return false;
  }

  next = node->_next;
  while (next) {
    if (QLOptimizeUsesBindParameters(next)) {
      return true;
    }
    next = next->_next;
  }

  if (node->_lhs) {
    if (QLOptimizeUsesBindParameters(node->_lhs)) {
      return true;
    }
  }

  if (node->_rhs) {
    if (QLOptimizeUsesBindParameters(node->_rhs)) {
      return true;
    }
  }

  if (node->_type == TRI_QueryNodeValueParameterNamed) {
    return true;
  }
  
  return false;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief optimize order by by removing constant parts
////////////////////////////////////////////////////////////////////////////////

void QLOptimizeOrder (TRI_query_node_t* node) {
  TRI_query_node_t* responsibleNode;

  responsibleNode = node;
  node = node->_next;
  while (node) {
    // lhs contains the order expression, rhs contains the sort order
    QLOptimizeExpression(node->_lhs);
    if (TRI_QueryNodeIsBooleanizable(node->_lhs)) {
      // skip constant parts in order by
      responsibleNode->_next = node->_next;
    }
    else {
      responsibleNode = node;
    }
    node = node->_next;
  }
}

////////////////////////////////////////////////////////////////////////////////
/// @brief recursively optimize nodes in an expression AST
////////////////////////////////////////////////////////////////////////////////

void QLOptimizeExpression (TRI_query_node_t* node) {
  TRI_query_node_type_e type;
  TRI_query_node_t *lhs, *rhs, *next;

  if (!node) {
    return;
  }

  type = node->_type;
  if (type == TRI_QueryNodeContainerList) {
    next = node->_next;
    while (next) {
      if (!TRI_QueryNodeIsValueNode(node)) {
        // no need to optimize value nodes
        QLOptimizeExpression(next);
      }
      next = next->_next;
    }
  }

  if (TRI_QueryNodeIsValueNode(node)) {
    // exit early, no need to optimize value nodes
    return;
  }

  lhs = node->_lhs;
  if (lhs) {
    QLOptimizeExpression(lhs);
  }

  rhs = node->_rhs;
  if (rhs) {
    QLOptimizeExpression(rhs);
  }

  if (TRI_QueryNodeIsUnaryOperator(node)) {
    QLOptimizeUnaryOperator(node);
  }
  else if (TRI_QueryNodeIsBinaryOperator(node)) {
    QLOptimizeBinaryOperator(node);
  }
  else if (TRI_QueryNodeIsTernaryOperator(node)) {
    QLOptimizeTernaryOperator(node);
  }
}

////////////////////////////////////////////////////////////////////////////////
/// @brief Reference count all collections in an AST part by walking it
/// recursively
///
/// For each found collection, the counter value will be increased by one.
/// Reference counting is necessary to detect which collections in the from
/// clause are not used in the select, where and order by operations. Unused
/// collections that are left or list join'd can be removed.
////////////////////////////////////////////////////////////////////////////////

static void QLOptimizeRefCountCollections (QL_ast_query_t* const query,
                                           const TRI_query_node_t* node,
                                           const QL_ast_query_ref_type_e type) {
  TRI_query_node_t* lhs;
  TRI_query_node_t* rhs;
  TRI_query_node_t* next;

  if (!node) {
    return; 
  }

  if (node->_type == TRI_QueryNodeContainerList) {
    next = node->_next;
    while (next) {
      QLOptimizeRefCountCollections(query, next, type);
      next = next->_next;
    }
  }

  if (node->_type == TRI_QueryNodeReferenceCollectionAlias) {
    QLAstQueryAddRefCount(query, node->_value._stringValue, type);
  }

  lhs = node->_lhs;
  if (lhs) {
    QLOptimizeRefCountCollections(query, lhs, type);
  }

  rhs = node->_rhs;
  if (rhs) {
    QLOptimizeRefCountCollections(query, rhs, type);
  }
}

////////////////////////////////////////////////////////////////////////////////
/// @brief Reference count all used collections in a query
///
/// Reference counting is later used to remove unnecessary joins and when 
/// performing materialization of collection data
////////////////////////////////////////////////////////////////////////////////

static void QLOptimizeCountRefs (QL_ast_query_t* const query) {
  TRI_query_node_t* next;
  TRI_query_node_t* node = query->_from._base;

  // mark collections used in select, where and order
  QLOptimizeRefCountCollections(query, query->_select._base, REF_TYPE_SELECT);
  QLOptimizeRefCountCollections(query, query->_where._base,  REF_TYPE_WHERE);
  QLOptimizeRefCountCollections(query, query->_order._base,  REF_TYPE_ORDER);

  // mark collections used in on clauses
  node = node->_next;
  while (node) {
    TRI_query_node_t* alias;
    next = node->_next;
    if (!next) {
      break;
    }

    alias = next->_lhs->_rhs;
    assert(alias);

    if ((QLAstQueryGetTotalRefCount(query, alias->_value._stringValue) > 0) ||
        (next->_type != TRI_QueryNodeJoinList)) {
      QLOptimizeRefCountCollections(query, next->_rhs, REF_TYPE_JOIN);
    }
    node = node->_next;
  }
}

////////////////////////////////////////////////////////////////////////////////
/// @brief optimize from/joins
////////////////////////////////////////////////////////////////////////////////

void QLOptimizeFrom (QL_ast_query_t* const query) {
  TRI_query_node_t* responsibleNode;
  TRI_query_node_t* next = NULL;
  TRI_query_node_t* node = query->_from._base;

  // count usages of collections in select, where and order clause
  QLOptimizeCountRefs(query);

  responsibleNode = node;
  node = node->_next;

  // iterate over all joins
  while (node) {
    TRI_query_node_t* alias;
    if (node->_rhs) {
      if (node->_type == TRI_QueryNodeJoinInner &&
          QLOptimizeGetWhereType(node->_rhs) == QLQueryWhereTypeAlwaysFalse) {
        // inner join condition is always false, query will have no results
        // set marker that query is empty
        query->_isEmpty = true;
      }
    }
    next = node->_next;
    if (!next) {
      break;
    }

    assert(next->_lhs);

    alias = next->_lhs->_rhs;
    if ((QLAstQueryGetTotalRefCount(query, alias->_value._stringValue) < 1) &&
        (next->_type == TRI_QueryNodeJoinList)) {
      // collection is joined but unused in select, where, order clause
      // remove unused list joined collections
      // move joined collection one up
      node->_next = next->_next;
      // continue at the same position as the new collection at the current
      //  position might also be removed if it is useless
      LOG_DEBUG("joined collection %s optimized away", alias->_value._stringValue);
      continue;
    }

    if (next->_type == TRI_QueryNodeJoinRight) {
      TRI_query_node_t* temp;
      // convert a right join into a left join
      next->_type = TRI_QueryNodeJoinLeft;
      temp = next->_lhs;
      node->_next = NULL;
      next->_lhs = node;
      temp->_next = next;
      responsibleNode->_next = temp;
      node = temp;
    }

    responsibleNode = node;
    node = node->_next;
  }
}

////////////////////////////////////////////////////////////////////////////////
/// @brief Find a specific range in a range vector
/// 
/// The value is looked up using its hash value. A value of 0 will be returned
/// to indicate the range is not contained in the vector. Otherwise, a value
/// of >= 1 will be returned that indicates the range's position in the vector.
////////////////////////////////////////////////////////////////////////////////

static QL_optimize_range_t* QLOptimizeGetRangeByHash (const uint64_t hash, 
                                                      TRI_vector_pointer_t* ranges) {
  QL_optimize_range_t* range;
  size_t i;

  assert(ranges);

  for (i = 0; i < ranges->_length; i++) {
    range = (QL_optimize_range_t*) ranges->_buffer[i];
    if (range && range->_hash == hash) {
      return range;
    }
  }

  // range is not contained in the vector  
  return NULL;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief Free all existing ranges in a range vector
////////////////////////////////////////////////////////////////////////////////

void QLOptimizeFreeRangeVector (TRI_vector_pointer_t* vector) {
  QL_optimize_range_t* range;
  size_t i;
  
  for (i = 0; i < vector->_length; i++) {
    range = (QL_optimize_range_t*) vector->_buffer[i];
    if (!range) {
      continue;
    }
    
    if (range->_collection) {
      TRI_Free(range->_collection);
    }
    
    if (range->_field) {
      TRI_Free(range->_field);
    }

    if (range->_refValue._collection) {
      TRI_FreeString(range->_refValue._collection);
    }
    
    if (range->_refValue._field) {
      TRI_FreeString(range->_refValue._field);
    }

    if (range->_valueType == RANGE_TYPE_JSON) {
      // minValue and maxValue point to the same string, just free it once!
      TRI_FreeString(range->_minValue._stringValue);
    }

    if (range->_valueType == RANGE_TYPE_STRING) {
      if (range->_minValue._stringValue) {
        TRI_FreeString(range->_minValue._stringValue);
        range->_minValue._stringValue = 0;
      }
      if (range->_maxValue._stringValue) {
        TRI_FreeString(range->_maxValue._stringValue);
        range->_maxValue._stringValue = 0;
      }
    }

    TRI_Free(range);
  }

  TRI_DestroyVectorPointer(vector);
}

////////////////////////////////////////////////////////////////////////////////
/// @brief Combine multiple ranges into less ranges if possible
///
/// Multiple ranges for the same field will be merged into one range if 
/// possible. Definitely senseless ranges will be removed and replaced by (bool)
/// false values. They can then be removed later by further expression 
/// optimization.
////////////////////////////////////////////////////////////////////////////////

static TRI_vector_pointer_t* QLOptimizeCombineRanges (const TRI_query_node_type_e type, 
                                                      TRI_query_node_t* node, 
                                                      TRI_vector_pointer_t* ranges) {
  TRI_vector_pointer_t* vector;
  QL_optimize_range_t* range;
  QL_optimize_range_t* previous;
  size_t i;
  int compareResult;

  vector = (TRI_vector_pointer_t*) TRI_Allocate(sizeof(TRI_vector_pointer_t));
  if (!vector) {
    QLOptimizeFreeRangeVector(ranges);
    TRI_Free(ranges);
    return NULL;
  }

  TRI_InitVectorPointer(vector);

  for (i = 0; i < ranges->_length; i++) {
    range = (QL_optimize_range_t*) ranges->_buffer[i];

    if (!range) {
      if (type == TRI_QueryNodeBinaryOperatorAnd) {
        goto INVALIDATE_NODE;
      }

      continue;
    }

    assert(range);

    if (type == TRI_QueryNodeBinaryOperatorAnd) {
      if (range->_minStatus == RANGE_VALUE_INFINITE && 
          range->_maxStatus == RANGE_VALUE_INFINITE) {
        // ignore !== and != operators in logical &&
        continue;
      }
    }


    previous = QLOptimizeGetRangeByHash(range->_hash, vector);
    if (type == TRI_QueryNodeBinaryOperatorOr) {
      // only use logical || operator for same field. if field name differs, an ||
      // effectively kills all ranges
      if (vector->_length >0 && !previous) { 
        QLOptimizeFreeRangeVector(vector);
        TRI_InitVectorPointer(vector);
        goto EXIT;
      }
    }

    if (!previous) {
      // push range into result vector
      TRI_PushBackVectorPointer(vector, range);

      // remove range from original vector to avoid double freeing
      ranges->_buffer[i] = NULL;
      continue;
    }

    if (type == TRI_QueryNodeBinaryOperatorOr) {
      // logical || operator
      if (range->_minStatus == RANGE_VALUE_INFINITE && 
          range->_maxStatus == RANGE_VALUE_INFINITE) {
        // !== and != operators in an || always set result range to infinite
        previous->_minStatus = range->_minStatus;
        previous->_maxStatus = range->_maxStatus;
        continue;
      }
      if ((previous->_maxStatus == RANGE_VALUE_INFINITE &&
           range->_minStatus == RANGE_VALUE_INFINITE) ||
          (previous->_minStatus == RANGE_VALUE_INFINITE &&
           range->_maxStatus == RANGE_VALUE_INFINITE)) {
        previous->_minStatus = RANGE_VALUE_INFINITE;
        previous->_maxStatus = RANGE_VALUE_INFINITE;
        continue;
      }

      if (previous->_valueType != range->_valueType) {
        // The two ranges have different data types. 
        // Thus set result range to infinite because we cannot merge these ranges
        previous->_minStatus = RANGE_VALUE_INFINITE;
        previous->_maxStatus = RANGE_VALUE_INFINITE;
        continue;
      }

      if (previous->_valueType == RANGE_TYPE_DOUBLE) {
        // combine two double ranges
        if (previous->_minStatus != RANGE_VALUE_INFINITE && 
            range->_minStatus != RANGE_VALUE_INFINITE) {
          if (range->_minValue._doubleValue <= previous->_minValue._doubleValue) {
            // adjust lower bound
            if (range->_minValue._doubleValue == previous->_minValue._doubleValue) {
              if (previous->_minStatus == RANGE_VALUE_INCLUDED ||
                  range->_minStatus == RANGE_VALUE_INCLUDED) {
                previous->_minStatus = RANGE_VALUE_INCLUDED;
              } 
              else {
                previous->_minStatus = RANGE_VALUE_EXCLUDED;
              }
            } 
            else {
              previous->_minStatus = range->_minStatus;
            }
            previous->_minValue._doubleValue = range->_minValue._doubleValue;
          }
        }
        if (previous->_maxStatus != RANGE_VALUE_INFINITE && 
            range->_maxStatus != RANGE_VALUE_INFINITE) {
          if (range->_maxValue._doubleValue >= previous->_maxValue._doubleValue) {
            // adjust upper bound
            if (range->_maxValue._doubleValue == previous->_maxValue._doubleValue) {
              if (previous->_maxStatus == RANGE_VALUE_INCLUDED || 
                  range->_maxStatus == RANGE_VALUE_INCLUDED) {
                previous->_maxStatus = RANGE_VALUE_INCLUDED;
              } 
              else {
                previous->_maxStatus = RANGE_VALUE_EXCLUDED;
              }
            } 
            else {
              previous->_maxStatus = range->_maxStatus;
            }
            previous->_maxValue._doubleValue = range->_maxValue._doubleValue;
          }
        }
      } 
      else if (previous->_valueType == RANGE_TYPE_STRING) {
        // combine two string ranges
        if (previous->_minStatus != RANGE_VALUE_INFINITE && 
            range->_minStatus != RANGE_VALUE_INFINITE) {
          compareResult = strcmp(range->_minValue._stringValue, 
                                 previous->_minValue._stringValue);
          if (compareResult <= 0) {
            // adjust lower bound
            if (compareResult == 0) {
              if (previous->_minStatus == RANGE_VALUE_INCLUDED ||
                  range->_minStatus == RANGE_VALUE_INCLUDED) {
                previous->_minStatus = RANGE_VALUE_INCLUDED;
              } 
              else {
                previous->_minStatus = RANGE_VALUE_EXCLUDED;
              }
            }
            else {
              previous->_minStatus = range->_minStatus;
            }

            if (compareResult == 0) {
              if (previous->_minStatus == RANGE_VALUE_INCLUDED || 
                  range->_minStatus == RANGE_VALUE_INCLUDED) {
                previous->_minStatus = RANGE_VALUE_INCLUDED;
              } 
              else {
                previous->_minStatus = RANGE_VALUE_EXCLUDED;
              }
            }
            else {
              previous->_minStatus = range->_minStatus;
            }
            previous->_minValue._stringValue = range->_minValue._stringValue;
          }
        }
        if (previous->_maxStatus != RANGE_VALUE_INFINITE && 
            range->_maxStatus != RANGE_VALUE_INFINITE) {
          compareResult = strcmp(range->_maxValue._stringValue, 
                                 previous->_maxValue._stringValue);
          if (compareResult >= 0) {
            // adjust upper bound
            if (compareResult == 0) {
              if (previous->_maxStatus == RANGE_VALUE_INCLUDED ||
                  range->_maxStatus == RANGE_VALUE_INCLUDED) {
                previous->_maxStatus = RANGE_VALUE_INCLUDED;
              }
              else {
                previous->_maxStatus = RANGE_VALUE_EXCLUDED;
              }
            }
            else {
              previous->_maxStatus = range->_maxStatus;
            }
            previous->_maxValue._stringValue = range->_maxValue._stringValue;
          }
        }
      }
    }
    else {
      // logical && operator
      if (previous->_valueType != range->_valueType) {
        // ranges have different data types. set result range to infinite
        previous->_minStatus = RANGE_VALUE_INFINITE;
        previous->_maxStatus = RANGE_VALUE_INFINITE;
        continue;
      }

      if (previous->_valueType == RANGE_TYPE_DOUBLE) {
        // combine two double ranges
        if (previous->_minStatus != RANGE_VALUE_INFINITE && 
            range->_maxStatus != RANGE_VALUE_INFINITE) {
          if (range->_maxValue._doubleValue < previous->_minValue._doubleValue || 
              (range->_maxValue._doubleValue <= previous->_minValue._doubleValue && 
               previous->_minStatus == RANGE_VALUE_EXCLUDED)) {
            // new upper bound is lower than previous lower bound => empty range
            // old:               |      |
            // new:     |      |
            goto INVALIDATE_NODE;
          }
        }
        if (previous->_maxStatus != RANGE_VALUE_INFINITE && 
            range->_minStatus != RANGE_VALUE_INFINITE) {
          if (range->_minValue._doubleValue < previous->_maxValue._doubleValue || 
              (range->_minValue._doubleValue <= previous->_maxValue._doubleValue && 
               previous->_maxStatus == RANGE_VALUE_EXCLUDED)) {
            // new lower bound is higher than previous upper bound => empty range
            // old:     |      |
            // new:               |      |
            goto INVALIDATE_NODE;
          }
        }

        if (previous->_minStatus != RANGE_VALUE_INFINITE) {
          if (range->_minStatus == RANGE_VALUE_INFINITE || 
              previous->_minValue._doubleValue > range->_minValue._doubleValue) {
            // adjust lower bound
            range->_minValue._doubleValue = previous->_minValue._doubleValue;
            range->_minStatus = previous->_minStatus;
          }
        }
        if (previous->_maxStatus != RANGE_VALUE_INFINITE) {
          if (range->_maxStatus == RANGE_VALUE_INFINITE || 
              previous->_maxValue._doubleValue < range->_maxValue._doubleValue) {
            // adjust upper bound
            range->_maxValue._doubleValue = previous->_maxValue._doubleValue;
            range->_maxStatus = previous->_maxStatus;
          }
        }

        if (range->_minStatus != RANGE_VALUE_INFINITE && 
            range->_maxStatus != RANGE_VALUE_INFINITE) {
          if (range->_minValue._doubleValue > range->_maxValue._doubleValue) {
            goto INVALIDATE_NODE;
          }
        }

        previous->_minValue._doubleValue = range->_minValue._doubleValue;
        previous->_maxValue._doubleValue = range->_maxValue._doubleValue;
        previous->_minStatus             = range->_minStatus;
        previous->_maxStatus             = range->_maxStatus;
      }
      else if (previous->_valueType == RANGE_TYPE_STRING) {
        // combine two string ranges
        if (previous->_minStatus != RANGE_VALUE_INFINITE && 
            range->_maxStatus != RANGE_VALUE_INFINITE) {
          compareResult = strcmp(range->_maxValue._stringValue, 
                                 previous->_minValue._stringValue);
          if (compareResult < 0 || 
              (compareResult <= 0 && previous->_minStatus == RANGE_VALUE_EXCLUDED)) {
            // new upper bound is lower than previous lower bound => empty range
            // old:               |      |
            // new:     |      |
            goto INVALIDATE_NODE;
          }
        }
        if (previous->_maxStatus != RANGE_VALUE_INFINITE && 
            range->_minStatus != RANGE_VALUE_INFINITE) {
          compareResult = strcmp(range->_minValue._stringValue, 
                                 previous->_maxValue._stringValue);
          if (compareResult < 0 || 
              (compareResult <= 0 && previous->_maxStatus == RANGE_VALUE_EXCLUDED)) {
            // new lower bound is higher than previous upper bound => empty range
            // old:     |      |
            // new:               |      |
            goto INVALIDATE_NODE;
          }
        }

        if (previous->_minStatus != RANGE_VALUE_INFINITE) {
          if (range->_minStatus == RANGE_VALUE_INFINITE) {
            compareResult = 1;
          } 
          else {
            compareResult = strcmp(previous->_minValue._stringValue, 
                                   range->_minValue._stringValue);
          }
          if (range->_minStatus == RANGE_VALUE_INFINITE || compareResult > 0) {
            // adjust lower bound
            range->_minValue._stringValue = previous->_minValue._stringValue;
            range->_minStatus = previous->_minStatus;
          }
        }
        if (previous->_maxStatus != RANGE_VALUE_INFINITE) {
          if (range->_maxStatus == RANGE_VALUE_INFINITE) {
            compareResult = -1;
          }
          else {
            compareResult = strcmp(previous->_maxValue._stringValue, 
                                   range->_maxValue._stringValue);
          }
          if (range->_maxStatus == RANGE_VALUE_INFINITE || compareResult < 0) {
            // adjust upper bound
            range->_maxValue._stringValue = previous->_maxValue._stringValue;
            range->_maxStatus = previous->_maxStatus;
          }
        }

        if (range->_minStatus != RANGE_VALUE_INFINITE && 
            range->_maxStatus != RANGE_VALUE_INFINITE) {
          compareResult = strcmp(range->_minValue._stringValue, 
                                 range->_maxValue._stringValue);
          if (compareResult > 0) {
            goto INVALIDATE_NODE;
          }
        }

        previous->_minValue._stringValue = range->_minValue._stringValue;
        previous->_maxValue._stringValue = range->_maxValue._stringValue;
        previous->_minStatus             = range->_minStatus;
        previous->_maxStatus             = range->_maxStatus;

      }
    }
  }

  goto EXIT;


INVALIDATE_NODE:
  QLOptimizeMakeValueBool(node, false);
  QLOptimizeFreeRangeVector(vector);
  TRI_InitVectorPointer(vector);
  // push nil pointer to indicate range is invalid
  TRI_PushBackVectorPointer(vector, NULL); 

EXIT:
  QLOptimizeFreeRangeVector(ranges);
  TRI_Free(ranges);

  return vector;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief Merge two range vectors into one
////////////////////////////////////////////////////////////////////////////////

static TRI_vector_pointer_t* QLOptimizeMergeRangeVectors (TRI_vector_pointer_t* left,
                                                          TRI_vector_pointer_t* right) {
  size_t i;

  if (!left && !right) {
    // both vectors invalid => nothing to do
    return NULL;
  }
  if (left && !right) {
    // left vector is valid, right is not => return left vector
    return left;
  }
  if (!left && right) {
    // right vector is valid, left is not => return right vector
    return right;
  }

  // both vectors are valid, move elements from right vector into left one
  for (i = 0; i < right->_length; i++) {
    TRI_PushBackVectorPointer(left, right->_buffer[i]);
  }

  TRI_DestroyVectorPointer(right);
  TRI_Free(right);

  return left;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief create a vector for ranges with one initial element
////////////////////////////////////////////////////////////////////////////////

static TRI_vector_pointer_t* QLOptimizeCreateRangeVector (QL_optimize_range_t* range) {
  TRI_vector_pointer_t* vector;

  if (!range) {
    return NULL;
  }
  
  vector = (TRI_vector_pointer_t*) TRI_Allocate(sizeof(TRI_vector_pointer_t));
  if (!vector) {
    return NULL;
  }

  TRI_PushBackVectorPointer(vector, range);
  return vector;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief create a value range from a name, relop, value combination
///
/// This function is called for each (name relop value) combination found.
/// The range will get a type matching the data type for the comparison.
/// Currently supported data types are doubles and strings.
/// The range will have a lower and an upper bound (minValue and maxValue), both 
/// of which can be infinite. 
///
/// The ranges will be composed as follows:
///
/// Comparison type                I/E  Lower value    Upper value  I/E   
/// -----------------------------------------------------------------------------
/// - equality   (field == value)    I  value                value  I
/// - unequality (field != value)    -  -inf                  +inf  - 
/// - greater    (field >  value)    E  value                 +inf  -
/// - greater eq (field >= value)    I  value                 +inf  -
/// - less       (field <  value)    -   -inf                value  E
/// - less eq    (field <= value)    -   -inf                value  I
///
/// "I" means that the value itself is included in the range.
/// "E" means that the value itself is excluded from the range.
/// "-" means "not relevant"
///
/// The ranges created are used later to combined for logical && and || 
/// operations and reduced to simpler or impossible ranges if possible.
////////////////////////////////////////////////////////////////////////////////

static QL_optimize_range_t* QLOptimizeCreateRange (TRI_query_node_t* memberNode,
                                                   TRI_query_node_t* valueNode,
                                                   const TRI_query_node_type_e type,
                                                   TRI_associative_pointer_t* bindParameters) {
  QL_optimize_range_t* range;
  TRI_string_buffer_t* name;
  TRI_query_node_t* lhs;
  TRI_query_javascript_converter_t* documentJs;

  // get the field name 
  name = QLAstQueryGetMemberNameString(memberNode, false);
  if (!name) {
    return NULL;
  }

  range = (QL_optimize_range_t*) TRI_Allocate(sizeof(QL_optimize_range_t));
  if (!range) {
    // clean up
    TRI_DestroyStringBuffer(name);
    TRI_Free(name);
    return NULL;
  }

  range->_collection           = NULL;
  range->_field                = NULL;
  range->_refValue._field      = NULL;
  range->_refValue._collection = NULL;

  // get value
  if (valueNode->_type == TRI_QueryNodeValueNumberDouble || 
      valueNode->_type == TRI_QueryNodeValueNumberDoubleString) {
    // range is of type double
    range->_valueType = RANGE_TYPE_DOUBLE;
  }
  else if (valueNode->_type == TRI_QueryNodeValueString) {
    // range is of type string
    range->_valueType = RANGE_TYPE_STRING;
  }
  else if (valueNode->_type == TRI_QueryNodeValueDocument || 
           valueNode->_type == TRI_QueryNodeValueArray) {
    range->_valueType = RANGE_TYPE_JSON;
  }
  else if (valueNode->_type == TRI_QueryNodeContainerMemberAccess) {
    range->_valueType = RANGE_TYPE_FIELD;
  }
  else {
    assert(false);
  }

  // store collection, field name and hash
  lhs = memberNode->_lhs;
  range->_collection = TRI_DuplicateString(lhs->_value._stringValue);
  range->_field      = TRI_DuplicateString(name->_buffer);
  range->_hash       = QLAstQueryGetMemberNameHash(memberNode);

  // we can now free the temporary name buffer
  TRI_DestroyStringBuffer(name);
  TRI_Free(name);

  if (type == TRI_QueryNodeBinaryOperatorIdentical || 
      type == TRI_QueryNodeBinaryOperatorEqual) {
    // === and == ,  range is [ value (inc) ... value (inc) ]
    if (range->_valueType == RANGE_TYPE_FIELD) {
      range->_refValue._collection = TRI_DuplicateString(
        ((TRI_query_node_t*) valueNode->_lhs)->_value._stringValue);
      name = QLAstQueryGetMemberNameString(valueNode, false);
      if (name) {
        range->_refValue._field = TRI_DuplicateString(name->_buffer);
        TRI_DestroyStringBuffer(name);
        TRI_Free(name);
      }
    }
    else if (range->_valueType == RANGE_TYPE_DOUBLE) {
      range->_minValue._doubleValue = QLOptimizeGetDouble(valueNode);
      range->_maxValue._doubleValue = range->_minValue._doubleValue;
    }
    else if (range->_valueType == RANGE_TYPE_STRING) { 
      range->_minValue._stringValue = TRI_DuplicateString(valueNode->_value._stringValue);
      range->_maxValue._stringValue = TRI_DuplicateString(valueNode->_value._stringValue);
    }
    else if (range->_valueType == RANGE_TYPE_JSON) {
      documentJs = TRI_InitQueryJavascript();
      if (!documentJs) {
        TRI_DestroyStringBuffer(name);
        TRI_Free(name);
        TRI_Free(range);
        return NULL;
      }
      TRI_ConvertQueryJavascript(documentJs, valueNode, bindParameters);
      range->_minValue._stringValue = TRI_DuplicateString(documentJs->_buffer->_buffer);
      range->_maxValue._stringValue = TRI_DuplicateString(documentJs->_buffer->_buffer);
      TRI_FreeQueryJavascript(documentJs);
      if (!range->_minValue._stringValue) {
        TRI_DestroyStringBuffer(name);
        TRI_Free(name);
        TRI_Free(range);
        return NULL;
      }
    }
    range->_minStatus = RANGE_VALUE_INCLUDED;
    range->_maxStatus = RANGE_VALUE_INCLUDED;
  }
  else if (type == TRI_QueryNodeBinaryOperatorUnidentical || 
           type == TRI_QueryNodeBinaryOperatorUnequal) {
    // !== and != ,  range is [ -inf ... +inf ]
    range->_minStatus = RANGE_VALUE_INFINITE;
    range->_maxStatus = RANGE_VALUE_INFINITE;
  }
  else if (type == TRI_QueryNodeBinaryOperatorGreaterEqual || 
           type == TRI_QueryNodeBinaryOperatorGreater) {
    // >= and > ,  range is [ value ... +inf ]
    if (range->_valueType == RANGE_TYPE_DOUBLE) {
      range->_minValue._doubleValue = QLOptimizeGetDouble(valueNode);
    }
    else if (range->_valueType == RANGE_TYPE_STRING) {
      range->_minValue._stringValue = TRI_DuplicateString(valueNode->_value._stringValue);
    }

    if (type == TRI_QueryNodeBinaryOperatorGreaterEqual) {
      // value is included (>=),  range is [ value (inc) ... +inf ]
      range->_minStatus = RANGE_VALUE_INCLUDED;
    }
    else {
      // value is excluded (>),  range is [ value (enc) ... +inf ]
      range->_minStatus = RANGE_VALUE_EXCLUDED;
    }
    range->_maxStatus = RANGE_VALUE_INFINITE;
  }
  else if (type == TRI_QueryNodeBinaryOperatorLessEqual || 
           type == TRI_QueryNodeBinaryOperatorLess) {
    // <= and < ,  range is [ -inf ... value ]
    if (range->_valueType == RANGE_TYPE_DOUBLE) {
      range->_maxValue._doubleValue = QLOptimizeGetDouble(valueNode);
    }
    else if (range->_valueType == RANGE_TYPE_STRING) {
      range->_maxValue._stringValue = TRI_DuplicateString(valueNode->_value._stringValue);
    }

    range->_minStatus = RANGE_VALUE_INFINITE;
    if (type == TRI_QueryNodeBinaryOperatorLessEqual) {
      // value is included (<=) ,  range is [ -inf ... value (inc) ]
      range->_maxStatus = RANGE_VALUE_INCLUDED;
    } 
    else {
      // value is excluded (<) ,  range is [ -inf ... value (exc) ]
      range->_maxStatus = RANGE_VALUE_EXCLUDED;
    }
  }

  return range;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief recursively optimize nodes in an expression AST
////////////////////////////////////////////////////////////////////////////////

TRI_vector_pointer_t* QLOptimizeRanges (TRI_query_node_t* node, 
                                        TRI_associative_pointer_t* bindParameters) {
  TRI_query_node_t *lhs, *rhs;
  TRI_vector_pointer_t* ranges;
  TRI_vector_pointer_t* combinedRanges;
  TRI_query_node_type_e type;

  if (!node) {
    return NULL;
  }

  if (TRI_QueryNodeIsValueNode(node)) {
    return NULL;
  }

  type = node->_type;
  lhs  = node->_lhs;
  rhs  = node->_rhs;

  if (type == TRI_QueryNodeBinaryOperatorAnd || type == TRI_QueryNodeBinaryOperatorOr) {
    // logical && or logical ||

    // get the range vectors from both operands
    ranges = QLOptimizeMergeRangeVectors(QLOptimizeRanges(lhs, bindParameters), 
                                         QLOptimizeRanges(rhs, bindParameters));
    if (ranges) {
      if (ranges->_length > 0) {
        // try to merge the ranges
        combinedRanges = QLOptimizeCombineRanges(type, node, ranges);
      } 
      else {
        combinedRanges = NULL;
      }
      return combinedRanges;
    }
  }
  else if (type == TRI_QueryNodeBinaryOperatorIdentical || 
           type == TRI_QueryNodeBinaryOperatorUnidentical ||
           type == TRI_QueryNodeBinaryOperatorEqual ||
           type == TRI_QueryNodeBinaryOperatorUnequal ||
           type == TRI_QueryNodeBinaryOperatorLess ||
           type == TRI_QueryNodeBinaryOperatorGreater ||
           type == TRI_QueryNodeBinaryOperatorLessEqual ||
           type == TRI_QueryNodeBinaryOperatorGreaterEqual) {
    // comparison operator 
    if (lhs->_type == TRI_QueryNodeContainerMemberAccess && 
        rhs->_type == TRI_QueryNodeContainerMemberAccess &&
        (type == TRI_QueryNodeBinaryOperatorIdentical ||
         type == TRI_QueryNodeBinaryOperatorEqual)) {
      // collection.attribute relop collection.attribute
      return QLOptimizeMergeRangeVectors(
        QLOptimizeCreateRangeVector(QLOptimizeCreateRange(lhs, rhs, type, bindParameters)),
        QLOptimizeCreateRangeVector(QLOptimizeCreateRange(rhs, lhs, type, bindParameters))
      );
    }
    else if (lhs->_type == TRI_QueryNodeContainerMemberAccess &&
        (type == TRI_QueryNodeBinaryOperatorIdentical || 
         type == TRI_QueryNodeBinaryOperatorEqual) &&
        (rhs->_type == TRI_QueryNodeValueDocument || rhs->_type == TRI_QueryNodeValueArray) && 
        QLOptimizeIsStaticDocument(rhs)) {
      // collection.attribute == document
      return QLOptimizeCreateRangeVector(QLOptimizeCreateRange(lhs, rhs, type, bindParameters));
    }
    else if (lhs->_type == TRI_QueryNodeContainerMemberAccess && 
        (rhs->_type == TRI_QueryNodeValueNumberDouble || 
         rhs->_type == TRI_QueryNodeValueNumberDoubleString || 
         rhs->_type == TRI_QueryNodeValueString)) {
      // collection.attribute relop value
      return QLOptimizeCreateRangeVector(QLOptimizeCreateRange(lhs, rhs, type, bindParameters));
    }
    else if (rhs->_type == TRI_QueryNodeContainerMemberAccess &&
             (type == TRI_QueryNodeBinaryOperatorIdentical ||
              type == TRI_QueryNodeBinaryOperatorEqual) &&
             lhs->_type == TRI_QueryNodeValueDocument &&
             QLOptimizeIsStaticDocument(lhs)) {
      // document == collection.attribute
      return QLOptimizeCreateRangeVector(QLOptimizeCreateRange(rhs, lhs, type, bindParameters));
    } else if (rhs->_type == TRI_QueryNodeContainerMemberAccess && 
               (lhs->_type == TRI_QueryNodeValueNumberDouble || 
                lhs->_type == TRI_QueryNodeValueNumberDoubleString || 
                lhs->_type == TRI_QueryNodeValueString)) { 
      // value relop collection.attrbiute 
      return QLOptimizeCreateRangeVector(
        QLOptimizeCreateRange(rhs, lhs, TRI_QueryNodeGetReversedRelationalOperator(type), bindParameters));
    }
  }

  return NULL;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief get the type of a query's SELECT part
////////////////////////////////////////////////////////////////////////////////

QL_ast_query_select_type_e QLOptimizeGetSelectType (const TRI_query_node_t* const selectNode, 
                                                    const char* primaryAlias) {
  if (!selectNode) {
    return QLQuerySelectTypeUndefined; 
  }

  if (selectNode->_type == TRI_QueryNodeValueIdentifier && 
      selectNode->_value._stringValue) {

    if (primaryAlias && strcmp(primaryAlias, selectNode->_value._stringValue) == 0) {
      // primary document alias specified as (only) SELECT part
      return QLQuerySelectTypeSimple;
    }
  } 

  // SELECT part must be evaluated for all rows
  return QLQuerySelectTypeEvaluated;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief get the type of a query's WHERE/ON condition
////////////////////////////////////////////////////////////////////////////////

QL_ast_query_where_type_e QLOptimizeGetWhereType (const TRI_query_node_t* const node) {
  if (!node) {
    // query does not have a WHERE part 
    return QLQueryWhereTypeAlwaysTrue;
  }

  if (TRI_QueryNodeIsBooleanizable(node)) {
    // WHERE part is constant
    if (QLOptimizeGetBool(node)) {
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
/// @brief get the type of a query's ORDER BY condition
////////////////////////////////////////////////////////////////////////////////

QL_ast_query_order_type_e QLOptimizeGetOrderType (const TRI_query_node_t* const node) {
  TRI_query_node_t* nodePtr;

  if (!node) {
    // query does not have an ORDER BY part 
    return QLQueryOrderTypeNone;
  }

  nodePtr = node->_next;
  while (nodePtr) {
    if (!TRI_QueryNodeIsBooleanizable(nodePtr->_lhs)) {
      // ORDER BY must be evaluated for all records
      return QLQueryOrderTypeMustEvaluate;
    }

    nodePtr = nodePtr->_next;
  }

  // ORDER BY is constant (same for all records) and can be ignored
  return QLQueryOrderTypeNone;
}

////////////////////////////////////////////////////////////////////////////////
/// @}
////////////////////////////////////////////////////////////////////////////////

// Local Variables:
// mode: outline-minor
// outline-regexp: "^\\(/// @brief\\|/// {@inheritDoc}\\|/// @addtogroup\\|// --SECTION--\\|/// @\\}\\)"
// End:

