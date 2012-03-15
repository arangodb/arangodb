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
/// @addtogroup AvocadoErrors
/// @{
////////////////////////////////////////////////////////////////////////////////

// -----------------------------------------------------------------------------
// --SECTION--                                                       definitions
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @brief error numbers for specific errors during query parsing and execution
///
/// Note that the error numbers defined here must not conflict with error
/// numbers defined for other parts of the program (e.g. in VocBase/vocbase.h)
////////////////////////////////////////////////////////////////////////////////

#define TRI_ERROR_QUERY_BASE                                            (1500)

////////////////////////////////////////////////////////////////////////////////
/// @brief 1500: Out of memory.
///
/// Will be raised during query execution when a memory allocation request can
/// not be satisfied.
////////////////////////////////////////////////////////////////////////////////

#define TRI_ERROR_QUERY_OOM                                             (1500)

////////////////////////////////////////////////////////////////////////////////
/// @brief 1501: Query was killed by administrator.
///
/// Will be raised when a running query is killed by an explicit admin command.
////////////////////////////////////////////////////////////////////////////////

#define TRI_ERROR_QUERY_KILLED                                          (1501)

////////////////////////////////////////////////////////////////////////////////
/// @brief 1510: Parse error.
///
/// Will be raised when query is parsed and is found to be syntactially invalid.
////////////////////////////////////////////////////////////////////////////////

#define TRI_ERROR_QUERY_PARSE                                           (1510)

////////////////////////////////////////////////////////////////////////////////
/// @brief 1511: Query is emtpy / no command specified.
///
/// Will be raised when an empty query is specified.
////////////////////////////////////////////////////////////////////////////////

#define TRI_ERROR_QUERY_EMPTY                                           (1511)

////////////////////////////////////////////////////////////////////////////////
/// @brief 1520: Specified numeric value is out of range.
///
/// Will be raised when a numeric value inside a query is out of the allowed
/// value range.
////////////////////////////////////////////////////////////////////////////////

#define TRI_ERROR_QUERY_NUMBER_OUT_OF_RANGE                             (1520)

////////////////////////////////////////////////////////////////////////////////
/// @brief 1521: Specified limit value is out of range.
///
/// Will be raised when a limit value in the query is outside the allowed range
/// (e. g. when passing a negative skip value)
////////////////////////////////////////////////////////////////////////////////

#define TRI_ERROR_QUERY_LIMIT_VALUE_OUT_OF_RANGE                        (1521)

////////////////////////////////////////////////////////////////////////////////
/// @brief 1540: Too many joins.
///
/// Will be raised when the number of joins in a query is beyond the allowed 
/// value.
////////////////////////////////////////////////////////////////////////////////

#define TRI_ERROR_QUERY_TOO_MANY_JOINS                                  (1540)

////////////////////////////////////////////////////////////////////////////////
/// @brief 1550: Invalid name for collection.
///
/// Will be raised when an invalid collection name is used in the from clause
/// of a query.
////////////////////////////////////////////////////////////////////////////////

#define TRI_ERROR_QUERY_COLLECTION_NAME_INVALID                         (1550)

////////////////////////////////////////////////////////////////////////////////
/// @brief 1551: Invalid alias for collection.
///
/// Will be raised when an invalid alias name is used for a collection.
////////////////////////////////////////////////////////////////////////////////

#define TRI_ERROR_QUERY_COLLECTION_ALIAS_INVALID                        (1551)

////////////////////////////////////////////////////////////////////////////////
/// @brief 1552: Redeclaration of alias within query.
///
/// Will be raised when the same alias name is declared multiple times in the
/// same query's from clause.
////////////////////////////////////////////////////////////////////////////////

#define TRI_ERROR_QUERY_COLLECTION_ALIAS_REDECLARED                     (1552)

////////////////////////////////////////////////////////////////////////////////
/// @brief 1553: Usage of undeclared alias in query.
///
/// Will be raised when an alias not declared in the from clause is used in the
/// query.
////////////////////////////////////////////////////////////////////////////////

#define TRI_ERROR_QUERY_COLLECTION_ALIAS_UNDECLARED                     (1553)

////////////////////////////////////////////////////////////////////////////////
/// @brief 1560: Collection not found.
///
/// Will be raised when one of the collections referenced in the query was not
/// found.
////////////////////////////////////////////////////////////////////////////////

#define TRI_ERROR_QUERY_COLLECTION_NOT_FOUND                            (1560)

////////////////////////////////////////////////////////////////////////////////
/// @brief 1570: Invalid geo restriction specification.
///
/// Will be raised when a specified geo restriction is invalid.
////////////////////////////////////////////////////////////////////////////////

#define TRI_ERROR_QUERY_GEO_RESTRICTION_INVALID                         (1570)

////////////////////////////////////////////////////////////////////////////////
/// @brief 1571: No suitable geo index found to resolve query.
///
/// Will be raised when a geo restriction was specified but no suitable geo
/// index is found to resolve it.
////////////////////////////////////////////////////////////////////////////////

#define TRI_ERROR_QUERY_GEO_INDEX_MISSING                               (1571)

////////////////////////////////////////////////////////////////////////////////
/// @brief 1590: No value specified for declared bind parameter.
///
/// Will be raised when a bind parameter was declared in the query but the
/// query is being executed with no value for that parameter.
////////////////////////////////////////////////////////////////////////////////

#define TRI_ERROR_QUERY_BIND_PARAMETER_MISSING                          (1590)

////////////////////////////////////////////////////////////////////////////////
/// @brief 1591: Redeclaration of same bind parameter value.
///
/// Will be raised when a value gets specified multiple times for the same bind 
/// parameter.
////////////////////////////////////////////////////////////////////////////////

#define TRI_ERROR_QUERY_BIND_PARAMETER_REDECLARED                       (1591)

////////////////////////////////////////////////////////////////////////////////
/// @brief 1592: Value specified for undeclared bind parameter.
///
/// Will be raised when a value gets specified for an undeclared bind parameter.
////////////////////////////////////////////////////////////////////////////////

#define TRI_ERROR_QUERY_BIND_PARAMETER_UNDECLARED                       (1592)

////////////////////////////////////////////////////////////////////////////////
/// @brief 1593: Invalid value for bind parameter.
///
/// Will be raised when an invalid value is specified for one of the bind 
/// parameters.
////////////////////////////////////////////////////////////////////////////////

#define TRI_ERROR_QUERY_BIND_PARAMETER_VALUE_INVALID                    (1593)

////////////////////////////////////////////////////////////////////////////////
/// @brief 1594: Bind parameter number is out of range.
///
/// Will be specified when the numeric index for a bind parameter of type 
/// @LIT{\@n} is out of the allowed range.
////////////////////////////////////////////////////////////////////////////////

#define TRI_ERROR_QUERY_BIND_PARAMETER_NUMBER_OUT_OF_RANGE              (1594)

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
