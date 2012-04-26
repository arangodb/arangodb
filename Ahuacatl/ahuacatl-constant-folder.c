////////////////////////////////////////////////////////////////////////////////
/// @brief Ahuacatl, constant folding
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

#include "Ahuacatl/ahuacatl-constant-folder.h"

// -----------------------------------------------------------------------------
// --SECTION--                                                 private functions
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @addtogroup Ahuacatl
/// @{
////////////////////////////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////////////////////////
/// @brief optimise an arithmetic operation with one operand
////////////////////////////////////////////////////////////////////////////////

static TRI_aql_node_t* OptimiseUnaryArithmeticOperation (TRI_aql_context_t* const context,
                                                      TRI_aql_node_t* node) {
  TRI_aql_node_t* operand = (TRI_aql_node_t*) node->_subNodes._buffer[0];

  if (!operand || !TRI_IsConstantValueNodeAql(operand)) {
    return node;
  }

  if (!TRI_IsNumericValueNodeAql(operand)) {
    // todo: fix error message
    TRI_SetErrorAql(context, TRI_ERROR_OUT_OF_MEMORY, NULL);
    return node;
  }


  assert(node->_type == AQL_NODE_OPERATOR_UNARY_PLUS ||
         node->_type == AQL_NODE_OPERATOR_UNARY_MINUS);

  if (node->_type == AQL_NODE_OPERATOR_UNARY_PLUS) {
    // + number => number
    node = operand;
  }
  else if (node->_type == AQL_NODE_OPERATOR_UNARY_MINUS) {
    // - number => eval!
    node = TRI_CreateNodeValueDoubleAql(context, - TRI_GetNumericNodeValueAql(operand));
    if (!node) {
      TRI_SetErrorAql(context, TRI_ERROR_OUT_OF_MEMORY, NULL);
    }
  }

  return node;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief optimise a boolean operation with one operand
////////////////////////////////////////////////////////////////////////////////

static TRI_aql_node_t* OptimiseUnaryLogicalOperation (TRI_aql_context_t* const context,
                                                      TRI_aql_node_t* node) {
  TRI_aql_node_t* operand = (TRI_aql_node_t*) node->_subNodes._buffer[0];

  if (!operand || !TRI_IsConstantValueNodeAql(operand)) {
    return node;
  }

  if (!TRI_IsBooleanValueNodeAql(operand)) {
    // todo: fix error message
    TRI_SetErrorAql(context, TRI_ERROR_OUT_OF_MEMORY, NULL);
    return node;
  }
  
  assert(node->_type == AQL_NODE_OPERATOR_UNARY_NOT);

  if (node->_type == AQL_NODE_OPERATOR_UNARY_NOT) {
    // ! bool => evaluate
    node = TRI_CreateNodeValueBoolAql(context, ! TRI_GetBooleanNodeValueAql(operand));
    if (!node) {
      TRI_SetErrorAql(context, TRI_ERROR_OUT_OF_MEMORY, NULL);
    }
  }

  return node;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief optimise a boolean operation with two operands
////////////////////////////////////////////////////////////////////////////////

static TRI_aql_node_t* OptimiseBinaryLogicalOperation (TRI_aql_context_t* const context,
                                                       TRI_aql_node_t* node) {
  TRI_aql_node_t* lhs = (TRI_aql_node_t*) node->_subNodes._buffer[0];
  TRI_aql_node_t* rhs = (TRI_aql_node_t*) node->_subNodes._buffer[1];
  bool isEligibleLhs;
  bool isEligibleRhs;
  bool lhsValue;

  if (!lhs || !rhs) {
    return node;
  } 

  isEligibleLhs = TRI_IsConstantValueNodeAql(lhs);
  isEligibleRhs = TRI_IsConstantValueNodeAql(rhs);

  if (isEligibleLhs && !TRI_IsBooleanValueNodeAql(lhs)) {
    // todo: fix error message
    TRI_SetErrorAql(context, TRI_ERROR_OUT_OF_MEMORY, NULL);
    return node;
  }

  if (isEligibleRhs && !TRI_IsBooleanValueNodeAql(rhs)) {
    // todo: fix error message
    TRI_SetErrorAql(context, TRI_ERROR_OUT_OF_MEMORY, NULL);
    return node;
  }

  if (!isEligibleLhs || !isEligibleRhs) {
    return node;
  }

  lhsValue = TRI_GetBooleanNodeValueAql(lhs);
  
  assert(node->_type == AQL_NODE_OPERATOR_BINARY_AND ||
         node->_type == AQL_NODE_OPERATOR_BINARY_OR);

  if (node->_type == AQL_NODE_OPERATOR_BINARY_AND) {
    if (lhsValue) {
      // if (true && rhs) => rhs
      return rhs;
    }

    // if (false && rhs) => false
    return lhs;
  } 
  else if (node->_type == AQL_NODE_OPERATOR_BINARY_OR) {
    if (lhsValue) {
      // if (true || rhs) => true
      return lhs;
    }

    // if (false || rhs) => rhs
    return rhs;
  }

  assert(false);

  return node;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief optimise an arithmetic operation with two operands
////////////////////////////////////////////////////////////////////////////////

static TRI_aql_node_t* OptimiseBinaryArithmeticOperation (TRI_aql_context_t* const context,
                                                          TRI_aql_node_t* node) {
  TRI_aql_node_t* lhs = (TRI_aql_node_t*) node->_subNodes._buffer[0];
  TRI_aql_node_t* rhs = (TRI_aql_node_t*) node->_subNodes._buffer[1];
  bool isEligibleLhs;
  bool isEligibleRhs;
  double value;
  
  if (!lhs || !rhs) {
    return node;
  } 

  isEligibleLhs = TRI_IsConstantValueNodeAql(lhs);
  isEligibleRhs = TRI_IsConstantValueNodeAql(rhs);
  
  if (isEligibleLhs && !TRI_IsNumericValueNodeAql(lhs)) {
    // todo: fix error message
    TRI_SetErrorAql(context, TRI_ERROR_OUT_OF_MEMORY, NULL);
    return node;
  }
  
  if (isEligibleRhs && !TRI_IsNumericValueNodeAql(rhs)) {
    // todo: fix error message
    TRI_SetErrorAql(context, TRI_ERROR_OUT_OF_MEMORY, NULL);
    return node;
  }
  
  if (TRI_IsConstantValueNodeAql(rhs) && !TRI_IsNumericValueNodeAql(rhs)) {
    // todo: fix error message
    TRI_SetErrorAql(context, TRI_ERROR_OUT_OF_MEMORY, NULL);
    return node;
  }
  
  if (!isEligibleLhs || !isEligibleRhs) {
    return node;
  }
  
  assert(node->_type == AQL_NODE_OPERATOR_BINARY_PLUS ||
         node->_type == AQL_NODE_OPERATOR_BINARY_MINUS ||
         node->_type == AQL_NODE_OPERATOR_BINARY_TIMES ||
         node->_type == AQL_NODE_OPERATOR_BINARY_DIV ||
         node->_type == AQL_NODE_OPERATOR_BINARY_MOD);

  if (node->_type == AQL_NODE_OPERATOR_BINARY_PLUS) {
    value = TRI_GetNumericNodeValueAql(lhs) + TRI_GetNumericNodeValueAql(rhs);
  }
  else if (node->_type == AQL_NODE_OPERATOR_BINARY_MINUS) {
    value = TRI_GetNumericNodeValueAql(lhs) - TRI_GetNumericNodeValueAql(rhs);
  }
  else if (node->_type == AQL_NODE_OPERATOR_BINARY_TIMES) {
    value = TRI_GetNumericNodeValueAql(lhs) * TRI_GetNumericNodeValueAql(rhs);
  }
  else if (node->_type == AQL_NODE_OPERATOR_BINARY_DIV) {
    if (TRI_GetNumericNodeValueAql(rhs) == 0.0) {
      // todo: fix error message
      TRI_SetErrorAql(context, TRI_ERROR_OUT_OF_MEMORY, NULL);
      return node;
    }
    value = TRI_GetNumericNodeValueAql(lhs) / TRI_GetNumericNodeValueAql(rhs);
  }
  else if (node->_type == AQL_NODE_OPERATOR_BINARY_MOD) {
    if (TRI_GetNumericNodeValueAql(rhs) == 0.0) {
      // todo: fix error message
      TRI_SetErrorAql(context, TRI_ERROR_OUT_OF_MEMORY, NULL);
      return node;
    }
    value = fmod(TRI_GetNumericNodeValueAql(lhs), TRI_GetNumericNodeValueAql(rhs));
  }
  
  node = TRI_CreateNodeValueDoubleAql(context, value);
  if (!node) {
    TRI_SetErrorAql(context, TRI_ERROR_OUT_OF_MEMORY, NULL);
    return NULL;
  }

  return node;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief fold constants in a node
////////////////////////////////////////////////////////////////////////////////

static TRI_aql_node_t* ModifyNode (void* data, TRI_aql_node_t* node) {
  TRI_aql_context_t* context = (TRI_aql_context_t*) data;

  if (!node) {
    return NULL;
  }

  switch (node->_type) {
    case AQL_NODE_OPERATOR_UNARY_PLUS:
    case AQL_NODE_OPERATOR_UNARY_MINUS:
      return OptimiseUnaryArithmeticOperation(context, node);
    case AQL_NODE_OPERATOR_UNARY_NOT:
      return OptimiseUnaryLogicalOperation(context, node);
    case AQL_NODE_OPERATOR_BINARY_AND:
    case AQL_NODE_OPERATOR_BINARY_OR:
      return OptimiseBinaryLogicalOperation(context, node);
    case AQL_NODE_OPERATOR_BINARY_PLUS:
    case AQL_NODE_OPERATOR_BINARY_MINUS:
    case AQL_NODE_OPERATOR_BINARY_TIMES:
    case AQL_NODE_OPERATOR_BINARY_DIV:
    case AQL_NODE_OPERATOR_BINARY_MOD:
      return OptimiseBinaryArithmeticOperation(context, node);
    default: 
      break;
  }

  return node;
}

////////////////////////////////////////////////////////////////////////////////
/// @} 
////////////////////////////////////////////////////////////////////////////////

// -----------------------------------------------------------------------------
// --SECTION--                                                  public functions
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @addtogroup Ahuacatl
/// @{
////////////////////////////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////////////////////////
/// @brief fold constants recursively
////////////////////////////////////////////////////////////////////////////////

TRI_aql_node_t* TRI_FoldConstantsAql (TRI_aql_context_t* const context,
                                      TRI_aql_node_t* node) {
  TRI_aql_modify_tree_walker_t* walker;

  walker = TRI_CreateModifyTreeWalkerAql((void*) context, &ModifyNode);
  if (!walker) {
    TRI_SetErrorAql(context, TRI_ERROR_OUT_OF_MEMORY, NULL);
    return node;
  }

  node = TRI_ModifyWalkTreeAql(walker, node); 

  TRI_FreeModifyTreeWalkerAql(walker);

  return node;
}

////////////////////////////////////////////////////////////////////////////////
/// @}
////////////////////////////////////////////////////////////////////////////////

// Local Variables:
// mode: outline-minor
// outline-regexp: "^\\(/// @brief\\|/// {@inheritDoc}\\|/// @addtogroup\\|// --SECTION--\\|/// @\\}\\)"
// End:
