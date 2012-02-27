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

#ifndef TRIAGENS_DURHAM_QL_ERROR
#define TRIAGENS_DURHAM_QL_ERROR

#include <stdarg.h>
#include <stdio.h>

#include <BasicsC/strings.h>


#ifdef __cplusplus
extern "C" {
#endif

////////////////////////////////////////////////////////////////////////////////
/// @addtogroup QL
/// @{
////////////////////////////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////////////////////////
/// @brief enumeration of possible parse errors 
////////////////////////////////////////////////////////////////////////////////

typedef enum {
  ERR_OK                               = 0,
  ERR_PARSE                            = 1,
  ERR_OOM                              = 2,
  ERR_NUMBER_OUT_OF_RANGE              = 100,
  ERR_PARAMETER_NUMBER_OUT_OF_RANGE    = 101,
  ERR_LIMIT_VALUE_OUT_OF_RANGE         = 102,
  ERR_COLLECTION_NAME_INVALID          = 110, 
  ERR_COLLECTION_ALIAS_REDECLARED      = 111, 
  ERR_COLLECTION_ALIAS_UNDECLARED      = 112, 
  ERR_COLLECTION_ALIAS_INVALID         = 113, 
  ERR_GEO_RESTRICTION_INVALID          = 115, 

  ERR_LAST       
}
QL_error_type_e;

////////////////////////////////////////////////////////////////////////////////
/// @brief get label/translation for an error code
////////////////////////////////////////////////////////////////////////////////

char* QLErrorGetLabel (const QL_error_type_e);

////////////////////////////////////////////////////////////////////////////////
/// @brief create a formatted error message with wildcards replaced
////////////////////////////////////////////////////////////////////////////////

char* QLErrorFormat (const QL_error_type_e, va_list args);

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

