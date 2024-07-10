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

#pragma once

#include "VocBase/vocbase.h"

#include <array>
#include <memory>
#include <string>
#include <string_view>
#include <vector>

namespace arangodb {
class KeyGenerator;
class LogicalCollection;

namespace application_features {
class ApplicationServer;
}
namespace velocypack {
class Builder;
class Slice;
}  // namespace velocypack

// static helper functions for key generators
struct KeyGeneratorHelper {
  // smallest possible key
  static std::string const lowestKey;
  // greatest possible key
  static std::string const highestKey;

  static std::vector<std::string> generatorNames();

  static uint64_t decodePadded(char const* p, size_t length) noexcept;
  static std::string encodePadded(uint64_t value);

  /// @brief validate a key
  static bool validateKey(char const* key, size_t len) noexcept;

  /// @brief validate a document id (collection name + / + document key)
  static bool validateId(char const* key, size_t len, bool extendedNames,
                         size_t& split) noexcept;

  /// @brief create a key generator based on the options specified
  static std::unique_ptr<KeyGenerator> createKeyGenerator(
      LogicalCollection const& collection, velocypack::Slice);

  static std::unique_ptr<KeyGenerator> createEnterpriseKeyGenerator(
      std::unique_ptr<KeyGenerator> generator);

  /// @brief maximum length of a key in a collection
  static constexpr size_t maxKeyLength = 254;
};

/// generic key generator interface
///
/// please note that coordinator-based key generators are frequently
/// created and discarded, so ctor & dtor need to be very efficient.
/// additionally, do not put any state into this object, as for the
/// same logical collection the ClusterInfo may create many different
/// temporary LogicalCollection objects one after the other, which
/// will also discard the collection's particular KeyGenerator object!
class KeyGenerator {
  friend struct KeyGeneratorHelper;

  KeyGenerator(KeyGenerator const&) = delete;
  KeyGenerator& operator=(KeyGenerator const&) = delete;

 protected:
  /// @brief create the generator
  explicit KeyGenerator(LogicalCollection const& collection,
                        bool allowUserKeys);

 public:
  /// @brief destroy the generator
  virtual ~KeyGenerator() = default;

  /// @brief whether or not the key generator has dynamic state
  /// that needs to be stored and recovered
  virtual bool hasDynamicState() const noexcept { return true; }

  /// @brief generate a key
  /// if the returned string is empty, it means no proper key was
  /// generated, and the caller must handle the situation
  virtual std::string generate(velocypack::Slice input) = 0;

  /// @brief validate a key
  virtual ErrorCode validate(std::string_view key, velocypack::Slice input,
                             bool isRestore) noexcept;

  /// @brief track usage of a key
  virtual void track(std::string_view key) noexcept = 0;

  /// @brief build a VelocyPack representation of the generator in the builder
  virtual void toVelocyPack(velocypack::Builder&) const;

  /// @brief initialize key generator state, reading data/state from the
  /// state object. state is guaranteed to be a velocypack object
  virtual void initState(velocypack::Slice state);

  bool allowUserKeys() const noexcept { return _allowUserKeys; }

  LogicalCollection const& collection() const noexcept { return _collection; }

 protected:
  /// @brief check global key attributes
  ErrorCode globalCheck(char const* p, size_t length,
                        bool isRestore) const noexcept;

  LogicalCollection const& _collection;

  /// @brief whether or not the users can specify their own keys
  bool const _allowUserKeys;
};

/// @brief wrapper class that wraps another KeyGenerator.
/// all public methods delegate to the wrapped KeyGenerator.
/// can be used as a base class for creating own KeyGenerators
/// that reuse functionality from other KeyGenerators.
class KeyGeneratorWrapper : public KeyGenerator {
 public:
  /// @brief create the generator
  explicit KeyGeneratorWrapper(std::unique_ptr<KeyGenerator> wrapped)
      : KeyGenerator(wrapped->collection(), wrapped->allowUserKeys()),
        _wrapped(std::move(wrapped)) {}

  /// @brief whether or not the key generator has dynamic state
  /// that needs to be stored and recovered
  bool hasDynamicState() const noexcept override {
    return _wrapped->hasDynamicState();
  }

  /// @brief generate a key
  /// if the returned string is empty, it means no proper key was
  /// generated, and the caller must handle the situation
  std::string generate(velocypack::Slice input) override {
    return _wrapped->generate(input);
  }

  /// @brief validate a key
  ErrorCode validate(std::string_view key, velocypack::Slice input,
                     bool isRestore) noexcept override {
    return _wrapped->validate(key, input, isRestore);
  }

  /// @brief track usage of a key
  void track(std::string_view key) noexcept override { _wrapped->track(key); }

  /// @brief build a VelocyPack representation of the generator in the builder
  void toVelocyPack(velocypack::Builder& result) const override {
    _wrapped->toVelocyPack(result);
  }

 protected:
  std::unique_ptr<KeyGenerator> _wrapped;
};

}  // namespace arangodb
