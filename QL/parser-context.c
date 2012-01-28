////////////////////////////////////////////////////////////////////////////////
/// @brief parser context for flex-based query scanner
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
#include "parser-context.h"


////////////////////////////////////////////////////////////////////////////////
/// @addtogroup QL
/// @{
////////////////////////////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////////////////////////
/// @brief initializes the parser context for a query
////////////////////////////////////////////////////////////////////////////////

bool QLParseInit (QL_parser_context_t *context, const char *query) {
  // init vectors needed for book-keeping memory
  TRI_InitVectorPointer(&context->_nodes);
  TRI_InitVectorPointer(&context->_strings);
  TRI_InitVectorPointer(&context->_listHeads);
  TRI_InitVectorPointer(&context->_listTails);

  // init lexer/scanner
  QLlex_init(&context->_scanner);
  QLset_extra(context, context->_scanner);
  
  // init query data  
  context->_lexState._length = strlen(query);
  context->_lexState._buffer = (char *) query;

  // reset position and error message
  context->_lexState._errorState._message = "";
  context->_lexState._errorState._line    = 1;
  context->_lexState._errorState._column  = 1;

  // init query structure
  context->_query = (QL_ast_query_t *) TRI_Allocate(sizeof(QL_ast_query_t));
  if (!context->_query) { 
    return false;
  }

  QLAstQueryInit(context->_query);

  return true;
}


////////////////////////////////////////////////////////////////////////////////
/// @brief free all memory allocated in context of a query
////////////////////////////////////////////////////////////////////////////////

void QLParseFree (QL_parser_context_t *context) {
  size_t i;
  void *nodePtr;
  char *stringPtr;

  // nodes
  i = context->_nodes._length;
  // free all nodes in vector, starting at the end (prevents copying the remaining elements in vector)
  while (i > 0) {
    i--;
    nodePtr = TRI_RemoveVectorPointer(&context->_nodes, i);
    QLParseFreeNode(nodePtr);
    if (i == 0) {
      break;
    }
  }

  // strings
  i = context->_strings._length;
  // free all strings in vector, starting at the end (prevents copying the remaining elements in vector)
  while (i > 0) {
    i--;
    stringPtr = TRI_RemoveVectorPointer(&context->_strings, i);
    QLParseFreeString(stringPtr);
    if (i == 0) {
      break;
    }
  }

  // list elements in _listHeads and _listTails must not be freed separately as they are AstNodes handled
  // by _nodes already

  // free vectors themselves
  TRI_DestroyVectorPointer(&context->_strings);
  TRI_DestroyVectorPointer(&context->_nodes);
  TRI_DestroyVectorPointer(&context->_listHeads);
  TRI_DestroyVectorPointer(&context->_listTails);
  
  // free lexer/scanner
  QLlex_destroy(context->_scanner);
  
  // free collections array in query
  QLAstQueryFree(context->_query);
 
  // free query struct itself
  if (context->_query != 0) {
    TRI_Free(context->_query);
    context->_query = 0;
  }
}


////////////////////////////////////////////////////////////////////////////////
/// @brief keep track of an allocated ast node
////////////////////////////////////////////////////////////////////////////////

void QLParseRegisterNode (QL_parser_context_t *context, QL_ast_node_t *element) {
  TRI_PushBackVectorPointer(&context->_nodes, element);
}


////////////////////////////////////////////////////////////////////////////////
/// @brief free an ast node
////////////////////////////////////////////////////////////////////////////////

void QLParseFreeNode (QL_ast_node_t *element) {
  if (element != 0) {
    TRI_Free(element);
    element = 0;
  }
}


////////////////////////////////////////////////////////////////////////////////
/// @brief copies a string and keeps track of its memory location in a vector
////////////////////////////////////////////////////////////////////////////////

char *QLParseAllocString (QL_parser_context_t *context, const char *string) {
  // do string duplication
  char *copy = TRI_DuplicateString(string);

  // store pointer to copy
  return QLParseRegisterString(context, copy);
}


////////////////////////////////////////////////////////////////////////////////
/// @brief copies a string part and keeps track of its memory location in a vector
////////////////////////////////////////////////////////////////////////////////

char *QLParseAllocString2 (QL_parser_context_t *context, const char *string, const size_t length) {
  // do string part duplication
  char *copy = TRI_DuplicateString2(string, length);

  // store pointer to copy and return it
  return QLParseRegisterString(context, copy);
}


////////////////////////////////////////////////////////////////////////////////
/// @brief keep track of an allocated string
////////////////////////////////////////////////////////////////////////////////

char *QLParseRegisterString (QL_parser_context_t *context, const char *string) {
  TRI_PushBackVectorPointer(&context->_strings, (char *) string);

  return (char*) string;
}


////////////////////////////////////////////////////////////////////////////////
/// @brief free a string
////////////////////////////////////////////////////////////////////////////////

void QLParseFreeString (char *string) {
  if (string != 0) {
    TRI_Free(string);
    string = 0;
  }
}


////////////////////////////////////////////////////////////////////////////////
/// @brief create a new node for the ast
////////////////////////////////////////////////////////////////////////////////

QL_ast_node_t *QLAstNodeCreate (QL_parser_context_t *context, const QL_ast_node_type_e type) { 
  // allocate memory
  QL_ast_node_t *node = (QL_ast_node_t *) TRI_Allocate(sizeof(QL_ast_node_t));

  if (node == 0) {
    return 0; 
  }

  // keep track of memory location
  QLParseRegisterNode(context, node);

  // set node type
  node->_type               = type;

  // set initial pointers to 0
  node->_value._stringValue = 0;
  node->_lhs                = 0;
  node->_rhs                = 0;
  node->_next               = 0;
  
  return node;
}


////////////////////////////////////////////////////////////////////////////////
/// @brief open a new context layer for the parser
////////////////////////////////////////////////////////////////////////////////

void QLParseContextPush (QL_parser_context_t *context, QL_ast_node_t *element) {
  TRI_PushBackVectorPointer(&context->_listHeads, element);
  TRI_PushBackVectorPointer(&context->_listTails, element);
}


////////////////////////////////////////////////////////////////////////////////
/// @brief close the current context layer of the parser and return it
////////////////////////////////////////////////////////////////////////////////

QL_ast_node_t *QLParseContextPop (QL_parser_context_t *context) {
  QL_ast_node_t *head;
  size_t i;

  i = context->_listHeads._length;

  if (i > 0) {
    TRI_RemoveVectorPointer(&context->_listTails, i - 1);
    head = TRI_RemoveVectorPointer(&context->_listHeads, i - 1);
    return head;
  } 

  return 0;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief add an element to the current parsing context
////////////////////////////////////////////////////////////////////////////////

void QLParseContextAddElement (QL_parser_context_t *context, QL_ast_node_t *element) {
  QL_ast_node_t *last;
  size_t i;

  i = context->_listTails._length;

  if (i > 0) {
    last = *(context->_listTails._buffer + i -1);
    if (last != 0) {
      last->_next = element;
      *(context->_listTails._buffer + i -1) = element;
    }
  }
}

////////////////////////////////////////////////////////////////////////////////
/// @}
////////////////////////////////////////////////////////////////////////////////

// Local Variables:
// mode: outline-minor
// outline-regexp: "^\\(/// @brief\\|/// {@inheritDoc}\\|/// @addtogroup\\|// --SECTION--\\|/// @\\}\\)"
// End:
