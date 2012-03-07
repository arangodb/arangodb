////////////////////////////////////////////////////////////////////////////////
/// @brief order by
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

#include "V8/v8-c-utils.h"
#include "VocBase/vocbase.h"
#include "VocBase/query-order.h"

////////////////////////////////////////////////////////////////////////////////
/// @addtogroup VocBase
/// @{
////////////////////////////////////////////////////////////////////////////////

// -----------------------------------------------------------------------------
// --SECTION--                                                          ORDER BY
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @brief callback function used for order by comparisons (called by fruitsort)
////////////////////////////////////////////////////////////////////////////////

static int OrderDataCompareFunc (const TRI_sr_index_t* left,
                                 const TRI_sr_index_t* right,
                                 size_t size,
                                 void* data) {

  TRI_rc_result_t* resultState = (TRI_rc_result_t*) data;
  int result;

  assert(resultState->_orderContext);

  TRI_DefineCompareExecutionContext(resultState->_orderContext,
                                    resultState->_selectResult, 
                                    (TRI_sr_documents_t*) *left, 
                                    (TRI_sr_documents_t*) *right);
  TRI_ExecuteOrderExecutionContext(resultState->_orderContext, &result);

  return result;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief fruitsort initialization parameters
///
/// Included fsrt.inc with these parameters will create a function OrderData()
/// that is used to do the sorting. OrderData() will call OrderDataCompareFunc()
/// to do the actual element comparisons
////////////////////////////////////////////////////////////////////////////////

#define FSRT_INSTANCE Query
#define FSRT_NAME TRI_OrderData
#define FSRT_TYPE TRI_sr_index_t
#define FSRT_EXTRA_ARGS result
#define FSRT_EXTRA_DECL TRI_rc_result_t* result
#define FSRT_COMP(l,r,s) OrderDataCompareFunc(l,r,s)

uint32_t QueryFSRT_Rand = 0;
static uint32_t QuerySortRandomGenerator (void) {
  return (QueryFSRT_Rand = QueryFSRT_Rand * 31415 + 27818);
}
#define FSRT__RAND \
  ((fs_b) + FSRT__UNIT * (QuerySortRandomGenerator() % FSRT__DIST(fs_e,fs_b,FSRT__SIZE)))

#include <BasicsC/fsrt.inc>


////////////////////////////////////////////////////////////////////////////////
/// @brief executes sorting
////////////////////////////////////////////////////////////////////////////////

void TRI_OrderDataQuery (TRI_rc_result_t* result) {
  assert(result);

  if (result->_selectResult->_numRows < 1) {
    // result set is empty, no need to sort it
    return;
  }

  TRI_OrderData(result->_selectResult->first(result->_selectResult),
                result->_selectResult->last(result->_selectResult), 
                result);
}

////////////////////////////////////////////////////////////////////////////////
/// @}
////////////////////////////////////////////////////////////////////////////////

// Local Variables:
// mode: outline-minor
// outline-regexp: "^\\(/// @brief\\|/// {@inheritDoc}\\|/// @addtogroup\\|// --SECTION--\\|/// @\\}\\)"
// End:
