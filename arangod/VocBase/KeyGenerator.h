////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2014-2021 ArangoDB GmbH, Cologne, Germany
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
#include "VocBase/vocbase.h"

#include <array>
#include <string>

namespace arangodb {
namespace application_features {
class ApplicationServer;
}
namespace velocypack {
class Builder;
class Slice;
}  // namespace velocypack

// static helper functions for key generators
struct KeyGeneratorHelper {
  static std::string encodePadded(uint64_t value);
  static uint64_t decodePadded(char const* p, size_t length);
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
  KeyGenerator(KeyGenerator const&) = delete;
  KeyGenerator& operator=(KeyGenerator const&) = delete;

 protected:
  /// @brief create the generator
  explicit KeyGenerator(bool allowUserKeys);

 public:
  /// @brief destroy the generator
  virtual ~KeyGenerator() = default;

  /// @brief create a key generator based on the options specified
  static KeyGenerator* factory(application_features::ApplicationServer&,
                               arangodb::velocypack::Slice);

  /// @brief whether or not the key generator has dynamic state
  /// that needs to be stored and recovered
  virtual bool hasDynamicState() const { return true; }

  /// @brief generate a key
  /// if the returned string is empty, it means no proper key was
  /// generated, and the caller must handle the situation
  virtual std::string generate() = 0;

  /// @brief validate a key
  virtual ErrorCode validate(char const* p, size_t length, bool isRestore);

  /// @brief track usage of a key
  virtual void track(char const* p, size_t length) = 0;

  /// @brief build a VelocyPack representation of the generator in the builder
  virtual void toVelocyPack(arangodb::velocypack::Builder&) const;

  /// @brief validate a key
  static bool validateKey(char const* key, size_t len);

  /// @brief validate a document id (collection name + / + document key)
  static bool validateId(char const* key, size_t len, size_t* split = nullptr);

  /// @brief maximum length of a key in a collection
  static constexpr size_t maxKeyLength = 254;

 protected:
  /// @brief check global key attributes
  ErrorCode globalCheck(char const* p, size_t length, bool isRestore);

 protected:
  /// @brief whether or not the users can specify their own keys
  bool const _allowUserKeys;
  /// @brief whether or not we are running on a DB server
  bool const _isDBServer;
};

}  // namespace arangodb

#endif
