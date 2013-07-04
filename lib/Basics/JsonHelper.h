////////////////////////////////////////////////////////////////////////////////
/// @brief json helper functions
///
/// @file
///
/// DISCLAIMER
///
/// Copyright 2004-2013 triAGENS GmbH, Cologne, Germany
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
/// @author Copyright 2012-2013, triAGENS GmbH, Cologne, Germany
////////////////////////////////////////////////////////////////////////////////

#ifndef TRIAGENS_BASICS_JSON_HELPER_H
#define TRIAGENS_BASICS_JSON_HELPER_H 1

#include "BasicsC/json.h"
#include "BasicsC/string-buffer.h"

namespace triagens {
  namespace basics {

// -----------------------------------------------------------------------------
// --SECTION--                                                  class JsonHelper
// -----------------------------------------------------------------------------

    class JsonHelper {

// -----------------------------------------------------------------------------
// --SECTION--                                        constructors / destructors
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @addtogroup ArangoDB
/// @{
////////////////////////////////////////////////////////////////////////////////

      private:

        JsonHelper ();
        ~JsonHelper ();
        
////////////////////////////////////////////////////////////////////////////////
/// @}
////////////////////////////////////////////////////////////////////////////////

// -----------------------------------------------------------------------------
// --SECTION--                                                    public methods
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @addtogroup ArangoDB
/// @{
////////////////////////////////////////////////////////////////////////////////
      
      public:

////////////////////////////////////////////////////////////////////////////////
/// @brief stringify json
////////////////////////////////////////////////////////////////////////////////
        
        static string toString (TRI_json_t const* json) {
          TRI_string_buffer_t buffer;

          TRI_InitStringBuffer(&buffer, TRI_UNKNOWN_MEM_ZONE);
          int res = TRI_StringifyJson(&buffer, json);

          if (res != TRI_ERROR_NO_ERROR) {
            return "";
          }

          string out(TRI_BeginStringBuffer(&buffer), TRI_LengthStringBuffer(&buffer));
          TRI_DestroyStringBuffer(&buffer);

          return out;
        }

////////////////////////////////////////////////////////////////////////////////
/// @brief returns true for arrays
////////////////////////////////////////////////////////////////////////////////
        
        static bool isArray (TRI_json_t const* json) {
          return json != 0 && json->_type == TRI_JSON_ARRAY;
        }

////////////////////////////////////////////////////////////////////////////////
/// @brief returns true for lists
////////////////////////////////////////////////////////////////////////////////
        
        static bool isList (TRI_json_t const* json) {
          return json != 0 && json->_type == TRI_JSON_LIST;
        }

////////////////////////////////////////////////////////////////////////////////
/// @brief returns true for strings
////////////////////////////////////////////////////////////////////////////////
        
        static bool isString (TRI_json_t const* json) {
          return json != 0 && json->_type == TRI_JSON_STRING && json->_value._string.data != 0;
        }

////////////////////////////////////////////////////////////////////////////////
/// @brief returns true for numbers
////////////////////////////////////////////////////////////////////////////////
        
        static bool isNumber (TRI_json_t const* json) {
          return json != 0 && json->_type == TRI_JSON_NUMBER;
        }

////////////////////////////////////////////////////////////////////////////////
/// @brief returns true for booleans
////////////////////////////////////////////////////////////////////////////////
        
        static bool isBoolean (TRI_json_t const* json) {
          return json != 0 && json->_type == TRI_JSON_BOOLEAN;
        }

////////////////////////////////////////////////////////////////////////////////
/// @brief returns an array sub-element
////////////////////////////////////////////////////////////////////////////////
        
        static TRI_json_t* getArrayElement (TRI_json_t const* json, const char* name) {
          if (! isArray(json)) {
            return 0;
          }

          return TRI_LookupArrayJson(json, name);
        }

////////////////////////////////////////////////////////////////////////////////
/// @brief returns a string sub-element, or a default it is does not exist
////////////////////////////////////////////////////////////////////////////////
        
        static string getStringValue (TRI_json_t const* json, const char* name, const string& defaultValue) {
          TRI_json_t const* sub = getArrayElement(json, name);

          if (isString(sub)) {
            return string(sub->_value._string.data, sub->_value._string.length - 1);
          }
          return defaultValue;
        }


////////////////////////////////////////////////////////////////////////////////
/// @brief returns a numeric sub-element, or a default it is does not exist
////////////////////////////////////////////////////////////////////////////////

        static double getNumberValue (TRI_json_t const* json, const char* name, double defaultValue) {
          TRI_json_t const* sub = getArrayElement(json, name);

          if (isNumber(sub)) {
            return sub->_value._number;
          }
          return defaultValue;
        }

////////////////////////////////////////////////////////////////////////////////
/// @brief returns a boolean sub-element, or a default it is does not exist
////////////////////////////////////////////////////////////////////////////////
        
        static double getBooleanValue (TRI_json_t const* json, const char* name, bool defaultValue) {
          TRI_json_t const* sub = getArrayElement(json, name);

          if (isBoolean(sub)) {
            return sub->_value._boolean;
          }
          return defaultValue;
        }

////////////////////////////////////////////////////////////////////////////////
/// @}
////////////////////////////////////////////////////////////////////////////////
        
    };
  }
}

#endif

// Local Variables:
// mode: outline-minor
// outline-regexp: "/// @brief\\|/// {@inheritDoc}\\|/// @addtogroup\\|/// @page\\|// --SECTION--\\|/// @\\}"
// End:
