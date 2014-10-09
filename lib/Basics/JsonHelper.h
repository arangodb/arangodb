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
#include "Basics/Exceptions.h"
#include "Basics/json.h"

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
          return json != nullptr && json->_type == TRI_JSON_ARRAY;
        }

////////////////////////////////////////////////////////////////////////////////
/// @brief returns true for lists
////////////////////////////////////////////////////////////////////////////////

        static inline bool isList (TRI_json_t const* json) {
          return json != nullptr && json->_type == TRI_JSON_LIST;
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
          return json != nullptr && json->_type == TRI_JSON_NUMBER;
        }

////////////////////////////////////////////////////////////////////////////////
/// @brief returns true for booleans
////////////////////////////////////////////////////////////////////////////////

        static inline bool isBoolean (TRI_json_t const* json) {
          return json != nullptr && json->_type == TRI_JSON_BOOLEAN;
        }

////////////////////////////////////////////////////////////////////////////////
/// @brief returns an array sub-element
////////////////////////////////////////////////////////////////////////////////

        static TRI_json_t* getArrayElement (TRI_json_t const*,
                                            char const* name);

////////////////////////////////////////////////////////////////////////////////
/// @brief returns a string element, or a default it is does not exist
////////////////////////////////////////////////////////////////////////////////

        static std::string getStringValue (TRI_json_t const*,
                                           std::string const&);

////////////////////////////////////////////////////////////////////////////////
/// @brief returns a string sub-element, or a default it is does not exist
////////////////////////////////////////////////////////////////////////////////

        static std::string getStringValue (TRI_json_t const*,
                                           char const*,
                                           std::string const&);

////////////////////////////////////////////////////////////////////////////////
/// @brief returns a numeric value
////////////////////////////////////////////////////////////////////////////////

        template<typename T> 
        static T getNumericValue (TRI_json_t const* json,
                                  T defaultValue) {
          if (isNumber(json)) {
            return (T) json->_value._number;
          }

          return defaultValue;
        }

////////////////////////////////////////////////////////////////////////////////
/// @brief returns a numeric sub-element, or a default it is does not exist
////////////////////////////////////////////////////////////////////////////////

        template<typename T> 
        static T getNumericValue (TRI_json_t const* json,
                                  char const* name,
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
                                     char const*,
                                     bool);

////////////////////////////////////////////////////////////////////////////////
/// @brief returns a boolean sub-element, or a throws an exception if the
/// sub-element does not exist or if it is not boolean
////////////////////////////////////////////////////////////////////////////////

        static bool checkAndGetBooleanValue (TRI_json_t const*,
                                             char const*);

////////////////////////////////////////////////////////////////////////////////
/// @brief returns a numeric sub-element, or throws if <name> does not exist
/// or it is not a number
////////////////////////////////////////////////////////////////////////////////

        template<typename T>     
        static T checkAndGetNumericValue (TRI_json_t const* json,
                                          char const* name) {
          TRI_json_t const* sub = getArrayElement(json, name);

          if (! isNumber(sub)) {
            std::string msg = "The attribute '" + std::string(name)  
              + "' was not found or is not a number.";
            THROW_INTERNAL_ERROR(msg);
          }
          return static_cast<T>(sub->_value._number);
        }
        
////////////////////////////////////////////////////////////////////////////////
/// @brief returns a string sub-element, or throws if <name> does not exist
/// or it is not a string 
////////////////////////////////////////////////////////////////////////////////

        static std::string checkAndGetStringValue (TRI_json_t const*,
                                                   char const*);

////////////////////////////////////////////////////////////////////////////////
/// @brief returns an array sub-element, or a throws an exception if the
/// sub-element does not exist or if it is not an array
////////////////////////////////////////////////////////////////////////////////
        
        static TRI_json_t const* checkAndGetArrayValue (TRI_json_t const*, 
                                                        char const*); 

////////////////////////////////////////////////////////////////////////////////
/// @brief returns a list sub-element, or a throws an exception if the
/// sub-element does not exist or if it is not a list
////////////////////////////////////////////////////////////////////////////////

        static TRI_json_t const* checkAndGetListValue (TRI_json_t const*, 
                                                       char const*); 
    };

////////////////////////////////////////////////////////////////////////////////
/// @brief JsonException, an exception class for the Json class
////////////////////////////////////////////////////////////////////////////////

    class JsonException : public virtual std::exception {
        std::string _msg;
      public:
        JsonException () : _msg("Json exception") {
#ifdef TRI_ENABLE_MAINTAINER_MODE
#if HAVE_BACKTRACE
          _msg += std::string("\n\n");
          _getBacktrace(_msg);
          _msg += std::string("\n\n");
#endif
#endif
        }
        JsonException (string msg) : _msg(msg) {
#ifdef TRI_ENABLE_MAINTAINER_MODE
#if HAVE_BACKTRACE
          _msg += std::string("\n\n");
          _getBacktrace(_msg);
          _msg += std::string("\n\n");
#endif
#endif
        }
        char const* what () const throw() {
          return _msg.c_str();
        }
    };

////////////////////////////////////////////////////////////////////////////////
/// @brief Json, a class to fabricate TRI_json_t* conveniently
////////////////////////////////////////////////////////////////////////////////

    class Json {

      public:

////////////////////////////////////////////////////////////////////////////////
/// @brief enum for the type of a Json structure
////////////////////////////////////////////////////////////////////////////////

        enum type_e {
          Null,
          Bool,
          Number,
          String,
          List,
          Array
        };

////////////////////////////////////////////////////////////////////////////////
/// @brief enum to say whether we are automatically freeing the TRI_json_t*
////////////////////////////////////////////////////////////////////////////////

        enum autofree_e {
          AUTOFREE,
          NOFREE
        };

////////////////////////////////////////////////////////////////////////////////
/// @brief internal helper for the generic constructor
////////////////////////////////////////////////////////////////////////////////

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
              _json = TRI_CreateArray2Json(_zone, 2 * size_hint);
              break;
          }
          if (_json == nullptr) {
            throw JsonException("Json: out of memory");
          }
        }

// -----------------------------------------------------------------------------
// --SECTION--                                               public constructors
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @brief default constructor making an empty Json
////////////////////////////////////////////////////////////////////////////////

      public:

        explicit Json ()
          : _zone(TRI_UNKNOWN_MEM_ZONE), _json(nullptr), _autofree(AUTOFREE) {
        }
         
////////////////////////////////////////////////////////////////////////////////
/// @brief generic constructor for a type_e
////////////////////////////////////////////////////////////////////////////////

        explicit Json (type_e t, autofree_e autofree = AUTOFREE) 
          : _zone(TRI_UNKNOWN_MEM_ZONE), _json(nullptr), _autofree(autofree) {
          make(t, 0);
        }

////////////////////////////////////////////////////////////////////////////////
/// @brief generic constructor for a memzone and a type_e
////////////////////////////////////////////////////////////////////////////////

        explicit Json (TRI_memory_zone_t* z, type_e t, autofree_e autofree = AUTOFREE)
          : _zone(z), _json(nullptr), _autofree(autofree) {
          make(t, 0);
        }
          
////////////////////////////////////////////////////////////////////////////////
/// @brief generic constructor for a type_e with a size hint
////////////////////////////////////////////////////////////////////////////////

        explicit Json (type_e t, size_t size_hint, autofree_e autofree = AUTOFREE)
          : _zone(TRI_UNKNOWN_MEM_ZONE), _json(nullptr), _autofree(autofree) {
          make(t, size_hint);
        }

////////////////////////////////////////////////////////////////////////////////
/// @brief generic constructor for a memzone, a type_e and a size hint
////////////////////////////////////////////////////////////////////////////////

        explicit Json (TRI_memory_zone_t* z, type_e t, size_t size_hint, 
              autofree_e autofree = AUTOFREE)
          : _zone(z), _json(nullptr), _autofree(autofree) {
          make(t, size_hint);
        }

////////////////////////////////////////////////////////////////////////////////
/// @brief constructor for a bool
////////////////////////////////////////////////////////////////////////////////

        explicit Json (bool x, autofree_e autofree = AUTOFREE) 
          : _zone(TRI_UNKNOWN_MEM_ZONE), _json(nullptr), _autofree(autofree) {
          _json = TRI_CreateBooleanJson(_zone, x);

          if (_json == nullptr) {
            throw JsonException("Json: out of memory");
          }
        }

////////////////////////////////////////////////////////////////////////////////
/// @brief constructor for a memzone and a bool
////////////////////////////////////////////////////////////////////////////////

        explicit Json (TRI_memory_zone_t* z, bool x, autofree_e autofree = AUTOFREE) 
          : _zone(z), _json(nullptr), _autofree(autofree) {
          _json = TRI_CreateBooleanJson(_zone, x);

          if (_json == nullptr) {
            throw JsonException("Json: out of memory");
          }
        }

////////////////////////////////////////////////////////////////////////////////
/// @brief constructor for an int32_t
////////////////////////////////////////////////////////////////////////////////

        explicit Json (int32_t x, autofree_e autofree = AUTOFREE) 
          : _zone(TRI_UNKNOWN_MEM_ZONE), _json(nullptr), _autofree(autofree) {
          _json = TRI_CreateNumberJson(_zone, static_cast<double>(x));

          if (_json == nullptr) {
            throw JsonException("Json: out of memory");
          }
        }

////////////////////////////////////////////////////////////////////////////////
/// @brief constructor for a memzone and an int32_t
////////////////////////////////////////////////////////////////////////////////

        explicit Json (TRI_memory_zone_t* z, int32_t x, autofree_e autofree = AUTOFREE) 
          : _zone(z), _json(nullptr), _autofree(autofree) {
          _json = TRI_CreateNumberJson(_zone, static_cast<double>(x));

          if (_json == nullptr) {
            throw JsonException("Json: out of memory");
          }
        }

////////////////////////////////////////////////////////////////////////////////
/// @brief constructor for a double
////////////////////////////////////////////////////////////////////////////////

        explicit Json (double x, autofree_e autofree = AUTOFREE) 
          : _zone(TRI_UNKNOWN_MEM_ZONE), _json(nullptr), _autofree(autofree) {
          _json = TRI_CreateNumberJson(_zone, x);

          if (_json == nullptr) {
            throw JsonException("Json: out of memory");
          }
        }

////////////////////////////////////////////////////////////////////////////////
/// @brief constructor for a memzone and a double
////////////////////////////////////////////////////////////////////////////////

        explicit Json (TRI_memory_zone_t* z, double x, autofree_e autofree = AUTOFREE) 
          : _zone(z), _json(nullptr), _autofree(autofree) {
          _json = TRI_CreateNumberJson(_zone, x);

          if (_json == nullptr) {
            throw JsonException("Json: out of memory");
          }
        }

////////////////////////////////////////////////////////////////////////////////
/// @brief constructor for a char const*
////////////////////////////////////////////////////////////////////////////////

        explicit Json (char const* x, autofree_e autofree = AUTOFREE) 
          : _zone(TRI_UNKNOWN_MEM_ZONE), _json(nullptr), _autofree(autofree) {
          _json = TRI_CreateStringCopyJson(_zone, x);

          if (_json == nullptr) {
            throw JsonException("Json: out of memory");
          }
        }

////////////////////////////////////////////////////////////////////////////////
/// @brief constructor for a memzone and a char const*
////////////////////////////////////////////////////////////////////////////////

        explicit Json (TRI_memory_zone_t* z, char const* x, autofree_e autofree = AUTOFREE) 
          : _zone(z), _json(nullptr), _autofree(autofree) {
          _json = TRI_CreateStringCopyJson(_zone, x);

          if (_json == nullptr) {
            throw JsonException("Json: out of memory");
          }
        }

////////////////////////////////////////////////////////////////////////////////
/// @brief constructor for a string
////////////////////////////////////////////////////////////////////////////////

        explicit Json (std::string const x, autofree_e autofree = AUTOFREE) 
          : _zone(TRI_UNKNOWN_MEM_ZONE), _json(nullptr), _autofree(autofree) {
          _json = TRI_CreateString2CopyJson(_zone, x.c_str(), x.size());

          if (_json == nullptr) {
            throw JsonException("Json: out of memory");
          }
        }

////////////////////////////////////////////////////////////////////////////////
/// @brief constructor for a memzone and a string
////////////////////////////////////////////////////////////////////////////////

        explicit Json (TRI_memory_zone_t* z, std::string const& x, autofree_e autofree = AUTOFREE) 
          : _zone(z), _json(nullptr), _autofree(autofree) {
          _json = TRI_CreateString2CopyJson(_zone, x.c_str(), x.size());

          if (_json == nullptr) {
            throw JsonException("Json: out of memory");
          }
        }

////////////////////////////////////////////////////////////////////////////////
/// @brief constructor for a memzone and a TRI_json_t*
////////////////////////////////////////////////////////////////////////////////

        explicit Json (TRI_memory_zone_t* z, TRI_json_t* j, autofree_e autofree = AUTOFREE)
          : _zone(z), _json(j), _autofree(autofree) {
        }

////////////////////////////////////////////////////////////////////////////////
/// @brief constructor for a memzone and a const TRI_json_t*
////////////////////////////////////////////////////////////////////////////////

        explicit Json (TRI_memory_zone_t* z, TRI_json_t const* j, autofree_e autofree = NOFREE)
          : _zone(z), _json(const_cast<TRI_json_t*>(j)), _autofree(autofree) {
        }

        explicit Json (TRI_json_t* j) = delete;
        explicit Json (TRI_json_t const *) = delete;

////////////////////////////////////////////////////////////////////////////////
/// @brief "copy" constructor, note that in the AUTOFREE case this steals
/// the structure from j to allow returning Json objects by value without
/// copying the whole structure.
////////////////////////////////////////////////////////////////////////////////

        Json (Json& j)
          : _zone(j._zone), _json(j.steal()), _autofree(j._autofree) {
        }

////////////////////////////////////////////////////////////////////////////////
/// @brief move constructor, note that in the AUTOFREE case this steals
/// the structure from j to allow returning Json objects by value without
/// copying the whole structure.
////////////////////////////////////////////////////////////////////////////////

        Json (Json&& j)
          : _zone(j._zone), _json(j.steal()), _autofree(j._autofree) {
        }

////////////////////////////////////////////////////////////////////////////////
/// @brief delete the normal move constructor, because we do not want
/// invisible recursive copies to be taken of TRI_json_t* managed by this
/// Json class. Also, we do not want a const copy constructor stealing
/// the reference.
////////////////////////////////////////////////////////////////////////////////

        Json (Json const&&) = delete;

////////////////////////////////////////////////////////////////////////////////
/// @brief destructor
////////////////////////////////////////////////////////////////////////////////

        ~Json () throw() {
          if (_json != nullptr && _autofree == AUTOFREE) {
            TRI_FreeJson(_zone, _json);
          }
        }

// -----------------------------------------------------------------------------
// --SECTION--                                                    public methods
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @brief return internal TRI_json_t*, this does not change the ownership
/// of the pointer
////////////////////////////////////////////////////////////////////////////////

        TRI_json_t* json () const throw() {
          return _json;
        }

////////////////////////////////////////////////////////////////////////////////
/// @brief steal the TRI_json_t*, that is, in the AUTOFREE case the pointer
/// _json is changed to a nullptr. This is used in the copy and the move 
/// constructor and in the cast operator to TRI_json_t*.
////////////////////////////////////////////////////////////////////////////////

        TRI_json_t* steal () throw() {
          TRI_json_t* res = _json;
          if (_autofree == AUTOFREE) {
            _json = nullptr;
          }
          return res;
        }
   
////////////////////////////////////////////////////////////////////////////////
/// @brief assignment operator, note that, as the copy constructor, this 
/// has steal semantics, which avoids deep copies in situations that 
/// people will use. If you need an actual copy, use the copy method.
////////////////////////////////////////////////////////////////////////////////

        Json& operator= (Json& j) {
          if (_json != nullptr && _autofree == AUTOFREE) {
            TRI_FreeJson(_zone, _json);
          }
          _zone = j._zone;
          _autofree = j._autofree;
          _json = j.steal();
          return *this;
        }
        
////////////////////////////////////////////////////////////////////////////////
/// @brief move assignment operator, this has steal semantics.
////////////////////////////////////////////////////////////////////////////////

        Json& operator= (Json&& j) {
          if (_json != nullptr && _autofree == AUTOFREE) {
            TRI_FreeJson(_zone, _json);
          }
          _zone = j._zone;
          _autofree = j._autofree;
          _json = j.steal();
          return *this;
        }
        
////////////////////////////////////////////////////////////////////////////////
/// @brief copy recursively, even if NOFREE is set! Note that the copy is
//  AUTOFREE
////////////////////////////////////////////////////////////////////////////////

        Json copy (autofree_e autofree = AUTOFREE) const {
          Json c;
          c._zone = _zone;
          if (_json != nullptr) {
            c._json = TRI_CopyJson(_zone, _json);
          }
          else {
            c._json = nullptr;
          }
          c._autofree = autofree;
          return c;
        }

////////////////////////////////////////////////////////////////////////////////
/// @brief set an attribute value in an array, an exception is thrown
/// if *this is not a Json array. The pointer managed by sub is
/// stolen. The purpose of this method is that you can do
///   Json(Json::Array).set("a",Json(12)).set("b",Json(true))
/// and that this is both legal and efficient.
////////////////////////////////////////////////////////////////////////////////

        Json& set (char const* name, Json sub) {
          if (! TRI_IsArrayJson(_json)) {
            throw JsonException("Json is no array");
          }
          TRI_Insert3ArrayJson(_zone, _json, name, sub.steal());
          return *this;
        }

////////////////////////////////////////////////////////////////////////////////
/// @brief this is a syntactic shortcut for the set method using operator()
////////////////////////////////////////////////////////////////////////////////

        Json& operator() (char const* name, Json sub) {
          return set(name, sub);
        }

////////////////////////////////////////////////////////////////////////////////
/// @brief set an attribute value in an array, an exception is thrown if
/// *this is not a Json array. The pointer sub is integrated into the
/// list and will be freed if and only if the main thing is freed.
////////////////////////////////////////////////////////////////////////////////

        Json& set (char const* name, TRI_json_t* sub) {
          if (! TRI_IsArrayJson(_json)) {
            throw JsonException("Json is no array");
          }
          TRI_Insert3ArrayJson(_zone, _json, name, sub);
          return *this;
        }

////////////////////////////////////////////////////////////////////////////////
/// @brief this is a syntactic shortcut for the set method using operator()
////////////////////////////////////////////////////////////////////////////////

        Json& operator() (char const* name, TRI_json_t* sub) {
          return set(name, sub);
        }

////////////////////////////////////////////////////////////////////////////////
/// @brief append a Json value to the end of a Json list, an exception
/// is thrown if *this is not a Json list. The pointer managed by sub is
/// stolen. The purpose of this method is that you can do
///   Json(Json::List).add(Json(12)).add(Json(13))
/// and that this is both legal and efficient.
////////////////////////////////////////////////////////////////////////////////

        Json& add (Json sub) {
          if (! TRI_IsListJson(_json)) {
            throw JsonException("Json is no list");
          }
          TRI_PushBack3ListJson(_zone, _json, sub.steal());
          return *this;
        }

////////////////////////////////////////////////////////////////////////////////
/// @brief reserve space for n additional items in a list
////////////////////////////////////////////////////////////////////////////////

        Json& reserve (size_t n) {
          if (! TRI_IsListJson(_json)) {
            throw JsonException("Json is no list");
          }

          if (TRI_ReserveVector(&_json->_value._objects, n) != TRI_ERROR_NO_ERROR) {
            throw JsonException("Json: out of memory");
          }
          return *this;
        }

////////////////////////////////////////////////////////////////////////////////
/// @brief this is a syntactic shortcut for the add method using operator()
////////////////////////////////////////////////////////////////////////////////

        Json& operator() (Json sub) {
          return add(sub);
        }
 
////////////////////////////////////////////////////////////////////////////////
/// @brief append a TRI_json_t value to the end of a Json list, an exception
/// is thrown if *this is not a Json list. The pointer sub is integrated
/// into the list and will be freed if and only if the main thing is
/// freed.
////////////////////////////////////////////////////////////////////////////////

        Json& add (TRI_json_t* sub) {
          if (! TRI_IsListJson(_json)) {
            throw JsonException("Json is no list");
          }
          TRI_PushBack3ListJson(_zone, _json, sub);
          return *this;
        }

////////////////////////////////////////////////////////////////////////////////
/// @brief this is a syntactic shortcut for the add method using operator()
////////////////////////////////////////////////////////////////////////////////

        Json& operator() (TRI_json_t* sub) {
          return add(sub);
        }
 
////////////////////////////////////////////////////////////////////////////////
/// @brief this gets an attribute value of a Json array. An exception is
/// thrown if *this is not a Json array. The resulting TRI_json_t* is
/// wrapped in a NOFREE Json to allow things like
///   j.get("a").get("b")
/// to access j.a.b. The ownership of the whole structure remains with
/// *this.
////////////////////////////////////////////////////////////////////////////////

        Json get (char const* name) const {
          if (! TRI_IsArrayJson(_json)) {
            throw JsonException("Json is no array");
          }
          return Json(_zone, TRI_LookupArrayJson(_json, name), NOFREE);
        }

////////////////////////////////////////////////////////////////////////////////
/// @brief this gets a list entry of a Json list. An exception is
/// thrown if *this is not a Json list. The resulting TRI_json_t* is
/// wrapped in a NOFREE Json to allow things like
///   j.at(0).at(1)
/// to access j[0][1]. The ownership of the whole structure remains with
/// *this. If the position given is not bound, then an empty Json is
/// returned.
////////////////////////////////////////////////////////////////////////////////

        Json at (size_t pos) const {
          return at(static_cast<int>(pos));
        }

        Json at (int pos) const {
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
              return Json(_zone, Json::Null, AUTOFREE);
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

////////////////////////////////////////////////////////////////////////////////
/// @brief destroy the contents of the JSON
////////////////////////////////////////////////////////////////////////////////

        void destroy () {
          if (_json != nullptr) {
            TRI_FreeJson(_zone, _json);
            _json = nullptr;
          }
        }

////////////////////////////////////////////////////////////////////////////////
/// @brief checks whether *this is a Json that is equal to null.
////////////////////////////////////////////////////////////////////////////////

        bool isNull () throw() {
          return _json != nullptr && _json->_type == TRI_JSON_NULL;
        }

////////////////////////////////////////////////////////////////////////////////
/// @brief checks whether *this is a boolean Json.
////////////////////////////////////////////////////////////////////////////////

        bool isBoolean () throw() {
          return TRI_IsBooleanJson(_json);
        }

////////////////////////////////////////////////////////////////////////////////
/// @brief checks whether *this is a number Json.
////////////////////////////////////////////////////////////////////////////////

        bool isNumber () throw() {
          return TRI_IsNumberJson(_json);
        }

////////////////////////////////////////////////////////////////////////////////
/// @brief checks whether *this is a string Json.
////////////////////////////////////////////////////////////////////////////////

        bool isString () throw() {
          return TRI_IsStringJson(_json);
        }

////////////////////////////////////////////////////////////////////////////////
/// @brief checks whether *this is an array Json.
////////////////////////////////////////////////////////////////////////////////

        bool isArray () throw() {
          return TRI_IsArrayJson(_json);
        }

////////////////////////////////////////////////////////////////////////////////
/// @brief checks whether *this is a list Json.
////////////////////////////////////////////////////////////////////////////////

        bool isList () throw() {
          return TRI_IsListJson(_json);
        }

////////////////////////////////////////////////////////////////////////////////
/// @brief returns the length of the list
////////////////////////////////////////////////////////////////////////////////

        size_t size () const {
          if (! TRI_IsListJson(_json)) {
            throw JsonException("Json is no list");
          }
          return _json->_value._objects._length;
        }

////////////////////////////////////////////////////////////////////////////////
/// @brief checks whether *this is an empty Json (not even null).
////////////////////////////////////////////////////////////////////////////////

        bool isEmpty () const throw() {
          return _json == nullptr;
        }

////////////////////////////////////////////////////////////////////////////////
/// @brief converts the Json recursively into a string.
////////////////////////////////////////////////////////////////////////////////

        std::string toString () const {
          if (_json != nullptr) {
            return JsonHelper::toString(_json);
          }
          else {
            return std::string("");
          }
        }

      private:

////////////////////////////////////////////////////////////////////////////////
/// @brief store the memory zone used
////////////////////////////////////////////////////////////////////////////////

        TRI_memory_zone_t* _zone;

////////////////////////////////////////////////////////////////////////////////
/// @brief the actual TRI_json_t*
////////////////////////////////////////////////////////////////////////////////

        TRI_json_t* _json;

////////////////////////////////////////////////////////////////////////////////
/// @brief flag, whether we automatically free the TRI_json_t*.
////////////////////////////////////////////////////////////////////////////////

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
