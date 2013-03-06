////////////////////////////////////////////////////////////////////////////////
/// @brief Ahuacatl, optimiser
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

#include "Ahuacatl/ahuacatl-optimiser.h"

#include "BasicsC/logging.h"
#include "BasicsC/strings.h"

#include "Ahuacatl/ahuacatl-ast-node.h"
#include "Ahuacatl/ahuacatl-access-optimiser.h"
#include "Ahuacatl/ahuacatl-collections.h"
#include "Ahuacatl/ahuacatl-conversions.h"
#include "Ahuacatl/ahuacatl-functions.h"
#include "Ahuacatl/ahuacatl-scope.h"
#include "Ahuacatl/ahuacatl-statement-walker.h"
#include "Ahuacatl/ahuacatl-variable.h"

#include "VocBase/document-collection.h"
#include "VocBase/index.h"

#include "V8/v8-execution.h"

// -----------------------------------------------------------------------------
// --SECTION--                                                          forwards
// -----------------------------------------------------------------------------

static TRI_aql_node_t* OptimiseNode (TRI_aql_statement_walker_t* const, 
                                     TRI_aql_node_t*);

static TRI_aql_node_t* ProcessStatement (TRI_aql_statement_walker_t* const, 
                                         TRI_aql_node_t*);

// -----------------------------------------------------------------------------
// --SECTION--                                                   private defines
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @addtogroup Ahuacatl
/// @{
////////////////////////////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////////////////////////
/// @brief maximum buffer length when comparing sort conditions
////////////////////////////////////////////////////////////////////////////////

#define COMPARE_LENGTH 128

////////////////////////////////////////////////////////////////////////////////
/// @}
////////////////////////////////////////////////////////////////////////////////
 
// -----------------------------------------------------------------------------
// --SECTION--                                                     private types
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @addtogroup Ahuacatl
/// @{
////////////////////////////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////////////////////////
/// @brief a local optimiser structure that is used temporarily during the
/// AST traversal
////////////////////////////////////////////////////////////////////////////////

typedef struct aql_optimiser_s {
  TRI_aql_context_t* _context;
}
aql_optimiser_t;

////////////////////////////////////////////////////////////////////////////////
/// @}
////////////////////////////////////////////////////////////////////////////////

// -----------------------------------------------------------------------------
// --SECTION--                                                 private functions
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @addtogroup Ahuacatl
/// @{
////////////////////////////////////////////////////////////////////////////////
 
////////////////////////////////////////////////////////////////////////////////
/// @brief create an optimiser structure
////////////////////////////////////////////////////////////////////////////////

static aql_optimiser_t* CreateOptimiser (TRI_aql_context_t* const context) {
  aql_optimiser_t* optimiser;
  
  optimiser = (aql_optimiser_t*) TRI_Allocate(TRI_UNKNOWN_MEM_ZONE, sizeof(aql_optimiser_t), false);

  if (optimiser == NULL) {
    return NULL;
  }

  optimiser->_context = context;

  return optimiser;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief free an optimiser structure
////////////////////////////////////////////////////////////////////////////////

static void FreeOptimiser (aql_optimiser_t* const optimiser) {
  assert(optimiser);

  TRI_Free(TRI_UNKNOWN_MEM_ZONE, optimiser);
}

////////////////////////////////////////////////////////////////////////////////
/// @brief get a vector of sort criteria from a SORT statement in stringified
/// format (e.g. ["u.name", "u._id"]
////////////////////////////////////////////////////////////////////////////////

static bool GetSorts (TRI_aql_statement_walker_t* const walker,
                      const TRI_aql_node_t* const node,
                      TRI_vector_pointer_t* const sorts) {
  TRI_aql_node_t* list = TRI_AQL_NODE_MEMBER(node, 0);
  TRI_string_buffer_t buffer;
  size_t i;

  if (! list || list->_members._length == 0) {
    // no sorts defined. this should not happen if we get here
    return false;
  }
  
  TRI_InitStringBuffer(&buffer, TRI_UNKNOWN_MEM_ZONE);

  for (i = 0; i < list->_members._length; ++i) { 
    // sort element
    TRI_aql_node_t* element    = TRI_AQL_NODE_MEMBER(list, i);
    TRI_aql_node_t* expression = TRI_AQL_NODE_MEMBER(element, 0);
  
    if (! expression) {
      TRI_DestroyStringBuffer(&buffer);
      return false;
    }

    // check sort order. we can only optimise ascending sorts
    if (TRI_AQL_NODE_BOOL(element)) {
      // order is ascending
      char* copy;
      
      TRI_ClearStringBuffer(&buffer);
      TRI_NodeStringAql(&buffer, expression);

      copy = TRI_DuplicateStringZ(TRI_UNKNOWN_MEM_ZONE, buffer._buffer);
      if (copy == NULL) {
        // out of memory
        TRI_DestroyStringBuffer(&buffer);
        return false;
      }
      TRI_PushBackVectorPointer(sorts, copy);
    }
    else {
      // order is descending
      TRI_DestroyStringBuffer(&buffer);
      return false;
    }
  }
  
  TRI_DestroyStringBuffer(&buffer);

  return true;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief check if the rhs vector of sort conditions is fully contained in the
/// lhs vector of sort conditions
////////////////////////////////////////////////////////////////////////////////

static bool IsSortContained (const TRI_vector_pointer_t* const lhs,
                             const TRI_vector_pointer_t* const rhs) {
  size_t i;

  for (i = 0; i < rhs->_length; ++i) {
    char* lhsName = (char*) TRI_AtVectorPointer(lhs, i);
    char* rhsName = (char*) TRI_AtVectorPointer(rhs, i);
  
    if (! TRI_EqualString(lhsName, rhsName)) {
      return false;
    }
  }

  return true;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief check if a sort can be avoided. this compares sort criteria
/// implicitly introduced by using an index with explicit sort criteria defined
/// by a SORT statement. If the SORT criteria is fully covered by using the
/// index, the SORT statement can be avoided
////////////////////////////////////////////////////////////////////////////////

static bool CanAvoidSort (TRI_aql_statement_walker_t* const walker,
                          const TRI_aql_node_t* const node) {
  TRI_aql_scope_t* scope;
  TRI_vector_pointer_t nodeSorts;
  size_t i;
  bool canUse;
  bool result;

  scope = TRI_GetCurrentScopeStatementWalkerAql(walker);
  assert(scope);

  if (scope->_type == TRI_AQL_SCOPE_MAIN) {
    // will not optimise main scope
    return false;
  }

  if (scope->_sorts._length == 0) {
    // no sort criteria collected
    return false;
  }

  TRI_InitVectorPointer(&nodeSorts, TRI_UNKNOWN_MEM_ZONE);
  result = false;

  // get all sort criteria from the SORT statement
  canUse = GetSorts(walker, node, &nodeSorts);
  if (canUse && nodeSorts._length > 0 && nodeSorts._length <= scope->_sorts._length) {
    result = IsSortContained(&scope->_sorts, &nodeSorts);
  }
  
  // free all sort criteria fetched from the node
  for (i = 0; i < nodeSorts._length; ++i) {
    char* criterion = (char*) TRI_AtVectorPointer(&nodeSorts, i);

    TRI_Free(TRI_UNKNOWN_MEM_ZONE, criterion);
  }

  TRI_DestroyVectorPointer(&nodeSorts);

  return result;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief pick an index for the ranges found
////////////////////////////////////////////////////////////////////////////////

static void AttachCollectionHint (TRI_aql_context_t* const context,
                                  TRI_aql_node_t* const node) {
  TRI_aql_node_t* nameNode = TRI_AQL_NODE_MEMBER(node, 0);
  TRI_vector_pointer_t* availableIndexes;
  TRI_aql_collection_hint_t* hint;
  TRI_aql_index_t* idx;
  TRI_aql_collection_t* collection;
  TRI_primary_collection_t* primary;
  char* collectionName;

  collectionName = TRI_AQL_NODE_STRING(nameNode);
  assert(collectionName);
  
  hint = (TRI_aql_collection_hint_t*) TRI_AQL_NODE_DATA(node);
    
  if (hint == NULL) {
    TRI_SetErrorContextAql(context, TRI_ERROR_OUT_OF_MEMORY, NULL);
    return;
  }
  
  if (hint->_ranges == NULL) {
    // no ranges found to be used as indexes
    return;
  }

  collection = TRI_GetCollectionAql(context, collectionName);
  if (collection == NULL) {
    TRI_SetErrorContextAql(context, TRI_ERROR_OUT_OF_MEMORY, NULL);

    return;
  }

  hint->_collection = collection;

  primary = collection->_collection->_collection;

  availableIndexes = &(((TRI_document_collection_t*) primary)->_allIndexes);

  if (availableIndexes == NULL) {
    TRI_SetErrorContextAql(context, TRI_ERROR_OUT_OF_MEMORY, NULL);

    return;
  }

  idx = TRI_DetermineIndexAql(context, 
                              availableIndexes, 
                              collectionName, 
                              hint->_ranges);

  hint->_index = idx;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief copy the sort criteria implicitly introduced by using an index to
/// the current scope. when we encounter a SORT statement, we'll compare the
/// sort criteria in the statement and in the scope, and if they match, we
/// can avoid the extra sorting
////////////////////////////////////////////////////////////////////////////////
    
static void AttachSort (TRI_aql_scope_t* const scope, 
                        const TRI_aql_collection_hint_t* const hint) {
  TRI_index_t* idx;
  char tmp[COMPARE_LENGTH + 2];
  size_t offset;
  size_t i;

  assert(hint);
  assert(scope);
  
  if (scope->_type == TRI_AQL_SCOPE_MAIN) {
    // will not optimise sort in main scope
    return;
  }

  if (scope->_sorts._length > 0) {
    // sorts already defined for the scope. do not overwrite
    return;
  }
  
  if (hint->_variableName == NULL) {
    // no variable name set up for the hint. we don't know enough about it to use it
    return;
  }
  
  if (hint->_index == NULL || hint->_index->_idx == NULL) {
    return;
  }

  idx = hint->_index->_idx;

  // got a valid index access for the collection

  // we are looking for skiplist indexes only, as they are sorted
  if (idx->_type != TRI_IDX_TYPE_SKIPLIST_INDEX) {
    // not a skiplist
    return;
  }
  
  // offset for variable name, containing the dot (e.g. 2 for "u")
  offset = strlen(hint->_variableName);
  if (offset > COMPARE_LENGTH) {
    // variable name is too long
    return;
  }
  
  // copy variable name into buffer for comparisions
  memcpy(tmp, hint->_variableName, offset);
  tmp[offset++] = '.';
  
  for (i = 0; i < idx->_fields._length; ++i) {
    char* indexName = (char*) idx->_fields._buffer[i];
    char* copy;
    size_t len;
   
    len = strlen(indexName);
    if (len + offset > COMPARE_LENGTH) {
      // name is too long
      return;
    }
    memcpy(tmp + offset, indexName, len);
    tmp[offset + len] = '\0'; 

    copy = TRI_DuplicateString2Z(TRI_UNKNOWN_MEM_ZONE, tmp, offset + len);
    if (copy == NULL) {
      // out of memory. now free all criteria we have collected
      size_t j;

      for (j = 0; j < scope->_sorts._length; ++j) {
        char* criterion = (char*) scope->_sorts._buffer[j];

        if (criterion != NULL) {
          TRI_Free(TRI_UNKNOWN_MEM_ZONE, criterion);
        }
      }
      return;
    }

    TRI_PushBackVectorPointer(&scope->_sorts, copy);
  }
}

////////////////////////////////////////////////////////////////////////////////
/// @brief annotate a node with context information
///
/// this is a callback function used by the statement walker
////////////////////////////////////////////////////////////////////////////////

static TRI_aql_node_t* AnnotateNode (TRI_aql_statement_walker_t* const walker,
                                     TRI_aql_node_t* node) {
  if (node->_type == TRI_AQL_NODE_COLLECTION) {
    aql_optimiser_t* optimiser;
    TRI_aql_scope_t* scope;
    TRI_aql_collection_hint_t* hint;

    optimiser = (aql_optimiser_t*) walker->_data;

    AttachCollectionHint(optimiser->_context, node);

    hint = (TRI_aql_collection_hint_t*) TRI_AQL_NODE_DATA(node);
    
    scope = TRI_GetCurrentScopeStatementWalkerAql(walker);
    assert(scope);

    // check if an index is to be used
    if (hint != NULL) {
      AttachSort(scope, hint);
    }
    
    // check if we can apply a scope limit and push it into the collection

    if (scope->_limit._status == TRI_AQL_LIMIT_USE && 
        ! scope->_limit._hasFilter) {
      // yes!
      LOG_TRACE("using limit hint for collection");

      if (hint != NULL) {
        hint->_limit._offset = scope->_limit._offset;
        hint->_limit._limit  = scope->_limit._limit;
        hint->_limit._status = TRI_AQL_LIMIT_USE;
      }
      
      // deactive this limit for any further tries
      scope->_limit._status = TRI_AQL_LIMIT_IGNORE;
    }
  }
  
  return node;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief annotate a statement with context information
///
/// this is a callback function used by the statement walker
////////////////////////////////////////////////////////////////////////////////

static TRI_aql_node_t* AnnotateLoop (TRI_aql_statement_walker_t* const walker,
                                     TRI_aql_node_t* node) {
  if (node->_type == TRI_AQL_NODE_FOR) {
    TRI_aql_scope_t* scope;

    scope = TRI_GetCurrentScopeStatementWalkerAql(walker);
    assert(scope);

    // check if we can apply a scope limit and push it into the for loop
    if (scope->_limit._status == TRI_AQL_LIMIT_USE) {
      // yes!
      TRI_aql_node_t* expression = TRI_AQL_NODE_MEMBER(node, 1);

      if (expression->_type == TRI_AQL_NODE_COLLECTION && ! scope->_limit._hasFilter) {
        // move limit into the COLLECTION node
        TRI_aql_collection_hint_t* hint = (TRI_aql_collection_hint_t*) TRI_AQL_NODE_DATA(expression);

        if (hint != NULL) {
          hint->_limit._offset    = scope->_limit._offset;
          hint->_limit._limit     = scope->_limit._limit;
          hint->_limit._status    = TRI_AQL_LIMIT_USE;
          hint->_limit._hasFilter = scope->_limit._hasFilter;
        }
      }
      else {
        // move limit into the FOR node
        TRI_aql_for_hint_t* hint = (TRI_aql_for_hint_t*) TRI_AQL_NODE_DATA(node);

        if (hint != NULL) {
          // we'll now modify the hint for the for loop
          hint->_limit._offset    = scope->_limit._offset;
          hint->_limit._limit     = scope->_limit._limit;
          hint->_limit._status    = TRI_AQL_LIMIT_USE;
          hint->_limit._hasFilter = scope->_limit._hasFilter;

          LOG_TRACE("using limit hint for for loop");
        }
      }
      
      // deactive this limit for any further tries
      scope->_limit._status = TRI_AQL_LIMIT_IGNORE;
    }
  }
  else if (node->_type == TRI_AQL_NODE_SORT) {
    // we have found a SORT statement
    // now check if we can avoid it. this will be the case if we access the
    // collection via a sorted index (skiplist)
    TRI_aql_scope_t* scope;
      
    if (CanAvoidSort(walker, node)) {
      LOG_TRACE("removing unnecessary sort statement");

      return TRI_GetDummyNopNodeAql();
    }

    // we have found a SORT statement, but it must be preserved

    // we must now free the sort criteria we collected for the scope
    // otherwise we would get potentially invalid SORT results
    scope = TRI_GetCurrentScopeStatementWalkerAql(walker);
    assert(scope);

    while (scope->_sorts._length > 0) {
      char* criterion = TRI_RemoveVectorPointer(&scope->_sorts, (size_t) (scope->_sorts._length - 1));

      if (criterion != NULL) {
        TRI_Free(TRI_UNKNOWN_MEM_ZONE, criterion);
      }
    }
  }

  return node;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief create javascript function code for a relational operation
////////////////////////////////////////////////////////////////////////////////

static TRI_string_buffer_t* RelationCode (const char* const name, 
                                          const TRI_aql_node_t* const lhs,
                                          const TRI_aql_node_t* const rhs) {
  TRI_string_buffer_t* buffer = TRI_CreateStringBuffer(TRI_UNKNOWN_MEM_ZONE);

  if (! lhs || ! rhs) {
    return NULL;
  }
  
  if (TRI_AppendStringStringBuffer(buffer, "(function(){ var aql = require(\"org/arangodb/ahuacatl\"); return aql.RELATIONAL_") != TRI_ERROR_NO_ERROR) {
    TRI_FreeStringBuffer(TRI_UNKNOWN_MEM_ZONE, buffer);
    return NULL;
  }

  if (TRI_AppendStringStringBuffer(buffer, name) != TRI_ERROR_NO_ERROR) {
    TRI_FreeStringBuffer(TRI_UNKNOWN_MEM_ZONE, buffer);
    return NULL;
  }
  
  if (TRI_AppendStringStringBuffer(buffer, "(") != TRI_ERROR_NO_ERROR) {
    TRI_FreeStringBuffer(TRI_UNKNOWN_MEM_ZONE, buffer);
    return NULL;
  }
    
  if (! TRI_NodeJavascriptAql(buffer, lhs)) {
    TRI_FreeStringBuffer(TRI_UNKNOWN_MEM_ZONE, buffer);
    return NULL;
  }
      
  if (TRI_AppendCharStringBuffer(buffer, ',') != TRI_ERROR_NO_ERROR) {
    TRI_FreeStringBuffer(TRI_UNKNOWN_MEM_ZONE, buffer);
    return NULL;
  }
  
  if (! TRI_NodeJavascriptAql(buffer, rhs)) {
    TRI_FreeStringBuffer(TRI_UNKNOWN_MEM_ZONE, buffer);
    return NULL;
  }

  if (TRI_AppendStringStringBuffer(buffer, ");})") != TRI_ERROR_NO_ERROR) {
    TRI_FreeStringBuffer(TRI_UNKNOWN_MEM_ZONE, buffer);
    return NULL;
  }

  return buffer;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief create javascript function code for a function call
////////////////////////////////////////////////////////////////////////////////

static TRI_string_buffer_t* FcallCode (const char* const name, 
                                       const TRI_aql_node_t* const args) {
  TRI_string_buffer_t* buffer = TRI_CreateStringBuffer(TRI_UNKNOWN_MEM_ZONE);
  size_t i;
  size_t n;

  if (! buffer) {
    return NULL;
  }
  
  if (TRI_AppendStringStringBuffer(buffer, "(function(){ var aql = require(\"org/arangodb/ahuacatl\"); return aql.") != TRI_ERROR_NO_ERROR) {
    TRI_FreeStringBuffer(TRI_UNKNOWN_MEM_ZONE, buffer);
    return NULL;
  }
  
  if (TRI_AppendStringStringBuffer(buffer, name) != TRI_ERROR_NO_ERROR) {
    TRI_FreeStringBuffer(TRI_UNKNOWN_MEM_ZONE, buffer);
    return NULL;
  }

  if (TRI_AppendCharStringBuffer(buffer, '(') != TRI_ERROR_NO_ERROR) {
    TRI_FreeStringBuffer(TRI_UNKNOWN_MEM_ZONE, buffer);
    return NULL;
  }

  n = args->_members._length;
  for (i = 0; i < n; ++i) {
    TRI_aql_node_t* arg = (TRI_aql_node_t*) args->_members._buffer[i];
    if (i > 0) {
      if (TRI_AppendCharStringBuffer(buffer, ',') != TRI_ERROR_NO_ERROR) {
        TRI_FreeStringBuffer(TRI_UNKNOWN_MEM_ZONE, buffer);
        return NULL;
      }
    }

    if (! TRI_NodeJavascriptAql(buffer, arg)) {
      TRI_FreeStringBuffer(TRI_UNKNOWN_MEM_ZONE, buffer);
      return NULL;
    }
  }

  if (TRI_AppendStringStringBuffer(buffer, ");})") != TRI_ERROR_NO_ERROR) {
    TRI_FreeStringBuffer(TRI_UNKNOWN_MEM_ZONE, buffer);
    return NULL;
  }

  return buffer;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief optimise a function call
////////////////////////////////////////////////////////////////////////////////

static TRI_aql_node_t* OptimiseFcall (TRI_aql_context_t* const context,
                                      TRI_aql_node_t* node) {
  TRI_aql_node_t* args = TRI_AQL_NODE_MEMBER(node, 0);
  TRI_aql_function_t* function;
  TRI_js_exec_context_t* execContext;
  TRI_string_buffer_t* code;
  TRI_json_t* json;
  size_t i;
  size_t n;

  function = (TRI_aql_function_t*) TRI_AQL_NODE_DATA(node);
  assert(function);

  // check if function is deterministic
  if (! function->_isDeterministic) {
    return node;
  }

  // check if function call arguments are deterministic
  n = args->_members._length;
  for (i = 0; i < n; ++i) {
    TRI_aql_node_t* arg = (TRI_aql_node_t*) args->_members._buffer[i];

    if (! arg || ! TRI_IsConstantValueNodeAql(arg)) {
      return node;
    }
  }
 
  // all arguments are constants
  // create the function code
  code = FcallCode(function->_internalName, args);
  if (! code) {
    TRI_SetErrorContextAql(context, TRI_ERROR_OUT_OF_MEMORY, NULL);
    return node;
  }
    
  // execute the function code
  execContext = TRI_CreateExecutionContext(code->_buffer);
  TRI_FreeStringBuffer(TRI_UNKNOWN_MEM_ZONE, code);

  if (! execContext) {
    TRI_SetErrorContextAql(context, TRI_ERROR_OUT_OF_MEMORY, NULL);
    return node;
  }

  json = TRI_ExecuteResultContext(execContext);
  TRI_FreeExecutionContext(execContext);
  if (! json) {
    // cannot optimise the function call due to an internal error

    // TODO: check whether we can validate the arguments here already and return an error early
    // TRI_SetErrorContextAql(context, TRI_ERROR_QUERY_SCRIPT, "function optimisation");
    return node;
  }

  // use the constant values instead of the function call node
  node = TRI_JsonNodeAql(context, json);
  if (! node) {
    TRI_SetErrorContextAql(context, TRI_ERROR_OUT_OF_MEMORY, NULL);
  }

  TRI_FreeJson(TRI_UNKNOWN_MEM_ZONE, json);
  
  LOG_TRACE("optimised function call");

  return node;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief optimise a FOR statement
/// this will remove the complete statement if the value to iterate over is
/// an empty list
////////////////////////////////////////////////////////////////////////////////

static TRI_aql_node_t* OptimiseFor (TRI_aql_statement_walker_t* const walker,
                                    TRI_aql_node_t* node) {
  TRI_aql_node_t* expression = TRI_AQL_NODE_MEMBER(node, 1);

  if (expression->_type == TRI_AQL_NODE_LIST && expression->_members._length == 0) {
    // for statement with a list expression
    // list is empty => we can eliminate the for statement
    LOG_TRACE("optimised away empty for loop");

    return TRI_GetDummyReturnEmptyNodeAql();
  }

  return node;
}
 
////////////////////////////////////////////////////////////////////////////////
/// @brief optimise a SORT statement
/// this will remove constant sort expressions (which should not have 
/// any impact on the result)
////////////////////////////////////////////////////////////////////////////////

static TRI_aql_node_t* OptimiseSort (TRI_aql_statement_walker_t* const walker,
                                     TRI_aql_node_t* node) {
  TRI_aql_node_t* list = TRI_AQL_NODE_MEMBER(node, 0);
  size_t i, n;
  
  if (! list) {
    return node;
  }

  i = 0;
  n = list->_members._length;
  while (i < n) {
    // sort element
    TRI_aql_node_t* element    = TRI_AQL_NODE_MEMBER(list, i);
    TRI_aql_node_t* expression = TRI_AQL_NODE_MEMBER(element, 0);
  
    // check if the sort element is constant
    if (! expression || ! TRI_IsConstantValueNodeAql(expression)) {
      ++i;
      continue;
    }

    // sort element is constant so it can be removed
    TRI_RemoveVectorPointer(&list->_members, i);
    --n;

    LOG_TRACE("optimised away sort element");
  }

  if (n == 0) {
    // no members left => sort removed
    LOG_TRACE("optimised away sort");

    return TRI_GetDummyNopNodeAql();
  }
  
  // if we got here, at least one sort criterion remained

  return node;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief optimise a LIMIT statement
/// optimise away a limit x, 0 statement
////////////////////////////////////////////////////////////////////////////////

static TRI_aql_node_t* OptimiseLimit (TRI_aql_statement_walker_t* const walker,
                                      TRI_aql_node_t* node) {
  TRI_aql_scope_t* scope; 
  TRI_aql_node_t* limit;
  int64_t limitValue;
  
  assert(node);

  scope = TRI_GetCurrentScopeStatementWalkerAql(walker);
  assert(scope);
    
  limit = TRI_AQL_NODE_MEMBER(node, 1);
  limitValue = TRI_AQL_NODE_INT(limit);

  // check for the easy case, a limit value of 0, e.g. LIMIT 10, 0
  if (limitValue == 0) {
    // LIMIT x, 0 makes the complete scope useless
    LOG_TRACE("optimised away limit");

    return TRI_GetDummyReturnEmptyNodeAql();
  }

  // we will not optimise in the main scope, e.g. LIMIT 5 RETURN 1
  if (scope->_type == TRI_AQL_SCOPE_MAIN || scope->_type == TRI_AQL_SCOPE_FOR_NESTED) {
    return node;
  }
      
  // now for the more complex checks
  // is a limit optimisation allowed in the scope?
  if (scope->_limit._status == TRI_AQL_LIMIT_USE || scope->_limit._status == TRI_AQL_LIMIT_UNDEFINED) {
    // we have found a limit that we can potentially use

    // we'll only push up the first limit in a scope, as there might be queries such as LIMIT 10 LIMIT 2
    if (++scope->_limit._found == 1) {
      // we can push the limit up, into the for loop or the collection access
      LOG_TRACE("pushed up limit");
      return TRI_GetDummyNopNodeAql();
    }
  }

  return node;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief optimise a constant FILTER expression
/// constant filters that evaluate to true will be replaced by a NOP node
/// constant filters that evaluate to false will be replaced by an empty node
/// that will remove the complete scope on statement list compaction
////////////////////////////////////////////////////////////////////////////////

static TRI_aql_node_t* OptimiseConstantFilter (TRI_aql_node_t* const node) {
  if (TRI_GetBooleanNodeValueAql(node)) {
    // filter expression is always true => remove it
    LOG_TRACE("optimised away constant (true) filter");

    return TRI_GetDummyNopNodeAql();
  }

  // filter expression is always false => invalidate surrounding scope(s)
  LOG_TRACE("optimised away scope"); 

  return TRI_GetDummyReturnEmptyNodeAql();
}

////////////////////////////////////////////////////////////////////////////////
/// @brief optimise a FILTER statement
////////////////////////////////////////////////////////////////////////////////

static TRI_aql_node_t* OptimiseFilter (TRI_aql_statement_walker_t* const walker,
                                       TRI_aql_node_t* node) {
  aql_optimiser_t* optimiser = (aql_optimiser_t*) walker->_data;
  TRI_aql_node_t* expression = TRI_AQL_NODE_MEMBER(node, 0);
  TRI_aql_scope_t* scope;

  while (true) {
    TRI_vector_pointer_t* oldRanges;
    TRI_vector_pointer_t* newRanges;
    bool changed;

    if (! expression) {
      return node;
    }

    if (TRI_IsConstantValueNodeAql(expression)) {
      // filter expression is a constant value
      return OptimiseConstantFilter(expression);
    }

    // filter expression is non-constant
    oldRanges = TRI_GetCurrentRangesStatementWalkerAql(walker);
    
    changed = false;
    newRanges = TRI_OptimiseRangesAql(optimiser->_context, expression, &changed, oldRanges);
    
    if (newRanges) {
      TRI_SetCurrentRangesStatementWalkerAql(walker, newRanges);
    }
    
    if (! changed) {
      break;
    }

    // expression code was changed, set pointer to new value re-optimise it
    node->_members._buffer[0] = OptimiseNode(walker, expression);
    expression = TRI_AQL_NODE_MEMBER(node, 0);

    // next iteration
  }
  
  // in case we got here, the filter was not optimised away completely
  scope = TRI_GetCurrentScopeStatementWalkerAql(walker);
  assert(scope);

  // mark in the scope that we found a filter
  scope->_limit._hasFilter = true;

  return node;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief optimise a reference expression
///
/// this looks up the source node that defines the variable and checks if the
/// variable has a constant value. if yes, then the reference is replaced with
/// the constant value
/// e.g. in the query "let a = 1 for x in a ...", the latter a would be replaced
/// by the value "1".
////////////////////////////////////////////////////////////////////////////////

static TRI_aql_node_t* OptimiseReference (TRI_aql_statement_walker_t* const walker,
                                          TRI_aql_node_t* node) {
  TRI_aql_variable_t* variable;
  TRI_aql_node_t* definingNode;
  char* variableName = (char*) TRI_AQL_NODE_STRING(node);
  size_t scopeCount; // ignored

  assert(variableName);
  variable = TRI_GetVariableStatementWalkerAql(walker, variableName, &scopeCount);

  if (variable == NULL) {
    return node;
  }

  definingNode = variable->_definingNode;
  if (definingNode == NULL) {
    return node;
  }

  if (definingNode->_type == TRI_AQL_NODE_LET) {
    // variable is defined via a let
    TRI_aql_node_t* expressionNode;

    expressionNode = TRI_AQL_NODE_MEMBER(definingNode, 1);
    if (expressionNode && TRI_IsConstantValueNodeAql(expressionNode)) {
      // the source variable is constant, so we can replace the reference with 
      // the source's value
      return expressionNode;
    }
  }

  return node;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief optimise an arithmetic operation with one operand
////////////////////////////////////////////////////////////////////////////////

static TRI_aql_node_t* OptimiseUnaryArithmeticOperation (TRI_aql_context_t* const context,
                                                         TRI_aql_node_t* node) {
  TRI_aql_node_t* operand = TRI_AQL_NODE_MEMBER(node, 0);
  
  assert(node->_type == TRI_AQL_NODE_OPERATOR_UNARY_PLUS ||
         node->_type == TRI_AQL_NODE_OPERATOR_UNARY_MINUS);

  if (! operand || ! TRI_IsConstantValueNodeAql(operand)) {
    return node;
  }

  if (! TRI_IsNumericValueNodeAql(operand)) {
    TRI_SetErrorContextAql(context, TRI_ERROR_QUERY_INVALID_ARITHMETIC_VALUE, NULL);
    return node;
  }
  

  if (node->_type == TRI_AQL_NODE_OPERATOR_UNARY_PLUS) {
    // + number => number
    node = operand;
  }
  else if (node->_type == TRI_AQL_NODE_OPERATOR_UNARY_MINUS) {
    // - number => eval!
    double value = - TRI_GetNumericNodeValueAql(operand);
    
    // check for result validity
    if (value != value) {
      // IEEE754 NaN values have an interesting property that we can exploit...
      // if the architecture does not use IEEE754 values then this shouldn't do
      // any harm either
      LOG_TRACE("nan value detected after arithmetic optimisation");
      TRI_SetErrorContextAql(context, TRI_ERROR_QUERY_INVALID_ARITHMETIC_VALUE, NULL);
      return NULL;
    }
   
    // test for infinity 
    if (value == HUGE_VAL || value == -HUGE_VAL) {
      LOG_TRACE("inf value detected after arithmetic optimisation");
      TRI_SetErrorContextAql(context, TRI_ERROR_QUERY_NUMBER_OUT_OF_RANGE, NULL);
      return NULL;
    }

    node = TRI_CreateNodeValueDoubleAql(context, value);
    if (node == NULL) {
      TRI_SetErrorContextAql(context, TRI_ERROR_OUT_OF_MEMORY, NULL);
    }
  }

  return node;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief optimise a boolean operation with one operand
////////////////////////////////////////////////////////////////////////////////

static TRI_aql_node_t* OptimiseUnaryLogicalOperation (TRI_aql_context_t* const context,
                                                      TRI_aql_node_t* node) {
  TRI_aql_node_t* operand = TRI_AQL_NODE_MEMBER(node, 0);
  
  assert(node->_type == TRI_AQL_NODE_OPERATOR_UNARY_NOT);

  if (! operand || ! TRI_IsConstantValueNodeAql(operand)) {
    // node is not a constant value
    return node;
  }

  if (! TRI_IsBooleanValueNodeAql(operand)) {
    // value type is not boolean => error
    TRI_SetErrorContextAql(context, TRI_ERROR_QUERY_INVALID_LOGICAL_VALUE, NULL);
    return node;
  }
  
  // ! (bool value) => evaluate and replace with result
  node = TRI_CreateNodeValueBoolAql(context, ! TRI_GetBooleanNodeValueAql(operand));
  if (! node) {
    TRI_SetErrorContextAql(context, TRI_ERROR_OUT_OF_MEMORY, NULL);
  }

  LOG_TRACE("optimised away unary logical operation");

  return node;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief optimise a boolean operation with two operands
////////////////////////////////////////////////////////////////////////////////

static TRI_aql_node_t* OptimiseBinaryLogicalOperation (TRI_aql_context_t* const context,
                                                       TRI_aql_node_t* node) {
  TRI_aql_node_t* lhs = TRI_AQL_NODE_MEMBER(node, 0);
  TRI_aql_node_t* rhs = TRI_AQL_NODE_MEMBER(node, 1);
  bool isEligibleLhs;
  bool isEligibleRhs;
  bool lhsValue;

  if (! lhs || ! rhs) {
    return node;
  } 

  isEligibleLhs = TRI_IsConstantValueNodeAql(lhs);
  isEligibleRhs = TRI_IsConstantValueNodeAql(rhs);

  if (isEligibleLhs && ! TRI_IsBooleanValueNodeAql(lhs)) {
    // value type is not boolean => error
    TRI_SetErrorContextAql(context, TRI_ERROR_QUERY_INVALID_LOGICAL_VALUE, NULL);
    return node;
  }

  if (isEligibleRhs && ! TRI_IsBooleanValueNodeAql(rhs)) {
    // value type is not boolean => error
    TRI_SetErrorContextAql(context, TRI_ERROR_QUERY_INVALID_LOGICAL_VALUE, NULL);
    return node;
  }

  if (! isEligibleLhs || ! isEligibleRhs) {
    // node is not a constant value
    return node;
  }

  lhsValue = TRI_GetBooleanNodeValueAql(lhs);
  
  assert(node->_type == TRI_AQL_NODE_OPERATOR_BINARY_AND ||
         node->_type == TRI_AQL_NODE_OPERATOR_BINARY_OR);

  LOG_TRACE("optimised away binary logical operation");

  if (node->_type == TRI_AQL_NODE_OPERATOR_BINARY_AND) {
    if (lhsValue) {
      // if (true && rhs) => rhs
      return rhs;
    }

    // if (false && rhs) => false
    return lhs;
  } 
  else if (node->_type == TRI_AQL_NODE_OPERATOR_BINARY_OR) {
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
/// @brief optimise a relational operation with two operands
////////////////////////////////////////////////////////////////////////////////

static TRI_aql_node_t* OptimiseBinaryRelationalOperation (TRI_aql_context_t* const context,
                                                          TRI_aql_node_t* node) {
  TRI_aql_node_t* lhs = TRI_AQL_NODE_MEMBER(node, 0);
  TRI_aql_node_t* rhs = TRI_AQL_NODE_MEMBER(node, 1);
  TRI_js_exec_context_t* execContext;
  TRI_string_buffer_t* code;
  TRI_json_t* json;
  char* func;
  
  if (! lhs || ! TRI_IsConstantValueNodeAql(lhs) || ! rhs || ! TRI_IsConstantValueNodeAql(rhs)) {
    return node;
  } 
  
  if (node->_type == TRI_AQL_NODE_OPERATOR_BINARY_EQ) {
    func = "EQUAL";
  }
  else if (node->_type == TRI_AQL_NODE_OPERATOR_BINARY_NE) {
    func = "UNEQUAL";
  }
  else if (node->_type == TRI_AQL_NODE_OPERATOR_BINARY_GT) {
    func = "GREATER";
  }
  else if (node->_type == TRI_AQL_NODE_OPERATOR_BINARY_GE) {
    func = "GREATEREQUAL";
  }
  else if (node->_type == TRI_AQL_NODE_OPERATOR_BINARY_LT) {
    func = "LESS";
  }
  else if (node->_type == TRI_AQL_NODE_OPERATOR_BINARY_LE) {
    func = "LESSEQUAL";
  }
  else if (node->_type == TRI_AQL_NODE_OPERATOR_BINARY_IN) {
    func = "IN";
  }
  else {
    // not what we expected, however, simply continue
    return node;
  }
  
  code = RelationCode(func, lhs, rhs); 
  if (! code) {
    TRI_SetErrorContextAql(context, TRI_ERROR_OUT_OF_MEMORY, NULL);
    return node;
  }
    
  // execute the function code
  execContext = TRI_CreateExecutionContext(code->_buffer);
  TRI_FreeStringBuffer(TRI_UNKNOWN_MEM_ZONE, code);

  if (! execContext) {
    TRI_SetErrorContextAql(context, TRI_ERROR_OUT_OF_MEMORY, NULL);
    return node;
  }

  json = TRI_ExecuteResultContext(execContext);
  TRI_FreeExecutionContext(execContext);
  if (! json) {
    TRI_SetErrorContextAql(context, TRI_ERROR_QUERY_SCRIPT, NULL);
    return NULL;
  }
  
  // use the constant values instead of the function call node
  node = TRI_JsonNodeAql(context, json);
  if (! node) {
    TRI_SetErrorContextAql(context, TRI_ERROR_OUT_OF_MEMORY, NULL);
  }
    
  LOG_TRACE("optimised away binary relational operation");

  TRI_FreeJson(TRI_UNKNOWN_MEM_ZONE, json);

  return node;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief optimise an arithmetic operation with two operands
////////////////////////////////////////////////////////////////////////////////

static TRI_aql_node_t* OptimiseBinaryArithmeticOperation (TRI_aql_context_t* const context,
                                                          TRI_aql_node_t* node) {
  TRI_aql_node_t* lhs = TRI_AQL_NODE_MEMBER(node, 0);
  TRI_aql_node_t* rhs = TRI_AQL_NODE_MEMBER(node, 1);
  bool isEligibleLhs;
  bool isEligibleRhs;
  double value;
  
  if (! lhs || ! rhs) {
    return node;
  } 

  isEligibleLhs = TRI_IsConstantValueNodeAql(lhs);
  isEligibleRhs = TRI_IsConstantValueNodeAql(rhs);

  if (isEligibleLhs && ! TRI_IsNumericValueNodeAql(lhs)) {
    // node is not a numeric value => error
    TRI_SetErrorContextAql(context, TRI_ERROR_QUERY_INVALID_ARITHMETIC_VALUE, NULL);
    return node;
  }

  
  if (isEligibleRhs && ! TRI_IsNumericValueNodeAql(rhs)) {
    // node is not a numeric value => error
    TRI_SetErrorContextAql(context, TRI_ERROR_QUERY_INVALID_ARITHMETIC_VALUE, NULL);
    return node;
  }
  

  if (! isEligibleLhs || ! isEligibleRhs) {
    return node;
  }
  
  assert(node->_type == TRI_AQL_NODE_OPERATOR_BINARY_PLUS ||
         node->_type == TRI_AQL_NODE_OPERATOR_BINARY_MINUS ||
         node->_type == TRI_AQL_NODE_OPERATOR_BINARY_TIMES ||
         node->_type == TRI_AQL_NODE_OPERATOR_BINARY_DIV ||
         node->_type == TRI_AQL_NODE_OPERATOR_BINARY_MOD);

  if (node->_type == TRI_AQL_NODE_OPERATOR_BINARY_PLUS) {
    value = TRI_GetNumericNodeValueAql(lhs) + TRI_GetNumericNodeValueAql(rhs);
  }
  else if (node->_type == TRI_AQL_NODE_OPERATOR_BINARY_MINUS) {
    value = TRI_GetNumericNodeValueAql(lhs) - TRI_GetNumericNodeValueAql(rhs);
  }
  else if (node->_type == TRI_AQL_NODE_OPERATOR_BINARY_TIMES) {
    value = TRI_GetNumericNodeValueAql(lhs) * TRI_GetNumericNodeValueAql(rhs);
  }
  else if (node->_type == TRI_AQL_NODE_OPERATOR_BINARY_DIV) {
    if (TRI_GetNumericNodeValueAql(rhs) == 0.0) {
      // division by zero
      TRI_SetErrorContextAql(context, TRI_ERROR_QUERY_DIVISION_BY_ZERO, NULL);
      return node;
    }
    value = TRI_GetNumericNodeValueAql(lhs) / TRI_GetNumericNodeValueAql(rhs);
  }
  else if (node->_type == TRI_AQL_NODE_OPERATOR_BINARY_MOD) {
    if (TRI_GetNumericNodeValueAql(rhs) == 0.0) {
      // division by zero
      TRI_SetErrorContextAql(context, TRI_ERROR_QUERY_DIVISION_BY_ZERO, NULL);
      return node;
    }
    value = fmod(TRI_GetNumericNodeValueAql(lhs), TRI_GetNumericNodeValueAql(rhs));
  }
  else {
    value = 0.0;
  }

  // check for result validity
  if (value != value) {
    // IEEE754 NaN values have an interesting property that we can exploit...
    // if the architecture does not use IEEE754 values then this shouldn't do
    // any harm either
    LOG_TRACE("nan value detected after arithmetic optimisation");
    TRI_SetErrorContextAql(context, TRI_ERROR_QUERY_INVALID_ARITHMETIC_VALUE, NULL);
    return NULL;
  }
   
  // test for infinity 
  if (value == HUGE_VAL || value == -HUGE_VAL) {
    LOG_TRACE("inf value detected after arithmetic optimisation");
    TRI_SetErrorContextAql(context, TRI_ERROR_QUERY_NUMBER_OUT_OF_RANGE, NULL);
    return NULL;
  }

  node = TRI_CreateNodeValueDoubleAql(context, value);

  if (node == NULL) {
    TRI_SetErrorContextAql(context, TRI_ERROR_OUT_OF_MEMORY, NULL);
    return NULL;
  }
  
  LOG_TRACE("optimised away binary arithmetic operation");

  return node;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief optimise the ternary operation
////////////////////////////////////////////////////////////////////////////////

static TRI_aql_node_t* OptimiseTernaryOperation (TRI_aql_context_t* const context,
                                                 TRI_aql_node_t* node) {
  TRI_aql_node_t* condition = TRI_AQL_NODE_MEMBER(node, 0);
  TRI_aql_node_t* truePart = TRI_AQL_NODE_MEMBER(node, 1);
  TRI_aql_node_t* falsePart = TRI_AQL_NODE_MEMBER(node, 2);
  
  assert(node->_type == TRI_AQL_NODE_OPERATOR_TERNARY);

  if (! condition || ! TRI_IsConstantValueNodeAql(condition)) {
    // node is not a constant value
    return node;
  }

  if (! TRI_IsBooleanValueNodeAql(condition)) {
    // node is not a boolean value => error
    TRI_SetErrorContextAql(context, TRI_ERROR_QUERY_INVALID_LOGICAL_VALUE, NULL);
    return node;
  }
  
  if (! truePart || ! falsePart) {
    // true or false parts not defined
    //  should not happen but we must not continue in this case
    return node;
  }
    
  LOG_TRACE("optimised away ternary operation");
  
  // evaluate condition
  if (TRI_GetBooleanNodeValueAql(condition)) {
    // condition is true, replace with truePart
    return truePart;
  }
  
  // condition is true, replace with falsePart
  return falsePart;
}


////////////////////////////////////////////////////////////////////////////////
/// @brief optimise a specific node
///
/// This is a callback function called by the statement walker
////////////////////////////////////////////////////////////////////////////////

static TRI_aql_node_t* OptimiseNode (TRI_aql_statement_walker_t* const walker, 
                                     TRI_aql_node_t* node) {
  TRI_aql_context_t* context = ((aql_optimiser_t*) walker->_data)->_context;

  assert(node);

  // node optimisations
  switch (node->_type) {
    case TRI_AQL_NODE_OPERATOR_UNARY_PLUS:
    case TRI_AQL_NODE_OPERATOR_UNARY_MINUS:
      return OptimiseUnaryArithmeticOperation(context, node);
    case TRI_AQL_NODE_OPERATOR_UNARY_NOT:
      return OptimiseUnaryLogicalOperation(context, node);
    case TRI_AQL_NODE_OPERATOR_BINARY_AND:
    case TRI_AQL_NODE_OPERATOR_BINARY_OR:
      return OptimiseBinaryLogicalOperation(context, node);
    case TRI_AQL_NODE_OPERATOR_BINARY_EQ:
    case TRI_AQL_NODE_OPERATOR_BINARY_NE:
    case TRI_AQL_NODE_OPERATOR_BINARY_LT:
    case TRI_AQL_NODE_OPERATOR_BINARY_LE:
    case TRI_AQL_NODE_OPERATOR_BINARY_GT:
    case TRI_AQL_NODE_OPERATOR_BINARY_GE:
    case TRI_AQL_NODE_OPERATOR_BINARY_IN:
      return OptimiseBinaryRelationalOperation(context, node);
    case TRI_AQL_NODE_OPERATOR_BINARY_PLUS:
    case TRI_AQL_NODE_OPERATOR_BINARY_MINUS:
    case TRI_AQL_NODE_OPERATOR_BINARY_TIMES:
    case TRI_AQL_NODE_OPERATOR_BINARY_DIV:
    case TRI_AQL_NODE_OPERATOR_BINARY_MOD:
      return OptimiseBinaryArithmeticOperation(context, node);
    case TRI_AQL_NODE_OPERATOR_TERNARY:
      return OptimiseTernaryOperation(context, node);
    case TRI_AQL_NODE_FCALL:
      return OptimiseFcall(context, node);
    case TRI_AQL_NODE_REFERENCE:
      return OptimiseReference(walker, node);
    default: 
      break;
  }

  return node;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief optimise statement, first iteration
/// this will only recurse into certain top-level statements
////////////////////////////////////////////////////////////////////////////////

static TRI_aql_node_t* OptimiseStatement (TRI_aql_statement_walker_t* const walker, 
                                          TRI_aql_node_t* node) {
  assert(walker);
  assert(node);
  
  // node optimisations
  switch (node->_type) {
    case TRI_AQL_NODE_FOR:
      return OptimiseFor(walker, node);
    case TRI_AQL_NODE_SORT:
      return OptimiseSort(walker, node);
    case TRI_AQL_NODE_LIMIT:
      return OptimiseLimit(walker, node);
    case TRI_AQL_NODE_FILTER:
      return OptimiseFilter(walker, node);
    default: {
    }
  }

  return node;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief patch variables with range information
////////////////////////////////////////////////////////////////////////////////

static void PatchVariables (TRI_aql_statement_walker_t* const walker) {
  TRI_aql_context_t* context = ((aql_optimiser_t*) walker->_data)->_context;
  TRI_vector_pointer_t* ranges;
  size_t i, n;

  ranges = TRI_GetCurrentRangesStatementWalkerAql(walker);
  if (ranges == NULL) {
    // no ranges defined, exit early
    return;
  }

  // iterate over all ranges found
  n = ranges->_length;
  for (i = 0; i < n; ++i) {
    TRI_aql_field_access_t* fieldAccess;
    TRI_aql_variable_t* variable;
    TRI_aql_node_t* definingNode;
    TRI_aql_node_t* expressionNode;
    char* variableName;
    size_t scopeCount;
    bool isReference;

    fieldAccess = (TRI_aql_field_access_t*) TRI_AtVectorPointer(ranges, i);
    assert(fieldAccess);
    assert(fieldAccess->_fullName);
    assert(fieldAccess->_variableNameLength > 0);

    variableName = TRI_DuplicateString2Z(TRI_UNKNOWN_MEM_ZONE, fieldAccess->_fullName, fieldAccess->_variableNameLength);
    if (variableName == NULL) {
      // out of memory!
      TRI_SetErrorContextAql(context, TRI_ERROR_OUT_OF_MEMORY, NULL);
      return;
    }

    isReference = (fieldAccess->_type == TRI_AQL_ACCESS_REFERENCE);

    variable = TRI_GetVariableStatementWalkerAql(walker, variableName, &scopeCount);

    if (variable == NULL) {
      TRI_FreeString(TRI_UNKNOWN_MEM_ZONE, variableName);
      continue;
    }

    if (isReference && scopeCount > 0) {
      // unfortunately, the referenced variable is in an outer scope, so we cannot use it
      TRI_FreeString(TRI_UNKNOWN_MEM_ZONE, variableName);
      continue;
    }

    // note: we must not modify outer variables of subqueries 

    // get the node that defines the variable
    definingNode = variable->_definingNode;

    assert(definingNode != NULL);

    expressionNode = NULL;
    switch (definingNode->_type) {
      case TRI_AQL_NODE_LET:
        expressionNode = TRI_AQL_NODE_MEMBER(definingNode, 1);
        break;
      case TRI_AQL_NODE_FOR:
        expressionNode = TRI_AQL_NODE_MEMBER(definingNode, 1);
        break;
      default: {
      }
    }

    if (expressionNode != NULL) {
      if (expressionNode->_type == TRI_AQL_NODE_FCALL) {
        // the defining node is a function call
        // get the function name
        TRI_aql_function_t* function = TRI_AQL_NODE_DATA(expressionNode);

        if (function->optimise != NULL) {
          // call the function's optimise callback
          function->optimise(expressionNode, context, fieldAccess);
        }
      }

      if (expressionNode->_type == TRI_AQL_NODE_COLLECTION) {
        TRI_aql_collection_hint_t* hint = (TRI_aql_collection_hint_t*) (TRI_AQL_NODE_DATA(expressionNode));
        if (hint->_variableName == NULL) {
          // note the variable name used for the collection (e.g. "u" for "FOR u IN users")
          hint->_variableName = TRI_DuplicateStringZ(TRI_UNKNOWN_MEM_ZONE, variableName);
        }

        // set new value
        hint->_ranges = TRI_AddAccessAql(context, hint->_ranges, fieldAccess);
      }
    }
    
    TRI_FreeString(TRI_UNKNOWN_MEM_ZONE, variableName);
  } 
}

////////////////////////////////////////////////////////////////////////////////
/// @brief note LIMIT values for the current scope
/// these values may be used later for pushing a LIMIT into a for-loop or a 
/// collection accessor
////////////////////////////////////////////////////////////////////////////////

static void NoteLimit (TRI_aql_statement_walker_t* const walker,
                       const TRI_aql_node_t* const node) {
  TRI_aql_node_t* offset  = TRI_AQL_NODE_MEMBER(node, 0);
  TRI_aql_node_t* limit  = TRI_AQL_NODE_MEMBER(node, 1);
  int64_t offsetValue = TRI_AQL_NODE_INT(offset);
  int64_t limitValue = TRI_AQL_NODE_INT(limit);
  TRI_aql_scope_t* scope;
      
  scope = TRI_GetCurrentScopeStatementWalkerAql(walker);
  if (scope->_type != TRI_AQL_SCOPE_MAIN) {
    // will not optimise limit in main scope, e.g. "LIMIT 5, 0 RETURN 1"
    TRI_SetCurrentLimitStatementWalkerAql(walker, offsetValue, limitValue);
  }
}

////////////////////////////////////////////////////////////////////////////////
/// @brief optimise a statement
///
/// this is a callback function used by the statement walker
/// it is called after the node is initially optimised
////////////////////////////////////////////////////////////////////////////////

static TRI_aql_node_t* ProcessStatement (TRI_aql_statement_walker_t* const walker,
                                         TRI_aql_node_t* node) {
  if (node) {
    if (node->_type == TRI_AQL_NODE_SORT) {
      // SORT means we must not push a following LIMIT clause up
      TRI_IgnoreCurrentLimitStatementWalkerAql(walker);
    } 
    else if (node->_type == TRI_AQL_NODE_COLLECT) {
      // COLLECT means we must not push a following LIMIT clause up
      TRI_IgnoreCurrentLimitStatementWalkerAql(walker);
    }
    else if (node->_type == TRI_AQL_NODE_FILTER) {
      // FILTER means we can push a following LIMIT clause up to the for loop, but not into the collection accessor
      TRI_RestrictCurrentLimitStatementWalkerAql(walker);
    }
    else if (node->_type == TRI_AQL_NODE_LIMIT) {
      // note the current limit, we might use it later, pushing it up
      NoteLimit(walker, node);
    }

    // this may change the node pointer
    node = OptimiseStatement(walker, node);

    // patch variables with range infos
    if (node->_type == TRI_AQL_NODE_SCOPE_END) {
      PatchVariables(walker);
    }
  }

  return node;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief optimise the AST, first iteration
////////////////////////////////////////////////////////////////////////////////

static bool OptimiseAst (aql_optimiser_t* const optimiser) {
  TRI_aql_statement_walker_t* walker;
  
  walker = TRI_CreateStatementWalkerAql((void*) optimiser, 
                                        true,
                                        &OptimiseNode,
                                        NULL, 
                                        &ProcessStatement);
  if (walker == NULL) {
    TRI_SetErrorContextAql(optimiser->_context, TRI_ERROR_OUT_OF_MEMORY, NULL);

    return false;
  }

  TRI_WalkStatementsAql(walker, optimiser->_context->_statements); 
  TRI_FreeStatementWalkerAql(walker);

  return true;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief determine which indexes to use in the query
////////////////////////////////////////////////////////////////////////////////

static bool DetermineIndexes (aql_optimiser_t* const optimiser) {
  TRI_aql_statement_walker_t* walker;

  walker = TRI_CreateStatementWalkerAql((void*) optimiser, 
                                        true,
                                        &AnnotateNode,
                                        &AnnotateLoop, 
                                        NULL);
  if (walker == NULL) {
    TRI_SetErrorContextAql(optimiser->_context, TRI_ERROR_OUT_OF_MEMORY, NULL);

    return false;
  }

  TRI_WalkStatementsAql(walker, optimiser->_context->_statements); 

  TRI_FreeStatementWalkerAql(walker);

  return true;
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
/// @brief optimise the query
////////////////////////////////////////////////////////////////////////////////

bool TRI_OptimiseAql (TRI_aql_context_t* const context) {
  aql_optimiser_t* optimiser;
  bool result;
  
  optimiser = CreateOptimiser(context);
  if (optimiser == NULL) {
    TRI_SetErrorContextAql(context, TRI_ERROR_OUT_OF_MEMORY, NULL);
    return false;
  }

  result = (OptimiseAst(optimiser) && DetermineIndexes(optimiser));
  FreeOptimiser(optimiser);

  return result;
}

////////////////////////////////////////////////////////////////////////////////
/// @}
////////////////////////////////////////////////////////////////////////////////

// Local Variables:
// mode: outline-minor
// outline-regexp: "^\\(/// @brief\\|/// {@inheritDoc}\\|/// @addtogroup\\|// --SECTION--\\|/// @\\}\\)"
// End:
