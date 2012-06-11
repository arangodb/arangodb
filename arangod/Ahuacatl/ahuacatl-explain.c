////////////////////////////////////////////////////////////////////////////////
/// @brief Ahuacatl, explain
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

#include "Ahuacatl/ahuacatl-explain.h"
#include "Ahuacatl/ahuacatl-conversions.h"
#include "Ahuacatl/ahuacatl-scope.h"
#include "Ahuacatl/ahuacatl-statement-walker.h"

// -----------------------------------------------------------------------------
// --SECTION--                                                 private functions
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @addtogroup Ahuacatl
/// @{
////////////////////////////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////////////////////////
/// @brief convert a node to its string representation
////////////////////////////////////////////////////////////////////////////////
      
static TRI_json_t* StringifyNode (const TRI_aql_node_t* const node) {
  TRI_json_t* result;
  TRI_string_buffer_t buffer;

  assert(node);
  
  TRI_InitStringBuffer(&buffer, TRI_UNKNOWN_MEM_ZONE);

  if (!TRI_NodeStringAql(&buffer, node)) {
    return NULL;
  }

  result = TRI_CreateStringCopyJson(TRI_UNKNOWN_MEM_ZONE, buffer._buffer);
  
  TRI_DestroyStringBuffer(&buffer);

  return result;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief add a row to the result json structure
////////////////////////////////////////////////////////////////////////////////
      
static inline bool AddRow (TRI_aql_explain_t* const explain, TRI_json_t* value) {
  return TRI_PushBack3ListJson(TRI_UNKNOWN_MEM_ZONE, explain->_result, value) == TRI_ERROR_NO_ERROR;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief get an empty row protoype
////////////////////////////////////////////////////////////////////////////////

static inline TRI_json_t* GetRowProtoType (TRI_aql_explain_t* const explain,
                                           const TRI_aql_node_t* const node) {
  TRI_json_t* row;

  row = TRI_CreateArrayJson(TRI_UNKNOWN_MEM_ZONE);
  if (row == NULL) {
    return NULL;
  }
  
  TRI_Insert3ArrayJson(TRI_UNKNOWN_MEM_ZONE, 
                       row,
                       "id", 
                       TRI_CreateNumberJson(TRI_UNKNOWN_MEM_ZONE, (double) ++explain->_count));
  
  TRI_Insert3ArrayJson(TRI_UNKNOWN_MEM_ZONE, 
                       row,
                       "level", 
                       TRI_CreateNumberJson(TRI_UNKNOWN_MEM_ZONE, (double) explain->_level - 1));
  
  TRI_Insert3ArrayJson(TRI_UNKNOWN_MEM_ZONE, 
                       row,
                       "type", 
                       TRI_CreateStringCopyJson(TRI_UNKNOWN_MEM_ZONE, TRI_NodeNameAql(node->_type)));

  return row;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief create an explain structure
////////////////////////////////////////////////////////////////////////////////

static TRI_aql_explain_t* CreateExplain (void) {
  TRI_aql_explain_t* explain;

  explain = (TRI_aql_explain_t*) TRI_Allocate(TRI_UNKNOWN_MEM_ZONE, sizeof(TRI_aql_explain_t), false);

  if (explain == NULL) {
    return NULL;
  }
  
  explain->_count = 0;
  explain->_level = 0;

  explain->_result = TRI_CreateListJson(TRI_UNKNOWN_MEM_ZONE);
  if (explain->_result == NULL) {
    TRI_Free(TRI_UNKNOWN_MEM_ZONE, explain);

    return NULL;
  }

  return explain;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief free an explain structure
////////////////////////////////////////////////////////////////////////////////

static void FreeExplain (TRI_aql_explain_t* const explain) {
  assert(explain);

  // note: explain->_result is intentionally not freed here
  // the value is returned by TRI_ExplainAql() and must be
  // freed by the caller

  TRI_Free(TRI_UNKNOWN_MEM_ZONE, explain);
}

////////////////////////////////////////////////////////////////////////////////
/// @brief process a single statement
////////////////////////////////////////////////////////////////////////////////

static TRI_aql_node_t* ProcessStatement (TRI_aql_statement_walker_t* const walker, 
                                         TRI_aql_node_t* node) {
  TRI_aql_explain_t* explain;
  TRI_aql_node_type_e type = node->_type;
  
  explain = (TRI_aql_explain_t*) walker->_data;

  switch (type) {
    case TRI_AQL_NODE_SCOPE_START: {
      ++explain->_level;
      break;
    }
    case TRI_AQL_NODE_SCOPE_END: {
      --explain->_level;
      break;
    }
    case TRI_AQL_NODE_FOR: {
      TRI_aql_node_t* variableNode = TRI_AQL_NODE_MEMBER(node, 0);
      TRI_aql_node_t* expressionNode = TRI_AQL_NODE_MEMBER(node, 1);
      TRI_json_t* row;
      TRI_json_t* condition;

      row = GetRowProtoType(explain, node);
      TRI_Insert3ArrayJson(TRI_UNKNOWN_MEM_ZONE, 
                           row,
                           "variable", 
                           TRI_CreateStringCopyJson(TRI_UNKNOWN_MEM_ZONE, TRI_AQL_NODE_STRING(variableNode)));
      
      condition = StringifyNode(expressionNode);

      if (condition != NULL) {
        TRI_Insert3ArrayJson(TRI_UNKNOWN_MEM_ZONE, 
                             row,
                             "value", 
                             condition);
      }

      AddRow(explain, row);
      break;
    }
    case TRI_AQL_NODE_FILTER: {
      TRI_aql_node_t* expressionNode = TRI_AQL_NODE_MEMBER(node, 0);
      TRI_json_t* row;
      TRI_json_t* condition;

      row = GetRowProtoType(explain, node);
      condition = StringifyNode(expressionNode);

      if (condition != NULL) {
        TRI_Insert3ArrayJson(TRI_UNKNOWN_MEM_ZONE, 
                             row,
                             "condition", 
                             condition);
      }

      AddRow(explain, row);
      break;
    }
    case TRI_AQL_NODE_LET: {
      TRI_aql_node_t* variableNode = TRI_AQL_NODE_MEMBER(node, 0);
      TRI_aql_node_t* expressionNode = TRI_AQL_NODE_MEMBER(node, 1);
      TRI_json_t* row;
      TRI_json_t* expression;

      row = GetRowProtoType(explain, node);
      TRI_Insert3ArrayJson(TRI_UNKNOWN_MEM_ZONE, 
                           row,
                           "variable", 
                           TRI_CreateStringCopyJson(TRI_UNKNOWN_MEM_ZONE, TRI_AQL_NODE_STRING(variableNode)));
      
      expression = StringifyNode(expressionNode);
      if (expression != NULL) {
        TRI_Insert3ArrayJson(TRI_UNKNOWN_MEM_ZONE, 
                             row,
                             "value", 
                             expression);
      }

      AddRow(explain, row);
      break;
    }
    case TRI_AQL_NODE_SORT: {
      // TODO
      /*
      TRI_aql_node_t* conditionNode = TRI_AQL_NODE_MEMBER(node, 0);

      value = GetRowProtoType(explain, node);
      TRI_Insert3ArrayJson(TRI_UNKNOWN_MEM_ZONE, 
                           value,
                           "condition", 
                           TRI_CreateStringCopyJson(TRI_UNKNOWN_MEM_ZONE, TRI_AQL_NODE_STRING(variableNode)));
      AddRow(explain, value);
      */
      break;
    }
    case TRI_AQL_NODE_LIMIT: {
      TRI_aql_node_t* offsetNode = TRI_AQL_NODE_MEMBER(node, 0);
      TRI_aql_node_t* countNode = TRI_AQL_NODE_MEMBER(node, 1);
      TRI_json_t* row;
      
      row = GetRowProtoType(explain, node);

      TRI_Insert3ArrayJson(TRI_UNKNOWN_MEM_ZONE, 
                           row,
                           "offset", 
                           TRI_CreateNumberJson(TRI_UNKNOWN_MEM_ZONE, TRI_AQL_NODE_DOUBLE(offsetNode)));

      TRI_Insert3ArrayJson(TRI_UNKNOWN_MEM_ZONE, 
                           row,
                           "count", 
                           TRI_CreateNumberJson(TRI_UNKNOWN_MEM_ZONE, (double) TRI_AQL_NODE_DOUBLE(countNode)));
      
      AddRow(explain, row);
      break;
    }

    case TRI_AQL_NODE_COLLECT: {
      // TODO
      break;
    }

    default: {
    }
  }

  return NULL;
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
/// @brief explain a query
////////////////////////////////////////////////////////////////////////////////

TRI_json_t* TRI_ExplainAql (TRI_aql_context_t* const context) {
  TRI_aql_statement_walker_t* walker;
  TRI_aql_explain_t* explain;
  TRI_json_t* result;

  explain = CreateExplain();
  if (explain == NULL) {
    TRI_SetErrorContextAql(context, TRI_ERROR_OUT_OF_MEMORY, NULL);

    return NULL;
  }

  walker = TRI_CreateStatementWalkerAql((void*) explain, 
                                        false,
                                        NULL,
                                        &ProcessStatement,
                                        NULL);
   
  if (walker == NULL) {
    FreeExplain(explain);
    TRI_SetErrorContextAql(context, TRI_ERROR_OUT_OF_MEMORY, NULL);

    return NULL;
  }

  TRI_WalkStatementsAql(walker, context->_statements); 
  result = explain->_result;

  FreeExplain(explain);
   
  TRI_FreeStatementWalkerAql(walker);

  return result;
}

////////////////////////////////////////////////////////////////////////////////
/// @}
////////////////////////////////////////////////////////////////////////////////

// Local Variables:
// mode: outline-minor
// outline-regexp: "^\\(/// @brief\\|/// {@inheritDoc}\\|/// @addtogroup\\|// --SECTION--\\|/// @\\}\\)"
// End:
