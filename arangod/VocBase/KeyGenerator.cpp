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

#include "KeyGenerator.h"
#include "Basics/conversions.h"
#include "Basics/MutexLocker.h"
#include "Basics/StringUtils.h"
#include "Basics/tri-strings.h"
#include "Basics/VelocyPackHelper.h"
#include "Basics/voc-errors.h"
#include "VocBase/vocbase.h"

#include <array>

using namespace arangodb::basics;

////////////////////////////////////////////////////////////////////////////////
/// @brief lookup table for key checks
////////////////////////////////////////////////////////////////////////////////

std::array<bool, 256> KeyGenerator::LookupTable;

////////////////////////////////////////////////////////////////////////////////
/// @brief initialize the lookup table for key checks
////////////////////////////////////////////////////////////////////////////////

void KeyGenerator::Initialize() {
  for (int c = 0; c < 256; ++c) {
    if ((c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') ||
        (c >= '0' && c <= '9') || c == '_' || c == ':' || c == '-' ||
        c == '@' || c == '.' || c == '(' || c == ')' || c == '+' || c == ',' ||
        c == '=' || c == ';' || c == '$' || c == '!' || c == '*' || c == '\'' ||
        c == '%') {
      LookupTable[c] = true;
    } else {
      LookupTable[c] = false;
    }
  }
}

////////////////////////////////////////////////////////////////////////////////
/// @brief create the key enerator
////////////////////////////////////////////////////////////////////////////////

KeyGenerator::KeyGenerator(bool allowUserKeys)
    : _allowUserKeys(allowUserKeys) {}

////////////////////////////////////////////////////////////////////////////////
/// @brief destroy the key enerator
////////////////////////////////////////////////////////////////////////////////

KeyGenerator::~KeyGenerator() {}

////////////////////////////////////////////////////////////////////////////////
/// @brief get the generator type from VelocyPack
////////////////////////////////////////////////////////////////////////////////

KeyGenerator::GeneratorType KeyGenerator::generatorType(
    VPackSlice const& parameters) {
  if (!parameters.isObject()) {
    return KeyGenerator::TYPE_TRADITIONAL;
  }
  VPackSlice const type = parameters.get("type");

  if (!type.isString()) {
    return KeyGenerator::TYPE_TRADITIONAL;
  }

  std::string const typeName =
      arangodb::basics::StringUtils::tolower(type.copyString());

  if (typeName == TraditionalKeyGenerator::name()) {
    return KeyGenerator::TYPE_TRADITIONAL;
  }

  if (typeName == AutoIncrementKeyGenerator::name()) {
    return KeyGenerator::TYPE_AUTOINCREMENT;
  }

  // error
  return KeyGenerator::TYPE_UNKNOWN;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief create a key generator based on the options specified
////////////////////////////////////////////////////////////////////////////////

KeyGenerator* KeyGenerator::factory(VPackSlice const& options) {
  KeyGenerator::GeneratorType type;

  bool const readOptions = options.isObject();

  if (readOptions) {
    type = generatorType(options);
  } else {
    type = TYPE_TRADITIONAL;
  }

  if (type == TYPE_UNKNOWN) {
    return nullptr;
  }

  bool allowUserKeys = true;

  if (readOptions) {
    // Change allowUserKeys only if it is a boolean value, otherwise use default
    allowUserKeys = arangodb::basics::VelocyPackHelper::getBooleanValue(
        options, "allowUserKeys", allowUserKeys);
  }

  if (type == TYPE_TRADITIONAL) {
    return new TraditionalKeyGenerator(allowUserKeys);
  }

  else if (type == TYPE_AUTOINCREMENT) {
    uint64_t offset = 0;
    uint64_t increment = 1;

    if (readOptions) {
      VPackSlice const incrementSlice = options.get("increment");

      if (incrementSlice.isNumber()) {
        if (incrementSlice.isDouble()) {
          if (incrementSlice.getDouble() <= 0.0) {
            // negative or 0 increment is not allowed
            return nullptr;
          }
        }

        increment = incrementSlice.getNumericValue<uint64_t>();

        if (increment == 0 || increment >= (1ULL << 16)) {
          return nullptr;
        }
      }

      VPackSlice const offsetSlice = options.get("offset");

      if (offsetSlice.isNumber()) {
        if (offsetSlice.isDouble()) {
          if (offsetSlice.getDouble() < 0.0) {
            // negative or 0 offset is not allowed
            return nullptr;
          }
        }
        offset = offsetSlice.getNumericValue<uint64_t>();

        if (offset >= UINT64_MAX) {
          return nullptr;
        }
      }
    }

    return new AutoIncrementKeyGenerator(allowUserKeys, offset, increment);
  }

  return nullptr;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief check global key attributes
////////////////////////////////////////////////////////////////////////////////

int KeyGenerator::globalCheck(std::string const& key, bool isRestore) {
  // user has specified a key
  if (!key.empty() && !_allowUserKeys && !isRestore) {
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

//////////////////////////////////////////////////////////////////////////////
/// @brief return a VelocyPack representation of the generator
///        Not virtual because this is identical for all of them
//////////////////////////////////////////////////////////////////////////////

std::shared_ptr<VPackBuilder> KeyGenerator::toVelocyPack() const {
  auto builder = std::make_shared<VPackBuilder>();
  toVelocyPack(*builder);
  builder->close();
  return builder;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief create the key enerator
////////////////////////////////////////////////////////////////////////////////

TraditionalKeyGenerator::TraditionalKeyGenerator(bool allowUserKeys)
    : KeyGenerator(allowUserKeys) {}

////////////////////////////////////////////////////////////////////////////////
/// @brief destroy the key enerator
////////////////////////////////////////////////////////////////////////////////

TraditionalKeyGenerator::~TraditionalKeyGenerator() {}

////////////////////////////////////////////////////////////////////////////////
/// @brief validate a key
////////////////////////////////////////////////////////////////////////////////

bool TraditionalKeyGenerator::validateKey(char const* key) {
  unsigned char const* p = reinterpret_cast<unsigned char const*>(key);
  unsigned char const* s = p;

  while (true) {
    unsigned char c = *p;

    if (c == '\0') {
      return ((p - s) > 0) && ((p - s) <= TRI_VOC_KEY_MAX_LENGTH);
    }

    if (LookupTable[c]) {
      ++p;
      continue;
    }

    return false;
  }
}

////////////////////////////////////////////////////////////////////////////////
/// @brief generate a key
////////////////////////////////////////////////////////////////////////////////

std::string TraditionalKeyGenerator::generate(TRI_voc_tick_t tick) {
  return arangodb::basics::StringUtils::itoa(tick);
}

////////////////////////////////////////////////////////////////////////////////
/// @brief validate a key
////////////////////////////////////////////////////////////////////////////////

int TraditionalKeyGenerator::validate(std::string const& key, bool isRestore) {
  int res = globalCheck(key, isRestore);

  if (res != TRI_ERROR_NO_ERROR) {
    return res;
  }

  // validate user-supplied key
  if (!validateKey(key.c_str())) {
    return TRI_ERROR_ARANGO_DOCUMENT_KEY_BAD;
  }

  return TRI_ERROR_NO_ERROR;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief track usage of a key
////////////////////////////////////////////////////////////////////////////////

void TraditionalKeyGenerator::track(TRI_voc_key_t) {}

////////////////////////////////////////////////////////////////////////////////
/// @brief create a VPack representation of the generator
////////////////////////////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////////////////////////
/// @brief create a VPack representation of the generator
////////////////////////////////////////////////////////////////////////////////

void TraditionalKeyGenerator::toVelocyPack(VPackBuilder& builder) const {
  TRI_ASSERT(!builder.isClosed());
  builder.add("type", VPackValue(name()));
  builder.add("allowUserKeys", VPackValue(_allowUserKeys));
}

////////////////////////////////////////////////////////////////////////////////
/// @brief create the generator
////////////////////////////////////////////////////////////////////////////////

AutoIncrementKeyGenerator::AutoIncrementKeyGenerator(bool allowUserKeys,
                                                     uint64_t offset,
                                                     uint64_t increment)
    : KeyGenerator(allowUserKeys),
      _lastValue(0),
      _offset(offset),
      _increment(increment) {}

////////////////////////////////////////////////////////////////////////////////
/// @brief destroy the generator
////////////////////////////////////////////////////////////////////////////////

AutoIncrementKeyGenerator::~AutoIncrementKeyGenerator() {}

////////////////////////////////////////////////////////////////////////////////
/// @brief validate a numeric key
////////////////////////////////////////////////////////////////////////////////

bool AutoIncrementKeyGenerator::validateKey(char const* key) {
  char const* p = key;

  while (true) {
    char c = *p;

    if (c == '\0') {
      return ((p - key) > 0) && ((p - key) <= TRI_VOC_KEY_MAX_LENGTH);
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

std::string AutoIncrementKeyGenerator::generate(TRI_voc_tick_t tick) {
  uint64_t keyValue;

  {
    MUTEX_LOCKER(mutexLocker, _lock);

    // user has not specified a key, generate one based on algorithm
    if (_lastValue < _offset) {
      keyValue = _offset;
    } else {
      keyValue =
          _lastValue + _increment - ((_lastValue - _offset) % _increment);
    }

    // bounds and sanity checks
    if (keyValue == UINT64_MAX || keyValue < _lastValue) {
      return "";
    }

    TRI_ASSERT(keyValue > _lastValue);
    // update our last value
    _lastValue = keyValue;
  }

  return arangodb::basics::StringUtils::itoa(keyValue);
}

////////////////////////////////////////////////////////////////////////////////
/// @brief validate a key
////////////////////////////////////////////////////////////////////////////////

int AutoIncrementKeyGenerator::validate(std::string const& key,
                                        bool isRestore) {
  int res = globalCheck(key, isRestore);

  if (res != TRI_ERROR_NO_ERROR) {
    return res;
  }

  // validate user-supplied key
  if (!validateKey(key.c_str())) {
    return TRI_ERROR_ARANGO_DOCUMENT_KEY_BAD;
  }

  uint64_t intValue = arangodb::basics::StringUtils::uint64(key);

  if (intValue > _lastValue) {
    MUTEX_LOCKER(mutexLocker, _lock);
    // update our last value
    _lastValue = intValue;
  }

  return TRI_ERROR_NO_ERROR;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief track usage of a key
////////////////////////////////////////////////////////////////////////////////

void AutoIncrementKeyGenerator::track(TRI_voc_key_t key) {
  // check the numeric key part
  uint64_t value = StringUtils::uint64(key);

  if (value > _lastValue) {
    // and update our last value
    _lastValue = value;
  }
}

////////////////////////////////////////////////////////////////////////////////
/// @brief create a VelocyPack representation of the generator
////////////////////////////////////////////////////////////////////////////////

void AutoIncrementKeyGenerator::toVelocyPack(VPackBuilder& builder) const {
  TRI_ASSERT(!builder.isClosed());
  builder.add("type", VPackValue(name()));
  builder.add("allowUserKeys", VPackValue(_allowUserKeys));
  builder.add("offset", VPackValue(_offset));
  builder.add("increment", VPackValue(_increment));
}

////////////////////////////////////////////////////////////////////////////////
/// @brief validate a document id (collection name + / + document key)
////////////////////////////////////////////////////////////////////////////////

bool TRI_ValidateDocumentIdKeyGenerator(char const* key, size_t* split) {
  char const* p = key;
  char c = *p;

  // extract collection name
  if (!(c == '_' || (c >= '0' && c <= '9') || (c >= 'a' && c <= 'z') ||
        (c >= 'A' && c <= 'Z'))) {
    return false;
  }

  ++p;

  while (1) {
    c = *p;

    if (c == '_' || c == '-' || (c >= '0' && c <= '9') ||
        (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z')) {
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
