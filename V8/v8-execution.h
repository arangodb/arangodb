////////////////////////////////////////////////////////////////////////////////
/// @brief V8 execution context
///
/// @file
///
/// DISCLAIMER
///
/// Copyright 2004-2012 triagens GmbH, Cologne, Germany
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
/// @author Dr. Frank Celler
/// @author Copyright 2011-2012, triAGENS GmbH, Cologne, Germany
////////////////////////////////////////////////////////////////////////////////

#ifndef TRIAGENS_V8_V8_EXECUTION_H
#define TRIAGENS_V8_V8_EXECUTION_H 1

#include "VocBase/query-context.h"
#include "VocBase/query-result.h"
#include "VocBase/query-base.h"
#include "VocBase/document-collection.h"

#ifdef __cplusplus
extern "C" {
#endif

// -----------------------------------------------------------------------------
// --SECTION--                                                 EXECUTION CONTEXT
// -----------------------------------------------------------------------------

// -----------------------------------------------------------------------------
// --SECTION--                                                  public functions
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @addtogroup V8Utils
/// @{
////////////////////////////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////////////////////////
/// @brief sets an json array
////////////////////////////////////////////////////////////////////////////////

bool TRI_DefineJsonArrayExecutionContext (TRI_js_exec_context_t,
                                          TRI_json_t*);


////////////////////////////////////////////////////////////////////////////////
/// @brief defines documents in a join/where - DEPRECATED
////////////////////////////////////////////////////////////////////////////////

bool TRI_DefineWhereExecutionContextX (TRI_js_exec_context_t,
                                      const TRI_select_join_t*,
                                      const size_t,
                                      const bool); 

////////////////////////////////////////////////////////////////////////////////
/// @brief defines documents in a join/where
////////////////////////////////////////////////////////////////////////////////

bool TRI_DefineWhereExecutionContext (TRI_query_instance_t* const,
                                      TRI_js_exec_context_t,
                                      const size_t,
                                      const bool); 

////////////////////////////////////////////////////////////////////////////////
/// @brief defines a documents composed of multiple elements
////////////////////////////////////////////////////////////////////////////////

bool TRI_DefineSelectExecutionContext (TRI_js_exec_context_t,
                                       TRI_rc_result_t*);

////////////////////////////////////////////////////////////////////////////////
/// @brief defines a document
////////////////////////////////////////////////////////////////////////////////

bool TRI_DefineDocumentExecutionContext (TRI_js_exec_context_t,
                                         char const* name,
                                         TRI_doc_collection_t* collection,
                                         TRI_doc_mptr_t const* document);

////////////////////////////////////////////////////////////////////////////////
/// @brief defines two documents for comparsions (e.g. used in order by)
////////////////////////////////////////////////////////////////////////////////

bool TRI_DefineCompareExecutionContext (TRI_js_exec_context_t,
                                        TRI_select_result_t*,
                                        TRI_sr_documents_t*, 
                                        TRI_sr_documents_t*); 

////////////////////////////////////////////////////////////////////////////////
/// @brief executes an execution context - DEPRECATED
////////////////////////////////////////////////////////////////////////////////

bool TRI_ExecuteExecutionContextX (TRI_js_exec_context_t, void* storage);


////////////////////////////////////////////////////////////////////////////////
/// @brief executes an execution context
////////////////////////////////////////////////////////////////////////////////

bool TRI_ExecuteExecutionContext (TRI_js_exec_context_t, void* storage);

////////////////////////////////////////////////////////////////////////////////
/// @brief executes an execution context for a condition - DEPRECATED
////////////////////////////////////////////////////////////////////////////////

bool TRI_ExecuteConditionExecutionContextX (TRI_js_exec_context_t, bool* result);

////////////////////////////////////////////////////////////////////////////////
/// @brief executes an execution context for a condition
////////////////////////////////////////////////////////////////////////////////

bool TRI_ExecuteConditionExecutionContext (TRI_query_instance_t* const, 
                                           TRI_js_exec_context_t, 
                                           bool* result);

////////////////////////////////////////////////////////////////////////////////
/// @brief executes an execution context for a ref access
////////////////////////////////////////////////////////////////////////////////

bool TRI_ExecuteRefExecutionContext (TRI_memory_zone_t*,
                                     TRI_js_exec_context_t, 
                                     TRI_json_t*);

////////////////////////////////////////////////////////////////////////////////
/// @brief executes an execution context for order by
////////////////////////////////////////////////////////////////////////////////

bool TRI_ExecuteOrderExecutionContext (TRI_js_exec_context_t, int* result);

////////////////////////////////////////////////////////////////////////////////
/// @brief executes a result context
////////////////////////////////////////////////////////////////////////////////

TRI_json_t* TRI_ExecuteResultContext (TRI_js_exec_context_t context);

////////////////////////////////////////////////////////////////////////////////
/// @}
////////////////////////////////////////////////////////////////////////////////

#ifdef __cplusplus
}
#endif

#endif

// Local Variables:
// mode: outline-minor
// outline-regexp: "^\\(/// @brief\\|/// {@inheritDoc}\\|/// @addtogroup\\|// --SECTION--\\|/// @\\}\\)"
// End:
