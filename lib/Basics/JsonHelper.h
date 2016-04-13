////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2014-2016 ArangoDB GmbH, Cologne, Germany
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
////////////////////////////////////////////////////////////////////////////////

#ifndef LIB_BASICS_JSON_HELPER_H
#define LIB_BASICS_JSON_HELPER_H 1

#include "Basics/Common.h"
#include "Basics/Exceptions.h"
#include "Basics/json.h"
#include "Basics/StringBuffer.h"
#include <iosfwd>

#include "velocypack/Builder.h"
#include "velocypack/Parser.h"

namespace arangodb {
namespace basics {

class JsonHelper {
 private:
  JsonHelper() = delete;
  ~JsonHelper() = delete;

 public:

  //////////////////////////////////////////////////////////////////////////////
  /// @brief convert a uint64 into a JSON string
  //////////////////////////////////////////////////////////////////////////////

  static uint64_t stringUInt64(TRI_json_t const*);

  //////////////////////////////////////////////////////////////////////////////
  /// @brief convert a uint64 into a JSON string
  //////////////////////////////////////////////////////////////////////////////

  static uint64_t stringUInt64(TRI_json_t const*, char const*);

  //////////////////////////////////////////////////////////////////////////////
  /// @brief creates an array of strings from a JSON (sub-) object
  //////////////////////////////////////////////////////////////////////////////

  static std::vector<std::string> stringArray(TRI_json_t const*);

  //////////////////////////////////////////////////////////////////////////////
  /// @brief create JSON from string
  //////////////////////////////////////////////////////////////////////////////

  static TRI_json_t* fromString(std::string const&);

  //////////////////////////////////////////////////////////////////////////////
  /// @brief stringify json
  //////////////////////////////////////////////////////////////////////////////

  static std::string toString(TRI_json_t const*);

  //////////////////////////////////////////////////////////////////////////////
  /// @brief returns true for arrays
  //////////////////////////////////////////////////////////////////////////////

  static inline bool isArray(TRI_json_t const* json) {
    return json != nullptr && json->_type == TRI_JSON_ARRAY;
  }

  //////////////////////////////////////////////////////////////////////////////
  /// @brief returns true for numbers
  //////////////////////////////////////////////////////////////////////////////

  static inline bool isNumber(TRI_json_t const* json) {
    return json != nullptr && json->_type == TRI_JSON_NUMBER;
  }

  //////////////////////////////////////////////////////////////////////////////
  /// @brief returns an object sub-element
  //////////////////////////////////////////////////////////////////////////////

  static TRI_json_t* getObjectElement(TRI_json_t const*, char const* name);

  //////////////////////////////////////////////////////////////////////////////
  /// @brief returns a string element, or a default it is does not exist
  //////////////////////////////////////////////////////////////////////////////

  static std::string getStringValue(TRI_json_t const*, std::string const&);

  //////////////////////////////////////////////////////////////////////////////
  /// @brief returns a string sub-element, or a default it is does not exist
  //////////////////////////////////////////////////////////////////////////////

  static std::string getStringValue(TRI_json_t const*, char const*,
                                    std::string const&);

  //////////////////////////////////////////////////////////////////////////////
  /// @brief returns a numeric value
  //////////////////////////////////////////////////////////////////////////////

  template <typename T>
  static T getNumericValue(TRI_json_t const* json, T defaultValue) {
    if (isNumber(json)) {
      return (T)json->_value._number;
    }

    return defaultValue;
  }

  //////////////////////////////////////////////////////////////////////////////
  /// @brief returns a numeric sub-element, or a default it is does not exist
  //////////////////////////////////////////////////////////////////////////////

  template <typename T>
  static T getNumericValue(TRI_json_t const* json, char const* name,
                           T defaultValue) {
    TRI_json_t const* sub = getObjectElement(json, name);

    if (isNumber(sub)) {
      return (T)sub->_value._number;
    }

    return defaultValue;
  }

  //////////////////////////////////////////////////////////////////////////////
  /// @brief returns a boolean sub-element, or a default it is does not exist
  //////////////////////////////////////////////////////////////////////////////

  static bool getBooleanValue(TRI_json_t const*, char const*, bool);

  //////////////////////////////////////////////////////////////////////////////
  /// @brief returns a boolean sub-element, or a throws an exception if the
  /// sub-element does not exist or if it is not boolean
  //////////////////////////////////////////////////////////////////////////////

  static bool checkAndGetBooleanValue(TRI_json_t const*, char const*);

  //////////////////////////////////////////////////////////////////////////////
  /// @brief returns a numeric sub-element, or throws if <name> does not exist
  /// or it is not a number
  //////////////////////////////////////////////////////////////////////////////

  template <typename T>
  static T checkAndGetNumericValue(TRI_json_t const* json, char const* name) {
    TRI_json_t const* sub = getObjectElement(json, name);

    if (!isNumber(sub)) {
      std::string msg = "The attribute '" + std::string(name) +
                        "' was not found or is not a number.";
      THROW_ARANGO_EXCEPTION(TRI_ERROR_INTERNAL);
    }
    return static_cast<T>(sub->_value._number);
  }

  //////////////////////////////////////////////////////////////////////////////
  /// @brief returns a string sub-element, or throws if <name> does not exist
  /// or it is not a string
  //////////////////////////////////////////////////////////////////////////////

  static std::string checkAndGetStringValue(TRI_json_t const*, char const*);

  //////////////////////////////////////////////////////////////////////////////
  /// @brief returns an object sub-element, or a throws an exception if the
  /// sub-element does not exist or if it is not an object
  //////////////////////////////////////////////////////////////////////////////

  static TRI_json_t const* checkAndGetObjectValue(TRI_json_t const*,
                                                  char const*);

  //////////////////////////////////////////////////////////////////////////////
  /// @brief returns an array sub-element, or a throws an exception if the
  /// sub-element does not exist or if it is not an array
  //////////////////////////////////////////////////////////////////////////////

  static TRI_json_t const* checkAndGetArrayValue(TRI_json_t const*,
                                                 char const*);

  //////////////////////////////////////////////////////////////////////////////
  /// @brief TRI_json_t to VelocyPack, this is a temporary, inefficient method
  /// which must be removed later on.
  //////////////////////////////////////////////////////////////////////////////

  static std::shared_ptr<arangodb::velocypack::Builder> toVelocyPack(
      TRI_json_t const* json) {
    std::string tmp = toString(json);
    arangodb::velocypack::Parser parser;
    try {
      parser.parse(tmp);
    } catch (...) {
      return std::shared_ptr<arangodb::velocypack::Builder>();
    }
    return parser.steal();
  }

  //////////////////////////////////////////////////////////////////////////////
  /// @brief TRI_json_t to VelocyPack, writing into an existing Builder
  //////////////////////////////////////////////////////////////////////////////

  static int toVelocyPack(TRI_json_t const* json,
                          arangodb::velocypack::Builder& builder) {
    std::string tmp = toString(json);
    arangodb::velocypack::Options opt;
    opt.clearBuilderBeforeParse = false;
    arangodb::velocypack::Parser parser(builder, &opt);
    try {
      parser.parse(tmp);
    } catch (...) {
      return TRI_ERROR_INTERNAL;
    }
    return TRI_ERROR_NO_ERROR;
  }
};

////////////////////////////////////////////////////////////////////////////////
/// @brief Json, a class to fabricate TRI_json_t* conveniently
////////////////////////////////////////////////////////////////////////////////

class Json {
 public:
  //////////////////////////////////////////////////////////////////////////////
  /// @brief enum for the type of a Json structure
  //////////////////////////////////////////////////////////////////////////////

  enum type_e { Null, Bool, Number, String, Array, Object };

  //////////////////////////////////////////////////////////////////////////////
  /// @brief enum to say whether we are automatically freeing the TRI_json_t*
  //////////////////////////////////////////////////////////////////////////////

  enum autofree_e { AUTOFREE, NOFREE };

 private:
  //////////////////////////////////////////////////////////////////////////////
  /// @brief internal helper for the generic constructor
  //////////////////////////////////////////////////////////////////////////////

  void make(type_e t, size_t sizeHint) {
    // convert to an actual memory zone
    TRI_memory_zone_t* zone = TRI_MemoryZone(_zone);

    switch (t) {
      case Null:
        _json = TRI_CreateNullJson(zone);
        break;
      case Bool:
        _json = TRI_CreateBooleanJson(zone, true);
        break;
      case Number:
        _json = TRI_CreateNumberJson(zone, 0.0);
        break;
      case String:
        _json = TRI_CreateStringCopyJson(zone, "", 0);
        break;
      case Array:
        _json = TRI_CreateArrayJson(zone, sizeHint);
        break;
      case Object:
        _json = TRI_CreateObjectJson(zone, 2 * sizeHint);
        break;
    }

    if (_json == nullptr) {
      THROW_ARANGO_EXCEPTION(TRI_ERROR_OUT_OF_MEMORY);
    }
  }

  //////////////////////////////////////////////////////////////////////////////
  /// @brief default constructor making an empty Json
  //////////////////////////////////////////////////////////////////////////////

 public:
  Json()
      : _json(nullptr),
        _zone(TRI_MemoryZoneId(TRI_UNKNOWN_MEM_ZONE)),
        _autofree(AUTOFREE) {}

  //////////////////////////////////////////////////////////////////////////////
  /// @brief generic constructor for a type_e
  //////////////////////////////////////////////////////////////////////////////

  explicit Json(type_e t, autofree_e autofree = AUTOFREE)
      : _json(nullptr),
        _zone(TRI_MemoryZoneId(TRI_UNKNOWN_MEM_ZONE)),
        _autofree(autofree) {
    make(t, 0);
  }

  //////////////////////////////////////////////////////////////////////////////
  /// @brief generic constructor for a memzone and a type_e
  //////////////////////////////////////////////////////////////////////////////

  Json(TRI_memory_zone_t* z, type_e t, autofree_e autofree = AUTOFREE)
      : _json(nullptr), _zone(TRI_MemoryZoneId(z)), _autofree(autofree) {
    make(t, 0);
  }

  //////////////////////////////////////////////////////////////////////////////
  /// @brief generic constructor for a type_e with a size hint
  //////////////////////////////////////////////////////////////////////////////

  Json(type_e t, size_t sizeHint, autofree_e autofree = AUTOFREE)
      : _json(nullptr),
        _zone(TRI_MemoryZoneId(TRI_UNKNOWN_MEM_ZONE)),
        _autofree(autofree) {
    make(t, sizeHint);
  }

  //////////////////////////////////////////////////////////////////////////////
  /// @brief generic constructor for a memzone, a type_e and a size hint
  //////////////////////////////////////////////////////////////////////////////

  Json(TRI_memory_zone_t* z, type_e t, size_t sizeHint,
                autofree_e autofree = AUTOFREE)
      : _json(nullptr), _zone(TRI_MemoryZoneId(z)), _autofree(autofree) {
    make(t, sizeHint);
  }

  //////////////////////////////////////////////////////////////////////////////
  /// @brief constructor for a bool
  //////////////////////////////////////////////////////////////////////////////

  explicit Json(bool x, autofree_e autofree = AUTOFREE)
      : _json(nullptr),
        _zone(TRI_MemoryZoneId(TRI_UNKNOWN_MEM_ZONE)),
        _autofree(autofree) {
    _json = TRI_CreateBooleanJson(TRI_UNKNOWN_MEM_ZONE, x);

    if (_json == nullptr) {
      THROW_ARANGO_EXCEPTION(TRI_ERROR_OUT_OF_MEMORY);
    }
  }

  //////////////////////////////////////////////////////////////////////////////
  /// @brief constructor for a memzone and a bool
  //////////////////////////////////////////////////////////////////////////////

  Json(TRI_memory_zone_t* z, bool x, autofree_e autofree = AUTOFREE)
      : _json(nullptr), _zone(TRI_MemoryZoneId(z)), _autofree(autofree) {
    _json = TRI_CreateBooleanJson(z, x);

    if (_json == nullptr) {
      THROW_ARANGO_EXCEPTION(TRI_ERROR_OUT_OF_MEMORY);
    }
  }

  //////////////////////////////////////////////////////////////////////////////
  /// @brief constructor for an int32_t
  //////////////////////////////////////////////////////////////////////////////

  explicit Json(int32_t x, autofree_e autofree = AUTOFREE)
      : _json(nullptr),
        _zone(TRI_MemoryZoneId(TRI_UNKNOWN_MEM_ZONE)),
        _autofree(autofree) {
    _json = TRI_CreateNumberJson(TRI_UNKNOWN_MEM_ZONE, static_cast<double>(x));

    if (_json == nullptr) {
      THROW_ARANGO_EXCEPTION(TRI_ERROR_OUT_OF_MEMORY);
    }
  }

  //////////////////////////////////////////////////////////////////////////////
  /// @brief constructor for a memzone and an int32_t
  //////////////////////////////////////////////////////////////////////////////

  Json(TRI_memory_zone_t* z, int32_t x, autofree_e autofree = AUTOFREE)
      : _json(nullptr), _zone(TRI_MemoryZoneId(z)), _autofree(autofree) {
    _json = TRI_CreateNumberJson(z, static_cast<double>(x));

    if (_json == nullptr) {
      THROW_ARANGO_EXCEPTION(TRI_ERROR_OUT_OF_MEMORY);
    }
  }

  //////////////////////////////////////////////////////////////////////////////
  /// @brief constructor for a double
  //////////////////////////////////////////////////////////////////////////////

  explicit Json(double x, autofree_e autofree = AUTOFREE)
      : _json(nullptr),
        _zone(TRI_MemoryZoneId(TRI_UNKNOWN_MEM_ZONE)),
        _autofree(autofree) {
    _json = TRI_CreateNumberJson(TRI_UNKNOWN_MEM_ZONE, x);

    if (_json == nullptr) {
      THROW_ARANGO_EXCEPTION(TRI_ERROR_OUT_OF_MEMORY);
    }
  }

  //////////////////////////////////////////////////////////////////////////////
  /// @brief constructor for a memzone and a double
  //////////////////////////////////////////////////////////////////////////////

  Json(TRI_memory_zone_t* z, double x, autofree_e autofree = AUTOFREE)
      : _json(nullptr), _zone(TRI_MemoryZoneId(z)), _autofree(autofree) {
    _json = TRI_CreateNumberJson(z, x);

    if (_json == nullptr) {
      THROW_ARANGO_EXCEPTION(TRI_ERROR_OUT_OF_MEMORY);
    }
  }

  //////////////////////////////////////////////////////////////////////////////
  /// @brief constructor for a char const*
  //////////////////////////////////////////////////////////////////////////////

  explicit Json(char const* x, autofree_e autofree = AUTOFREE)
      : _json(nullptr),
        _zone(TRI_MemoryZoneId(TRI_UNKNOWN_MEM_ZONE)),
        _autofree(autofree) {
    _json = TRI_CreateStringCopyJson(TRI_UNKNOWN_MEM_ZONE, x, strlen(x));

    if (_json == nullptr) {
      THROW_ARANGO_EXCEPTION(TRI_ERROR_OUT_OF_MEMORY);
    }
  }

  //////////////////////////////////////////////////////////////////////////////
  /// @brief constructor for a char const*
  //////////////////////////////////////////////////////////////////////////////

  Json(char const* x, size_t length, autofree_e autofree = AUTOFREE)
      : _json(nullptr),
        _zone(TRI_MemoryZoneId(TRI_UNKNOWN_MEM_ZONE)),
        _autofree(autofree) {
    _json = TRI_CreateStringCopyJson(TRI_UNKNOWN_MEM_ZONE, x, length);

    if (_json == nullptr) {
      THROW_ARANGO_EXCEPTION(TRI_ERROR_OUT_OF_MEMORY);
    }
  }

  //////////////////////////////////////////////////////////////////////////////
  /// @brief constructor for a memzone and a char const*
  //////////////////////////////////////////////////////////////////////////////

  Json(TRI_memory_zone_t* z, char const* x,
                autofree_e autofree = AUTOFREE)
      : _json(nullptr), _zone(TRI_MemoryZoneId(z)), _autofree(autofree) {
    _json = TRI_CreateStringCopyJson(z, x, strlen(x));

    if (_json == nullptr) {
      THROW_ARANGO_EXCEPTION(TRI_ERROR_OUT_OF_MEMORY);
    }
  }

  //////////////////////////////////////////////////////////////////////////////
  /// @brief constructor for a memzone and a char const*
  //////////////////////////////////////////////////////////////////////////////

  Json(TRI_memory_zone_t* z, char const* x, size_t length,
                autofree_e autofree = AUTOFREE)
      : _json(nullptr), _zone(TRI_MemoryZoneId(z)), _autofree(autofree) {
    _json = TRI_CreateStringCopyJson(z, x, length);

    if (_json == nullptr) {
      THROW_ARANGO_EXCEPTION(TRI_ERROR_OUT_OF_MEMORY);
    }
  }

  //////////////////////////////////////////////////////////////////////////////
  /// @brief constructor for a string
  //////////////////////////////////////////////////////////////////////////////

  explicit Json(std::string const& x, autofree_e autofree = AUTOFREE)
      : _json(nullptr),
        _zone(TRI_MemoryZoneId(TRI_UNKNOWN_MEM_ZONE)),
        _autofree(autofree) {
    _json = TRI_CreateStringCopyJson(TRI_UNKNOWN_MEM_ZONE, x.c_str(), x.size());

    if (_json == nullptr) {
      THROW_ARANGO_EXCEPTION(TRI_ERROR_OUT_OF_MEMORY);
    }
  }

  //////////////////////////////////////////////////////////////////////////////
  /// @brief constructor for a memzone and a string
  //////////////////////////////////////////////////////////////////////////////

  Json(TRI_memory_zone_t* z, std::string const& x,
                autofree_e autofree = AUTOFREE)
      : _json(nullptr), _zone(TRI_MemoryZoneId(z)), _autofree(autofree) {
    _json = TRI_CreateStringCopyJson(z, x.c_str(), x.size());

    if (_json == nullptr) {
      THROW_ARANGO_EXCEPTION(TRI_ERROR_OUT_OF_MEMORY);
    }
  }

  //////////////////////////////////////////////////////////////////////////////
  /// @brief constructor for a memzone and a TRI_json_t*
  //////////////////////////////////////////////////////////////////////////////

  Json(TRI_memory_zone_t* z, TRI_json_t* j,
                autofree_e autofree = AUTOFREE)
      : _json(j), _zone(TRI_MemoryZoneId(z)), _autofree(autofree) {}

  //////////////////////////////////////////////////////////////////////////////
  /// @brief constructor for a memzone and a const TRI_json_t*
  //////////////////////////////////////////////////////////////////////////////

  Json(TRI_memory_zone_t* z, TRI_json_t const* j,
                autofree_e autofree = NOFREE)
      : _json(const_cast<TRI_json_t*>(j)),
        _zone(TRI_MemoryZoneId(z)),
        _autofree(autofree) {}

  explicit Json(TRI_json_t* j) = delete;
  explicit Json(TRI_json_t const*) = delete;

  //////////////////////////////////////////////////////////////////////////////
  /// @brief "copy" constructor, note that in the AUTOFREE case this steals
  /// the structure from j to allow returning Json objects by value without
  /// copying the whole structure.
  //////////////////////////////////////////////////////////////////////////////

  Json(Json& j) : _json(j.steal()), _zone(j._zone), _autofree(j._autofree) {}

  //////////////////////////////////////////////////////////////////////////////
  /// @brief move constructor, note that in the AUTOFREE case this steals
  /// the structure from j to allow returning Json objects by value without
  /// copying the whole structure.
  //////////////////////////////////////////////////////////////////////////////

  Json(Json&& j) : _json(j.steal()), _zone(j._zone), _autofree(j._autofree) {}

  //////////////////////////////////////////////////////////////////////////////
  /// @brief delete the normal move constructor, because we do not want
  /// invisible recursive copies to be taken of TRI_json_t* managed by this
  /// Json class. Also, we do not want a const copy constructor stealing
  /// the reference.
  //////////////////////////////////////////////////////////////////////////////

  Json(Json const&&) = delete;

  ~Json() noexcept {
    if (_json != nullptr && _autofree == AUTOFREE) {
      TRI_FreeJson(TRI_MemoryZone(_zone), _json);
    }
  }

  //////////////////////////////////////////////////////////////////////////////
  /// @brief return internal TRI_json_t*, this does not change the ownership
  /// of the pointer
  //////////////////////////////////////////////////////////////////////////////

  TRI_json_t* json() const noexcept { return _json; }

  //////////////////////////////////////////////////////////////////////////////
  /// @brief steal the TRI_json_t*, that is, in the AUTOFREE case the pointer
  /// _json is changed to a nullptr. This is used in the copy and the move
  /// constructor and in the cast operator to TRI_json_t*.
  //////////////////////////////////////////////////////////////////////////////

  TRI_json_t* steal() noexcept {
    TRI_json_t* res = _json;
    if (_autofree == AUTOFREE) {
      _json = nullptr;
    }
    return res;
  }

  //////////////////////////////////////////////////////////////////////////////
  /// @brief assignment operator, note that, as the copy constructor, this
  /// has steal semantics, which avoids deep copies in situations that
  /// people will use. If you need an actual copy, use the copy method.
  //////////////////////////////////////////////////////////////////////////////

  Json& operator=(Json& j) {
    if (_json != nullptr && _autofree == AUTOFREE) {
      TRI_FreeJson(TRI_MemoryZone(_zone), _json);
    }
    _zone = j._zone;
    _autofree = j._autofree;
    _json = j.steal();
    return *this;
  }

  //////////////////////////////////////////////////////////////////////////////
  /// @brief move assignment operator, this has steal semantics.
  //////////////////////////////////////////////////////////////////////////////

  Json& operator=(Json&& j) {
    if (_json != nullptr && _autofree == AUTOFREE) {
      TRI_FreeJson(TRI_MemoryZone(_zone), _json);
    }
    _zone = j._zone;
    _autofree = j._autofree;
    _json = j.steal();
    return *this;
  }

  //////////////////////////////////////////////////////////////////////////////
  /// @brief copy recursively, even if NOFREE is set! Note that the copy is
  //  AUTOFREE
  //////////////////////////////////////////////////////////////////////////////

  Json copy(autofree_e autofree = AUTOFREE) const {
    Json c;
    c._zone = _zone;
    if (_json != nullptr) {
      c._json = TRI_CopyJson(TRI_MemoryZone(_zone), _json);
      if (c._json == nullptr) {
        THROW_ARANGO_EXCEPTION(TRI_ERROR_OUT_OF_MEMORY);
      }
    } else {
      c._json = nullptr;
    }
    c._autofree = autofree;
    return c;
  }

  //////////////////////////////////////////////////////////////////////////////
  /// @brief set an attribute value in an array, an exception is thrown
  /// if *this is not a Json object. The pointer managed by sub is
  /// stolen. The purpose of this method is that you can do
  ///   Json(Json::Object).set("a",Json(12)).set("b",Json(true))
  /// and that this is both legal and efficient.
  //////////////////////////////////////////////////////////////////////////////

  Json& set(char const* name, Json sub) {
    if (!TRI_IsObjectJson(_json)) {
      THROW_ARANGO_EXCEPTION_MESSAGE(TRI_ERROR_INTERNAL, "json is no object");
    }
    TRI_Insert3ObjectJson(TRI_MemoryZone(_zone), _json, name, sub.steal());
    return *this;
  }

  Json& set(std::string const& name, Json sub) {
    if (!TRI_IsObjectJson(_json)) {
      THROW_ARANGO_EXCEPTION_MESSAGE(TRI_ERROR_INTERNAL, "json is no object");
    }
    TRI_Insert3ObjectJson(TRI_MemoryZone(_zone), _json, name.c_str(),
                          sub.steal());
    return *this;
  }

  bool unset(char const* name) {
    if (!TRI_IsObjectJson(_json)) {
      THROW_ARANGO_EXCEPTION_MESSAGE(TRI_ERROR_INTERNAL, "json is no object");
    }
    return TRI_DeleteObjectJson(TRI_MemoryZone(_zone), _json, name);
  }

  bool unset(std::string const& name) {
    if (!TRI_IsObjectJson(_json)) {
      THROW_ARANGO_EXCEPTION_MESSAGE(TRI_ERROR_INTERNAL, "json is no object");
    }
    return TRI_DeleteObjectJson(TRI_MemoryZone(_zone), _json, name.c_str());
  }

  //////////////////////////////////////////////////////////////////////////////
  /// @brief set an attribute value in an object, an exception is thrown if
  /// *this is not a Json object. The pointer sub is integrated into the
  /// object and will be freed if and only if the main thing is freed.
  //////////////////////////////////////////////////////////////////////////////

  Json& set(char const* name, TRI_json_t* sub) {
    if (!TRI_IsObjectJson(_json)) {
      THROW_ARANGO_EXCEPTION_MESSAGE(TRI_ERROR_INTERNAL, "json is no object");
    }
    TRI_Insert3ObjectJson(TRI_MemoryZone(_zone), _json, name, sub);
    return *this;
  }

  //////////////////////////////////////////////////////////////////////////////
  /// @brief this gets an attribute value of a Json object. An exception is
  /// thrown if *this is not a Json object. The resulting TRI_json_t* is
  /// wrapped in a NOFREE Json to allow things like
  ///   j.get("a").get("b")
  /// to access j.a.b. The ownership of the whole structure remains with
  /// *this.
  //////////////////////////////////////////////////////////////////////////////

  Json get(char const* name) const {
    if (!TRI_IsObjectJson(_json)) {
      THROW_ARANGO_EXCEPTION_MESSAGE(TRI_ERROR_INTERNAL, "json is no object");
    }
    return Json(TRI_MemoryZone(_zone), TRI_LookupObjectJson(_json, name),
                NOFREE);
  }

  //////////////////////////////////////////////////////////////////////////////
  /// @brief this is a syntactic shortcut for the set method using operator()
  //////////////////////////////////////////////////////////////////////////////

  Json& operator()(char const* name, Json sub) { return set(name, sub); }

  //////////////////////////////////////////////////////////////////////////////
  /// @brief this is a syntactic shortcut for the set method using operator()
  //////////////////////////////////////////////////////////////////////////////

  Json& operator()(char const* name, TRI_json_t* sub) { return set(name, sub); }

  //////////////////////////////////////////////////////////////////////////////
  /// @brief whether or not the JSON object has a specific attribute
  //////////////////////////////////////////////////////////////////////////////

  bool has(char const* name) const {
    if (!TRI_IsObjectJson(_json)) {
      THROW_ARANGO_EXCEPTION_MESSAGE(TRI_ERROR_INTERNAL, "json is no object");
    }
    return (TRI_LookupObjectJson(_json, name) != nullptr);
  }

  //////////////////////////////////////////////////////////////////////////////
  /// @brief append a Json value to the end of a Json array, an exception
  /// is thrown if *this is not a Json array. The pointer managed by sub is
  /// stolen. The purpose of this method is that you can do
  ///   Json(Json::Array).add(Json(12)).add(Json(13))
  /// and that this is both legal and efficient.
  //////////////////////////////////////////////////////////////////////////////

  Json& add(Json sub) {
    if (!TRI_IsArrayJson(_json)) {
      THROW_ARANGO_EXCEPTION_MESSAGE(TRI_ERROR_INTERNAL, "json is no array");
    }
    TRI_PushBack3ArrayJson(TRI_MemoryZone(_zone), _json, sub.steal());
    return *this;
  }

  //////////////////////////////////////////////////////////////////////////////
  /// @brief append a TRI_json_t value to the end of a Json array, an exception
  /// is thrown if *this is not a Json array. The pointer sub is integrated
  /// into the array and will be freed if and only if the main thing is
  /// freed.
  //////////////////////////////////////////////////////////////////////////////

  Json& add(TRI_json_t* sub) {
    if (!TRI_IsArrayJson(_json)) {
      THROW_ARANGO_EXCEPTION_MESSAGE(TRI_ERROR_INTERNAL, "json is no array");
    }
    TRI_PushBack3ArrayJson(TRI_MemoryZone(_zone), _json, sub);
    return *this;
  }

  //////////////////////////////////////////////////////////////////////////////
  /// @brief append a Json value to the end of a Json array, an exception
  /// is thrown if *this is not a Json array. The data pointed to by the source
  /// json pointer is copied into the result, and the original data is nullified
  /// (so it can be destroyed safely later by its original possessor).
  //////////////////////////////////////////////////////////////////////////////

  Json& transfer(TRI_json_t* json) {
    if (!TRI_IsArrayJson(_json)) {
      THROW_ARANGO_EXCEPTION_MESSAGE(TRI_ERROR_INTERNAL, "json is no array");
    }
    TRI_PushBack2ArrayJson(_json, json);
    TRI_InitNullJson(json);
    return *this;
  }

  //////////////////////////////////////////////////////////////////////////////
  /// @brief reserve space for n additional items in an array
  //////////////////////////////////////////////////////////////////////////////

  Json& reserve(size_t n) {
    if (!TRI_IsArrayJson(_json)) {
      THROW_ARANGO_EXCEPTION_MESSAGE(TRI_ERROR_INTERNAL, "json is no array");
    }

    if (TRI_ReserveVector(&_json->_value._objects, n) != TRI_ERROR_NO_ERROR) {
      THROW_ARANGO_EXCEPTION(TRI_ERROR_OUT_OF_MEMORY);
    }
    return *this;
  }

  //////////////////////////////////////////////////////////////////////////////
  /// @brief this is a syntactic shortcut for the add method using operator()
  //////////////////////////////////////////////////////////////////////////////

  Json& operator()(Json sub) { return add(sub); }

  //////////////////////////////////////////////////////////////////////////////
  /// @brief this is a syntactic shortcut for the add method using operator()
  //////////////////////////////////////////////////////////////////////////////

  Json& operator()(TRI_json_t* sub) { return add(sub); }

  //////////////////////////////////////////////////////////////////////////////
  /// @brief this gets an array entry of a Json array. An exception is
  /// thrown if *this is not a Json array. The resulting TRI_json_t* is
  /// wrapped in a NOFREE Json to allow things like
  ///   j.at(0).at(1)
  /// to access j[0][1]. The ownership of the whole structure remains with
  /// *this. If the position given is not bound, then an empty Json is
  /// returned.
  //////////////////////////////////////////////////////////////////////////////

  Json at(size_t pos) const { return at(static_cast<int>(pos)); }

  Json at(int pos) const {
    if (!TRI_IsArrayJson(_json)) {
      THROW_ARANGO_EXCEPTION_MESSAGE(TRI_ERROR_INTERNAL, "json is no array");
    }
    TRI_json_t* j;
    if (pos >= 0) {
      j = TRI_LookupArrayJson(_json, pos);
      if (j != nullptr) {
        return Json(TRI_MemoryZone(_zone), j, NOFREE);
      } else {
        return Json(TRI_MemoryZone(_zone), Json::Null, AUTOFREE);
      }
    } else {
      size_t pos2 = -pos;
      size_t len = TRI_LengthVector(&_json->_value._objects);
      if (pos2 > len) {
        return Json(TRI_MemoryZone(_zone), Json::Null, AUTOFREE);
      }
      j = TRI_LookupArrayJson(_json, len - pos2);
      if (j != nullptr) {
        return Json(TRI_MemoryZone(_zone), j, NOFREE);
      } else {
        return Json(TRI_MemoryZone(_zone), Json::Null, AUTOFREE);
      }
    }
  }

  //////////////////////////////////////////////////////////////////////////////
  /// @brief destroy the contents of the JSON
  //////////////////////////////////////////////////////////////////////////////

  void destroy() {
    if (_json != nullptr) {
      TRI_FreeJson(TRI_MemoryZone(_zone), _json);
      _json = nullptr;
    }
  }

  //////////////////////////////////////////////////////////////////////////////
  /// @brief return the JSON's memory zone
  //////////////////////////////////////////////////////////////////////////////

  TRI_memory_zone_t* zone() const { return TRI_MemoryZone(_zone); }

  //////////////////////////////////////////////////////////////////////////////
  /// @brief checks whether *this is a Json that is equal to null.
  //////////////////////////////////////////////////////////////////////////////

  bool isNull() const noexcept {
    return _json != nullptr && _json->_type == TRI_JSON_NULL;
  }

  //////////////////////////////////////////////////////////////////////////////
  /// @brief checks whether *this is a boolean Json.
  //////////////////////////////////////////////////////////////////////////////

  bool isBoolean() const noexcept { return TRI_IsBooleanJson(_json); }

  //////////////////////////////////////////////////////////////////////////////
  /// @brief checks whether *this is a number Json.
  //////////////////////////////////////////////////////////////////////////////

  bool isNumber() const noexcept { return TRI_IsNumberJson(_json); }

  //////////////////////////////////////////////////////////////////////////////
  /// @brief checks whether *this is a string Json.
  //////////////////////////////////////////////////////////////////////////////

  bool isString() const noexcept { return TRI_IsStringJson(_json); }

  //////////////////////////////////////////////////////////////////////////////
  /// @brief checks whether *this is an object Json.
  //////////////////////////////////////////////////////////////////////////////

  bool isObject() const noexcept { return TRI_IsObjectJson(_json); }

  //////////////////////////////////////////////////////////////////////////////
  /// @brief checks whether *this is an array Json.
  //////////////////////////////////////////////////////////////////////////////

  bool isArray() const noexcept { return TRI_IsArrayJson(_json); }

  //////////////////////////////////////////////////////////////////////////////
  /// @brief returns the length of the array
  //////////////////////////////////////////////////////////////////////////////

  size_t size() const {
    if (!TRI_IsArrayJson(_json)) {
      THROW_ARANGO_EXCEPTION_MESSAGE(TRI_ERROR_INTERNAL, "json is no array");
    }
    return TRI_LengthVector(&_json->_value._objects);
  }

  //////////////////////////////////////////////////////////////////////////////
  /// @brief returns the length of the array
  //////////////////////////////////////////////////////////////////////////////

  size_t members() const {
    if (!TRI_IsObjectJson(_json)) {
      THROW_ARANGO_EXCEPTION_MESSAGE(TRI_ERROR_INTERNAL, "json is no object");
    }
    return TRI_LengthVector(&_json->_value._objects) / 2;
  }

  //////////////////////////////////////////////////////////////////////////////
  /// @brief checks whether *this is an empty Json (not even null).
  //////////////////////////////////////////////////////////////////////////////

  bool isEmpty() const noexcept { return _json == nullptr; }

  //////////////////////////////////////////////////////////////////////////////
  /// @brief converts the Json recursively into a string.
  //////////////////////////////////////////////////////////////////////////////

  std::string toString() const {
    if (_json != nullptr) {
      return JsonHelper::toString(_json);
    }
    return std::string("");
  }

  //////////////////////////////////////////////////////////////////////////////
  /// @brief appends JSON to a string buffer
  //////////////////////////////////////////////////////////////////////////////

  void dump(arangodb::basics::StringBuffer& buffer) const {
    if (_json != nullptr) {
      int res = TRI_StringifyJson(buffer.stringBuffer(), _json);

      if (res != TRI_ERROR_NO_ERROR) {
        THROW_ARANGO_EXCEPTION(TRI_ERROR_OUT_OF_MEMORY);
      }
    }
  }

 private:
  //////////////////////////////////////////////////////////////////////////////
  /// @brief the actual TRI_json_t*
  //////////////////////////////////////////////////////////////////////////////

  TRI_json_t* _json;

  //////////////////////////////////////////////////////////////////////////////
  /// @brief the memory zone used
  //////////////////////////////////////////////////////////////////////////////

  TRI_memory_zone_id_t _zone;

  //////////////////////////////////////////////////////////////////////////////
  /// @brief flag, whether we automatically free the TRI_json_t*.
  //////////////////////////////////////////////////////////////////////////////

  autofree_e _autofree;
};
}
}

std::ostream& operator<<(std::ostream&, arangodb::basics::Json const*);
std::ostream& operator<<(std::ostream&, arangodb::basics::Json const&);
std::ostream& operator<<(std::ostream&, TRI_json_t const*);
std::ostream& operator<<(std::ostream&, TRI_json_t const&);

#endif
