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

#include "ApplicationFeatures/ApplicationServer.h"
#include "Basics/Endian.h"
#include "Basics/Mutex.h"
#include "Basics/MutexLocker.h"
#include "Basics/NumberUtils.h"
#include "Basics/StaticStrings.h"
#include "Basics/StringUtils.h"
#include "Basics/VelocyPackHelper.h"
#include "Basics/system-compiler.h"
#include "Cluster/ClusterFeature.h"
#include "Cluster/ClusterInfo.h"
#include "Cluster/ServerState.h"
#include "VocBase/ticks.h"
#include "VocBase/vocbase.h"

#include <boost/uuid/random_generator.hpp>
#include <boost/uuid/uuid.hpp>
#include <boost/uuid/uuid_io.hpp>

#include <array>

using namespace arangodb;
using namespace arangodb::basics;

namespace {

/// @brief lookup table for key checks
/// in case this table is changed, the regex in file
/// js/common/modules/@arangodb/common.js for function isValidDocumentKey
/// should be adjusted too
std::array<bool, 256> const keyCharLookupTable = {
    {/* 0x00 . */ false, /* 0x01 . */ false,
     /* 0x02 . */ false, /* 0x03 . */ false,
     /* 0x04 . */ false, /* 0x05 . */ false,
     /* 0x06 . */ false, /* 0x07 . */ false,
     /* 0x08 . */ false, /* 0x09 . */ false,
     /* 0x0a . */ false, /* 0x0b . */ false,
     /* 0x0c . */ false, /* 0x0d . */ false,
     /* 0x0e . */ false, /* 0x0f . */ false,
     /* 0x10 . */ false, /* 0x11 . */ false,
     /* 0x12 . */ false, /* 0x13 . */ false,
     /* 0x14 . */ false, /* 0x15 . */ false,
     /* 0x16 . */ false, /* 0x17 . */ false,
     /* 0x18 . */ false, /* 0x19 . */ false,
     /* 0x1a . */ false, /* 0x1b . */ false,
     /* 0x1c . */ false, /* 0x1d . */ false,
     /* 0x1e . */ false, /* 0x1f . */ false,
     /* 0x20   */ false, /* 0x21 ! */ true,
     /* 0x22 " */ false, /* 0x23 # */ false,
     /* 0x24 $ */ true,  /* 0x25 % */ true,
     /* 0x26 & */ false, /* 0x27 ' */ true,
     /* 0x28 ( */ true,  /* 0x29 ) */ true,
     /* 0x2a * */ true,  /* 0x2b + */ true,
     /* 0x2c , */ true,  /* 0x2d - */ true,
     /* 0x2e . */ true,  /* 0x2f / */ false,
     /* 0x30 0 */ true,  /* 0x31 1 */ true,
     /* 0x32 2 */ true,  /* 0x33 3 */ true,
     /* 0x34 4 */ true,  /* 0x35 5 */ true,
     /* 0x36 6 */ true,  /* 0x37 7 */ true,
     /* 0x38 8 */ true,  /* 0x39 9 */ true,
     /* 0x3a : */ true,  /* 0x3b ; */ true,
     /* 0x3c < */ false, /* 0x3d = */ true,
     /* 0x3e > */ false, /* 0x3f ? */ false,
     /* 0x40 @ */ true,  /* 0x41 A */ true,
     /* 0x42 B */ true,  /* 0x43 C */ true,
     /* 0x44 D */ true,  /* 0x45 E */ true,
     /* 0x46 F */ true,  /* 0x47 G */ true,
     /* 0x48 H */ true,  /* 0x49 I */ true,
     /* 0x4a J */ true,  /* 0x4b K */ true,
     /* 0x4c L */ true,  /* 0x4d M */ true,
     /* 0x4e N */ true,  /* 0x4f O */ true,
     /* 0x50 P */ true,  /* 0x51 Q */ true,
     /* 0x52 R */ true,  /* 0x53 S */ true,
     /* 0x54 T */ true,  /* 0x55 U */ true,
     /* 0x56 V */ true,  /* 0x57 W */ true,
     /* 0x58 X */ true,  /* 0x59 Y */ true,
     /* 0x5a Z */ true,  /* 0x5b [ */ false,
     /* 0x5c \ */ false, /* 0x5d ] */ false,
     /* 0x5e ^ */ false, /* 0x5f _ */ true,
     /* 0x60 ` */ false, /* 0x61 a */ true,
     /* 0x62 b */ true,  /* 0x63 c */ true,
     /* 0x64 d */ true,  /* 0x65 e */ true,
     /* 0x66 f */ true,  /* 0x67 g */ true,
     /* 0x68 h */ true,  /* 0x69 i */ true,
     /* 0x6a j */ true,  /* 0x6b k */ true,
     /* 0x6c l */ true,  /* 0x6d m */ true,
     /* 0x6e n */ true,  /* 0x6f o */ true,
     /* 0x70 p */ true,  /* 0x71 q */ true,
     /* 0x72 r */ true,  /* 0x73 s */ true,
     /* 0x74 t */ true,  /* 0x75 u */ true,
     /* 0x76 v */ true,  /* 0x77 w */ true,
     /* 0x78 x */ true,  /* 0x79 y */ true,
     /* 0x7a z */ true,  /* 0x7b { */ false,
     /* 0x7c | */ false, /* 0x7d } */ false,
     /* 0x7e ~ */ false, /* 0x7f   */ false,
     /* 0x80 . */ false, /* 0x81 . */ false,
     /* 0x82 . */ false, /* 0x83 . */ false,
     /* 0x84 . */ false, /* 0x85 . */ false,
     /* 0x86 . */ false, /* 0x87 . */ false,
     /* 0x88 . */ false, /* 0x89 . */ false,
     /* 0x8a . */ false, /* 0x8b . */ false,
     /* 0x8c . */ false, /* 0x8d . */ false,
     /* 0x8e . */ false, /* 0x8f . */ false,
     /* 0x90 . */ false, /* 0x91 . */ false,
     /* 0x92 . */ false, /* 0x93 . */ false,
     /* 0x94 . */ false, /* 0x95 . */ false,
     /* 0x96 . */ false, /* 0x97 . */ false,
     /* 0x98 . */ false, /* 0x99 . */ false,
     /* 0x9a . */ false, /* 0x9b . */ false,
     /* 0x9c . */ false, /* 0x9d . */ false,
     /* 0x9e . */ false, /* 0x9f . */ false,
     /* 0xa0 . */ false, /* 0xa1 . */ false,
     /* 0xa2 . */ false, /* 0xa3 . */ false,
     /* 0xa4 . */ false, /* 0xa5 . */ false,
     /* 0xa6 . */ false, /* 0xa7 . */ false,
     /* 0xa8 . */ false, /* 0xa9 . */ false,
     /* 0xaa . */ false, /* 0xab . */ false,
     /* 0xac . */ false, /* 0xad . */ false,
     /* 0xae . */ false, /* 0xaf . */ false,
     /* 0xb0 . */ false, /* 0xb1 . */ false,
     /* 0xb2 . */ false, /* 0xb3 . */ false,
     /* 0xb4 . */ false, /* 0xb5 . */ false,
     /* 0xb6 . */ false, /* 0xb7 . */ false,
     /* 0xb8 . */ false, /* 0xb9 . */ false,
     /* 0xba . */ false, /* 0xbb . */ false,
     /* 0xbc . */ false, /* 0xbd . */ false,
     /* 0xbe . */ false, /* 0xbf . */ false,
     /* 0xc0 . */ false, /* 0xc1 . */ false,
     /* 0xc2 . */ false, /* 0xc3 . */ false,
     /* 0xc4 . */ false, /* 0xc5 . */ false,
     /* 0xc6 . */ false, /* 0xc7 . */ false,
     /* 0xc8 . */ false, /* 0xc9 . */ false,
     /* 0xca . */ false, /* 0xcb . */ false,
     /* 0xcc . */ false, /* 0xcd . */ false,
     /* 0xce . */ false, /* 0xcf . */ false,
     /* 0xd0 . */ false, /* 0xd1 . */ false,
     /* 0xd2 . */ false, /* 0xd3 . */ false,
     /* 0xd4 . */ false, /* 0xd5 . */ false,
     /* 0xd6 . */ false, /* 0xd7 . */ false,
     /* 0xd8 . */ false, /* 0xd9 . */ false,
     /* 0xda . */ false, /* 0xdb . */ false,
     /* 0xdc . */ false, /* 0xdd . */ false,
     /* 0xde . */ false, /* 0xdf . */ false,
     /* 0xe0 . */ false, /* 0xe1 . */ false,
     /* 0xe2 . */ false, /* 0xe3 . */ false,
     /* 0xe4 . */ false, /* 0xe5 . */ false,
     /* 0xe6 . */ false, /* 0xe7 . */ false,
     /* 0xe8 . */ false, /* 0xe9 . */ false,
     /* 0xea . */ false, /* 0xeb . */ false,
     /* 0xec . */ false, /* 0xed . */ false,
     /* 0xee . */ false, /* 0xef . */ false,
     /* 0xf0 . */ false, /* 0xf1 . */ false,
     /* 0xf2 . */ false, /* 0xf3 . */ false,
     /* 0xf4 . */ false, /* 0xf5 . */ false,
     /* 0xf6 . */ false, /* 0xf7 . */ false,
     /* 0xf8 . */ false, /* 0xf9 . */ false,
     /* 0xfa . */ false, /* 0xfb . */ false,
     /* 0xfc . */ false, /* 0xfd . */ false,
     /* 0xfe . */ false, /* 0xff . */ false}};

/// @brief available key generators
enum class GeneratorType : int {
  UNKNOWN = 0,
  TRADITIONAL = 1,
  AUTOINCREMENT = 2,
  UUID = 3,
  PADDED = 4
};

/// @brief for older compilers
typedef std::underlying_type<GeneratorType>::type GeneratorMapType;

/// Actual key generators following...

/// @brief base class for traditional key generators
class TraditionalKeyGenerator : public KeyGenerator {
 public:
  /// @brief create the generator
  explicit TraditionalKeyGenerator(bool allowUserKeys)
      : KeyGenerator(allowUserKeys) {}

  bool hasDynamicState() const override { return true; }

  /// @brief generate a key
  std::string generate() override final {
    uint64_t tick = generateValue();

    if (ADB_UNLIKELY(tick == 0)) {
      // unlikely case we have run out of keys
      // returning an empty string will trigger an error on the call site
      return std::string();
    }

    return arangodb::basics::StringUtils::itoa(tick);
  }

  /// @brief validate a key
  int validate(char const* p, size_t length, bool isRestore) override {
    int res = KeyGenerator::validate(p, length, isRestore);

    if (res != TRI_ERROR_NO_ERROR) {
      return res;
    }

    track(p, length);

    return TRI_ERROR_NO_ERROR;
  }

  /// @brief track usage of a key
  void track(char const* p, size_t length) override {
    // check the numeric key part
    if (length > 0 && p[0] >= '0' && p[0] <= '9') {
      // potentially numeric key
      uint64_t value = NumberUtils::atoi_zero<uint64_t>(p, p + length);

      if (value > 0) {
        track(value);
      }
    }
  }

  /// @brief build a VelocyPack representation of the generator in the builder
  void toVelocyPack(arangodb::velocypack::Builder& builder) const override {
    KeyGenerator::toVelocyPack(builder);
    builder.add("type", VPackValue("traditional"));
  }

 protected:
  /// @brief generate a new key value (internal)
  virtual uint64_t generateValue() = 0;

  /// @brief track a value (internal)
  virtual void track(uint64_t value) = 0;
};

/// @brief traditional key generator for a single server
class TraditionalKeyGeneratorSingle final : public TraditionalKeyGenerator {
 public:
  /// @brief create the generator
  explicit TraditionalKeyGeneratorSingle(bool allowUserKeys)
      : TraditionalKeyGenerator(allowUserKeys), _lastValue(0) {
    TRI_ASSERT(!ServerState::instance()->isCoordinator());
  }

  /// @brief build a VelocyPack representation of the generator in the builder
  void toVelocyPack(arangodb::velocypack::Builder& builder) const override {
    TraditionalKeyGenerator::toVelocyPack(builder);

    // add our specific stuff
    builder.add(StaticStrings::LastValue, VPackValue(_lastValue.load(std::memory_order_relaxed)));
  }

 private:
  /// @brief generate a key value (internal)
  uint64_t generateValue() override {
    uint64_t tick = TRI_NewTickServer();

    if (ADB_UNLIKELY(tick == UINT64_MAX)) {
      // out of keys
      return 0;
    }

    // keep track of last assigned value, and make sure the value
    // we hand out is always higher than it
    auto lastValue = _lastValue.load(std::memory_order_relaxed);    
    if (ADB_UNLIKELY(lastValue >= UINT64_MAX - 1ULL)) {
      // oops, out of keys!
      return 0;
    }

    do {
      if (tick <= lastValue) {
        tick = _lastValue.fetch_add(1, std::memory_order_relaxed) + 1;
        break;
      }
    } while(!_lastValue.compare_exchange_weak(lastValue, tick, std::memory_order_relaxed));

    return tick;
  }

  /// @brief track a key value (internal)
  void track(uint64_t value) override {
    auto lastValue = _lastValue.load(std::memory_order_relaxed);
    while (value > lastValue) {
      // and update our last value
      if (_lastValue.compare_exchange_weak(lastValue, value, std::memory_order_relaxed)) {
        break;
      }
    }
  }

 private:
  std::atomic<uint64_t> _lastValue;
};

/// @brief traditional key generator for a coordinator
/// please note that coordinator-based key generators are frequently
/// created and discarded, so ctor & dtor need to be very efficient.
/// additionally, do not put any state into this object, as for the
/// same logical collection the ClusterInfo may create many different
/// temporary LogicalCollection objects one after the other, which
/// will also discard the collection's particular KeyGenerator object!
class TraditionalKeyGeneratorCluster final : public TraditionalKeyGenerator {
 public:
  /// @brief create the generator
  explicit TraditionalKeyGeneratorCluster(ClusterInfo& ci, bool allowUserKeys)
      : TraditionalKeyGenerator(allowUserKeys), _ci(ci) {
    TRI_ASSERT(ServerState::instance()->isCoordinator());
  }

 private:
  /// @brief generate a key value (internal)
  uint64_t generateValue() override { return _ci.uniqid(); }

  /// @brief track a key value (internal)
  void track(uint64_t /* value */) override {}

 private:
  ClusterInfo& _ci;
};

/// @brief base class for padded key generators
class PaddedKeyGenerator : public KeyGenerator {
 public:
  /// @brief create the generator
  explicit PaddedKeyGenerator(bool allowUserKeys)
      : KeyGenerator(allowUserKeys) {}

  bool hasDynamicState() const override { return true; }

  /// @brief generate a key
  std::string generate() override {
    uint64_t tick = generateValue();

    if (ADB_UNLIKELY(tick == 0)) {
      // unlikely case we have run out of keys
      // returning an empty string will trigger an error on the call site
      return std::string();
    }

    return encode(tick);
  }

  /// @brief validate a key
  int validate(char const* p, size_t length, bool isRestore) override {
    int res = KeyGenerator::validate(p, length, isRestore);

    if (res != TRI_ERROR_NO_ERROR) {
      return res;
    }

    track(p, length);

    return TRI_ERROR_NO_ERROR;
  }

  /// @brief track usage of a key
  void track(char const* p, size_t length) override {
    // check the numeric key part
    uint64_t value = decode(p, length);
    if (value > 0) {
      track(value);
    }
  }

  /// @brief build a VelocyPack representation of the generator in the builder
  void toVelocyPack(arangodb::velocypack::Builder& builder) const override {
    KeyGenerator::toVelocyPack(builder);
    builder.add("type", VPackValue("padded"));
  }

 protected:
  /// @brief generate a key value (internal)
  virtual uint64_t generateValue() = 0;

  /// @brief track a value (internal)
  virtual void track(uint64_t value) = 0;

 private:
  uint64_t decode(char const* p, size_t length) {
    uint64_t result = 0;

    if (length != sizeof(uint64_t) * 2) {
      return result;
    }

    char const* e = p + length;
    while (p < e) {
      uint64_t high, low;
      uint8_t c = (uint8_t)(*p++);
      if (c >= 'a' && c <= 'f') {
        high = (c - 'a') + 10;
      } else if (c >= '0' && c <= '9') {
        high = (c - '0');
      } else {
        return 0;
      }
      c = (uint8_t)(*p++);
      if (c >= 'a' && c <= 'f') {
        low = (c - 'a') + 10;
      } else if (c >= '0' && c <= '9') {
        low = (c - '0');
      } else {
        return 0;
      }
      result += ((high << 4) | low) << ((e - p) / 2);
    }

    return result;
  }

  std::string encode(uint64_t value) {
    // convert to big endian
    uint64_t big = basics::hostToBig(value);

    uint8_t const* p = reinterpret_cast<uint8_t const*>(&big);
    uint8_t const* e = p + sizeof(value);

    char buffer[16];
    uint8_t* out = reinterpret_cast<uint8_t*>(&buffer[0]);
    while (p < e) {
      uint8_t c = (uint8_t)(*p++);
      uint8_t n1 = c >> 4;
      uint8_t n2 = c & 0x0F;
      *out++ = ((n1 < 10) ? ('0' + n1) : ('a' + n1 - 10));
      *out++ = ((n2 < 10) ? ('0' + n2) : ('a' + n2 - 10));
    }

    return std::string(&buffer[0], sizeof(uint64_t) * 2);
  }
};

/// @brief padded key generator for a single server
class PaddedKeyGeneratorSingle final : public PaddedKeyGenerator {
 public:
  /// @brief create the generator
  explicit PaddedKeyGeneratorSingle(bool allowUserKeys)
      : PaddedKeyGenerator(allowUserKeys), _lastValue(0) {
    TRI_ASSERT(!ServerState::instance()->isCoordinator());
  }

  /// @brief build a VelocyPack representation of the generator in the builder
  void toVelocyPack(arangodb::velocypack::Builder& builder) const override {
    PaddedKeyGenerator::toVelocyPack(builder);

    // add our own specific values
    builder.add(StaticStrings::LastValue, VPackValue(_lastValue.load(std::memory_order_relaxed)));
  }

 private:
  /// @brief generate a key
  uint64_t generateValue() override {
    uint64_t tick = TRI_NewTickServer();

    if (ADB_UNLIKELY(tick == UINT64_MAX)) {
      // oops, out of keys!
      return 0;
    }

    auto lastValue = _lastValue.load(std::memory_order_relaxed);
    if (ADB_UNLIKELY(lastValue >= UINT64_MAX - 1ULL)) {
      // oops, out of keys!
      return 0;
    }

    do {
      if (tick <= lastValue) {
        tick = _lastValue.fetch_add(1, std::memory_order_relaxed) + 1;
        break;
      }
    } while(!_lastValue.compare_exchange_weak(lastValue, tick, std::memory_order_relaxed));


    return tick;
  }

  /// @brief generate a key value (internal)
  void track(uint64_t value) override {
    auto lastValue = _lastValue.load(std::memory_order_relaxed);
    while (value > lastValue) {
      // and update our last value
      if (_lastValue.compare_exchange_weak(lastValue, value, std::memory_order_relaxed)) {
        break;
      }
    }
  }

 private:
  std::atomic<uint64_t> _lastValue;
};

/// @brief padded key generator for a coordinator
/// please note that coordinator-based key generators are frequently
/// created and discarded, so ctor & dtor need to be very efficient.
/// additionally, do not put any state into this object, as for the
/// same logical collection the ClusterInfo may create many different
/// temporary LogicalCollection objects one after the other, which
/// will also discard the collection's particular KeyGenerator object!
class PaddedKeyGeneratorCluster final : public PaddedKeyGenerator {
 public:
  /// @brief create the generator
  explicit PaddedKeyGeneratorCluster(ClusterInfo& ci, bool allowUserKeys)
      : PaddedKeyGenerator(allowUserKeys), _ci(ci) {
    TRI_ASSERT(ServerState::instance()->isCoordinator());
  }

 private:
  /// @brief generate a key value (internal)
  uint64_t generateValue() override { return _ci.uniqid(); }

  /// @brief generate a key value (internal)
  void track(uint64_t /* value */) override {}

 private:
  ClusterInfo& _ci;
};

/// @brief auto-increment key generator - not usable in cluster
class AutoIncrementKeyGenerator final : public KeyGenerator {
 public:
  /// @brief create the generator
  AutoIncrementKeyGenerator(bool allowUserKeys, uint64_t offset, uint64_t increment)
      : KeyGenerator(allowUserKeys), _lastValue(0), _offset(offset), _increment(increment) {}

  bool hasDynamicState() const override { return true; }

  /// @brief generate a key
  std::string generate() override {
    uint64_t keyValue;

    auto lastValue = _lastValue.load(std::memory_order_relaxed);
    do {
      // user has not specified a key, generate one based on algorithm
      if (lastValue < _offset) {
        keyValue = _offset;
      } else {
        keyValue = lastValue + _increment - ((lastValue - _offset) % _increment);
      }

      // bounds and sanity checks
      if (keyValue == UINT64_MAX || keyValue < lastValue) {
        return "";
      }

      TRI_ASSERT(keyValue > lastValue);
      // update our last value
    } while(!_lastValue.compare_exchange_weak(lastValue, keyValue, std::memory_order_relaxed));

    return arangodb::basics::StringUtils::itoa(keyValue);
  }

  /// @brief validate a key
  int validate(char const* p, size_t length, bool isRestore) override {
    int res = KeyGenerator::validate(p, length, isRestore);

    if (res != TRI_ERROR_NO_ERROR) {
      return res;
    }

    char const* s = p;
    char const* e = s + length;
    TRI_ASSERT(s != e);
    do {
      if (*s < '0' || *s > '9') {
        return TRI_ERROR_ARANGO_DOCUMENT_KEY_BAD;
      }
    } while (++s < e);

    track(p, length);

    return TRI_ERROR_NO_ERROR;
  }

  /// @brief track usage of a key
  void track(char const* p, size_t length) override {
    // check the numeric key part
    if (length > 0 && p[0] >= '0' && p[0] <= '9') {
      uint64_t value = NumberUtils::atoi_zero<uint64_t>(p, p + length);

      if (value > 0) {
        auto lastValue = _lastValue.load(std::memory_order_relaxed);
        while (value > lastValue) {
          // and update our last value
          if (_lastValue.compare_exchange_weak(lastValue, value, std::memory_order_relaxed)) {
            break;
          }
        }
      }
    }
  }

  /// @brief build a VelocyPack representation of the generator in the builder
  void toVelocyPack(arangodb::velocypack::Builder& builder) const override {
    KeyGenerator::toVelocyPack(builder);
    builder.add("type", VPackValue("autoincrement"));

    builder.add("offset", VPackValue(_offset));
    builder.add("increment", VPackValue(_increment));
    builder.add(StaticStrings::LastValue, VPackValue(_lastValue.load(std::memory_order_relaxed)));
  }

 private:
  std::atomic<uint64_t> _lastValue;  // last value assigned
  const uint64_t _offset;  // start value
  const uint64_t _increment;  // increment value
};

/// @brief uuid key generator
class UuidKeyGenerator final : public KeyGenerator {
 public:
  /// @brief create the generator
  explicit UuidKeyGenerator(bool allowUserKeys) : KeyGenerator(allowUserKeys) {}

  bool hasDynamicState() const override { return false; }

  /// @brief generate a key
  std::string generate() override {
    MUTEX_LOCKER(locker, _lock);
    return boost::uuids::to_string(uuid());
  }

  /// @brief track usage of a key
  void track(char const* p, size_t length) override {}

  void toVelocyPack(arangodb::velocypack::Builder& builder) const override {
    KeyGenerator::toVelocyPack(builder);
    builder.add("type", VPackValue("uuid"));
  }

 private:
  arangodb::Mutex _lock;

  boost::uuids::random_generator uuid;
};

/// @brief all generators, by name
std::unordered_map<std::string, GeneratorType> const generatorNames = {
    {"traditional", GeneratorType::TRADITIONAL},
    {"autoincrement", GeneratorType::AUTOINCREMENT},
    {"uuid", GeneratorType::UUID},
    {"padded", GeneratorType::PADDED}};

/// @brief get the generator type from VelocyPack
GeneratorType generatorType(VPackSlice const& parameters) {
  if (!parameters.isObject()) {
    // the default
    return GeneratorType::TRADITIONAL;
  }

  VPackSlice const type = parameters.get("type");
  if (!type.isString()) {
    return GeneratorType::TRADITIONAL;
  }

  std::string const typeName = arangodb::basics::StringUtils::tolower(type.copyString());

  auto it = generatorNames.find(typeName);

  if (it != generatorNames.end()) {
    return (*it).second;
  }

  return GeneratorType::UNKNOWN;
}

std::unordered_map<GeneratorMapType, std::function<KeyGenerator*(application_features::ApplicationServer&, bool, VPackSlice)>> const factories = {
    {static_cast<GeneratorMapType>(GeneratorType::UNKNOWN),
     [](application_features::ApplicationServer&, bool, VPackSlice) -> KeyGenerator* {
       // unknown key generator type
       THROW_ARANGO_EXCEPTION_MESSAGE(TRI_ERROR_ARANGO_INVALID_KEY_GENERATOR,
                                      "invalid key generator type");
     }},
    {static_cast<GeneratorMapType>(GeneratorType::TRADITIONAL),
     [](application_features::ApplicationServer& server, bool allowUserKeys,
        VPackSlice options) -> KeyGenerator* {
       if (ServerState::instance()->isCoordinator()) {
         auto& ci = server.getFeature<ClusterFeature>().clusterInfo();
         return new TraditionalKeyGeneratorCluster(ci, allowUserKeys);
       }
       return new TraditionalKeyGeneratorSingle(allowUserKeys);
     }},
    {static_cast<GeneratorMapType>(GeneratorType::AUTOINCREMENT),
     [](application_features::ApplicationServer&, bool allowUserKeys,
        VPackSlice options) -> KeyGenerator* {
       if (ServerState::instance()->isCoordinator()) {
         THROW_ARANGO_EXCEPTION_MESSAGE(TRI_ERROR_CLUSTER_UNSUPPORTED,
                                        "the specified key generator is not "
                                        "supported for sharded collections");
       }
       uint64_t offset = 0;
       uint64_t increment = 1;

       VPackSlice const incrementSlice = options.get("increment");

       if (incrementSlice.isNumber()) {
         double v = incrementSlice.getNumericValue<double>();
         if (v <= 0.0) {
           // negative or 0 increment is not allowed
           THROW_ARANGO_EXCEPTION_MESSAGE(
               TRI_ERROR_ARANGO_INVALID_KEY_GENERATOR,
               "increment value must be greater than zero");
         }

         increment = incrementSlice.getNumericValue<uint64_t>();

         if (increment == 0 || increment >= (1ULL << 16)) {
           THROW_ARANGO_EXCEPTION_MESSAGE(
               TRI_ERROR_ARANGO_INVALID_KEY_GENERATOR,
               "increment value must be greater than zero and smaller than "
               "65536");
         }
       }

       VPackSlice const offsetSlice = options.get("offset");

       if (offsetSlice.isNumber()) {
         double v = offsetSlice.getNumericValue<double>();
         if (v < 0.0) {
           // negative or 0 offset is not allowed
           THROW_ARANGO_EXCEPTION_MESSAGE(
               TRI_ERROR_ARANGO_INVALID_KEY_GENERATOR,
               "offset value must be zero or greater");
         }

         offset = offsetSlice.getNumericValue<uint64_t>();

         if (offset >= UINT64_MAX) {
           THROW_ARANGO_EXCEPTION_MESSAGE(TRI_ERROR_ARANGO_INVALID_KEY_GENERATOR,
                                          "offset value is too high");
         }
       }

       return new AutoIncrementKeyGenerator(allowUserKeys, offset, increment);
     }},
    {static_cast<GeneratorMapType>(GeneratorType::UUID),
     [](application_features::ApplicationServer&, bool allowUserKeys, VPackSlice) -> KeyGenerator* {
       return new UuidKeyGenerator(allowUserKeys);
     }},
    {static_cast<GeneratorMapType>(GeneratorType::PADDED),
     [](application_features::ApplicationServer& server, bool allowUserKeys,
        VPackSlice options) -> KeyGenerator* {
       if (ServerState::instance()->isCoordinator()) {
         auto& ci = server.getFeature<ClusterFeature>().clusterInfo();
         return new PaddedKeyGeneratorCluster(ci, allowUserKeys);
       }
       return new PaddedKeyGeneratorSingle(allowUserKeys);
     }}};

}  // namespace

/// @brief create the key generator
KeyGenerator::KeyGenerator(bool allowUserKeys)
    : _allowUserKeys(allowUserKeys) {}

/// @brief build a VelocyPack representation of the generator in the builder
void KeyGenerator::toVelocyPack(arangodb::velocypack::Builder& builder) const {
  TRI_ASSERT(!builder.isClosed());
  builder.add("allowUserKeys", VPackValue(_allowUserKeys));
}

/// @brief create a key generator based on the options specified
KeyGenerator* KeyGenerator::factory(application_features::ApplicationServer& server,
                                    VPackSlice options) {
  if (!options.isObject()) {
    options = VPackSlice::emptyObjectSlice();
  }

  ::GeneratorType type = ::generatorType(options);

  bool allowUserKeys =
      arangodb::basics::VelocyPackHelper::getBooleanValue(options,
                                                          "allowUserKeys", true);

  auto it = ::factories.find(static_cast<::GeneratorMapType>(type));

  if (it == ::factories.end()) {
    it = ::factories.find(static_cast<::GeneratorMapType>(::GeneratorType::UNKNOWN));
  }

  TRI_ASSERT(it != ::factories.end());

  return (*it).second(server, allowUserKeys, options);
}

/// @brief validate a key
int KeyGenerator::validate(char const* p, size_t length, bool isRestore) {
  return globalCheck(p, length, isRestore);
}

/// @brief check global key attributes
int KeyGenerator::globalCheck(char const* p, size_t length, bool isRestore) {
  // user has specified a key
  if (length > 0 && !_allowUserKeys && !isRestore) {
    // we do not allow user-generated keys
    return TRI_ERROR_ARANGO_DOCUMENT_KEY_UNEXPECTED;
  }

  if (length == 0 || length > maxKeyLength) {
    // user key is empty or user key is too long
    return TRI_ERROR_ARANGO_DOCUMENT_KEY_BAD;
  }

  // validate user-supplied key
  if (!validateKey(p, length)) {
    return TRI_ERROR_ARANGO_DOCUMENT_KEY_BAD;
  }

  return TRI_ERROR_NO_ERROR;
}

/// @brief validate a key
bool KeyGenerator::validateKey(char const* key, size_t len) {
  if (len == 0 || len > maxKeyLength) {
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

/// @brief validate a document id (collection name + / + document key)
bool KeyGenerator::validateId(char const* key, size_t len, size_t* split) {
  if (len < 3) {
    // 3 bytes is the minimum length for any document id
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
  if (split != nullptr) {
    *split = pos;
  }
  ++p;
  ++pos;

  // validate document key
  return KeyGenerator::validateKey(p, len - pos);
}
