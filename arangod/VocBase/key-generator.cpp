////////////////////////////////////////////////////////////////////////////////
/// @brief collection key generators
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

#include "key-generator.h"

#include "BasicsC/conversions.h"
#include "BasicsC/json.h"
#include "BasicsC/logging.h"
#include "BasicsC/tri-strings.h"
#include "BasicsC/voc-errors.h"
#include "Basics/StringUtils.h"

#include "VocBase/vocbase.h"

// -----------------------------------------------------------------------------
// --SECTION--                                                     KEY GENERATOR 
// -----------------------------------------------------------------------------

// -----------------------------------------------------------------------------
// --SECTION--                                                     private types
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @brief available key generators
////////////////////////////////////////////////////////////////////////////////

typedef enum {
  TYPE_UNKNOWN       = 0,
  TYPE_TRADITIONAL   = 1,
  TYPE_AUTOINCREMENT = 2
}
generator_type_e;

// -----------------------------------------------------------------------------
// --SECTION--                                                  helper functions
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @brief validate a key
////////////////////////////////////////////////////////////////////////////////

static bool ValidateKey (char const* key) {
  char const* p = key;

  while (1) {
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
/// @brief validate a numeric key
////////////////////////////////////////////////////////////////////////////////

static bool ValidateNumericKey (char const* key) {
  char const* p = key;

  while (1) {
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

// -----------------------------------------------------------------------------
// --SECTION--                                        SPECIALIZED KEY GENERATORS
// -----------------------------------------------------------------------------

// -----------------------------------------------------------------------------
// --SECTION--                                         TRADITIONAL KEY GENERATOR
// -----------------------------------------------------------------------------

// -----------------------------------------------------------------------------
// --SECTION--                                                     private types
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @brief traditional keygen private data
////////////////////////////////////////////////////////////////////////////////

typedef struct traditional_keygen_s {
  bool    _allowUserKeys; // allow keys supplied by user?
}
traditional_keygen_t;

////////////////////////////////////////////////////////////////////////////////
/// @brief name of traditional key generator
////////////////////////////////////////////////////////////////////////////////

static const char* TraditionalName = "traditional";

// -----------------------------------------------------------------------------
// --SECTION--                                                 private functions
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @brief initialise the traditional key generator
////////////////////////////////////////////////////////////////////////////////

static int TraditionalInit (TRI_key_generator_t* generator,
                            TRI_json_t const* options) {
  traditional_keygen_t* data = static_cast<traditional_keygen_t*>(TRI_Allocate(TRI_UNKNOWN_MEM_ZONE, sizeof(traditional_keygen_t), false));

  if (data == nullptr) {
    return TRI_ERROR_OUT_OF_MEMORY;
  }

  // defaults
  data->_allowUserKeys = true;

  if (options != nullptr) {
    TRI_json_t* option;

    option = TRI_LookupArrayJson(options, "allowUserKeys");
    if (option != nullptr && option->_type == TRI_JSON_BOOLEAN) {
      data->_allowUserKeys = option->_value._boolean;
    }
  }

  generator->_data = static_cast<void*>(data);

  LOG_TRACE("created traditional key-generator with options (allowUserKeys: %d)",
            (int) data->_allowUserKeys);

  return TRI_ERROR_NO_ERROR;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief free the traditional key generator
////////////////////////////////////////////////////////////////////////////////

static void TraditionalFree (TRI_key_generator_t* generator) {
  traditional_keygen_t* data = static_cast<traditional_keygen_t*>(generator->_data);

  if (data != nullptr) {
    TRI_Free(TRI_UNKNOWN_MEM_ZONE, data);
  }
}

////////////////////////////////////////////////////////////////////////////////
/// @brief create a new document key
////////////////////////////////////////////////////////////////////////////////

static std::string TraditionalGenerateKey (TRI_key_generator_t* generator,
                                           TRI_voc_tick_t revision) {
  traditional_keygen_t* data = static_cast<traditional_keygen_t*>(generator->_data);
  assert(data != nullptr);

  // user has not specified a key, generate one based on tick
  return triagens::basics::StringUtils::itoa(revision);
}

////////////////////////////////////////////////////////////////////////////////
/// @brief validate a document key
////////////////////////////////////////////////////////////////////////////////

static int TraditionalValidateKey (TRI_key_generator_t const* generator,
                                   std::string const& key) {
  traditional_keygen_t* data = static_cast<traditional_keygen_t*>(generator->_data);
  assert(data != nullptr);

  // user has specified a key
  if (! key.empty() && ! data->_allowUserKeys) {
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

  // validate user-supplied key
  if (! ValidateKey(key.c_str())) {
    return TRI_ERROR_ARANGO_DOCUMENT_KEY_BAD;
  }

  return TRI_ERROR_NO_ERROR;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief return a JSON representation of the key generator
////////////////////////////////////////////////////////////////////////////////

static TRI_json_t* TraditionalToJson (TRI_key_generator_t const* generator) {
  traditional_keygen_t* data = static_cast<traditional_keygen_t*>(generator->_data);
  assert(data != nullptr);

  TRI_json_t* json = TRI_CreateArrayJson(TRI_CORE_MEM_ZONE);

  if (json != nullptr) {
    TRI_Insert3ArrayJson(TRI_CORE_MEM_ZONE, json, "type", TRI_CreateStringCopyJson(TRI_CORE_MEM_ZONE, TraditionalName));
    TRI_Insert3ArrayJson(TRI_CORE_MEM_ZONE, json, "allowUserKeys", TRI_CreateBooleanJson(TRI_CORE_MEM_ZONE, data->_allowUserKeys));
  }

  return json;
}

// -----------------------------------------------------------------------------
// --SECTION--                                      AUTO-INCREMENT KEY GENERATOR
// -----------------------------------------------------------------------------

// -----------------------------------------------------------------------------
// --SECTION--                                                     private types
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @brief autoincrement keygen private data
////////////////////////////////////////////////////////////////////////////////

typedef struct autoincrement_keygen_s {
  uint64_t    _lastValue;     // last value assigned
  uint64_t    _offset;        // start value
  uint64_t    _increment;     // increment value
  bool        _allowUserKeys; // allow keys supplied by user?

  TRI_mutex_t _lock;
}
autoincrement_keygen_t;

////////////////////////////////////////////////////////////////////////////////
/// @brief name of auto-increment key generator
////////////////////////////////////////////////////////////////////////////////

static const char* AutoIncrementName = "autoincrement";

// -----------------------------------------------------------------------------
// --SECTION--                                                 private functions
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @brief initialise the autoincrement key generator
////////////////////////////////////////////////////////////////////////////////

static int AutoIncrementInit (TRI_key_generator_t* generator,
                              TRI_json_t const* options) {
  autoincrement_keygen_t* data = static_cast<autoincrement_keygen_t*>(TRI_Allocate(TRI_UNKNOWN_MEM_ZONE, sizeof(autoincrement_keygen_t), false));

  if (data == nullptr) {
    return TRI_ERROR_OUT_OF_MEMORY;
  }

  // defaults
  data->_allowUserKeys = true;
  data->_lastValue     = 0;
  data->_offset        = 0;
  data->_increment     = 1;

  if (options != nullptr) {
    TRI_json_t* option;

    option = TRI_LookupArrayJson(options, "allowUserKeys");

    if (TRI_IsBooleanJson(option)) {
      data->_allowUserKeys = option->_value._boolean;
    }

    option = TRI_LookupArrayJson(options, "increment");

    if (TRI_IsNumberJson(option)) {
      data->_increment = (uint64_t) option->_value._number;

      if (data->_increment == 0 || data->_increment >= (1ULL << 16)) {
        TRI_Free(TRI_UNKNOWN_MEM_ZONE, data);

        return TRI_ERROR_ARANGO_INVALID_KEY_GENERATOR;
      }
    }

    option = TRI_LookupArrayJson(options, "offset");

    if (TRI_IsNumberJson(option)) {
      data->_offset = (uint64_t) option->_value._number;

      if (data->_offset >= UINT64_MAX) {
        TRI_Free(TRI_UNKNOWN_MEM_ZONE, data);

        return TRI_ERROR_ARANGO_INVALID_KEY_GENERATOR;
      }
    }
  }

  TRI_InitMutex(&data->_lock);

  generator->_data = static_cast<void*>(data);

  LOG_TRACE("created autoincrement key-generator with options (allowUserKeys: %d, increment: %llu, offset: %llu)",
            (int) data->_allowUserKeys,
            (unsigned long long) data->_increment,
            (unsigned long long) data->_offset);

  return TRI_ERROR_NO_ERROR;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief free the autoincrement key generator
////////////////////////////////////////////////////////////////////////////////

static void AutoIncrementFree (TRI_key_generator_t* generator) {
  autoincrement_keygen_t* data = static_cast<autoincrement_keygen_t*>(generator->_data);

  if (data != nullptr) {
    TRI_DestroyMutex(&data->_lock);
    TRI_Free(TRI_UNKNOWN_MEM_ZONE, data);
  }
}

////////////////////////////////////////////////////////////////////////////////
/// @brief generate the next auto-increment value based on 3 parameters:
/// - last (highest) value assigned (can be 0)
/// - increment
/// - offset (start value)
////////////////////////////////////////////////////////////////////////////////

static uint64_t AutoIncrementNext (uint64_t lastValue,
                                   uint64_t increment,
                                   uint64_t offset) {
  if (lastValue < offset) {
    return offset;
  }

  uint64_t next = lastValue + increment - ((lastValue - offset) % increment);

  // TODO: check if we can remove the following if
  if (next < offset) {
    next = offset;
  }

  return next;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief generate a new key
////////////////////////////////////////////////////////////////////////////////

static std::string AutoIncrementGenerateKey (TRI_key_generator_t* generator,
                                             TRI_voc_tick_t revision) {
  autoincrement_keygen_t* data = static_cast<autoincrement_keygen_t*>(generator->_data);
  assert(data != nullptr);
   
  TRI_LockMutex(&data->_lock);
   
  // user has not specified a key, generate one based on algorithm
  uint64_t keyValue = AutoIncrementNext(data->_lastValue, data->_increment, data->_offset);

  // bounds and sanity checks
  if (keyValue == UINT64_MAX || keyValue < data->_lastValue) {
    TRI_UnlockMutex(&data->_lock);
    return "";
  }

  assert(keyValue > data->_lastValue);
  // update our last value
  data->_lastValue = keyValue;

  TRI_UnlockMutex(&data->_lock);

  return triagens::basics::StringUtils::itoa(keyValue);
}

////////////////////////////////////////////////////////////////////////////////
/// @brief validate a key
////////////////////////////////////////////////////////////////////////////////

static int AutoIncrementValidateKey (TRI_key_generator_t const* generator,
                                     std::string const& key) {
  autoincrement_keygen_t* data = static_cast<autoincrement_keygen_t*>(generator->_data);
  assert(data != nullptr);

  if (! key.empty() && ! data->_allowUserKeys) {
    // we do not allow user-generated keys
    return TRI_ERROR_ARANGO_DOCUMENT_KEY_UNEXPECTED;
  }

  if (key.empty()) {
    return TRI_ERROR_ARANGO_DOCUMENT_KEY_BAD;
  }

  if (key.size() > TRI_VOC_KEY_MAX_LENGTH) {
    // user key is too long
    return TRI_ERROR_ARANGO_DOCUMENT_KEY_BAD;
  }

  // validate user-supplied key
  if (! ValidateNumericKey(key.c_str())) {
    return TRI_ERROR_ARANGO_DOCUMENT_KEY_BAD;
  }

  uint64_t intValue = triagens::basics::StringUtils::uint64(key);

  if (intValue > data->_lastValue) {
    // update our last value
    data->_lastValue = intValue;
  }

  return TRI_ERROR_NO_ERROR;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief track a key while a collection is opened
/// this function is used to update the _lastValue value
////////////////////////////////////////////////////////////////////////////////

static void AutoIncrementTrack (TRI_key_generator_t* generator,
                                TRI_voc_key_t key) {
  autoincrement_keygen_t* data = static_cast<autoincrement_keygen_t*>(generator->_data);
  assert(data != nullptr);

  // check the numeric key part
  uint64_t value = TRI_UInt64String(key);

  if (value > data->_lastValue) {
    // and update our last value
    data->_lastValue = value;
  }
}

////////////////////////////////////////////////////////////////////////////////
/// @brief return a JSON representation of the key generator
////////////////////////////////////////////////////////////////////////////////

static TRI_json_t* AutoIncrementToJson (TRI_key_generator_t const* generator) {
  autoincrement_keygen_t* data = static_cast<autoincrement_keygen_t*>(generator->_data);
  assert(data != nullptr);

  TRI_json_t* json = TRI_CreateArrayJson(TRI_CORE_MEM_ZONE);

  if (json != nullptr) {
    TRI_Insert3ArrayJson(TRI_CORE_MEM_ZONE, json, "type", TRI_CreateStringCopyJson(TRI_CORE_MEM_ZONE, AutoIncrementName));
    TRI_Insert3ArrayJson(TRI_CORE_MEM_ZONE, json, "allowUserKeys", TRI_CreateBooleanJson(TRI_CORE_MEM_ZONE, data->_allowUserKeys));
    TRI_Insert3ArrayJson(TRI_CORE_MEM_ZONE, json, "offset", TRI_CreateNumberJson(TRI_CORE_MEM_ZONE, (double) data->_offset));
    TRI_Insert3ArrayJson(TRI_CORE_MEM_ZONE, json, "increment", TRI_CreateNumberJson(TRI_CORE_MEM_ZONE, (double) data->_increment));
  }

  return json;
}

// -----------------------------------------------------------------------------
// --SECTION--                                       GENERAL GENERATOR FUNCTIONS
// -----------------------------------------------------------------------------

// -----------------------------------------------------------------------------
// --SECTION--                                                 private functions
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @brief get the generator type from JSON
////////////////////////////////////////////////////////////////////////////////

static generator_type_e GeneratorType (TRI_json_t const* parameters) {
  if (! TRI_IsArrayJson(parameters)) {
    return TYPE_TRADITIONAL;
  }

  TRI_json_t const* type = TRI_LookupArrayJson(parameters, "type");

  if (! TRI_IsStringJson(type)) {
    return TYPE_TRADITIONAL;
  }

  const char* typeName = type->_value._string.data;

  if (TRI_CaseEqualString(typeName, TraditionalName)) {
    return TYPE_TRADITIONAL;
  }

  if (TRI_CaseEqualString(typeName, AutoIncrementName)) {
    return TYPE_AUTOINCREMENT;
  }

  // error
  return TYPE_UNKNOWN;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief create a new generator
////////////////////////////////////////////////////////////////////////////////

static TRI_key_generator_t* CreateGenerator (TRI_json_t const* parameters) {
  generator_type_e type = GeneratorType(parameters);

  if (type == TYPE_UNKNOWN) {
    return nullptr;
  }

  TRI_key_generator_t* generator = static_cast<TRI_key_generator_t*>(TRI_Allocate(TRI_UNKNOWN_MEM_ZONE, sizeof(TRI_key_generator_t), false));

  if (generator == nullptr) {
    return nullptr;
  }

  generator->_data         = nullptr;

  if (type == TYPE_TRADITIONAL) {
    generator->init        = &TraditionalInit;
    generator->generateKey = &TraditionalGenerateKey;
    generator->validateKey = &TraditionalValidateKey;
    generator->trackKey    = nullptr;
    generator->free        = &TraditionalFree;
    generator->toJson      = &TraditionalToJson;
  }
  else if (type == TYPE_AUTOINCREMENT) {
    generator->init        = &AutoIncrementInit;
    generator->generateKey = &AutoIncrementGenerateKey;
    generator->validateKey = &AutoIncrementValidateKey;
    generator->trackKey    = &AutoIncrementTrack;
    generator->free        = &AutoIncrementFree;
    generator->toJson      = &AutoIncrementToJson;
  }

  return generator;
}

// -----------------------------------------------------------------------------
// --SECTION--                                        constructors / destructors
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @brief create a key generator
////////////////////////////////////////////////////////////////////////////////

int TRI_CreateKeyGenerator (TRI_json_t const* parameters,
                            TRI_key_generator_t** dst) {
  *dst = NULL;

  TRI_json_t const* options = nullptr;

  if (TRI_IsArrayJson(parameters)) {
    options = parameters;
  }

  TRI_key_generator_t* generator = CreateGenerator(options);

  if (generator == NULL) {
    return TRI_ERROR_ARANGO_INVALID_KEY_GENERATOR;
  }

  int res = generator->init(generator, options);

  if (res != TRI_ERROR_NO_ERROR) {
    TRI_FreeKeyGenerator(generator);

    return res;
  }

  *dst = generator;

  return TRI_ERROR_NO_ERROR;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief free a key generator
////////////////////////////////////////////////////////////////////////////////

void TRI_FreeKeyGenerator (TRI_key_generator_t* generator) {
  if (generator->free != nullptr) {
    generator->free(generator);
  }

  TRI_Free(TRI_UNKNOWN_MEM_ZONE, generator);
}

////////////////////////////////////////////////////////////////////////////////
/// @brief validate a key
////////////////////////////////////////////////////////////////////////////////

bool TRI_ValidateKeyKeyGenerator (char const* key) {
  return ValidateKey(key);
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
  return ValidateKey(p);
}

// Local Variables:
// mode: outline-minor
// outline-regexp: "/// @brief\\|/// {@inheritDoc}\\|/// @addtogroup\\|/// @page\\|// --SECTION--\\|/// @\\}"
// End:

