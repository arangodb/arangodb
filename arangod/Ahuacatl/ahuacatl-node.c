////////////////////////////////////////////////////////////////////////////////
/// @brief Ahuacatl, parser nodes
///
/// @file
///
/// DISCLAIMER
///
/// Copyright 2004-2013 triAGENS GmbH, Cologne, Germany
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
/// @author Copyright 2012-2013, triAGENS GmbH, Cologne, Germany
////////////////////////////////////////////////////////////////////////////////

#include "Ahuacatl/ahuacatl-node.h"

// -----------------------------------------------------------------------------
// --SECTION--                                                  public functions
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @addtogroup Ahuacatl
/// @{
////////////////////////////////////////////////////////////////////////////////

bool TRI_IsTopLevelTypeAql (const TRI_aql_node_type_e type) {
  if (type == TRI_AQL_NODE_SCOPE_START ||
      type == TRI_AQL_NODE_SCOPE_END ||
      type == TRI_AQL_NODE_SUBQUERY ||
      type == TRI_AQL_NODE_EXPAND ||
      type == TRI_AQL_NODE_FOR ||
      type == TRI_AQL_NODE_FILTER ||
      type == TRI_AQL_NODE_LET ||
      type == TRI_AQL_NODE_SORT ||
      type == TRI_AQL_NODE_LIMIT ||
      type == TRI_AQL_NODE_COLLECT ||
      type == TRI_AQL_NODE_RETURN) {
    return true;
  }

  return false;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief get the node type group
////////////////////////////////////////////////////////////////////////////////

const char* TRI_NodeGroupAql (const TRI_aql_node_t* const node,
                              const bool inspect) {
  switch (node->_type) {
    case TRI_AQL_NODE_VALUE:
      return "const value";
    case TRI_AQL_NODE_LIST:
      if (inspect && TRI_IsConstantValueNodeAql(node)) {
        return "const list";
      }
      else {
        return "list";
      }
    case TRI_AQL_NODE_ARRAY:
      if (inspect && TRI_IsConstantValueNodeAql(node)) {
        return "const document";
      }
      else {
        return "document";
      }
    case TRI_AQL_NODE_REFERENCE:
      return "reference";
    case TRI_AQL_NODE_COLLECTION:
      return "collection";
    default: {
      return "expression";
    }
  }
}

////////////////////////////////////////////////////////////////////////////////
/// @brief get the "nice" name of an AST node
////////////////////////////////////////////////////////////////////////////////

const char* TRI_NodeNameAql (const TRI_aql_node_type_e type) {
  switch (type) {
    case TRI_AQL_NODE_NOP:
      return "nop";
    case TRI_AQL_NODE_RETURN_EMPTY:
      return "return (empty)";
    case TRI_AQL_NODE_SCOPE_START:
      return "scope start";
    case TRI_AQL_NODE_SCOPE_END:
      return "scope end";
    case TRI_AQL_NODE_FOR:
      return "for";
    case TRI_AQL_NODE_LET:
      return "let";
    case TRI_AQL_NODE_FILTER:
      return "filter";
    case TRI_AQL_NODE_RETURN:
      return "return";
    case TRI_AQL_NODE_COLLECT:
      return "collect";
    case TRI_AQL_NODE_SORT:
      return "sort";
    case TRI_AQL_NODE_SORT_ELEMENT:
      return "sort element";
    case TRI_AQL_NODE_LIMIT:
      return "limit";
    case TRI_AQL_NODE_VARIABLE:
      return "variable";
    case TRI_AQL_NODE_COLLECTION:
      return "collection";
    case TRI_AQL_NODE_REFERENCE:
      return "reference";
    case TRI_AQL_NODE_ATTRIBUTE:
      return "attribute";
    case TRI_AQL_NODE_ASSIGN:
      return "assign";
    case TRI_AQL_NODE_OPERATOR_UNARY_PLUS:
      return "uplus";
    case TRI_AQL_NODE_OPERATOR_UNARY_MINUS:
      return "uminus";
    case TRI_AQL_NODE_OPERATOR_UNARY_NOT:
      return "unot";
    case TRI_AQL_NODE_OPERATOR_BINARY_AND:
      return "and";
    case TRI_AQL_NODE_OPERATOR_BINARY_OR:
      return "or";
    case TRI_AQL_NODE_OPERATOR_BINARY_PLUS:
      return "plus";
    case TRI_AQL_NODE_OPERATOR_BINARY_MINUS:
      return "minus";
    case TRI_AQL_NODE_OPERATOR_BINARY_TIMES:
      return "times";
    case TRI_AQL_NODE_OPERATOR_BINARY_DIV:
      return "div";
    case TRI_AQL_NODE_OPERATOR_BINARY_MOD:
      return "mod";
    case TRI_AQL_NODE_OPERATOR_BINARY_EQ:
      return "eq";
    case TRI_AQL_NODE_OPERATOR_BINARY_NE:
      return "ne";
    case TRI_AQL_NODE_OPERATOR_BINARY_LT:
      return "lt";
    case TRI_AQL_NODE_OPERATOR_BINARY_LE:
      return "le";
    case TRI_AQL_NODE_OPERATOR_BINARY_GT:
      return "gt";
    case TRI_AQL_NODE_OPERATOR_BINARY_GE:
      return "ge";
    case TRI_AQL_NODE_OPERATOR_BINARY_IN:
      return "in";
    case TRI_AQL_NODE_OPERATOR_TERNARY:
      return "ternary";
    case TRI_AQL_NODE_SUBQUERY:
      return "subquery";
    case TRI_AQL_NODE_ATTRIBUTE_ACCESS:
      return "attribute access";
    case TRI_AQL_NODE_INDEXED:
      return "indexed";
    case TRI_AQL_NODE_EXPAND:
      return "expand";
    case TRI_AQL_NODE_VALUE:
      return "value";
    case TRI_AQL_NODE_LIST:
      return "list";
    case TRI_AQL_NODE_ARRAY:
      return "array";
    case TRI_AQL_NODE_ARRAY_ELEMENT:
      return "array element";
    case TRI_AQL_NODE_PARAMETER:
      return "parameter";
    case TRI_AQL_NODE_FCALL:
      return "function call";
  }

  assert(false);
  return "undefined";
}

////////////////////////////////////////////////////////////////////////////////
/// @brief return true if a node is a list node
////////////////////////////////////////////////////////////////////////////////

bool TRI_IsListNodeAql (const TRI_aql_node_t* const node) {
  return (node->_type == TRI_AQL_NODE_LIST);
}

////////////////////////////////////////////////////////////////////////////////
/// @brief check if a node is a constant
////////////////////////////////////////////////////////////////////////////////

bool TRI_IsConstantValueNodeAql (const TRI_aql_node_t* const node) {
  assert(node);

  if (node->_type != TRI_AQL_NODE_VALUE &&
      node->_type != TRI_AQL_NODE_LIST &&
      node->_type != TRI_AQL_NODE_ARRAY) {
    return false;
  }

  if (node->_type == TRI_AQL_NODE_LIST) {
    // inspect all list elements
    size_t i;
    size_t n = node->_members._length;

    for (i = 0; i < n; ++i) {
      TRI_aql_node_t* member = TRI_AQL_NODE_MEMBER(node, i);

      if (!TRI_IsConstantValueNodeAql(member)) {
        return false;
      }
    }
  }
  else if (node->_type == TRI_AQL_NODE_ARRAY) {
    // inspect all array elements
    size_t i;
    size_t n = node->_members._length;

    for (i = 0; i < n; ++i) {
      TRI_aql_node_t* member = TRI_AQL_NODE_MEMBER(node, i);
      TRI_aql_node_t* value = TRI_AQL_NODE_MEMBER(member, 0);

      if (!TRI_IsConstantValueNodeAql(value)) {
        return false;
      }
    }
  }

  return true;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief checks if a node value is numeric
////////////////////////////////////////////////////////////////////////////////

bool TRI_IsNumericValueNodeAql (const TRI_aql_node_t* const node) {
  assert(node);

  if (node->_type != TRI_AQL_NODE_VALUE) {
    return false;
  }

  return (node->_value._type == TRI_AQL_TYPE_INT ||
          node->_value._type == TRI_AQL_TYPE_DOUBLE);
}

////////////////////////////////////////////////////////////////////////////////
/// @brief checks if a node value is boolean
////////////////////////////////////////////////////////////////////////////////

bool TRI_IsBooleanValueNodeAql (const TRI_aql_node_t* const node) {
  assert(node);

  return (node->_type == TRI_AQL_NODE_VALUE && node->_value._type == TRI_AQL_TYPE_BOOL);
}

////////////////////////////////////////////////////////////////////////////////
/// @}
////////////////////////////////////////////////////////////////////////////////

// Local Variables:
// mode: outline-minor
// outline-regexp: "/// @brief\\|/// {@inheritDoc}\\|/// @addtogroup\\|/// @page\\|// --SECTION--\\|/// @\\}"
// End:
