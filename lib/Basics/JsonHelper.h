////////////////////////////////////////////////////////////////////////////////
/// @brief json helper functions
///
/// @file
///
/// DISCLAIMER
///
/// Copyright 2014 ArangoDB GmbH, Cologne, Germany
/// Copyright 2004-2014 triAGENS GmbH, Cologne, Germany
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
/// Copyright holder is ArangoDB GmbH, Cologne, Germany
///
/// @author Jan Steemann
/// @author Copyright 2014, ArangoDB GmbH, Cologne, Germany
/// @author Copyright 2012-2013, triAGENS GmbH, Cologne, Germany
////////////////////////////////////////////////////////////////////////////////

#ifndef ARANGODB_BASICS_JSON_HELPER_H
#define ARANGODB_BASICS_JSON_HELPER_H 1

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

        static TRI_json_t* fromString (char const*);

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

////////////////////////////////////////////////////////////////////////////////
/// @brief Json, a class to fabricate TRI_json_t* conveniently
////////////////////////////////////////////////////////////////////////////////

    class JsonException : std::exception {
        std::string _msg;
      public:
        JsonException () : _msg("Json exception") {
        }
        JsonException (string msg) : _msg(msg) {
        }
        char const* what () const throw() {
          return _msg.c_str();
        }
    };

    class Json {

      public:

        enum type_e {
          Null,
          Bool,
          Number,
          String,
          List,
          Array
        };

        enum autofree_e {
          AUTOFREE,
          NOFREE
        };

      private:

        void make (type_e t, size_t size_hint) {
          switch (t) {
            case Null:
              _json = TRI_CreateNullJson(_zone);
              break;
            case Bool:
              _json = TRI_CreateBooleanJson(_zone, true);
              break;
            case Number:
              _json = TRI_CreateNumberJson(_zone, 0.0);
              break;
            case String:
              _json = TRI_CreateString2CopyJson(_zone, "", 0);
              break;
            case List:
              _json = TRI_CreateList2Json(_zone, size_hint);
              break;
            case Array:
              _json = TRI_CreateArray2Json(_zone, 2*size_hint);
              break;
          }
          if (_json == nullptr) {
            throw JsonException("Json: out of memory");
          }
        }

      public:

        Json (type_e t, autofree_e autofree = AUTOFREE) 
          : _zone(TRI_UNKNOWN_MEM_ZONE), _json(nullptr), _autofree(autofree) {
          make(t, 0);
        }

        Json (TRI_memory_zone_t* z, type_e t, autofree_e autofree = AUTOFREE)
          : _zone(z), _json(nullptr), _autofree(autofree) {
          make(t, 0);
        }
          
        Json (type_e t, size_t size_hint, autofree_e autofree = AUTOFREE)
          : _zone(TRI_UNKNOWN_MEM_ZONE), _json(nullptr), _autofree(autofree) {
          make(t, size_hint);
        }

        Json (TRI_memory_zone_t* z, type_e t, size_t size_hint, 
              autofree_e autofree = AUTOFREE)
          : _zone(z), _json(nullptr), _autofree(autofree) {
          make(t, size_hint);
        }

        Json (bool x, autofree_e autofree = AUTOFREE) 
          : _zone(TRI_UNKNOWN_MEM_ZONE), _json(nullptr), _autofree(autofree) {
          _json = TRI_CreateBooleanJson(_zone, x);
          if (_json == nullptr) {
            throw JsonException("Json: out of memory");
          }
        }

        Json (TRI_memory_zone_t* z, bool x, autofree_e autofree = AUTOFREE) 
          : _zone(z), _json(nullptr), _autofree(autofree) {
          _json = TRI_CreateBooleanJson(_zone, x);
          if (_json == nullptr) {
            throw JsonException("Json: out of memory");
          }
        }

        Json (int32_t x, autofree_e autofree = AUTOFREE) 
          : _zone(TRI_UNKNOWN_MEM_ZONE), _json(nullptr), _autofree(autofree) {
          _json = TRI_CreateNumberJson(_zone, static_cast<double>(x));
          if (_json == nullptr) {
            throw JsonException("Json: out of memory");
          }
        }

        Json (TRI_memory_zone_t* z, int32_t x, autofree_e autofree = AUTOFREE) 
          : _zone(z), _json(nullptr), _autofree(autofree) {
          _json = TRI_CreateNumberJson(_zone, static_cast<double>(x));
          if (_json == nullptr) {
            throw JsonException("Json: out of memory");
          }
        }

        Json (double x, autofree_e autofree = AUTOFREE) 
          : _zone(TRI_UNKNOWN_MEM_ZONE), _json(nullptr), _autofree(autofree) {
          _json = TRI_CreateNumberJson(_zone, x);
          if (_json == nullptr) {
            throw JsonException("Json: out of memory");
          }
        }

        Json (TRI_memory_zone_t* z, double x, autofree_e autofree = AUTOFREE) 
          : _zone(z), _json(nullptr), _autofree(autofree) {
          _json = TRI_CreateNumberJson(_zone, x);
          if (_json == nullptr) {
            throw JsonException("Json: out of memory");
          }
        }

        Json (char const* x, autofree_e autofree = AUTOFREE) 
          : _zone(TRI_UNKNOWN_MEM_ZONE), _json(nullptr), _autofree(autofree) {
          _json = TRI_CreateStringCopyJson(_zone, x);
          if (_json == nullptr) {
            throw JsonException("Json: out of memory");
          }
        }

        Json (TRI_memory_zone_t* z, char const* x, autofree_e autofree = AUTOFREE) 
          : _zone(z), _json(nullptr), _autofree(autofree) {
          _json = TRI_CreateStringCopyJson(_zone, x);
          if (_json == nullptr) {
            throw JsonException("Json: out of memory");
          }
        }

        Json (std::string const x, autofree_e autofree = AUTOFREE) 
          : _zone(TRI_UNKNOWN_MEM_ZONE), _json(nullptr), _autofree(autofree) {
          _json = TRI_CreateString2CopyJson(_zone, x.c_str(), x.size());
          if (_json == nullptr) {
            throw JsonException("Json: out of memory");
          }
        }

        Json (TRI_memory_zone_t* z, std::string const x, autofree_e autofree = AUTOFREE) 
          : _zone(z), _json(nullptr), _autofree(autofree) {
          _json = TRI_CreateString2CopyJson(_zone, x.c_str(), x.size());
          if (_json == nullptr) {
            throw JsonException("Json: out of memory");
          }
        }

        Json (TRI_memory_zone_t* z, TRI_json_t* j, autofree_e autofree = AUTOFREE)
          : _zone(z), _json(j), _autofree(autofree) {
        }

        Json (Json const& j)
          : _zone(j._zone), _json(nullptr), _autofree(j._autofree) {
          std::cout << "Copy constructor called!" << std::endl;
          if (_autofree == AUTOFREE) {
            _json = TRI_CopyJson(_zone, j._json);
          }
          else {
            _json = j._json;
          }
        }

        ~Json () throw() {
          if (_json != nullptr && _autofree == AUTOFREE) {
            TRI_FreeJson(_zone, _json);
          }
        }

        TRI_json_t* json () throw() {
          return _json;
        }

        operator TRI_json_t* () throw() {
          TRI_json_t* res = _json;
          if (_autofree == AUTOFREE) {
            _json = nullptr;
          }
          return res;
        }

        Json& operator= (Json const& j) {
          if (_json != nullptr && _autofree == AUTOFREE) {
            TRI_FreeJson(_zone, _json);
          }
          _zone = j._zone;
          _autofree = j._autofree;
          if (j._autofree == AUTOFREE) {
            _json = TRI_CopyJson(_zone, j._json);
          }
          else {
            _json = j._json;
          }

          return *this;
        }
        
        Json& set (char const* name, TRI_json_t* sub) {
          if (! TRI_IsArrayJson(_json)) {
            throw JsonException("Json is no array");
          }
          TRI_Insert3ArrayJson(_zone, _json, name, sub);
          return *this;
        }

        Json& operator() (char const* name, TRI_json_t* sub) {
          if (! TRI_IsArrayJson(_json)) {
            throw JsonException("Json is no array");
          }
          TRI_Insert3ArrayJson(_zone, _json, name, sub);
          return *this;
        }

        Json& add (TRI_json_t* sub) {
          if (! TRI_IsListJson(_json)) {
            throw JsonException("Json is no list");
          }
          TRI_PushBack3ListJson(_zone, _json, sub);
          return *this;
        }

        Json& operator() (TRI_json_t* sub) {
          if (! TRI_IsListJson(_json)) {
            throw JsonException("Json is no list");
          }
          TRI_PushBack3ListJson(_zone, _json, sub);
          return *this;
        }
 
        Json get (char const* name) {
          if (! TRI_IsArrayJson(_json)) {
            throw JsonException("Json is no array");
          }
          return Json(_zone, TRI_LookupArrayJson(_json, name), NOFREE);
        }

        Json at (int pos) {
          if (! TRI_IsListJson(_json)) {
            throw JsonException("Json is no list");
          }
          TRI_json_t* j;
          if (pos >= 0) {
            j = TRI_LookupListJson(_json, pos);
            if (j != nullptr) {
              return Json(_zone, j, NOFREE);
            }
            else {
              return Json(_zone, Json::Null, true);
            }
          }
          else {
            size_t pos2 = -pos;
            size_t len = TRI_LengthVector(&_json->_value._objects);
            if (pos2 > len) {
              return Json(_zone, Json::Null, AUTOFREE);
            }
            j = TRI_LookupListJson(_json, len-pos2);
            if (j != nullptr) {
              return Json(_zone, j, NOFREE);
            }
            else {
              return Json(_zone, Json::Null, AUTOFREE);
            }
          }
        }

        bool isNull () throw() {
          return _json != nullptr && _json->_type == TRI_JSON_NULL;
        }

        bool isBoolean () throw() {
          return TRI_IsBooleanJson(_json);
        }

        bool isNumber () throw() {
          return TRI_IsNumberJson(_json);
        }

        bool isString () throw() {
          return TRI_IsStringJson(_json);
        }

        bool isArray () throw() {
          return TRI_IsArrayJson(_json);
        }

        bool isList () throw() {
          return TRI_IsListJson(_json);
        }

        string toString () {
          return JsonHelper::toString(_json);
        }

      private:
        TRI_memory_zone_t* _zone;
        TRI_json_t* _json;
        autofree_e _autofree;
    };

  }
}

#endif

// -----------------------------------------------------------------------------
// --SECTION--                                                       END-OF-FILE
// -----------------------------------------------------------------------------

// Local Variables:
// mode: outline-minor
// outline-regexp: "/// @brief\\|/// {@inheritDoc}\\|/// @page\\|// --SECTION--\\|/// @\\}"
// End:
