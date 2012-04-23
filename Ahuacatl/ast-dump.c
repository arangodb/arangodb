////////////////////////////////////////////////////////////////////////////////
/// @brief Ahuacatl, AST dump functions
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

#include "Ahuacatl/ast-dump.h"
#include "Ahuacatl/ast-node.h"

// -----------------------------------------------------------------------------
// --SECTION--                                                 private functions
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @addtogroup Ahuacatl
/// @{
////////////////////////////////////////////////////////////////////////////////
      
static char* GetTypeName (const TRI_aql_node_type_e type) {
  switch (type) {
    case AQL_NODE_UNDEFINED:
      assert(false);
      return "undefined";
    case AQL_NODE_FOR:
      return "for";
    case AQL_NODE_LET:
      return "let";
    case AQL_NODE_FILTER:
      return "filter";
    case AQL_NODE_RETURN:
      return "return";
    case AQL_NODE_COLLECT:
      return "collect";
    case AQL_NODE_SORT:
      return "sort";
    case AQL_NODE_SORT_ELEMENT:
      return "sort element";
    case AQL_NODE_LIMIT:
      return "limit";
    case AQL_NODE_VARIABLE:
      return "variable";
    case AQL_NODE_REFERENCE:
      return "reference";
    case AQL_NODE_ATTRIBUTE:
      return "attribute";
    case AQL_NODE_ASSIGN:
      return "assign";
    case AQL_NODE_OPERATOR_UNARY_PLUS:
      return "uplus";
    case AQL_NODE_OPERATOR_UNARY_MINUS:
      return "uminus";
    case AQL_NODE_OPERATOR_UNARY_NOT:
      return "unot";
    case AQL_NODE_OPERATOR_BINARY_AND:
      return "and";
    case AQL_NODE_OPERATOR_BINARY_OR:
      return "or";
    case AQL_NODE_OPERATOR_BINARY_PLUS:
      return "plus";
    case AQL_NODE_OPERATOR_BINARY_MINUS:
      return "minus";
    case AQL_NODE_OPERATOR_BINARY_TIMES:
      return "times";
    case AQL_NODE_OPERATOR_BINARY_DIV:
      return "div";
    case AQL_NODE_OPERATOR_BINARY_MOD:
      return "mod";
    case AQL_NODE_OPERATOR_BINARY_EQ:
      return "eq";
    case AQL_NODE_OPERATOR_BINARY_NE:
      return "ne";
    case AQL_NODE_OPERATOR_BINARY_LT:
      return "lt";
    case AQL_NODE_OPERATOR_BINARY_LE:
      return "le";
    case AQL_NODE_OPERATOR_BINARY_GT:
      return "gt";
    case AQL_NODE_OPERATOR_BINARY_GE:
      return "ge";
    case AQL_NODE_OPERATOR_BINARY_IN:
      return "in";
    case AQL_NODE_OPERATOR_TERNARY:
      return "ternary";
    case AQL_NODE_SUBQUERY:
      return "subquery";
    case AQL_NODE_ATTRIBUTE_ACCESS:
      return "attribute access";
    case AQL_NODE_INDEXED:
      return "indexed";
    case AQL_NODE_EXPAND:
      return "expand";
    case AQL_NODE_VALUE:
      return "value";
    case AQL_NODE_LIST:
      return "list";
    case AQL_NODE_ARRAY:
      return "array";
    case AQL_NODE_ARRAY_ELEMENT:
      return "array element";
    case AQL_NODE_PARAMETER:
      return "parameter";
    case AQL_NODE_FCALL:
      return "function call";
  }
 
  printf("unhandled type: %lu\n", (unsigned long) type); 
  assert(false);
}

static void Indent (TRI_aql_dump_t* const state) {
  size_t i;
  
  for (i = 0; i < state->_indent; ++i) {
    printf("  ");
  }
}

static void PrintType (TRI_aql_dump_t* const state, const TRI_aql_node_type_e type) {
  Indent(state);
  printf("%s\n", GetTypeName(type));
}

static inline void IndentState (TRI_aql_dump_t* const state) {
  ++state->_indent;
}

static inline void OutdentState (TRI_aql_dump_t* const state) {
  assert(state->_indent > 0);
  --state->_indent;
}

static void DumpValue (TRI_aql_dump_t* const state, TRI_aql_node_value_t* node) {
  switch (node->_value._type) {
    case AQL_TYPE_FAIL:
      printf("fail\n");
      break;
    case AQL_TYPE_NULL:
      printf("null\n");
      break;
    case AQL_TYPE_BOOL:
      printf("bool (%lu)\n", (unsigned long) node->_value._value._bool);
      break;
    case AQL_TYPE_INT:
      printf("int (%ld)\n", (long) node->_value._value._int);
      break;
    case AQL_TYPE_DOUBLE:
      printf("double (%f)\n", node->_value._value._double);
      break;
    case AQL_TYPE_STRING:
      printf("string (%s)\n", node->_value._value._string);
      break;
  }
}

static void DumpNode (TRI_aql_dump_t* const state, TRI_aql_node_t* const data) { 
  TRI_aql_node_t* node;

  if (!data) {
    return;
  }

  node = data;

  while (node) {
    PrintType(state, node->_type);
    IndentState(state);

    switch (node->_type) {
      case AQL_NODE_FOR:
        DumpNode(state, ((TRI_aql_node_for_t*) node)->_variable);
        DumpNode(state, ((TRI_aql_node_for_t*) node)->_expression);
        break;
      case AQL_NODE_ASSIGN:
        DumpNode(state, ((TRI_aql_node_assign_t*) node)->_variable);
        DumpNode(state, ((TRI_aql_node_assign_t*) node)->_expression);
        break;
      case AQL_NODE_FILTER:
        DumpNode(state, ((TRI_aql_node_filter_t*) node)->_expression);
        break;
      case AQL_NODE_COLLECT:
        DumpNode(state, ((TRI_aql_node_sort_t*) node)->_list);
        if (((TRI_aql_node_collect_t*) node)->_into) {
          Indent(state);
          printf("into\n");
          DumpNode(state, ((TRI_aql_node_collect_t*) node)->_into);
        }
        break;
      case AQL_NODE_RETURN:
        DumpNode(state, ((TRI_aql_node_return_t*) node)->_expression);
        break;
      case AQL_NODE_SORT:
        DumpNode(state, ((TRI_aql_node_sort_t*) node)->_list);
        break;
      case AQL_NODE_SORT_ELEMENT:
        DumpNode(state, ((TRI_aql_node_sort_element_t*) node)->_expression);
        Indent(state);
        printf("asc/desc: %lu\n", (unsigned long) ((TRI_aql_node_sort_element_t*) node)->_ascending);
        break;
      case AQL_NODE_LIMIT:
        Indent(state);
        printf("offset: %ld\n", (long) ((TRI_aql_node_limit_t*) node)->_offset);
        Indent(state);
        printf("count: %ld\n", (long) ((TRI_aql_node_limit_t*) node)->_count);
        break;
      case AQL_NODE_VARIABLE:
        Indent(state);
        printf("name: %s\n", ((TRI_aql_node_variable_t*) node)->_name);
        break;
      case AQL_NODE_REFERENCE:
        Indent(state);
        printf("name: %s\n", ((TRI_aql_node_reference_t*) node)->_name);
        Indent(state);
        printf("is collection: %ld\n", (long) ((TRI_aql_node_reference_t*) node)->_isCollection);
        break;
      case AQL_NODE_ATTRIBUTE:
        Indent(state);
        printf("name: %s\n", ((TRI_aql_node_attribute_t*) node)->_name);
        break;
      case AQL_NODE_PARAMETER:
        Indent(state);
        printf("name: %s\n", ((TRI_aql_node_parameter_t*) node)->_name);
        break;
      case AQL_NODE_ATTRIBUTE_ACCESS:
        DumpNode(state, ((TRI_aql_node_attribute_access_t*) node)->_accessed);
        Indent(state);
        printf("name: %s\n", ((TRI_aql_node_attribute_access_t*) node)->_name);
        break;
      case AQL_NODE_INDEXED:
        DumpNode(state, ((TRI_aql_node_indexed_t*) node)->_accessed);
        DumpNode(state, ((TRI_aql_node_indexed_t*) node)->_index);
        break;
      case AQL_NODE_EXPAND:
        DumpNode(state, ((TRI_aql_node_expand_t*) node)->_expanded);
        DumpNode(state, ((TRI_aql_node_expand_t*) node)->_expansion);
        break;
      case AQL_NODE_OPERATOR_UNARY_PLUS:
      case AQL_NODE_OPERATOR_UNARY_MINUS:
      case AQL_NODE_OPERATOR_UNARY_NOT:
        DumpNode(state, ((TRI_aql_node_operator_unary_t*) node)->_operand);
        break;
      case AQL_NODE_OPERATOR_BINARY_AND:
      case AQL_NODE_OPERATOR_BINARY_OR:
      case AQL_NODE_OPERATOR_BINARY_EQ:
      case AQL_NODE_OPERATOR_BINARY_NE:
      case AQL_NODE_OPERATOR_BINARY_LT:
      case AQL_NODE_OPERATOR_BINARY_LE:
      case AQL_NODE_OPERATOR_BINARY_GT:
      case AQL_NODE_OPERATOR_BINARY_GE:
      case AQL_NODE_OPERATOR_BINARY_IN:
      case AQL_NODE_OPERATOR_BINARY_PLUS:
      case AQL_NODE_OPERATOR_BINARY_MINUS:
      case AQL_NODE_OPERATOR_BINARY_TIMES:
      case AQL_NODE_OPERATOR_BINARY_DIV:
      case AQL_NODE_OPERATOR_BINARY_MOD:
        DumpNode(state, ((TRI_aql_node_operator_binary_t*) node)->_lhs);
        DumpNode(state, ((TRI_aql_node_operator_binary_t*) node)->_rhs);
        break;
      case AQL_NODE_OPERATOR_TERNARY:
        DumpNode(state, ((TRI_aql_node_operator_ternary_t*) node)->_condition);
        DumpNode(state, ((TRI_aql_node_operator_ternary_t*) node)->_truePart);
        DumpNode(state, ((TRI_aql_node_operator_ternary_t*) node)->_falsePart);
        break;
      case AQL_NODE_SUBQUERY:
        DumpNode(state, ((TRI_aql_node_subquery_t*) node)->_query);
        break;
      case AQL_NODE_VALUE: 
        Indent(state);
        DumpValue(state, (TRI_aql_node_value_t*) node);
        break;
      case AQL_NODE_LIST: { 
        TRI_vector_pointer_t* values = (TRI_vector_pointer_t*) &((TRI_aql_node_list_t*) node)->_values;
        size_t i, n;

        n = values->_length;
        for (i = 0; i < n; ++i) {
          DumpNode(state, (TRI_aql_node_t*) values->_buffer[i]); 
        }
        break;
      }
      case AQL_NODE_ARRAY: { 
        TRI_associative_pointer_t* values = (TRI_associative_pointer_t*) &((TRI_aql_node_array_t*) node)->_values;
        size_t i, n;

        n = values->_nrAlloc;
        for (i = 0; i < n; ++i) {
          TRI_aql_node_array_element_t* element = (TRI_aql_node_array_element_t*) values->_table[i];
          if (!element) {
            continue;
          }
          Indent(state);
          printf("name: %s\n", element->_name);
          DumpNode(state, (TRI_aql_node_t*) element->_value);
        }
        break;
      }
      case AQL_NODE_FCALL: { 
        Indent(state);
        printf("name: %s\n", ((TRI_aql_node_fcall_t*) node)->_name);
        DumpNode(state, ((TRI_aql_node_fcall_t*) node)->_parameters);
        break;
      }
      default:
        break;
    }
    
    OutdentState(state);
    
    node = node->_next;
  }
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
/// @brief dump AST nodes recursively
////////////////////////////////////////////////////////////////////////////////

void TRI_DumpAql (const void* const data) {
  TRI_aql_dump_t state;

  state._indent = 0;

  DumpNode(&state, (TRI_aql_node_t*) data);
}

////////////////////////////////////////////////////////////////////////////////
/// @}
////////////////////////////////////////////////////////////////////////////////

// Local Variables:
// mode: outline-minor
// outline-regexp: "^\\(/// @brief\\|/// {@inheritDoc}\\|/// @addtogroup\\|// --SECTION--\\|/// @\\}\\)"
// End:
