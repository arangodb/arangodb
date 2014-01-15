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

      private:

        JsonHelper ();
        ~JsonHelper ();
        
// -----------------------------------------------------------------------------
// --SECTION--                                             public static methods
// -----------------------------------------------------------------------------

      public:

////////////////////////////////////////////////////////////////////////////////
/// @brief convert a uint64 into a JSON string 
////////////////////////////////////////////////////////////////////////////////

        static TRI_json_t* uint64String (TRI_memory_zone_t*,
                                         uint64_t);

////////////////////////////////////////////////////////////////////////////////
/// @brief convert a uint64 into a JSON string 
////////////////////////////////////////////////////////////////////////////////

        static uint64_t stringUInt64 (TRI_json_t const*);

////////////////////////////////////////////////////////////////////////////////
/// @brief convert a uint64 into a JSON string 
////////////////////////////////////////////////////////////////////////////////

        static uint64_t stringUInt64 (TRI_json_t const*,
                                      char const*);

////////////////////////////////////////////////////////////////////////////////
/// @brief creates a JSON object from a key/value object of strings
////////////////////////////////////////////////////////////////////////////////

        static TRI_json_t* stringObject (TRI_memory_zone_t*,
                                         std::map<std::string, std::string> const&);

////////////////////////////////////////////////////////////////////////////////
/// @brief creates a key/value object of strings from a JSON (sub-) object
////////////////////////////////////////////////////////////////////////////////

        static std::map<std::string, std::string> stringObject (TRI_json_t const*);

////////////////////////////////////////////////////////////////////////////////
/// @brief creates a JSON object from a list of strings
////////////////////////////////////////////////////////////////////////////////

        static TRI_json_t* stringList (TRI_memory_zone_t*,
                                       std::vector<std::string> const&);

////////////////////////////////////////////////////////////////////////////////
/// @brief creates a list of strings from a JSON (sub-) object
////////////////////////////////////////////////////////////////////////////////

        static std::vector<std::string> stringList (TRI_json_t const*);

////////////////////////////////////////////////////////////////////////////////
/// @brief create JSON from string
////////////////////////////////////////////////////////////////////////////////
        
        static TRI_json_t* fromString (std::string const&);

////////////////////////////////////////////////////////////////////////////////
/// @brief create JSON from string
////////////////////////////////////////////////////////////////////////////////
        
        static TRI_json_t* fromString (char const*, 
                                       size_t);

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
/// @brief returns a string element, or a default it is does not exist
////////////////////////////////////////////////////////////////////////////////
        
        static std::string getStringValue (TRI_json_t const*, 
                                           const std::string&);

////////////////////////////////////////////////////////////////////////////////
/// @brief returns a string sub-element, or a default it is does not exist
////////////////////////////////////////////////////////////////////////////////
        
        static std::string getStringValue (TRI_json_t const*, 
                                           const char*, 
                                           const std::string&);

////////////////////////////////////////////////////////////////////////////////
/// @brief returns a numeric sub-element, or a default it is does not exist
////////////////////////////////////////////////////////////////////////////////

        template<typename T> static T getNumericValue (TRI_json_t const* json,
                                                       const char* name,
                                                       T defaultValue) {
          TRI_json_t const* sub = getArrayElement(json, name);

          if (isNumber(sub)) {
            return (T) sub->_value._number;
          }

          return defaultValue;
        }

////////////////////////////////////////////////////////////////////////////////
/// @brief returns a boolean sub-element, or a default it is does not exist
////////////////////////////////////////////////////////////////////////////////
        
        static bool getBooleanValue (TRI_json_t const*, 
                                     const char*, 
                                     bool);

    };
  }
}

#endif

// Local Variables:
// mode: outline-minor
// outline-regexp: "/// @brief\\|/// {@inheritDoc}\\|/// @addtogroup\\|/// @page\\|// --SECTION--\\|/// @\\}"
// End:
