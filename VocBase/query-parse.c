////////////////////////////////////////////////////////////////////////////////
/// @brief query parsing
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

#include "VocBase/query-parse.h"

////////////////////////////////////////////////////////////////////////////////
/// @addtogroup VocBase
/// @{
////////////////////////////////////////////////////////////////////////////////

// -----------------------------------------------------------------------------
// --SECTION--                                                 private functions
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @brief Validate the structs contained in a query template
////////////////////////////////////////////////////////////////////////////////

static bool ValidateQueryTemplate (TRI_query_template_t* const template_) {
  size_t order = 0;

  if (!TRI_ParseQueryValidateCollections(template_,
                                         template_->_query->_select._base, 
                                         &QLAstQueryIsValidAlias,
                                         &order)) {
    // select expression invalid
    return false;
  }
  
  if (!TRI_ParseQueryValidateCollections(template_, 
                                         template_->_query->_where._base, 
                                         &QLAstQueryIsValidAlias,
                                         &order)) {
    // where expression invalid
    return false;
  }
  
  if (!TRI_ParseQueryValidateCollections(template_,
                                         template_->_query->_order._base, 
                                         &QLAstQueryIsValidAlias,
                                         &order)) {
    // order expression invalid
    return false;
  }
  
  order = 0;
  if (!TRI_ParseQueryValidateCollections(template_, 
                                         template_->_query->_from._base,
                                         &QLAstQueryIsValidAliasOrdered,
                                         &order)) {
    // from expression(s) invalid
    return false;
  }

  return true;
}

// -----------------------------------------------------------------------------
// --SECTION--                                                            parser
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @brief Create parser for the query
////////////////////////////////////////////////////////////////////////////////

static TRI_query_parser_t* InitParserQueryTemplate (TRI_query_template_t* const template_) {
  TRI_query_parser_t* parser;

  assert(template_);
  assert(template_->_queryString);

  parser = (TRI_query_parser_t*) TRI_Allocate(sizeof(TRI_query_parser_t));
  if (!parser) {
    TRI_SetQueryError(&template_->_error, TRI_ERROR_OUT_OF_MEMORY, NULL);
    return NULL;
  }
  
  // init query data  
  parser->_length = strlen(template_->_queryString);
  parser->_buffer = (char*) template_->_queryString;

  // init lexer/scanner
  QLlex_init(&parser->_scanner);
  QLset_extra(template_, parser->_scanner);

  return parser;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief Free the parser for the query
////////////////////////////////////////////////////////////////////////////////
  
static void FreeParserQueryTemplate (TRI_query_template_t* const template_) {
  assert(template_);
  assert(template_->_parser);
  
  // free lexer/scanner
  QLlex_destroy(template_->_parser->_scanner);

  TRI_Free(template_->_parser);
}

////////////////////////////////////////////////////////////////////////////////
/// @brief Parse the query contained in a query template
////////////////////////////////////////////////////////////////////////////////

bool TRI_ParseQueryTemplate (TRI_query_template_t* const template_) {
  assert(template_);
  assert(template_->_queryString);
  assert(template_->_query);
 
  template_->_parser = InitParserQueryTemplate(template_);
  if (!template_->_parser) {
    TRI_SetQueryError(&template_->_error, TRI_ERROR_OUT_OF_MEMORY, NULL);
    return false;
  }
  
  if (QLparse(template_)) {
    FreeParserQueryTemplate(template_);
    return false;
  }

  FreeParserQueryTemplate(template_);

  if (template_->_query->_type == QUERY_TYPE_EMPTY) {
    TRI_SetQueryError(&template_->_error, 
                      TRI_ERROR_QUERY_EMPTY,
                      NULL);
    return false;
  }

  if (!ValidateQueryTemplate(template_)) {
    return false;
  }
  
  return TRI_InitQueryTemplate(template_);
}

// -----------------------------------------------------------------------------
// --SECTION--                                           parser helper functions
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @brief copies a string and keeps track of its memory location in a vector
////////////////////////////////////////////////////////////////////////////////

char* TRI_ParseQueryAllocString (TRI_query_template_t* const template_, 
                                 const char* string) {
  // do string duplication
  char* copy = TRI_DuplicateString(string);

  // store pointer to copy
  return TRI_ParseQueryRegisterString(template_, copy);
}

////////////////////////////////////////////////////////////////////////////////
/// @brief copies a string part and keeps track of its memory location in a vector
////////////////////////////////////////////////////////////////////////////////

char* TRI_ParseQueryAllocString2 (TRI_query_template_t* const template_,
                                  const char* string, 
                                  const size_t length) {
  // do string part duplication
  char* copy = TRI_DuplicateString2(string, length);

  // store pointer to copy and return it
  return TRI_ParseQueryRegisterString(template_, copy);
}

////////////////////////////////////////////////////////////////////////////////
/// @brief keep track of an allocated string
////////////////////////////////////////////////////////////////////////////////

char* TRI_ParseQueryRegisterString (TRI_query_template_t* const template_, 
                                    const char* string) {
  TRI_PushBackVectorPointer(&template_->_memory._strings, (char*) string);

  return (char*) string;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief free a string
////////////////////////////////////////////////////////////////////////////////

void TRI_ParseQueryFreeString (char* string) {
  if (string) {
    TRI_Free(string);
    string = NULL;
  }
}

////////////////////////////////////////////////////////////////////////////////
/// @brief create a new node for the ast
////////////////////////////////////////////////////////////////////////////////

TRI_query_node_t* TRI_ParseQueryCreateNode (TRI_query_template_t* const template_, 
                                            const TRI_query_node_type_e type) { 
  
  return TRI_CreateNodeQuery(&template_->_memory._nodes, type);
}

////////////////////////////////////////////////////////////////////////////////
/// @brief open a new context layer for the parser
////////////////////////////////////////////////////////////////////////////////

void TRI_ParseQueryContextPush (TRI_query_template_t* const template_, 
                                TRI_query_node_t* element) {
  TRI_PushBackVectorPointer(&template_->_memory._listHeads, element);
  TRI_PushBackVectorPointer(&template_->_memory._listTails, element);
}

////////////////////////////////////////////////////////////////////////////////
/// @brief close the current context layer of the parser and return it
////////////////////////////////////////////////////////////////////////////////

TRI_query_node_t* TRI_ParseQueryContextPop (TRI_query_template_t* const template_) {
  TRI_query_node_t* head;
  size_t i;

  i = template_->_memory._listHeads._length;

  if (i > 0) {
    TRI_RemoveVectorPointer(&template_->_memory._listTails, i - 1);
    head = TRI_RemoveVectorPointer(&template_->_memory._listHeads, i - 1);
    return head;
  } 

  return 0;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief add an element to the current parsing context
////////////////////////////////////////////////////////////////////////////////

void TRI_ParseQueryContextAddElement (TRI_query_template_t* const template_, 
                                      TRI_query_node_t* element) {
  TRI_query_node_t* last;
  size_t i;

  i = template_->_memory._listTails._length;

  if (i > 0) {
    last = *(template_->_memory._listTails._buffer + i -1);
    if (last) {
      last->_next = element;
      *(template_->_memory._listTails._buffer + i -1) = element;
    }
  }
}

////////////////////////////////////////////////////////////////////////////////
/// @brief pop the current parse context from the stack into the rhs element
////////////////////////////////////////////////////////////////////////////////

void TRI_ParseQueryPopIntoRhs (TRI_query_node_t* node, 
                               TRI_query_template_t* const template_) {
  TRI_query_node_t* popped;
  popped = TRI_ParseQueryContextPop(template_);

  if (node) {
    node->_rhs = popped;
  }  
}

// -----------------------------------------------------------------------------
// --SECTION--                                                        validation
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @brief Validate a collection name
////////////////////////////////////////////////////////////////////////////////

bool TRI_ParseQueryValidateCollectionName (const char* name) {
  if (TRI_IsAllowedCollectionName(name) != 0) {
    return false;
  }

  return true;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief Validate a collection alias
////////////////////////////////////////////////////////////////////////////////

bool TRI_ParseQueryValidateCollectionAlias (const char* name) {
  const char* p = name;
  char c;
  size_t totalLength = 0;
  size_t charLength = 0;
   
  c = *p;
  // must start with one these chars
  if (!(c >= 'A' && c <= 'Z') && 
      !(c >= 'a' && c <= 'z') && 
      !(c == '_')) {
    return false;
  }

  while ('\0' != (c = *p++)) {
    if (!(c >= 'A' && c <= 'Z') && 
        !(c >= 'a' && c <= 'z') && 
        !(c >= '0' && c <='9') && 
        !(c == '_')) {
      return false;
    } 
    if (!(c >= '0' && c <='9') && 
        !(c == '_')) {
      // must include at least one letter
      charLength++;
    }
    totalLength++;
  }

  // if no letter is contained, the alias is invalid
  if (charLength == 0) {
    return false;
  }
  
  return ((totalLength > 0) && (totalLength <= TRI_QUERY_ALIAS_MAX_LENGTH));
}

////////////////////////////////////////////////////////////////////////////////
/// @brief Validate the collections used in a query part
////////////////////////////////////////////////////////////////////////////////

bool TRI_ParseQueryValidateCollections (TRI_query_template_t* const template_,
                                        TRI_query_node_t* node,
                                        QL_parser_validate_func validateFunc,
                                        size_t* order) {
  TRI_query_node_t* lhs;
  TRI_query_node_t* rhs;
  TRI_query_node_t* next;

  if (!node) {
    return true; 
  }
  
  if (node->_type == TRI_QueryNodeContainerList) {
    next = node->_next;
    while (next) {
      if (!TRI_ParseQueryValidateCollections(template_, next, validateFunc, order)) {
        return false;
      }
      next = next->_next;
    }
  }
  
  if (node->_type == TRI_QueryNodeReferenceCollection) {
    ++(*order);
  }
  
  if (node->_type == TRI_QueryNodeReferenceCollectionAlias) {
    if (!validateFunc(template_->_query, node->_value._stringValue, *order)) {
      TRI_SetQueryError(&template_->_error, 
                        TRI_ERROR_QUERY_COLLECTION_ALIAS_UNDECLARED, 
                        node->_value._stringValue);
      return false;
    }
  }

  lhs = node->_lhs;
  if (lhs) {
    if (!TRI_ParseQueryValidateCollections(template_, lhs, validateFunc, order)) { 
      return false;
    }
  }

  rhs = node->_rhs;
  if (rhs) {
    if (!TRI_ParseQueryValidateCollections(template_, rhs, validateFunc, order)) {
      return false;
    }
  }

  return true;
}

////////////////////////////////////////////////////////////////////////////////
/// @}
////////////////////////////////////////////////////////////////////////////////

// Local Variables:
// mode: outline-minor
// outline-regexp: "^\\(/// @brief\\|/// {@inheritDoc}\\|/// @addtogroup\\|// --SECTION--\\|/// @\\}\\)"
// End:
