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

#include "VocBase/query-error.h"

////////////////////////////////////////////////////////////////////////////////
/// @addtogroup VocBase
/// @{
////////////////////////////////////////////////////////////////////////////////

// -----------------------------------------------------------------------------
// --SECTION--                                                  public functions
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @brief initialize query error definitions
////////////////////////////////////////////////////////////////////////////////

void TRI_InitialiseQueryErrors (void) {
  REG_ERROR(OOM,                                "out of memory");
  REG_ERROR(PARSE,                              "parse error: %s");
  REG_ERROR(EMPTY,                              "query is empty");
  REG_ERROR(NUMBER_OUT_OF_RANGE,                "number '%s' is out of range");
  REG_ERROR(LIMIT_VALUE_OUT_OF_RANGE,           "limit value '%s' is out of range");
  
  REG_ERROR(COLLECTION_NAME_INVALID,            "collection name '%s' is invalid");
  REG_ERROR(COLLECTION_ALIAS_INVALID,           "collection alias '%s' is invalid");
  REG_ERROR(COLLECTION_ALIAS_REDECLARED,        "collection alias '%s' is declared multiple times in the same query");
  REG_ERROR(COLLECTION_ALIAS_UNDECLARED,        "collection alias '%s' is used but was not declared in the from clause");
  
  REG_ERROR(GEO_RESTRICTION_INVALID,            "geo restriction for alias '%s' is invalid");
  REG_ERROR(GEO_INDEX_MISSING,                  "no suitable geo index found for geo restriction on '%s'");
  
  REG_ERROR(BIND_PARAMETER_MISSING,             "no value specified for bind parameter '%s'");
  REG_ERROR(BIND_PARAMETER_REDECLARED,          "value for bind parameter '%s' is declared multiple times");
  REG_ERROR(BIND_PARAMETER_UNDECLARED,          "bind parameter '%s' was not declared in the query");
  REG_ERROR(BIND_PARAMETER_VALUE_INVALID,       "invalid value for bind parameter '%s'");
  REG_ERROR(BIND_PARAMETER_NUMBER_OUT_OF_RANGE, "bind parameter number '%s' out of range");
}

////////////////////////////////////////////////////////////////////////////////
/// @}
////////////////////////////////////////////////////////////////////////////////

// Local Variables:
// mode: outline-minor
// outline-regexp: "^\\(/// @brief\\|/// {@inheritDoc}\\|/// @addtogroup\\|// --SECTION--\\|/// @\\}\\)"
// End:
