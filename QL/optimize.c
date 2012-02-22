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

#include "QL/ast-node.h"
#include "QL/optimize.h"

// -----------------------------------------------------------------------------
// --SECTION--                                                 private functions
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @addtogroup QL
/// @{
////////////////////////////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////////////////////////
/// @brief Hash a member name for comparisons
////////////////////////////////////////////////////////////////////////////////

static uint64_t QLOptimizeGetMemberNameHash (QL_ast_node_t* node) {
  QL_ast_node_t *lhs, *rhs;
  uint64_t hashValue;

  lhs = node->_lhs;
  hashValue = TRI_FnvHashString(lhs->_value._stringValue);
  
  rhs = node->_rhs;
  node = rhs->_next;

  while (node) {
    hashValue ^= TRI_FnvHashString(node->_value._stringValue);
    node = node->_next;
  }

  return hashValue;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief Create a string from a member name
///
/// The result string may or may not include the collection name
////////////////////////////////////////////////////////////////////////////////

static TRI_string_buffer_t* QLOptimizeGetMemberNameString (QL_ast_node_t* node, 
                                                           bool includeCollection) {
  QL_ast_node_t *lhs, *rhs;
  TRI_string_buffer_t* buffer;

  buffer = (TRI_string_buffer_t*) TRI_Allocate(sizeof(TRI_string_buffer_t));
  if (!buffer) {
    return 0;
  }

  TRI_InitStringBuffer(buffer);

  if (includeCollection) {
    // add collection part
    lhs = node->_lhs;
    TRI_AppendStringStringBuffer(buffer, lhs->_value._stringValue);
    TRI_AppendCharStringBuffer(buffer, '.');
  }
  
  rhs = node->_rhs;
  node = rhs->_next;

  while (node) {
    // add individual name parts
    TRI_AppendStringStringBuffer(buffer, node->_value._stringValue);
    node = node->_next;
    if (node) {
      TRI_AppendCharStringBuffer(buffer, '.');
    }
  }

  return buffer;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief check whether a node is optimizable as an arithmetic operand
////////////////////////////////////////////////////////////////////////////////

bool QLOptimizeCanBeUsedAsArithmeticOperand (const QL_ast_node_t const* node) {
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

bool QLOptimizeCanBeUsedAsRelationalOperand (const QL_ast_node_t const* node) {
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

bool QLOptimizeCanBeUsedAsLogicalOperand (const QL_ast_node_t const* node) {
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

bool QLOptimizeGetBool (const QL_ast_node_t const* node) {
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

double QLOptimizeGetDouble (const QL_ast_node_t const* node) { 
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
/// @brief check if a document declaration is static or dynamic
////////////////////////////////////////////////////////////////////////////////

bool QLOptimizeIsStaticDocument (QL_ast_node_t* node) {
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
  if (node->_type == QLNodeReferenceCollectionAlias ||
      node->_type == QLNodeControlFunctionCall ||
      node->_type == QLNodeControlTernary ||
      node->_type == QLNodeContainerMemberAccess) {
    return false;
  }

  return true;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief convert a node to a null value node
////////////////////////////////////////////////////////////////////////////////

void QLOptimizeMakeValueNull (QL_ast_node_t* node) {
  node->_type               = QLNodeValueNull;
  node->_lhs                = 0;
  node->_rhs                = 0;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief convert a node to a bool value node
////////////////////////////////////////////////////////////////////////////////

void QLOptimizeMakeValueBool (QL_ast_node_t* node, bool value) {
  node->_type               = QLNodeValueBool;
  node->_value._boolValue   = value;
  node->_lhs                = 0;
  node->_rhs                = 0;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief convert a node to a double value node
////////////////////////////////////////////////////////////////////////////////

void QLOptimizeMakeValueNumberDouble (QL_ast_node_t* node, double value) {
  node->_type               = QLNodeValueNumberDouble;
  node->_value._doubleValue = value;
  node->_lhs                = 0;
  node->_rhs                = 0;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief make a node a copy of another node
////////////////////////////////////////////////////////////////////////////////

void QLOptimizeClone (QL_ast_node_t* target, QL_ast_node_t* source) {
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

void QLOptimizeUnaryOperator (QL_ast_node_t* node) {
  QL_ast_node_t* lhs;
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
    QLOptimizeMakeValueNumberDouble(node, 0.0 - QLOptimizeGetDouble(lhs));
  } 
  else if (type == QLNodeUnaryOperatorNot) {
    // logical !
    QLOptimizeMakeValueBool(node, !QLOptimizeGetBool(lhs));
  }
}

////////////////////////////////////////////////////////////////////////////////
/// @brief optimize an arithmetic operation
////////////////////////////////////////////////////////////////////////////////

void QLOptimizeArithmeticOperator (QL_ast_node_t* node) {
  double lhsValue, rhsValue;
  QL_ast_node_t *lhs, *rhs;
  QL_ast_node_type_e type;

  type = node->_type;
  lhs = node->_lhs;
  rhs = node->_rhs;

  if (QLOptimizeCanBeUsedAsArithmeticOperand(lhs) && 
      QLOptimizeCanBeUsedAsArithmeticOperand(rhs)) {
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

void QLOptimizeLogicalOperator (QL_ast_node_t* node) {
  bool lhsValue;
  QL_ast_node_t *lhs, *rhs;
  QL_ast_node_type_e type;

  type = node->_type;
  lhs  = node->_lhs;
  rhs  = node->_rhs;

  if (type == QLNodeBinaryOperatorAnd) {
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
  else if (type == QLNodeBinaryOperatorOr) {
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

static bool QLOptimizeStringComparison (QL_ast_node_t* node) {
  QL_ast_node_t *lhs, *rhs;
  QL_ast_node_type_e type;
  int compareResult;

  lhs = node->_lhs;
  rhs = node->_rhs;

  compareResult = strcmp(lhs->_value._stringValue, rhs->_value._stringValue);
  type = node->_type;

  if (type == QLNodeBinaryOperatorIdentical || type == QLNodeBinaryOperatorEqual) {
    QLOptimizeMakeValueBool(node, compareResult == 0);
    return true;
  }
  if (type == QLNodeBinaryOperatorUnidentical || type == QLNodeBinaryOperatorUnequal) {
    QLOptimizeMakeValueBool(node, compareResult != 0);
    return true;
  } 
  if (type == QLNodeBinaryOperatorGreater) {
    QLOptimizeMakeValueBool(node, compareResult > 0);
    return true;
  } 
  if (type == QLNodeBinaryOperatorGreaterEqual) {
    QLOptimizeMakeValueBool(node, compareResult >= 0);
    return true;
  } 
  if (type == QLNodeBinaryOperatorLess) {
    QLOptimizeMakeValueBool(node, compareResult < 0);
    return true;
  }
  if (type == QLNodeBinaryOperatorLessEqual) {
    QLOptimizeMakeValueBool(node, compareResult <= 0);
    return true;
  }
  return false;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief optimize a member comparison 
////////////////////////////////////////////////////////////////////////////////

static bool QLOptimizeMemberComparison (QL_ast_node_t* node) {
  QL_ast_node_t *lhs, *rhs;
  QL_ast_node_type_e type;
  bool isSameMember; 

  lhs = node->_lhs;
  rhs = node->_rhs;
  type = node->_type;

  isSameMember = (QLOptimizeGetMemberNameHash(lhs) == QLOptimizeGetMemberNameHash(rhs));
  if (isSameMember) {
    if (type == QLNodeBinaryOperatorIdentical || 
        type == QLNodeBinaryOperatorEqual || 
        type == QLNodeBinaryOperatorGreaterEqual ||
        type == QLNodeBinaryOperatorLessEqual) {
      QLOptimizeMakeValueBool(node, true);
      return true;
    }
    if (type == QLNodeBinaryOperatorUnidentical || 
        type == QLNodeBinaryOperatorUnequal || 
        type == QLNodeBinaryOperatorGreater ||
        type == QLNodeBinaryOperatorLess) {
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

void QLOptimizeRelationalOperator (QL_ast_node_t* node) {
  double lhsValue, rhsValue;
  QL_ast_node_t *lhs, *rhs;
  QL_ast_node_type_e type;
  
  lhs = node->_lhs;
  rhs = node->_rhs;
  type = node->_type;

  if (lhs->_type == QLNodeValueString && rhs->_type == QLNodeValueString) {
    // both operands are constant strings
    if (QLOptimizeStringComparison(node)) {
      return;
    }
  }

  if (lhs->_type == QLNodeContainerMemberAccess && 
      rhs->_type == QLNodeContainerMemberAccess) {
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

void QLOptimizeBinaryOperator (QL_ast_node_t* node) {
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

void QLOptimizeTernaryOperator (QL_ast_node_t* node) {
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
/// @brief optimize order by by removing constant parts
////////////////////////////////////////////////////////////////////////////////

void QLOptimizeOrder (QL_ast_node_t* node) {
  QL_ast_node_t* responsibleNode;

  responsibleNode = node;
  node = node->_next;
  while (node != 0) {
    // lhs contains the order expression, rhs contains the sort order
    QLOptimizeExpression(node->_lhs);
    if (QLAstNodeIsBooleanizable(node->_lhs)) {
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

void QLOptimizeExpression (QL_ast_node_t* node) {
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
/// @brief Reference count all collections in an AST part by walking it
/// recursively
///
/// For each found collection, the counter value will be increased by one.
/// Reference counting is necessary to detect which collections in the from
/// clause are not used in the select, where and order by operations. Unused
/// collections that are left or list join'd can be removed.
////////////////////////////////////////////////////////////////////////////////

static void QLOptimizeRefCountCollections (const QL_parser_context_t* context, 
                                           const QL_ast_node_t* node) {
  QL_ast_node_t *lhs, *rhs, *next;

  if (node == 0) {
    return; 
  }

  if (node->_type == QLNodeContainerList) {
    next = node->_next;
    while (next) {
      QLOptimizeRefCountCollections(context, next);
      next = next->_next;
    }
  }

  if (node->_type == QLNodeReferenceCollectionAlias) {
    QLAstQueryAddRefCount(context->_query, node->_value._stringValue);
  }

  lhs = node->_lhs;
  if (lhs != 0) {
    QLOptimizeRefCountCollections(context, lhs);
  }

  rhs = node->_rhs;
  if (rhs != 0) {
    QLOptimizeRefCountCollections(context, rhs);
  }
}

////////////////////////////////////////////////////////////////////////////////
/// @brief Reference count all used collections in a query
///
/// Reference counting is later used to remove unnecessary joins
////////////////////////////////////////////////////////////////////////////////

static void QLOptimizeCountRefs (const QL_parser_context_t* context) {
  QL_ast_node_t* next = 0;
  QL_ast_node_t* node = (QL_ast_node_t*) context->_query->_from._base;
  QL_ast_node_t* alias;

  if (context->_query->_from._collections._nrUsed < 2) {
    // we don't have a join, no need to refcount anything
    return;
  }

  // mark collections used in select, where and order
  QLOptimizeRefCountCollections(context, context->_query->_select._base);
  QLOptimizeRefCountCollections(context, context->_query->_where._base);
  QLOptimizeRefCountCollections(context, context->_query->_order._base);

  // mark collections used in on clauses
  node = node->_next;
  while (node != 0) {
    next = node->_next;
    if (next == 0) {
      break;
    }

    alias = (QL_ast_node_t*) ((QL_ast_node_t*) next->_lhs)->_rhs;
    if ((QLAstQueryGetRefCount(context->_query, alias->_value._stringValue) > 0) ||
        (next->_type == QLNodeJoinInner)) {
      QLOptimizeRefCountCollections(context, next->_rhs);
    }
    node = node->_next;
  }
}

////////////////////////////////////////////////////////////////////////////////
/// @brief optimize from/joins
////////////////////////////////////////////////////////////////////////////////

void QLOptimizeFrom (const QL_parser_context_t* context) {
  QL_ast_node_t* temp;
  QL_ast_node_t* alias;
  QL_ast_node_t* responsibleNode;
  QL_ast_node_t* next = 0;
  QL_ast_node_t* node = (QL_ast_node_t*) context->_query->_from._base;

  QLOptimizeCountRefs(context);

  responsibleNode = node;
  node = node->_next;

  // iterate over all joins
  while (node != 0) {
    if (node->_rhs) {
      // optimize on clause
      QLOptimizeExpression(node->_rhs);
      if (node->_type == QLNodeJoinInner &&
          QLOptimizeGetWhereType(node->_rhs) == QLQueryWhereTypeAlwaysFalse) {
        // inner join condition is always false, query will have no results
        // TODO: set marker that query is empty
      }
    }
    next = node->_next;
    if (next == 0) {
      break;
    }

    assert(next->_lhs);

    alias = (QL_ast_node_t*) ((QL_ast_node_t*) next->_lhs)->_rhs;
    if ((QLAstQueryGetRefCount(context->_query, alias->_value._stringValue) < 1) &&
        (next->_type == QLNodeJoinLeft || 
         next->_type == QLNodeJoinRight || 
         next->_type == QLNodeJoinList)) {
      // remove unused list or outer joined collections
      // move joined collection one up
      node->_next = next->_next;
      // continue at the same position as the new collection at the current
      //  position might also be removed if it is useless
      continue;
    }

    if (next->_type == QLNodeJoinRight) {
      // convert a right join into a left join
      next->_type = QLNodeJoinLeft;
      temp = next->_lhs;
      node->_next = 0;
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

    if ((range->_valueType == RANGE_TYPE_STRING || range->_valueType == RANGE_TYPE_JSON)
        && range->_minValue._stringValue) {
      TRI_FreeString(range->_minValue._stringValue);
    }
    if ((range->_valueType == RANGE_TYPE_STRING || range->_valueType == RANGE_TYPE_JSON) 
        && range->_maxValue._stringValue) {
      TRI_FreeString(range->_maxValue._stringValue);
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

static TRI_vector_pointer_t* QLOptimizeCombineRanges (const QL_ast_node_type_e type, 
                                                      QL_ast_node_t* node, 
                                                      TRI_vector_pointer_t* ranges) {
  TRI_vector_pointer_t* vector;
  QL_optimize_range_t* range;
  QL_optimize_range_t* previous;
  size_t i;
  int compareResult;

  vector = (TRI_vector_pointer_t*) TRI_Allocate(sizeof(TRI_vector_pointer_t));
  if (!vector) {
    return NULL;
  }

  TRI_InitVectorPointer(vector);

  for (i = 0; i < ranges->_length; i++) {
    range = (QL_optimize_range_t*) ranges->_buffer[i];

    if (!range) {
      if (type == QLNodeBinaryOperatorAnd) {
        goto INVALIDATE_NODE;
      }

      continue;
    }

    assert(range);

    if (type == QLNodeBinaryOperatorAnd) {
      if (range->_minStatus == RANGE_VALUE_INFINITE && 
          range->_maxStatus == RANGE_VALUE_INFINITE) {
        // ignore !== and != operators in logical &&
        continue;
      }
    }


    previous = QLOptimizeGetRangeByHash(range->_hash, vector);
    if (type == QLNodeBinaryOperatorOr) {
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

    if (type == QLNodeBinaryOperatorOr) {
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

static QL_optimize_range_t* QLOptimizeCreateRange (QL_ast_node_t* memberNode,
                                                   QL_ast_node_t* valueNode,
                                                   const QL_ast_node_type_e type) {
  QL_optimize_range_t* range;
  TRI_string_buffer_t* name;
  QL_ast_node_t* lhs;
  QL_javascript_conversion_t* documentJs;

  // get the field name 
  name = QLOptimizeGetMemberNameString(memberNode, false);
  if (!name) {
    return NULL;
  }

  range = (QL_optimize_range_t*) TRI_Allocate(sizeof(QL_optimize_range_t));
  if (!range) {
    // clean up
    TRI_FreeStringBuffer(name);
    TRI_Free(name);
    return NULL;
  }

  range->_collection           = NULL;
  range->_field                = NULL;
  range->_refValue._field      = NULL;
  range->_refValue._collection = NULL;

  // get value
  if (valueNode->_type == QLNodeValueNumberDouble || 
      valueNode->_type == QLNodeValueNumberDoubleString) {
    // range is of type double
    range->_valueType = RANGE_TYPE_DOUBLE;
  }
  else if (valueNode->_type == QLNodeValueString) {
    // range is of type string
    range->_valueType = RANGE_TYPE_STRING;
  }
  else if (valueNode->_type == QLNodeValueDocument) {
    range->_valueType = RANGE_TYPE_JSON;
  }
  else if (valueNode->_type == QLNodeContainerMemberAccess) {
    range->_valueType = RANGE_TYPE_FIELD;
  }
  else {
    assert(false);
  }

  // store collection, field name and hash
  lhs = memberNode->_lhs;
  range->_collection = TRI_DuplicateString(lhs->_value._stringValue);
  range->_field      = TRI_DuplicateString(name->_buffer);
  range->_hash       = QLOptimizeGetMemberNameHash(memberNode);

  // we can now free the temporary name buffer
  TRI_FreeStringBuffer(name);
  TRI_Free(name);

  if (type == QLNodeBinaryOperatorIdentical || 
      type == QLNodeBinaryOperatorEqual) {
    // === and == ,  range is [ value (inc) ... value (inc) ]
    if (range->_valueType == RANGE_TYPE_FIELD) {
      range->_refValue._collection = TRI_DuplicateString(
        ((QL_ast_node_t*) valueNode->_lhs)->_value._stringValue);
      name = QLOptimizeGetMemberNameString(valueNode, false);
      if (name) {
        range->_refValue._field = TRI_DuplicateString(name->_buffer);
        TRI_FreeStringBuffer(name);
        TRI_Free(name);
      }
    }
    else if (range->_valueType == RANGE_TYPE_DOUBLE) {
      range->_minValue._doubleValue = QLOptimizeGetDouble(valueNode);
      range->_maxValue._doubleValue = range->_minValue._doubleValue;
    }
    else if (range->_valueType == RANGE_TYPE_STRING) { 
      range->_minValue._stringValue = TRI_DuplicateString(valueNode->_value._stringValue);
      range->_maxValue._stringValue = range->_minValue._stringValue;
    }
    else if (range->_valueType == RANGE_TYPE_JSON) {
      documentJs = QLJavascripterInit();
      if (!documentJs) {
        TRI_FreeStringBuffer(name);
        TRI_Free(name);
        TRI_Free(range);
        return NULL;
      }
      QLJavascripterConvert(documentJs, valueNode);
      range->_minValue._stringValue = documentJs->_buffer->_buffer;
      range->_maxValue._stringValue = range->_minValue._stringValue;
      QLJavascripterFree(documentJs);
    }
    range->_minStatus = RANGE_VALUE_INCLUDED;
    range->_maxStatus = RANGE_VALUE_INCLUDED;
  }
  else if (type == QLNodeBinaryOperatorUnidentical || 
           type == QLNodeBinaryOperatorUnequal) {
    // !== and != ,  range is [ -inf ... +inf ]
    range->_minStatus = RANGE_VALUE_INFINITE;
    range->_maxStatus = RANGE_VALUE_INFINITE;
  }
  else if (type == QLNodeBinaryOperatorGreaterEqual || 
           type == QLNodeBinaryOperatorGreater) {
    // >= and > ,  range is [ value ... +inf ]
    if (range->_valueType == RANGE_TYPE_DOUBLE) {
      range->_minValue._doubleValue = QLOptimizeGetDouble(valueNode);
    }
    else if (range->_valueType == RANGE_TYPE_STRING) {
      range->_minValue._stringValue = TRI_DuplicateString(valueNode->_value._stringValue);
    }

    if (type == QLNodeBinaryOperatorGreaterEqual) {
      // value is included (>=),  range is [ value (inc) ... +inf ]
      range->_minStatus = RANGE_VALUE_INCLUDED;
    }
    else {
      // value is excluded (>),  range is [ value (enc) ... +inf ]
      range->_minStatus = RANGE_VALUE_EXCLUDED;
    }
    range->_maxStatus = RANGE_VALUE_INFINITE;
  }
  else if (type == QLNodeBinaryOperatorLessEqual || 
           type == QLNodeBinaryOperatorLess) {
    // <= and < ,  range is [ -inf ... value ]
    if (range->_valueType == RANGE_TYPE_DOUBLE) {
      range->_maxValue._doubleValue = QLOptimizeGetDouble(valueNode);
    }
    else if (range->_valueType == RANGE_TYPE_STRING) {
      range->_maxValue._stringValue = TRI_DuplicateString(valueNode->_value._stringValue);
    }

    range->_minStatus = RANGE_VALUE_INFINITE;
    if (type == QLNodeBinaryOperatorLessEqual) {
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

TRI_vector_pointer_t* QLOptimizeCondition (QL_ast_node_t* node) {
  QL_ast_node_t *lhs, *rhs;
  TRI_vector_pointer_t* ranges;
  TRI_vector_pointer_t* combinedRanges;
  QL_ast_node_type_e type;

  if (node == 0) {
    return 0;
  }

  if (QLAstNodeIsValueNode(node)) {
    return 0;
  }

  type = node->_type;
  lhs  = node->_lhs;
  rhs  = node->_rhs;

  if (type == QLNodeBinaryOperatorAnd || type == QLNodeBinaryOperatorOr) {
    // logical && or logical ||

    // get the range vectors from both operands
    ranges = QLOptimizeMergeRangeVectors(QLOptimizeCondition(lhs), 
                                         QLOptimizeCondition(rhs));
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
  else if (type == QLNodeBinaryOperatorIdentical || 
           type == QLNodeBinaryOperatorUnidentical ||
           type == QLNodeBinaryOperatorEqual ||
           type == QLNodeBinaryOperatorUnequal ||
           type == QLNodeBinaryOperatorLess ||
           type == QLNodeBinaryOperatorGreater ||
           type == QLNodeBinaryOperatorLessEqual ||
           type == QLNodeBinaryOperatorGreaterEqual) {
    // comparison operator 
    if (lhs->_type == QLNodeContainerMemberAccess && 
        rhs->_type == QLNodeContainerMemberAccess) {
      // collection.attribute relop collection.attribute
      return QLOptimizeMergeRangeVectors(
        QLOptimizeCreateRangeVector(QLOptimizeCreateRange(lhs, rhs, type)),
        QLOptimizeCreateRangeVector(QLOptimizeCreateRange(rhs, lhs, type))
      );
    }
    else if (lhs->_type == QLNodeContainerMemberAccess &&
        (type == QLNodeBinaryOperatorIdentical || 
         type == QLNodeBinaryOperatorEqual) &&
        rhs->_type == QLNodeValueDocument && 
        QLOptimizeIsStaticDocument(rhs)) {
      // collection.attribute == document
      return QLOptimizeCreateRangeVector(QLOptimizeCreateRange(lhs, rhs, type));
    }
    else if (lhs->_type == QLNodeContainerMemberAccess && 
        (rhs->_type == QLNodeValueNumberDouble || 
         rhs->_type == QLNodeValueNumberDoubleString || 
         rhs->_type == QLNodeValueString)) {
      // collection.attribute relop value
      return QLOptimizeCreateRangeVector(QLOptimizeCreateRange(lhs, rhs, type));
    }
    else if (rhs->_type == QLNodeContainerMemberAccess &&
             (type == QLNodeBinaryOperatorIdentical ||
              type == QLNodeBinaryOperatorEqual) &&
             lhs->_type == QLNodeValueDocument &&
             QLOptimizeIsStaticDocument(lhs)) {
      // document == collection.attribute
      return QLOptimizeCreateRangeVector(QLOptimizeCreateRange(rhs, lhs, type));
    } else if (rhs->_type == QLNodeContainerMemberAccess && 
               (lhs->_type == QLNodeValueNumberDouble || 
                lhs->_type == QLNodeValueNumberDoubleString || 
                lhs->_type == QLNodeValueString)) { 
      // value relop collection.attrbiute 
      return QLOptimizeCreateRangeVector(
        QLOptimizeCreateRange(rhs, lhs, QLAstNodeGetReversedRelationalOperator(type)));
    }
  }

  return 0;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief get the type of a query's SELECT part
////////////////////////////////////////////////////////////////////////////////

QL_ast_query_select_type_e QLOptimizeGetSelectType (const QL_ast_query_t* query) {
  char* alias;
  QL_ast_node_t* selectNode = query->_select._base;

  if (selectNode == 0) {
    return QLQuerySelectTypeUndefined; 
  }

  if (selectNode->_type == QLNodeValueIdentifier && selectNode->_value._stringValue != 0) {
    alias = QLAstQueryGetPrimaryAlias(query);

    if (alias != 0 && strcmp(alias, selectNode->_value._stringValue) == 0) {
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

QL_ast_query_where_type_e QLOptimizeGetWhereType (const QL_ast_node_t* node) {
  if (node == 0) {
    // query does not have a WHERE part 
    return QLQueryWhereTypeAlwaysTrue;
  }

  if (QLAstNodeIsBooleanizable(node)) {
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

QL_ast_query_order_type_e QLOptimizeGetOrderType (const QL_ast_node_t* node) {
  QL_ast_node_t* condition;

  if (node == 0) {
    // query does not have an ORDER BY part 
    return QLQueryOrderTypeNone;
  }

  node = node->_next;
  while (node) {
    condition = (QL_ast_node_t*) node->_lhs;
    if (!QLAstNodeIsBooleanizable(condition)) {
      // ORDER BY must be evaluated for all records
      return QLQueryOrderTypeMustEvaluate;
    }

    node = node->_next;
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

