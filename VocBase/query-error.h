////////////////////////////////////////////////////////////////////////////////
/// @brief query errors
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

#ifndef DURHAM_STORAGE_VOCBASE_QUERY_ERROR_H
#define DURHAM_STORAGE_VOCBASE_QUERY_ERROR_H 1

#include <BasicsC/common.h>

#ifdef __cplusplus
extern "C" {
#endif

////////////////////////////////////////////////////////////////////////////////
/// @addtogroup VocBase
/// @{
////////////////////////////////////////////////////////////////////////////////

// -----------------------------------------------------------------------------
// --SECTION--                                                       definitions
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @brief error numbers for specific errors during query parsing and execution
///
/// note that the error numbers defined here must not conflict with error
/// numbers defined for other parts of the program (e.g. in VocBase/vocbase.h)
////////////////////////////////////////////////////////////////////////////////

#define TRI_ERROR_QUERY_OOM                                             (8000)
#define TRI_ERROR_QUERY_KILLED                                          (8001)

#define TRI_ERROR_QUERY_PARSE                                           (8010)
#define TRI_ERROR_QUERY_EMPTY                                           (8011)

#define TRI_ERROR_QUERY_NUMBER_OUT_OF_RANGE                             (8020)
#define TRI_ERROR_QUERY_LIMIT_VALUE_OUT_OF_RANGE                        (8021)

#define TRI_ERROR_QUERY_COLLECTION_NAME_INVALID                         (8050)
#define TRI_ERROR_QUERY_COLLECTION_ALIAS_INVALID                        (8051)
#define TRI_ERROR_QUERY_COLLECTION_ALIAS_REDECLARED                     (8052)
#define TRI_ERROR_QUERY_COLLECTION_ALIAS_UNDECLARED                     (8053)

#define TRI_ERROR_QUERY_COLLECTION_NOT_FOUND                            (8060)

#define TRI_ERROR_QUERY_GEO_RESTRICTION_INVALID                         (8070)
#define TRI_ERROR_QUERY_GEO_INDEX_MISSING                               (8071)

#define TRI_ERROR_QUERY_BIND_PARAMETER_MISSING                          (8100)
#define TRI_ERROR_QUERY_BIND_PARAMETER_REDECLARED                       (8101)
#define TRI_ERROR_QUERY_BIND_PARAMETER_UNDECLARED                       (8102)
#define TRI_ERROR_QUERY_BIND_PARAMETER_VALUE_INVALID                    (8103)
#define TRI_ERROR_QUERY_BIND_PARAMETER_NUMBER_OUT_OF_RANGE              (8104)

////////////////////////////////////////////////////////////////////////////////
/// @brief helper macro to define an error string
////////////////////////////////////////////////////////////////////////////////

#define REG_ERROR(id, label) TRI_set_errno_string(TRI_ERROR_QUERY_ ## id, label);

// -----------------------------------------------------------------------------
// --SECTION--                                                  public functions
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @brief initialize query error definitions
////////////////////////////////////////////////////////////////////////////////

void TRI_InitialiseQueryErrors (void);

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
