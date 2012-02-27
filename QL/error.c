////////////////////////////////////////////////////////////////////////////////
/// @brief error codes and translations
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

#include "QL/error.h"

////////////////////////////////////////////////////////////////////////////////
/// @addtogroup QL
/// @{
////////////////////////////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////////////////////////
/// @brief get label/translation for an error code
////////////////////////////////////////////////////////////////////////////////

char* QLErrorGetLabel (const QL_error_type_e errorCode) {
  switch (errorCode) {
    case ERR_PARSE:
      return "parse error: %s";
    case ERR_OOM:
      return "out of memory";
    case ERR_NUMBER_OUT_OF_RANGE:
      return "number '%s' is out of range";
    case ERR_PARAMETER_NUMBER_OUT_OF_RANGE:
      return "parameter number '%s' is out of range";
    case ERR_LIMIT_VALUE_OUT_OF_RANGE:
      return "limit value '%s' is out of range";
    case ERR_COLLECTION_NAME_INVALID:
      return "collection name '%s' is invalid";
    case ERR_COLLECTION_ALIAS_INVALID:
      return "collection alias '%s' is invalid";
    case ERR_COLLECTION_ALIAS_REDECLARED:
      return "collection alias '%s' is declared multiple times in the same query";
    case ERR_COLLECTION_ALIAS_UNDECLARED:
      return "collection alias '%s' is used but was not declared in the from clause";
    case ERR_GEO_RESTRICTION_INVALID:
      return "geo restriction for alias '%s' is invalid";
    default:     
      return "unknown error";
  }
}

////////////////////////////////////////////////////////////////////////////////
/// @brief create a formatted error message with wildcards replaced
////////////////////////////////////////////////////////////////////////////////

char* QLErrorFormat (const QL_error_type_e errorCode, va_list args) {
  char buffer[1024];
  char *format;

  format = QLErrorGetLabel(errorCode);

  vsnprintf(buffer, sizeof(buffer), format, args);

  return (char *) TRI_DuplicateString((const char*) &buffer);
}

////////////////////////////////////////////////////////////////////////////////
/// @}
////////////////////////////////////////////////////////////////////////////////

// Local Variables:
// mode: outline-minor
// outline-regexp: "^\\(/// @brief\\|/// {@inheritDoc}\\|/// @addtogroup\\|// --SECTION--\\|/// @\\}\\)"
// End:

