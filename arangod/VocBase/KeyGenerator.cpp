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
#include "Basics/NumberUtils.h"
#include "Basics/StringUtils.h"
#include "Basics/tri-strings.h"
#include "Basics/VelocyPackHelper.h"
#include "Basics/voc-errors.h"
#include "VocBase/ticks.h"
#include "VocBase/vocbase.h"

#include <array>

using namespace arangodb;
using namespace arangodb::basics;

namespace {

/// @brief lookup table for key checks
static std::array<bool, 256> const keyCharLookupTable = { { 
  /* 0x00 . */ false,    /* 0x01 . */ false,    /* 0x02 . */ false,    /* 0x03 . */ false,    
  /* 0x04 . */ false,    /* 0x05 . */ false,    /* 0x06 . */ false,    /* 0x07 . */ false,    
  /* 0x08 . */ false,    /* 0x09 . */ false,    /* 0x0a . */ false,    /* 0x0b . */ false,    
  /* 0x0c . */ false,    /* 0x0d . */ false,    /* 0x0e . */ false,    /* 0x0f . */ false,    
  /* 0x10 . */ false,    /* 0x11 . */ false,    /* 0x12 . */ false,    /* 0x13 . */ false,    
  /* 0x14 . */ false,    /* 0x15 . */ false,    /* 0x16 . */ false,    /* 0x17 . */ false,    
  /* 0x18 . */ false,    /* 0x19 . */ false,    /* 0x1a . */ false,    /* 0x1b . */ false,    
  /* 0x1c . */ false,    /* 0x1d . */ false,    /* 0x1e . */ false,    /* 0x1f . */ false,    
  /* 0x20   */ false,    /* 0x21 ! */ true,     /* 0x22 " */ false,    /* 0x23 # */ false,    
  /* 0x24 $ */ true,     /* 0x25 % */ true,     /* 0x26 & */ false,    /* 0x27 ' */ true,     
  /* 0x28 ( */ true,     /* 0x29 ) */ true,     /* 0x2a * */ true,     /* 0x2b + */ true,     
  /* 0x2c , */ true,     /* 0x2d - */ true,     /* 0x2e . */ true,     /* 0x2f / */ false,    
  /* 0x30 0 */ true,     /* 0x31 1 */ true,     /* 0x32 2 */ true,     /* 0x33 3 */ true,     
  /* 0x34 4 */ true,     /* 0x35 5 */ true,     /* 0x36 6 */ true,     /* 0x37 7 */ true,     
  /* 0x38 8 */ true,     /* 0x39 9 */ true,     /* 0x3a : */ true,     /* 0x3b ; */ true,     
  /* 0x3c < */ false,    /* 0x3d = */ true,     /* 0x3e > */ false,    /* 0x3f ? */ false,    
  /* 0x40 @ */ true,     /* 0x41 A */ true,     /* 0x42 B */ true,     /* 0x43 C */ true,     
  /* 0x44 D */ true,     /* 0x45 E */ true,     /* 0x46 F */ true,     /* 0x47 G */ true,     
  /* 0x48 H */ true,     /* 0x49 I */ true,     /* 0x4a J */ true,     /* 0x4b K */ true,     
  /* 0x4c L */ true,     /* 0x4d M */ true,     /* 0x4e N */ true,     /* 0x4f O */ true,     
  /* 0x50 P */ true,     /* 0x51 Q */ true,     /* 0x52 R */ true,     /* 0x53 S */ true,     
  /* 0x54 T */ true,     /* 0x55 U */ true,     /* 0x56 V */ true,     /* 0x57 W */ true,     
  /* 0x58 X */ true,     /* 0x59 Y */ true,     /* 0x5a Z */ true,     /* 0x5b [ */ false,    
  /* 0x5c \ */ false,    /* 0x5d ] */ false,    /* 0x5e ^ */ false,    /* 0x5f _ */ true,     
  /* 0x60 ` */ false,    /* 0x61 a */ true,     /* 0x62 b */ true,     /* 0x63 c */ true,     
  /* 0x64 d */ true,     /* 0x65 e */ true,     /* 0x66 f */ true,     /* 0x67 g */ true,     
  /* 0x68 h */ true,     /* 0x69 i */ true,     /* 0x6a j */ true,     /* 0x6b k */ true,     
  /* 0x6c l */ true,     /* 0x6d m */ true,     /* 0x6e n */ true,     /* 0x6f o */ true,     
  /* 0x70 p */ true,     /* 0x71 q */ true,     /* 0x72 r */ true,     /* 0x73 s */ true,     
  /* 0x74 t */ true,     /* 0x75 u */ true,     /* 0x76 v */ true,     /* 0x77 w */ true,     
  /* 0x78 x */ true,     /* 0x79 y */ true,     /* 0x7a z */ true,     /* 0x7b { */ false,    
  /* 0x7c | */ false,    /* 0x7d } */ false,    /* 0x7e ~ */ false,    /* 0x7f  */ false,    
  /* 0x80 . */ false,    /* 0x81 . */ false,    /* 0x82 . */ false,    /* 0x83 . */ false,    
  /* 0x84 . */ false,    /* 0x85 . */ false,    /* 0x86 . */ false,    /* 0x87 . */ false,    
  /* 0x88 . */ false,    /* 0x89 . */ false,    /* 0x8a . */ false,    /* 0x8b . */ false,    
  /* 0x8c . */ false,    /* 0x8d . */ false,    /* 0x8e . */ false,    /* 0x8f . */ false,    
  /* 0x90 . */ false,    /* 0x91 . */ false,    /* 0x92 . */ false,    /* 0x93 . */ false,    
  /* 0x94 . */ false,    /* 0x95 . */ false,    /* 0x96 . */ false,    /* 0x97 . */ false,    
  /* 0x98 . */ false,    /* 0x99 . */ false,    /* 0x9a . */ false,    /* 0x9b . */ false,    
  /* 0x9c . */ false,    /* 0x9d . */ false,    /* 0x9e . */ false,    /* 0x9f . */ false,    
  /* 0xa0 . */ false,    /* 0xa1 . */ false,    /* 0xa2 . */ false,    /* 0xa3 . */ false,    
  /* 0xa4 . */ false,    /* 0xa5 . */ false,    /* 0xa6 . */ false,    /* 0xa7 . */ false,    
  /* 0xa8 . */ false,    /* 0xa9 . */ false,    /* 0xaa . */ false,    /* 0xab . */ false,    
  /* 0xac . */ false,    /* 0xad . */ false,    /* 0xae . */ false,    /* 0xaf . */ false,    
  /* 0xb0 . */ false,    /* 0xb1 . */ false,    /* 0xb2 . */ false,    /* 0xb3 . */ false,    
  /* 0xb4 . */ false,    /* 0xb5 . */ false,    /* 0xb6 . */ false,    /* 0xb7 . */ false,    
  /* 0xb8 . */ false,    /* 0xb9 . */ false,    /* 0xba . */ false,    /* 0xbb . */ false,    
  /* 0xbc . */ false,    /* 0xbd . */ false,    /* 0xbe . */ false,    /* 0xbf . */ false,    
  /* 0xc0 . */ false,    /* 0xc1 . */ false,    /* 0xc2 . */ false,    /* 0xc3 . */ false,    
  /* 0xc4 . */ false,    /* 0xc5 . */ false,    /* 0xc6 . */ false,    /* 0xc7 . */ false,    
  /* 0xc8 . */ false,    /* 0xc9 . */ false,    /* 0xca . */ false,    /* 0xcb . */ false,    
  /* 0xcc . */ false,    /* 0xcd . */ false,    /* 0xce . */ false,    /* 0xcf . */ false,    
  /* 0xd0 . */ false,    /* 0xd1 . */ false,    /* 0xd2 . */ false,    /* 0xd3 . */ false,    
  /* 0xd4 . */ false,    /* 0xd5 . */ false,    /* 0xd6 . */ false,    /* 0xd7 . */ false,    
  /* 0xd8 . */ false,    /* 0xd9 . */ false,    /* 0xda . */ false,    /* 0xdb . */ false,    
  /* 0xdc . */ false,    /* 0xdd . */ false,    /* 0xde . */ false,    /* 0xdf . */ false,    
  /* 0xe0 . */ false,    /* 0xe1 . */ false,    /* 0xe2 . */ false,    /* 0xe3 . */ false,    
  /* 0xe4 . */ false,    /* 0xe5 . */ false,    /* 0xe6 . */ false,    /* 0xe7 . */ false,    
  /* 0xe8 . */ false,    /* 0xe9 . */ false,    /* 0xea . */ false,    /* 0xeb . */ false,    
  /* 0xec . */ false,    /* 0xed . */ false,    /* 0xee . */ false,    /* 0xef . */ false,    
  /* 0xf0 . */ false,    /* 0xf1 . */ false,    /* 0xf2 . */ false,    /* 0xf3 . */ false,    
  /* 0xf4 . */ false,    /* 0xf5 . */ false,    /* 0xf6 . */ false,    /* 0xf7 . */ false,    
  /* 0xf8 . */ false,    /* 0xf9 . */ false,    /* 0xfa . */ false,    /* 0xfb . */ false,    
  /* 0xfc . */ false,    /* 0xfd . */ false,    /* 0xfe . */ false,    /* 0xff . */ false
} };

}

/// @brief create the key generator
KeyGenerator::KeyGenerator(bool allowUserKeys)
    : _allowUserKeys(allowUserKeys) {}

/// @brief destroy the key generator
KeyGenerator::~KeyGenerator() {}

/// @brief get the generator type from VelocyPack
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

/// @brief create a key generator based on the options specified
KeyGenerator* KeyGenerator::factory(VPackSlice const& options) {
  KeyGenerator::GeneratorType type;

  bool const readOptions = options.isObject();

  if (readOptions) {
    type = generatorType(options);
  } else {
    type = TYPE_TRADITIONAL;
  }

  if (type == TYPE_UNKNOWN) {
    THROW_ARANGO_EXCEPTION_MESSAGE(TRI_ERROR_ARANGO_INVALID_KEY_GENERATOR, "invalid key generator type");
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
        double v = incrementSlice.getNumericValue<double>();
        if (v <= 0.0) {
          // negative or 0 increment is not allowed
          THROW_ARANGO_EXCEPTION_MESSAGE(TRI_ERROR_ARANGO_INVALID_KEY_GENERATOR, "increment value must be greater than zero");
        }

        increment = incrementSlice.getNumericValue<uint64_t>();

        if (increment == 0 || increment >= (1ULL << 16)) {
          THROW_ARANGO_EXCEPTION_MESSAGE(TRI_ERROR_ARANGO_INVALID_KEY_GENERATOR, "increment value must be greater than zero");
        }
      }

      VPackSlice const offsetSlice = options.get("offset");

      if (offsetSlice.isNumber()) {
        double v = offsetSlice.getNumericValue<double>();
        if (v < 0.0) {
          // negative or 0 offset is not allowed
          THROW_ARANGO_EXCEPTION_MESSAGE(TRI_ERROR_ARANGO_INVALID_KEY_GENERATOR, "offset value must be zero or greater");
        }

        offset = offsetSlice.getNumericValue<uint64_t>();

        if (offset >= UINT64_MAX) {
          THROW_ARANGO_EXCEPTION_MESSAGE(TRI_ERROR_ARANGO_INVALID_KEY_GENERATOR, "offset value is too high");
        }
      }
    }

    return new AutoIncrementKeyGenerator(allowUserKeys, offset, increment);
  }

  // unknown key generator type
  THROW_ARANGO_EXCEPTION_MESSAGE(TRI_ERROR_ARANGO_INVALID_KEY_GENERATOR, "invalid key generator type");
}

/// @brief check global key attributes
int KeyGenerator::globalCheck(char const* p, size_t length, bool isRestore) {
  // user has specified a key
  if (length > 0 && !_allowUserKeys && !isRestore) {
    // we do not allow user-generated keys
    return TRI_ERROR_ARANGO_DOCUMENT_KEY_UNEXPECTED;
  }

  if (length == 0) {
    // user key is empty
    return TRI_ERROR_ARANGO_DOCUMENT_KEY_BAD;
  }

  if (length > TRI_VOC_KEY_MAX_LENGTH) {
    // user key is too long
    return TRI_ERROR_ARANGO_DOCUMENT_KEY_BAD;
  }

  return TRI_ERROR_NO_ERROR;
}

/// @brief return a VelocyPack representation of the generator
///        Not virtual because this is identical for all of them
std::shared_ptr<VPackBuilder> KeyGenerator::toVelocyPack() const {
  auto builder = std::make_shared<VPackBuilder>();
  toVelocyPack(*builder);
  builder->close();
  return builder;
}

/// @brief create the key generator
TraditionalKeyGenerator::TraditionalKeyGenerator(bool allowUserKeys)
    : KeyGenerator(allowUserKeys), _lastValue(0) {}

/// @brief destroy the key generator
TraditionalKeyGenerator::~TraditionalKeyGenerator() {}

/// @brief validate a key
bool TraditionalKeyGenerator::validateKey(char const* key, size_t len) {
  if (len == 0 || len > TRI_VOC_KEY_MAX_LENGTH) {
    return false;
  }

  unsigned char const* p = reinterpret_cast<unsigned char const*>(key);
  unsigned char const* e = p + len;
  TRI_ASSERT(p != e);
  do {
    if (!::keyCharLookupTable[*p]) {
      return false;
    }
  } while (++p < e);
  return true;
}

/// @brief generate a key
std::string TraditionalKeyGenerator::generate() {
  TRI_voc_tick_t tick = TRI_NewTickServer();
  
  {
    MUTEX_LOCKER(mutexLocker, _lock);

    if (tick <= _lastValue) {
      tick = ++_lastValue;
    } else {
      _lastValue = tick;
    }
  }

  if (tick == UINT64_MAX) {
    // sanity check
    return "";
  }
  return arangodb::basics::StringUtils::itoa(tick);
}

/// @brief validate a key
int TraditionalKeyGenerator::validate(char const* p, size_t length, bool isRestore) {
  int res = globalCheck(p, length, isRestore);

  if (res != TRI_ERROR_NO_ERROR) {
    return res;
  }

  // validate user-supplied key
  if (!validateKey(p, length)) {
    return TRI_ERROR_ARANGO_DOCUMENT_KEY_BAD;
  }
  
  if (length > 0 && p[0] >= '0' && p[0] <= '9') {
    // potentially numeric key
    uint64_t value = NumberUtils::atoi_zero<uint64_t>(p, p + length);
    
    if (value > 0) {
      MUTEX_LOCKER(mutexLocker, _lock);

      if (value > _lastValue) {
        // and update our last value
        _lastValue = value;
      }
    }
  }

  return TRI_ERROR_NO_ERROR;
}

/// @brief track usage of a key
void TraditionalKeyGenerator::track(char const* p, size_t length) {
  // check the numeric key part
  if (length > 0 && p[0] >= '0' && p[0] <= '9') {
    // potentially numeric key
    uint64_t value = NumberUtils::atoi_zero<uint64_t>(p, p + length);

    if (value > 0) {
      MUTEX_LOCKER(mutexLocker, _lock);

      if (value > _lastValue) {
        // and update our last value
        _lastValue = value;
      }
    }
  }
}

/// @brief create a VPack representation of the generator
void TraditionalKeyGenerator::toVelocyPack(VPackBuilder& builder) const {
  TRI_ASSERT(!builder.isClosed());
  builder.add("type", VPackValue(name()));
  builder.add("allowUserKeys", VPackValue(_allowUserKeys));
  builder.add("lastValue", VPackValue(_lastValue));
}

/// @brief create the generator
AutoIncrementKeyGenerator::AutoIncrementKeyGenerator(bool allowUserKeys,
                                                     uint64_t offset,
                                                     uint64_t increment)
    : KeyGenerator(allowUserKeys),
      _lastValue(0),
      _offset(offset),
      _increment(increment) {}

/// @brief destroy the generator
AutoIncrementKeyGenerator::~AutoIncrementKeyGenerator() {}

/// @brief validate a numeric key
bool AutoIncrementKeyGenerator::validateKey(char const* key, size_t len) {
  if (len == 0 || len > TRI_VOC_KEY_MAX_LENGTH) {
    return false;
  }

  char const* p = key;
  char const* e = p + len;
  TRI_ASSERT(p != e);
  do {
    if (*p < '0' || *p > '9') {
      return false;
    }
  } while (++p < e);
  return true;
}

/// @brief generate a key
std::string AutoIncrementKeyGenerator::generate() {
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

/// @brief validate a key
int AutoIncrementKeyGenerator::validate(char const* p, size_t length, bool isRestore) {
  int res = globalCheck(p, length, isRestore);

  if (res != TRI_ERROR_NO_ERROR) {
    return res;
  }

  // validate user-supplied key
  if (!validateKey(p, length)) {
    return TRI_ERROR_ARANGO_DOCUMENT_KEY_BAD;
  }

  uint64_t intValue = NumberUtils::atoi_zero<uint64_t>(p, p + length);
   
  if (intValue > 0) { 
    MUTEX_LOCKER(mutexLocker, _lock);

    if (intValue > _lastValue) {
      // update our last value
      _lastValue = intValue;
    }
  }

  return TRI_ERROR_NO_ERROR;
}

/// @brief track usage of a key
void AutoIncrementKeyGenerator::track(char const* p, size_t length) {
  // check the numeric key part
  if (length > 0 && p[0] >= '0' && p[0] <= '9') {
    uint64_t value = NumberUtils::atoi_zero<uint64_t>(p, p + length);
  
    if (value > 0) {
      MUTEX_LOCKER(mutexLocker, _lock);

      if (value > _lastValue) {
        // and update our last value
        _lastValue = value;
      }
    }
  }
}

/// @brief create a VelocyPack representation of the generator
void AutoIncrementKeyGenerator::toVelocyPack(VPackBuilder& builder) const {
  TRI_ASSERT(!builder.isClosed());
  builder.add("type", VPackValue(name()));
  builder.add("allowUserKeys", VPackValue(_allowUserKeys));
  builder.add("offset", VPackValue(_offset));
  builder.add("increment", VPackValue(_increment));
  builder.add("lastValue", VPackValue(_lastValue));
}

/// @brief validate a document id (collection name + / + document key)
bool TRI_ValidateDocumentIdKeyGenerator(char const* key, size_t len,
                                        size_t* split) {
  if (len == 0) {
    return false;
  }

  char const* p = key;
  char c = *p;
  size_t pos = 0;

  // extract collection name
  if (!(c == '_' || (c >= '0' && c <= '9') || (c >= 'a' && c <= 'z') ||
        (c >= 'A' && c <= 'Z'))) {
    return false;
  }

  ++p;
  ++pos;

  while (true) {
    if (pos >= len) {
      return false;
    }

    c = *p;
    if (c == '_' || c == '-' || (c >= '0' && c <= '9') ||
        (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z')) {
      ++p;
      ++pos;
      continue;
    }

    if (c == '/') {
      break;
    }

    return false;
  }

  if (pos > TRI_COL_NAME_LENGTH) {
    return false;
  }

  // store split position
  *split = pos;
  ++p;
  ++pos;

  // validate document key
  return TraditionalKeyGenerator::validateKey(p, len - pos);
}
