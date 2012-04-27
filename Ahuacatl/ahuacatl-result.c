////////////////////////////////////////////////////////////////////////////////
/// @brief Ahuacatl, query result
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

#include "Ahuacatl/ahuacatl-result.h"

static void FreeData (TRI_general_cursor_result_t* result) {
  TRI_json_t* json = (TRI_json_t*) result->_data;

  assert(json);

  TRI_FreeJson(json);
}

static TRI_general_cursor_row_t GetAt (TRI_general_cursor_result_t* result, 
                                       const TRI_general_cursor_length_t n) {
  TRI_json_t* json = (TRI_json_t*) result->_data;
  
  assert(json);

  return (TRI_general_cursor_row_t*) TRI_AtVector(&json->_value._objects, (size_t) n);
}

static TRI_general_cursor_length_t GetLength (TRI_general_cursor_result_t* result) {
  TRI_json_t* json = (TRI_json_t*) result->_data;
  
  assert(json);

  return (TRI_general_cursor_length_t) json->_value._objects._length;
}

// -----------------------------------------------------------------------------
// --SECTION--                                                  public functions
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @addtogroup Ahuacatl
/// @{
////////////////////////////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////////////////////////
/// @brief create a result set
////////////////////////////////////////////////////////////////////////////////

TRI_general_cursor_result_t* TRI_CreateResultAql (TRI_json_t* data) {
  if (!data || data->_type != TRI_JSON_LIST) {
    return NULL;
  } 

  return TRI_CreateCursorResult((void*) data, &FreeData, &GetAt, &GetLength);
}

////////////////////////////////////////////////////////////////////////////////
/// @}
////////////////////////////////////////////////////////////////////////////////

// Local Variables:
// mode: outline-minor
// outline-regexp: "^\\(/// @brief\\|/// {@inheritDoc}\\|/// @addtogroup\\|// --SECTION--\\|/// @\\}\\)"
// End:
