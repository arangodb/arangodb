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

#include "Basics/Common.h"
#include "BasicsC/json.h"

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
// --SECTION--                                             public static methods
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @addtogroup ArangoDB
/// @{
////////////////////////////////////////////////////////////////////////////////
      
      public:

////////////////////////////////////////////////////////////////////////////////
/// @brief stringify json
////////////////////////////////////////////////////////////////////////////////
        
        static std::string toString (TRI_json_t const*);

////////////////////////////////////////////////////////////////////////////////
/// @brief returns true for arrays
////////////////////////////////////////////////////////////////////////////////
        
        static inline bool isArray (TRI_json_t const* json) {
          return json != 0 && json->_type == TRI_JSON_ARRAY;
        }

////////////////////////////////////////////////////////////////////////////////
/// @brief returns true for lists
////////////////////////////////////////////////////////////////////////////////
        
        static inline bool isList (TRI_json_t const* json) {
          return json != 0 && json->_type == TRI_JSON_LIST;
        }

////////////////////////////////////////////////////////////////////////////////
/// @brief returns true for strings
////////////////////////////////////////////////////////////////////////////////
        
        static inline bool isString (TRI_json_t const* json) {
          return TRI_IsStringJson(json);
        }

////////////////////////////////////////////////////////////////////////////////
/// @brief returns true for numbers
////////////////////////////////////////////////////////////////////////////////
        
        static inline bool isNumber (TRI_json_t const* json) {
          return json != 0 && json->_type == TRI_JSON_NUMBER;
        }

////////////////////////////////////////////////////////////////////////////////
/// @brief returns true for booleans
////////////////////////////////////////////////////////////////////////////////
        
        static inline bool isBoolean (TRI_json_t const* json) {
          return json != 0 && json->_type == TRI_JSON_BOOLEAN;
        }

////////////////////////////////////////////////////////////////////////////////
/// @brief returns an array sub-element
////////////////////////////////////////////////////////////////////////////////
        
        static TRI_json_t* getArrayElement (TRI_json_t const*, 
                                                   const char* name);

////////////////////////////////////////////////////////////////////////////////
/// @brief returns a string sub-element, or a default it is does not exist
////////////////////////////////////////////////////////////////////////////////
        
        static std::string getStringValue (TRI_json_t const*, 
                                           const char*, 
                                           const std::string&);

////////////////////////////////////////////////////////////////////////////////
/// @brief returns a numeric sub-element, or a default it is does not exist
////////////////////////////////////////////////////////////////////////////////

        static double getNumberValue (TRI_json_t const*, 
                                      const char*, 
                                      double);

////////////////////////////////////////////////////////////////////////////////
/// @brief returns a boolean sub-element, or a default it is does not exist
////////////////////////////////////////////////////////////////////////////////
        
        static double getBooleanValue (TRI_json_t const*, 
                                       const char*, 
                                       bool);

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
