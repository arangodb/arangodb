////////////////////////////////////////////////////////////////////////////////
/// @brief collection key generators
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

#include "key-generator.h"

#include "Basics/conversions.h"
#include "Basics/json.h"
#include "Basics/logging.h"
#include "Basics/tri-strings.h"
#include "Basics/voc-errors.h"
#include "Basics/MutexLocker.h"
#include "Basics/StringUtils.h"

#include "VocBase/vocbase.h"

// -----------------------------------------------------------------------------
// --SECTION--                                             GENERAL KEY GENERATOR
// -----------------------------------------------------------------------------

// -----------------------------------------------------------------------------
// --SECTION--                                        constructors / destructors
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @brief create the key enerator
////////////////////////////////////////////////////////////////////////////////

KeyGenerator::KeyGenerator (bool allowUserKeys)
  : _allowUserKeys(allowUserKeys) {
}

////////////////////////////////////////////////////////////////////////////////
/// @brief destroy the key enerator
////////////////////////////////////////////////////////////////////////////////

KeyGenerator::~KeyGenerator () {
}

// -----------------------------------------------------------------------------
// --SECTION--                                                  public functions
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @brief get the generator type from JSON
////////////////////////////////////////////////////////////////////////////////

KeyGenerator::GeneratorType KeyGenerator::generatorType (TRI_json_t const* parameters) {
  if (! TRI_IsObjectJson(parameters)) {
    return KeyGenerator::TYPE_TRADITIONAL;
  }

  TRI_json_t const* type = TRI_LookupObjectJson(parameters, "type");

  if (! TRI_IsStringJson(type)) {
    return KeyGenerator::TYPE_TRADITIONAL;
  }

  char const* typeName = type->_value._string.data;

  if (TRI_CaseEqualString(typeName, TraditionalKeyGenerator::name().c_str())) {
    return KeyGenerator::TYPE_TRADITIONAL;
  }

  if (TRI_CaseEqualString(typeName, AutoIncrementKeyGenerator::name().c_str())) {
    return KeyGenerator::TYPE_AUTOINCREMENT;
  }

  // error
  return KeyGenerator::TYPE_UNKNOWN;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief create a key generator based on the options specified
////////////////////////////////////////////////////////////////////////////////

KeyGenerator* KeyGenerator::factory (TRI_json_t const* options) {
  KeyGenerator::GeneratorType type;

  bool const readOptions = TRI_IsObjectJson(options);

  if (readOptions) {
    type = generatorType(options);
  }
  else {
    type = TYPE_TRADITIONAL;
  }

  if (type == TYPE_UNKNOWN) {
    return nullptr;
  }

  bool allowUserKeys = true;

  if (readOptions) {
    TRI_json_t* option = TRI_LookupObjectJson(options, "allowUserKeys");

    if (TRI_IsBooleanJson(option)) {
      allowUserKeys = option->_value._boolean;
    }
  }

  if (type == TYPE_TRADITIONAL) {
    return new TraditionalKeyGenerator(allowUserKeys);
  }

  else if (type == TYPE_AUTOINCREMENT) {
    uint64_t offset = 0;
    uint64_t increment = 1;

    if (readOptions) {
      TRI_json_t* option;

      option = TRI_LookupObjectJson(options, "increment");

      if (TRI_IsNumberJson(option)) {
        if (option->_value._number <= 0.0) {
          // negative or 0 offset is not allowed
          return nullptr;
        }

        increment = static_cast<uint64_t>(option->_value._number);

        if (increment == 0 || increment >= (1ULL << 16)) {
          return nullptr;
        }
      }
    
      option = TRI_LookupObjectJson(options, "offset");

      if (TRI_IsNumberJson(option)) {
        if (option->_value._number < 0.0) {
          return nullptr;
        }
       
        offset = static_cast<uint64_t>(option->_value._number);

        if (offset >= UINT64_MAX) {
          return nullptr;
        }
      }
    }

    return new AutoIncrementKeyGenerator(allowUserKeys, offset, increment);
  }

  return nullptr;
}

// -----------------------------------------------------------------------------
// --SECTION--                                               protected functions
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @brief check global key attributes
////////////////////////////////////////////////////////////////////////////////

int KeyGenerator::globalCheck (std::string const& key,
                               bool isRestore) {
  // user has specified a key
  if (! key.empty() && ! _allowUserKeys && ! isRestore) {
    // we do not allow user-generated keys
    return TRI_ERROR_ARANGO_DOCUMENT_KEY_UNEXPECTED;
  }

  if (key.empty()) {
    // user key is empty
    return TRI_ERROR_ARANGO_DOCUMENT_KEY_BAD;
  }

  if (key.size() > TRI_VOC_KEY_MAX_LENGTH) {
    // user key is too long
    return TRI_ERROR_ARANGO_DOCUMENT_KEY_BAD;
  }

  return TRI_ERROR_NO_ERROR;
}

// -----------------------------------------------------------------------------
// --SECTION--                                         TRADITIONAL KEY GENERATOR
// -----------------------------------------------------------------------------

// -----------------------------------------------------------------------------
// --SECTION--                                        constructors / destructors
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @brief create the key enerator
////////////////////////////////////////////////////////////////////////////////

TraditionalKeyGenerator::TraditionalKeyGenerator (bool allowUserKeys)
  : KeyGenerator(allowUserKeys) {
}

////////////////////////////////////////////////////////////////////////////////
/// @brief destroy the key enerator
////////////////////////////////////////////////////////////////////////////////

TraditionalKeyGenerator::~TraditionalKeyGenerator () {
}

// -----------------------------------------------------------------------------
// --SECTION--                                                  public functions
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @brief validate a key
////////////////////////////////////////////////////////////////////////////////

bool TraditionalKeyGenerator::validateKey (char const* key) {
  char const* p = key;

  while (true) {
    char c = *p;

    if (c == '\0') {
      return ((p - key) > 0) &&
             ((p - key) <= TRI_VOC_KEY_MAX_LENGTH);
    }

    if ((c >= 'a' && c <= 'z') ||
        (c >= 'A' && c <= 'Z') ||
        (c >= '0' && c <= '9') ||
         c == '_' ||
         c == ':' ||
         c == '-') {
      ++p;
      continue;
    }

    return false;
  }
}

////////////////////////////////////////////////////////////////////////////////
/// @brief generate a key
////////////////////////////////////////////////////////////////////////////////

std::string TraditionalKeyGenerator::generate (TRI_voc_tick_t tick) {
  return triagens::basics::StringUtils::itoa(tick);
}

////////////////////////////////////////////////////////////////////////////////
/// @brief validate a key
////////////////////////////////////////////////////////////////////////////////

int TraditionalKeyGenerator::validate (std::string const& key,
                                       bool isRestore) {
  int res = globalCheck(key, isRestore);

  if (res != TRI_ERROR_NO_ERROR) {
    return res;
  }

  // validate user-supplied key
  if (! validateKey(key.c_str())) {
    return TRI_ERROR_ARANGO_DOCUMENT_KEY_BAD;
  }

  return TRI_ERROR_NO_ERROR;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief track usage of a key
////////////////////////////////////////////////////////////////////////////////

void TraditionalKeyGenerator::track (TRI_voc_key_t) {
}

////////////////////////////////////////////////////////////////////////////////
/// @brief create a JSON representation of the generator
////////////////////////////////////////////////////////////////////////////////

TRI_json_t* TraditionalKeyGenerator::toJson (TRI_memory_zone_t* zone) const {
  TRI_json_t* json = TRI_CreateObjectJson(zone);

  if (json != nullptr) {
    TRI_Insert3ObjectJson(zone, json, "type", TRI_CreateStringCopyJson(zone, name().c_str()));
    TRI_Insert3ObjectJson(zone, json, "allowUserKeys", TRI_CreateBooleanJson(zone, _allowUserKeys));
  }

  return json;
}

// -----------------------------------------------------------------------------
// --SECTION--                                      AUTO-INCREMENT KEY GENERATOR
// -----------------------------------------------------------------------------

// -----------------------------------------------------------------------------
// --SECTION--                                        constructors / destructors
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @brief create the generator
////////////////////////////////////////////////////////////////////////////////

AutoIncrementKeyGenerator::AutoIncrementKeyGenerator (bool allowUserKeys,
                                                      uint64_t offset,
                                                      uint64_t increment)
  : KeyGenerator(allowUserKeys),
    _lastValue(0),
    _offset(offset),
    _increment(increment) {
}

////////////////////////////////////////////////////////////////////////////////
/// @brief destroy the generator
////////////////////////////////////////////////////////////////////////////////

AutoIncrementKeyGenerator::~AutoIncrementKeyGenerator () {
}

// -----------------------------------------------------------------------------
// --SECTION--                                                  public functions
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @brief validate a numeric key
////////////////////////////////////////////////////////////////////////////////

bool AutoIncrementKeyGenerator::validateKey (char const* key) {
  char const* p = key;

  while (true) {
    char c = *p;

    if (c == '\0') {
      return ((p - key) > 0) &&
             ((p - key) <= TRI_VOC_KEY_MAX_LENGTH);
    }

    if (c >= '0' && c <= '9') {
      ++p;
      continue;
    }

    return false;
  }
}

////////////////////////////////////////////////////////////////////////////////
/// @brief generate a key
////////////////////////////////////////////////////////////////////////////////

std::string AutoIncrementKeyGenerator::generate (TRI_voc_tick_t tick) {
  uint64_t keyValue;

  {
    MUTEX_LOCKER(_lock);

    // user has not specified a key, generate one based on algorithm
    if (_lastValue < _offset) {
      keyValue = _offset;
    }
    else {
      uint64_t next = _lastValue + _increment - ((_lastValue - _offset) % _increment);

      // TODO: check if we can remove the following if
      if (next < _offset) {
        next = _offset;
      }

      keyValue = next;
    }

    // bounds and sanity checks
    if (keyValue == UINT64_MAX || keyValue < _lastValue) {
      return "";
    }

    TRI_ASSERT(keyValue > _lastValue);
    // update our last value
    _lastValue = keyValue;
  }

  return triagens::basics::StringUtils::itoa(keyValue);
}

////////////////////////////////////////////////////////////////////////////////
/// @brief validate a key
////////////////////////////////////////////////////////////////////////////////

int AutoIncrementKeyGenerator::validate (std::string const& key,
                                         bool isRestore) {
  int res = globalCheck(key, isRestore);

  if (res != TRI_ERROR_NO_ERROR) {
    return res;
  }

  // validate user-supplied key
  if (! validateKey(key.c_str())) {
    return TRI_ERROR_ARANGO_DOCUMENT_KEY_BAD;
  }

  uint64_t intValue = triagens::basics::StringUtils::uint64(key);

  if (intValue > _lastValue) {
    MUTEX_LOCKER(_lock);
    // update our last value
    _lastValue = intValue;
  }

  return TRI_ERROR_NO_ERROR;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief track usage of a key
////////////////////////////////////////////////////////////////////////////////

void AutoIncrementKeyGenerator::track (TRI_voc_key_t key) {
  // check the numeric key part
  uint64_t value = TRI_UInt64String(key);

  if (value > _lastValue) {
    // and update our last value
    _lastValue = value;
  }
}

////////////////////////////////////////////////////////////////////////////////
/// @brief create a JSON representation of the generator
////////////////////////////////////////////////////////////////////////////////

TRI_json_t* AutoIncrementKeyGenerator::toJson (TRI_memory_zone_t* zone) const {
  TRI_json_t* json = TRI_CreateObjectJson(zone);

  if (json != nullptr) {
    TRI_Insert3ObjectJson(zone, json, "type", TRI_CreateStringCopyJson(zone, name().c_str()));
    TRI_Insert3ObjectJson(zone, json, "allowUserKeys", TRI_CreateBooleanJson(zone, _allowUserKeys));
    TRI_Insert3ObjectJson(zone, json, "offset", TRI_CreateNumberJson(zone, (double) _offset));
    TRI_Insert3ObjectJson(zone, json, "increment", TRI_CreateNumberJson(zone, (double) _increment));
  }

  return json;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief validate a document id (collection name + / + document key)
////////////////////////////////////////////////////////////////////////////////

bool TRI_ValidateDocumentIdKeyGenerator (char const* key,
                                         size_t* split) {
  char const* p = key;
  char c = *p;

  // extract collection name
  if (! (c == '_' || (c >= '0' && c <= '9') || (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z'))) {
    return false;
  }

  ++p;

  while (1) {
    c = *p;

    if (c == '_' || c == '-' || (c >= '0' && c <= '9') || (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z')) {
      ++p;
      continue;
    }

    if (c == '/') {
      break;
    }

    return false;
  }

  if (p - key > TRI_COL_NAME_LENGTH) {
    return false;
  }

  // store split position
  *split = p - key;
  ++p;

  // validate document key
  return TraditionalKeyGenerator::validateKey(p);
}

// -----------------------------------------------------------------------------
// --SECTION--                                                       END-OF-FILE
// -----------------------------------------------------------------------------

// Local Variables:
// mode: outline-minor
// outline-regexp: "/// @brief\\|/// {@inheritDoc}\\|/// @page\\|// --SECTION--\\|/// @\\}"
// End:
