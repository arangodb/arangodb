////////////////////////////////////////////////////////////////////////////////
/// @brief join executor
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

#ifndef TRIAGENS_DURHAM_VOC_BASE_JOIN_EXECUTE_H
#define TRIAGENS_DURHAM_VOC_BASE_JOIN_EXECUTE_H 1

#include "QL/ast-query.h"
#include "QL/optimize.h"
#include "V8/v8-execution.h"
#include "VocBase/query-data-feeder.h"
#include "VocBase/query-join.h"
#include "VocBase/query-result.h"

#ifdef __cplusplus
extern "C" {
#endif

////////////////////////////////////////////////////////////////////////////////
/// @addtogroup VocBase
/// @{
////////////////////////////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////////////////////////
/// @brief Create a new select result from a join definition
////////////////////////////////////////////////////////////////////////////////

TRI_select_result_t* TRI_JoinSelectResult (TRI_query_instance_t* const);

////////////////////////////////////////////////////////////////////////////////
/// @brief Execute joins
////////////////////////////////////////////////////////////////////////////////

void TRI_ExecuteJoins (TRI_query_instance_t* const, 
                       TRI_select_result_t*,
                       const TRI_voc_size_t,
                       const TRI_voc_ssize_t);

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

