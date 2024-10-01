////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2014-2024 ArangoDB GmbH, Cologne, Germany
/// Copyright 2004-2014 triAGENS GmbH, Cologne, Germany
///
/// Licensed under the Business Source License 1.1 (the "License");
/// you may not use this file except in compliance with the License.
/// You may obtain a copy of the License at
///
///     https://github.com/arangodb/arangodb/blob/devel/LICENSE
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
#include "Basics/NumberUtils.h"
#include "Basics/StaticStrings.h"
#include "Basics/StringUtils.h"
#include "Basics/VelocyPackHelper.h"
#include "Basics/debugging.h"
#include "Basics/system-compiler.h"
#include "Cluster/ClusterFeature.h"
#include "Cluster/ClusterInfo.h"
#include "Cluster/ServerState.h"
#include "Utilities/NameValidator.h"
#include "VocBase/LogicalCollection.h"
#include "VocBase/ticks.h"
#include "VocBase/vocbase.h"

#include <boost/uuid/random_generator.hpp>
#include <boost/uuid/uuid.hpp>
#include <boost/uuid/uuid_io.hpp>

#include <array>
#include <limits>
#include <string>
#include <string_view>

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
  kUnknown = 0,
  kTraditional = 1,
  kAutoincrement = 2,
  kUuid = 3,
  kPadded = 4
};

/// @brief for older compilers
using GeneratorMapType = std::underlying_type<GeneratorType>::type;

uint64_t readLastValue(velocypack::Slice options) {
  uint64_t lastValue = 0;

  if (VPackSlice lastValueSlice = options.get(StaticStrings::LastValue);
      lastValueSlice.isNumber()) {
    double v = lastValueSlice.getNumericValue<double>();
    if (v < 0.0) {
      // negative lastValue is not allowed
      THROW_ARANGO_EXCEPTION_MESSAGE(
          TRI_ERROR_ARANGO_INVALID_KEY_GENERATOR,
          "'lastValue' value must be greater than zero");
    }

    lastValue = lastValueSlice.getNumericValue<uint64_t>();
  }
  return lastValue;
}

/// Actual key generators following...

/// @brief base class for traditional key generators
class TraditionalKeyGenerator : public KeyGenerator {
 public:
  /// @brief create the generator
  explicit TraditionalKeyGenerator(LogicalCollection const& collection,
                                   bool allowUserKeys)
      : KeyGenerator(collection, allowUserKeys) {}

  bool hasDynamicState() const noexcept override final { return true; }

  /// @brief generate a key
  std::string generate(velocypack::Slice /*input*/) override final {
    uint64_t tick = generateValue();

    if (ADB_UNLIKELY(tick == 0)) {
      // unlikely case we have run out of keys
      // returning an empty string will trigger an error on the call site
      return std::string();
    }

    return basics::StringUtils::itoa(tick);
  }

  /// @brief validate a key
  ErrorCode validate(std::string_view key, velocypack::Slice input,
                     bool isRestore) noexcept override {
    auto res = KeyGenerator::validate(key, input, isRestore);

    if (res == TRI_ERROR_NO_ERROR) {
      track(key);
    }

    return res;
  }

  /// @brief track usage of a key
  void track(std::string_view key) noexcept override {
    char const* p = key.data();
    size_t length = key.size();
    // check the numeric key part
    if (length > 0 && p[0] >= '0' && p[0] <= '9') {
      // potentially numeric key
      uint64_t value = NumberUtils::atoi_zero<uint64_t>(p, p + length);

      if (value > 0) {
        trackValue(value);
      }
    }
  }

  /// @brief build a VelocyPack representation of the generator in the builder
  void toVelocyPack(velocypack::Builder& builder) const override {
    KeyGenerator::toVelocyPack(builder);
    builder.add("type", VPackValue("traditional"));
  }

 protected:
  /// @brief generate a new key value (internal)
  virtual uint64_t generateValue() = 0;

  /// @brief track a value (internal)
  virtual void trackValue(uint64_t value) noexcept = 0;
};

/// @brief traditional key generator for a single server
class TraditionalKeyGeneratorSingle final : public TraditionalKeyGenerator {
 public:
  /// @brief create the generator
  explicit TraditionalKeyGeneratorSingle(LogicalCollection const& collection,
                                         bool allowUserKeys, uint64_t lastValue)
      : TraditionalKeyGenerator(collection, allowUserKeys),
        _lastValue(lastValue) {
    TRI_ASSERT(!ServerState::instance()->isCoordinator());
  }

  /// @brief build a VelocyPack representation of the generator in the builder
  void toVelocyPack(velocypack::Builder& builder) const override {
    TraditionalKeyGenerator::toVelocyPack(builder);

    // add our specific stuff
    builder.add(StaticStrings::LastValue,
                VPackValue(_lastValue.load(std::memory_order_relaxed)));
  }

 private:
  /// @brief generate a key value (internal)
  uint64_t generateValue() override {
    TRI_ASSERT(ServerState::instance()->isSingleServer() ||
               _collection.numberOfShards() == 1);
    TRI_IF_FAILURE("KeyGenerator::generateOnSingleServer") {
      // for testing purposes only
      THROW_ARANGO_EXCEPTION(TRI_ERROR_DEBUG);
    }

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
    } while (!_lastValue.compare_exchange_weak(lastValue, tick,
                                               std::memory_order_relaxed));

    return tick;
  }

  /// @brief track a key value (internal)
  void trackValue(uint64_t value) noexcept override {
    auto lastValue = _lastValue.load(std::memory_order_relaxed);
    while (value > lastValue) {
      // and update our last value
      if (_lastValue.compare_exchange_weak(lastValue, value,
                                           std::memory_order_relaxed)) {
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
class TraditionalKeyGeneratorCoordinator final
    : public TraditionalKeyGenerator {
 public:
  explicit TraditionalKeyGeneratorCoordinator(
      ClusterInfo& ci, LogicalCollection const& collection, bool allowUserKeys)
      : TraditionalKeyGenerator(collection, allowUserKeys), _ci(ci) {
    TRI_ASSERT(ServerState::instance()->isCoordinator());
  }

 private:
  /// @brief generate a key value (internal)
  uint64_t generateValue() override {
    TRI_ASSERT(ServerState::instance()->isCoordinator());
    TRI_ASSERT(_collection.numberOfShards() != 1 || _collection.isSmart());
    TRI_IF_FAILURE("KeyGenerator::generateOnCoordinator") {
      // for testing purposes only
      THROW_ARANGO_EXCEPTION(TRI_ERROR_DEBUG);
    }
    return _ci.uniqid();
  }

  /// @brief track a key value (internal)
  void trackValue(uint64_t /* value */) noexcept override {}

 private:
  ClusterInfo& _ci;
};

/// @brief base class for padded key generators
class PaddedKeyGenerator : public KeyGenerator {
 public:
  /// @brief create the generator
  explicit PaddedKeyGenerator(LogicalCollection const& collection,
                              bool allowUserKeys, uint64_t lastValue)
      : KeyGenerator(collection, allowUserKeys), _lastValue(lastValue) {}

  bool hasDynamicState() const noexcept override final { return true; }

  /// @brief generate a key
  std::string generate(velocypack::Slice /*input*/) override final {
    uint64_t tick = generateValue();

    if (ADB_UNLIKELY(tick == 0 || tick == UINT64_MAX)) {
      // unlikely case we have run out of keys
      // returning an empty string will trigger an error on the call site
      return std::string();
    }

    auto lastValue = _lastValue.load(std::memory_order_relaxed);
    if (ADB_UNLIKELY(lastValue >= UINT64_MAX - 1ULL)) {
      // oops, out of keys!
      return std::string();
    }

    do {
      if (tick <= lastValue) {
        tick = _lastValue.fetch_add(1, std::memory_order_relaxed) + 1;
        break;
      }
    } while (!_lastValue.compare_exchange_weak(lastValue, tick,
                                               std::memory_order_relaxed));

    return KeyGeneratorHelper::encodePadded(tick);
  }

  /// @brief validate a key
  ErrorCode validate(std::string_view key, velocypack::Slice input,
                     bool isRestore) noexcept override {
    auto res = KeyGenerator::validate(key, input, isRestore);

    if (res == TRI_ERROR_NO_ERROR) {
      track(key);
    }

    return res;
  }

  /// @brief track usage of a key
  void track(std::string_view key) noexcept override final {
    // check the numeric key part
    uint64_t value = KeyGeneratorHelper::decodePadded(key.data(), key.size());
    if (value > 0) {
      auto lastValue = _lastValue.load(std::memory_order_relaxed);
      while (value > lastValue) {
        // and update our last value
        if (_lastValue.compare_exchange_weak(lastValue, value,
                                             std::memory_order_relaxed)) {
          break;
        }
      }
    }
  }

  /// @brief build a VelocyPack representation of the generator in the builder
  void toVelocyPack(velocypack::Builder& builder) const override final {
    KeyGenerator::toVelocyPack(builder);
    builder.add("type", VPackValue("padded"));

    // add our own specific values
    builder.add(StaticStrings::LastValue,
                VPackValue(_lastValue.load(std::memory_order_relaxed)));
  }

  /// @brief initialize key generator state, reading data/state from the
  /// state object. state is guaranteed to be a velocypack object
  void initState(velocypack::Slice state) override {
    TRI_ASSERT(state.isObject());
    // special case here: we read a numeric, UNENCODED lastValue attribute from
    // the state object, but we need to pass an ENCODED value to the track()
    // method.
    track(KeyGeneratorHelper::encodePadded(::readLastValue(state)));
  }

 protected:
  /// @brief generate a key value (internal)
  virtual uint64_t generateValue() = 0;

 private:
  std::atomic<uint64_t> _lastValue;
};

/// @brief padded key generator for a single server
class PaddedKeyGeneratorSingle final : public PaddedKeyGenerator {
 public:
  /// @brief create the generator
  explicit PaddedKeyGeneratorSingle(LogicalCollection const& collection,
                                    bool allowUserKeys, uint64_t lastValue)
      : PaddedKeyGenerator(collection, allowUserKeys, lastValue) {
    TRI_ASSERT(!ServerState::instance()->isCoordinator());
  }

 private:
  /// @brief generate a key
  uint64_t generateValue() override {
    TRI_ASSERT(ServerState::instance()->isSingleServer() ||
               _collection.numberOfShards() == 1);
    TRI_IF_FAILURE("KeyGenerator::generateOnSingleServer") {
      // for testing purposes only
      THROW_ARANGO_EXCEPTION(TRI_ERROR_DEBUG);
    }
    return TRI_NewTickServer();
  }
};

/// @brief padded key generator for a coordinator
/// please note that coordinator-based key generators are frequently
/// created and discarded, so ctor & dtor need to be very efficient.
/// additionally, do not put any state into this object, as for the
/// same logical collection the ClusterInfo may create many different
/// temporary LogicalCollection objects one after the other, which
/// will also discard the collection's particular KeyGenerator object!
class PaddedKeyGeneratorCoordinator final : public PaddedKeyGenerator {
 public:
  explicit PaddedKeyGeneratorCoordinator(ClusterInfo& ci,
                                         LogicalCollection const& collection,
                                         bool allowUserKeys, uint64_t lastValue)
      : PaddedKeyGenerator(collection, allowUserKeys, lastValue), _ci(ci) {
    TRI_ASSERT(ServerState::instance()->isCoordinator());
  }

 private:
  /// @brief generate a key value (internal)
  uint64_t generateValue() override {
    TRI_ASSERT(ServerState::instance()->isCoordinator());
    TRI_ASSERT(_collection.numberOfShards() != 1 || _collection.isSmart());
    TRI_IF_FAILURE("KeyGenerator::generateOnCoordinator") {
      // for testing purposes only
      THROW_ARANGO_EXCEPTION(TRI_ERROR_DEBUG);
    }
    return _ci.uniqid();
  }

 private:
  ClusterInfo& _ci;
};

/// @brief auto-increment key generator - not usable in cluster
class AutoIncrementKeyGenerator final : public KeyGenerator {
 public:
  /// @brief create the generator
  AutoIncrementKeyGenerator(LogicalCollection const& collection,
                            bool allowUserKeys, uint64_t lastValue,
                            uint64_t offset, uint64_t increment)
      : KeyGenerator(collection, allowUserKeys),
        _lastValue(lastValue),
        _offset(offset),
        _increment(increment) {}

  bool hasDynamicState() const noexcept override { return true; }

  /// @brief generate a key
  std::string generate(velocypack::Slice /*input*/) override {
    TRI_ASSERT(ServerState::instance()->isSingleServer() ||
               _collection.numberOfShards() == 1);
    uint64_t keyValue;

    auto lastValue = _lastValue.load(std::memory_order_relaxed);
    do {
      // user has not specified a key, generate one based on algorithm
      if (lastValue < _offset) {
        keyValue = _offset;
      } else {
        TRI_ASSERT(_increment > 0);
        keyValue =
            lastValue + _increment - ((lastValue - _offset) % _increment);
      }

      // bounds and validity checks
      if (keyValue == UINT64_MAX || keyValue < lastValue) {
        return "";
      }

      TRI_ASSERT(keyValue > lastValue);
      // update our last value
    } while (!_lastValue.compare_exchange_weak(lastValue, keyValue,
                                               std::memory_order_relaxed));

    return basics::StringUtils::itoa(keyValue);
  }

  /// @brief validate a key
  ErrorCode validate(std::string_view key, velocypack::Slice input,
                     bool isRestore) noexcept override {
    auto res = KeyGenerator::validate(key, input, isRestore);

    if (res == TRI_ERROR_NO_ERROR) {
      char const* s = key.data();
      char const* e = s + key.size();
      TRI_ASSERT(s != e);
      do {
        if (*s < '0' || *s > '9') {
          return TRI_ERROR_ARANGO_DOCUMENT_KEY_BAD;
        }
      } while (++s < e);

      track(key);
    }

    return res;
  }

  /// @brief track usage of a key
  void track(std::string_view key) noexcept override {
    char const* p = key.data();
    size_t length = key.size();
    // check the numeric key part
    if (length > 0 && p[0] >= '0' && p[0] <= '9') {
      uint64_t value = NumberUtils::atoi_zero<uint64_t>(p, p + length);

      if (value > 0) {
        auto lastValue = _lastValue.load(std::memory_order_relaxed);
        while (value > lastValue) {
          // and update our last value
          if (_lastValue.compare_exchange_weak(lastValue, value,
                                               std::memory_order_relaxed)) {
            break;
          }
        }
      }
    }
  }

  /// @brief build a VelocyPack representation of the generator in the builder
  void toVelocyPack(velocypack::Builder& builder) const override {
    KeyGenerator::toVelocyPack(builder);
    builder.add("type", VPackValue("autoincrement"));

    builder.add("offset", VPackValue(_offset));
    builder.add("increment", VPackValue(_increment));
    builder.add(StaticStrings::LastValue,
                VPackValue(_lastValue.load(std::memory_order_relaxed)));
  }

 private:
  std::atomic<uint64_t> _lastValue;  // last value assigned
  uint64_t const _offset;            // start value
  uint64_t const _increment;         // increment value
};

/// @brief uuid key generator
class UuidKeyGenerator final : public KeyGenerator {
 public:
  /// @brief create the generator
  explicit UuidKeyGenerator(LogicalCollection const& collection,
                            bool allowUserKeys)
      : KeyGenerator(collection, allowUserKeys) {}

  bool hasDynamicState() const noexcept override { return false; }

  /// @brief generate a key
  std::string generate(velocypack::Slice /*input*/) override {
    std::lock_guard locker{_lock};
    return boost::uuids::to_string(_uuid());
  }

  /// @brief track usage of a key
  void track(std::string_view /*key*/) noexcept override {}

  void toVelocyPack(velocypack::Builder& builder) const override {
    KeyGenerator::toVelocyPack(builder);
    builder.add("type", VPackValue("uuid"));
  }

 private:
  std::mutex _lock;

  boost::uuids::random_generator _uuid;
};

/// @brief all generators, by name
std::unordered_map<std::string, GeneratorType> const generatorNames = {
    {"traditional", GeneratorType::kTraditional},
    {"autoincrement", GeneratorType::kAutoincrement},
    {"uuid", GeneratorType::kUuid},
    {"padded", GeneratorType::kPadded}};

/// @brief get the generator type from VelocyPack
GeneratorType generatorType(VPackSlice parameters) {
  if (!parameters.isObject()) {
    // the default
    return GeneratorType::kTraditional;
  }

  VPackSlice type = parameters.get("type");
  if (!type.isString()) {
    return GeneratorType::kTraditional;
  }

  std::string typeName = basics::StringUtils::tolower(type.copyString());

  auto it = generatorNames.find(typeName);

  if (it != generatorNames.end()) {
    return (*it).second;
  }

  return GeneratorType::kUnknown;
}

std::unordered_map<GeneratorMapType,
                   std::function<std::unique_ptr<KeyGenerator>(
                       LogicalCollection const&, VPackSlice)>> const factories =
    {{static_cast<GeneratorMapType>(GeneratorType::kUnknown),
      [](LogicalCollection const&,
         VPackSlice) -> std::unique_ptr<KeyGenerator> {
        // unknown key generator type
        THROW_ARANGO_EXCEPTION_MESSAGE(TRI_ERROR_ARANGO_INVALID_KEY_GENERATOR,
                                       "invalid key generator type");
      }},
     {static_cast<GeneratorMapType>(GeneratorType::kTraditional),
      [](LogicalCollection const& collection,
         VPackSlice options) -> std::unique_ptr<KeyGenerator> {
        bool allowUserKeys = basics::VelocyPackHelper::getBooleanValue(
            options, StaticStrings::AllowUserKeys, true);

        if (ServerState::instance()->isCoordinator()) {
          auto& ci = collection.vocbase()
                         .server()
                         .getFeature<ClusterFeature>()
                         .clusterInfo();
          return std::make_unique<TraditionalKeyGeneratorCoordinator>(
              ci, collection, allowUserKeys);
        }
        return std::make_unique<TraditionalKeyGeneratorSingle>(
            collection, allowUserKeys, ::readLastValue(options));
      }},
     {static_cast<GeneratorMapType>(GeneratorType::kAutoincrement),
      [](LogicalCollection const& collection,
         VPackSlice options) -> std::unique_ptr<KeyGenerator> {
        if (!ServerState::instance()->isSingleServer() &&
            collection.numberOfShards() > 1) {
          THROW_ARANGO_EXCEPTION_MESSAGE(
              TRI_ERROR_CLUSTER_UNSUPPORTED,
              "the specified key generator is not "
              "supported for collections with more than one shard");
        }

        uint64_t offset = 0;
        uint64_t increment = 1;

        if (VPackSlice incrementSlice = options.get("increment");
            incrementSlice.isNumber()) {
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

        if (VPackSlice offsetSlice = options.get("offset");
            offsetSlice.isNumber()) {
          double v = offsetSlice.getNumericValue<double>();
          if (v < 0.0) {
            // negative or 0 offset is not allowed
            THROW_ARANGO_EXCEPTION_MESSAGE(
                TRI_ERROR_ARANGO_INVALID_KEY_GENERATOR,
                "offset value must be zero or greater");
          }

          offset = offsetSlice.getNumericValue<uint64_t>();

          if (offset >= UINT64_MAX) {
            THROW_ARANGO_EXCEPTION_MESSAGE(
                TRI_ERROR_ARANGO_INVALID_KEY_GENERATOR,
                "offset value is too high");
          }
        }

        bool allowUserKeys = basics::VelocyPackHelper::getBooleanValue(
            options, StaticStrings::AllowUserKeys, true);

        return std::make_unique<AutoIncrementKeyGenerator>(
            collection, allowUserKeys, ::readLastValue(options), offset,
            increment);
      }},
     {static_cast<GeneratorMapType>(GeneratorType::kUuid),
      [](LogicalCollection const& collection,
         VPackSlice options) -> std::unique_ptr<KeyGenerator> {
        bool allowUserKeys = basics::VelocyPackHelper::getBooleanValue(
            options, StaticStrings::AllowUserKeys, true);

        return std::make_unique<UuidKeyGenerator>(collection, allowUserKeys);
      }},
     {static_cast<GeneratorMapType>(GeneratorType::kPadded),
      [](LogicalCollection const& collection,
         VPackSlice options) -> std::unique_ptr<KeyGenerator> {
        bool allowUserKeys = basics::VelocyPackHelper::getBooleanValue(
            options, StaticStrings::AllowUserKeys, true);

        if (ServerState::instance()->isCoordinator()) {
          auto& ci = collection.vocbase()
                         .server()
                         .getFeature<ClusterFeature>()
                         .clusterInfo();
          return std::make_unique<PaddedKeyGeneratorCoordinator>(
              ci, collection, allowUserKeys, ::readLastValue(options));
        }
        return std::make_unique<PaddedKeyGeneratorSingle>(
            collection, allowUserKeys, ::readLastValue(options));
      }}};

}  // namespace

// smallest possible key
std::string const KeyGeneratorHelper::lowestKey;
// greatest possible key
std::string const KeyGeneratorHelper::highestKey(
    KeyGeneratorHelper::maxKeyLength,
    std::numeric_limits<std::string::value_type>::max());

std::vector<std::string> KeyGeneratorHelper::generatorNames() {
  std::vector<std::string> names;
  names.reserve(::generatorNames.size());
  for (auto const& it : ::generatorNames) {
    names.push_back(it.first);
  }
  return names;
}

uint64_t KeyGeneratorHelper::decodePadded(char const* data,
                                          size_t length) noexcept {
  uint64_t result = 0;

  if (length != sizeof(uint64_t) * 2) {
    return result;
  }

  char const* p = data;
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
    result += ((high << 4) | low) << (uint64_t(8) * ((e - p) / 2));
  }

  return result;
}

std::string KeyGeneratorHelper::encodePadded(uint64_t value) {
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

/// @brief validate a key
bool KeyGeneratorHelper::validateKey(char const* key, size_t len) noexcept {
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

  TRI_ASSERT(std::string_view(key, len) > lowestKey);
  TRI_ASSERT(std::string_view(key, len) <= highestKey);
  return true;
}

/// @brief validate a document id (collection name + / + document key)
bool KeyGeneratorHelper::validateId(char const* key, size_t len,
                                    bool extendedNames,
                                    size_t& split) noexcept {
  // look for split character
  char const* found = static_cast<char const*>(memchr(key, '/', len));
  if (found == nullptr) {
    split = 0;
    return false;
  }

  split = found - key;
  if (split == 0 || split + 1 == len) {
    return false;
  }

  TRI_ASSERT(found != nullptr && split > 0);
  // check for numeric collection id
  char const* p = key;
  char const* e = key + split;
  TRI_ASSERT(p != e);
  while (p < e && *p >= '0' && *p <= '9') {
    ++p;
  }
  if (p == key) {
    // non-numeric id
    try {
      if (CollectionNameValidator::validateName(
              /*allowSystem*/ true, extendedNames, std::string_view(key, split))
              .fail()) {
        return false;
      }
    } catch (...) {
      return false;
    }
  } else {
    // numeric id. now check if it is all-numeric
    if (p != key + split) {
      return false;
    }
  }

  // validate document key
  return KeyGeneratorHelper::validateKey(key + split + 1, len - split - 1);
}

/// @brief create a key generator based on the options specified
std::unique_ptr<KeyGenerator> KeyGeneratorHelper::createKeyGenerator(
    LogicalCollection const& collection, VPackSlice options) {
  if (!options.isObject()) {
    options = VPackSlice::emptyObjectSlice();
  }

  auto type = ::generatorType(options);

  auto it = ::factories.find(static_cast<::GeneratorMapType>(type));

  if (it == ::factories.end()) {
    it = ::factories.find(
        static_cast<::GeneratorMapType>(::GeneratorType::kUnknown));
  }

  TRI_ASSERT(it != ::factories.end());

  // create key generator based on what the user requested
  std::unique_ptr<KeyGenerator> generator = (*it).second(collection, options);

  // optionally wrap it into a KeyGenerator for a smart graph collection,
  // if necessary
  return createEnterpriseKeyGenerator(std::move(generator));
}

#ifndef USE_ENTERPRISE

std::unique_ptr<KeyGenerator> KeyGeneratorHelper::createEnterpriseKeyGenerator(
    std::unique_ptr<KeyGenerator> generator) {
  // nothing to be done here
  return generator;
}

#endif

/// @brief create the key generator
KeyGenerator::KeyGenerator(LogicalCollection const& collection,
                           bool allowUserKeys)
    : _collection(collection), _allowUserKeys(allowUserKeys) {}

/// @brief build a VelocyPack representation of the generator in the builder
void KeyGenerator::toVelocyPack(velocypack::Builder& builder) const {
  TRI_ASSERT(!builder.isClosed());
  builder.add(StaticStrings::AllowUserKeys, VPackValue(_allowUserKeys));
}

/// @brief initialize key generator state, reading data/state from the
/// state object. state is guaranteed to be a velocypack object
void KeyGenerator::initState(velocypack::Slice state) {
  TRI_ASSERT(state.isObject());
  // default implementation is to simply read the lastValue attribute
  // as a number, and track its stringified version
  track(std::to_string(::readLastValue(state)));
}

/// @brief validate a key
ErrorCode KeyGenerator::validate(std::string_view key,
                                 velocypack::Slice /*input*/,
                                 bool isRestore) noexcept {
  return globalCheck(key.data(), key.size(), isRestore);
}

/// @brief check global key attributes
ErrorCode KeyGenerator::globalCheck(char const* p, size_t length,
                                    bool isRestore) const noexcept {
  // user has specified a key
  if (length > 0 && !_allowUserKeys && !isRestore &&
      !ServerState::instance()->isDBServer()) {
    // we do not allow user-generated keys
    // note: on a DB server the coordinator will already have generated the key
    return TRI_ERROR_ARANGO_DOCUMENT_KEY_UNEXPECTED;
  }

  if (length == 0 || length > KeyGeneratorHelper::maxKeyLength) {
    // user key is empty or user key is too long
    return TRI_ERROR_ARANGO_DOCUMENT_KEY_BAD;
  }

  // validate user-supplied key
  if (!KeyGeneratorHelper::validateKey(p, length)) {
    return TRI_ERROR_ARANGO_DOCUMENT_KEY_BAD;
  }

  return TRI_ERROR_NO_ERROR;
}
