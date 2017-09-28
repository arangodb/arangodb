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

#ifndef ARANGOD_VOC_BASE_KEY_GENERATOR_H
#define ARANGOD_VOC_BASE_KEY_GENERATOR_H 1

#include "Basics/Common.h"
#include "Basics/Mutex.h"
#include "VocBase/vocbase.h"

#include <array>

/// @brief maximum length of a key in a collection
#define TRI_VOC_KEY_MAX_LENGTH (254)

namespace arangodb {
namespace velocypack {
class Builder;
class Slice;
}

class KeyGenerator {
 public:
  /// @brief available key generators
  enum GeneratorType {
    TYPE_UNKNOWN = 0,
    TYPE_TRADITIONAL = 1,
    TYPE_AUTOINCREMENT = 2
  };

 protected:
  /// @brief create the generator
  explicit KeyGenerator(bool);

 public:
  /// @brief destroy the generator
  virtual ~KeyGenerator();

 public:
  /// @brief initialize the lookup table for key checks
  static void Initialize();

  /// @brief get the generator type from VelocyPack
  static GeneratorType generatorType(arangodb::velocypack::Slice const&);

  /// @brief create a key generator based on the options specified
  static KeyGenerator* factory(arangodb::velocypack::Slice const&);

 public:
  virtual bool trackKeys() const = 0;

  /// @brief generate a key
  virtual std::string generate() = 0;

  /// @brief validate a key
  virtual int validate(char const* p, size_t length, bool isRestore) = 0;

  /// @brief track usage of a key
  virtual void track(char const* p, size_t length) = 0;

  /// @brief return a VelocyPack representation of the generator
  std::shared_ptr<arangodb::velocypack::Builder> toVelocyPack() const;

  /// @brief build a VelocyPack representation of the generator in the builder
  virtual void toVelocyPack(arangodb::velocypack::Builder&) const = 0;

  /// @brief check global key attributes
  int globalCheck(char const* p, size_t length, bool isRestore);

 protected:
  /// @brief whether or not the users can specify their own keys
  bool _allowUserKeys;

  /// @brief lookup table for key checks
  static std::array<bool, 256> LookupTable;
};

class TraditionalKeyGenerator final : public KeyGenerator {
 public:
  /// @brief create the generator
  explicit TraditionalKeyGenerator(bool);

  /// @brief destroy the generator
  ~TraditionalKeyGenerator();

 public:
  /// @brief validate a key
  static bool validateKey(char const* key, size_t len);

 public:
  
  bool trackKeys() const override { return true; }

  /// @brief generate a key
  std::string generate() override;

  /// @brief validate a key
  int validate(char const* p, size_t length, bool isRestore) override;

  /// @brief track usage of a key
  void track(char const* p, size_t length) override final;

  /// @brief return the generator name (must be lowercase)
  static std::string name() { return "traditional"; }

  /// @brief build a VelocyPack representation of the generator in the builder
  virtual void toVelocyPack(arangodb::velocypack::Builder&) const override;

 private:
  arangodb::Mutex _lock;

  uint64_t _lastValue;
};

class AutoIncrementKeyGenerator final : public KeyGenerator {
 public:
  /// @brief create the generator
  AutoIncrementKeyGenerator(bool, uint64_t, uint64_t);

  /// @brief destroy the generator
  ~AutoIncrementKeyGenerator();

 public:
  /// @brief validate a key
  static bool validateKey(char const* key, size_t len);

 public:

  bool trackKeys() const override { return true; }

  /// @brief generate a key
  std::string generate() override;

  /// @brief validate a key
  int validate(char const* p, size_t length, bool isRestore) override;

  /// @brief track usage of a key
  void track(char const* p, size_t length) override final;

  /// @brief return the generator name (must be lowercase)
  static std::string name() { return "autoincrement"; }

  /// @brief build a VelocyPack representation of the generator in the builder
  virtual void toVelocyPack(arangodb::velocypack::Builder&) const override;

 private:
  arangodb::Mutex _lock;

  uint64_t _lastValue;  // last value assigned

  uint64_t _offset;  // start value

  uint64_t _increment;  // increment value
};

}

/// @brief validate a document id (collection name + / + document key)
bool TRI_ValidateDocumentIdKeyGenerator(char const*, size_t, size_t*);

#endif
