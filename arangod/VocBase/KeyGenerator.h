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
#include "VocBase/vocbase.h"

#include <array>

namespace arangodb {
namespace velocypack {
class Builder;
class Slice;
}

class KeyGenerator {
 protected:
  /// @brief create the generator
  explicit KeyGenerator(bool allowUserKeys);

 public:
  /// @brief destroy the generator
  virtual ~KeyGenerator() = default;

  /// @brief check if the specified key generator in the VelocyPack
  /// can be used in this setup (cluster / single-server)
  static bool canUseType(arangodb::velocypack::Slice const&);

  /// @brief create a key generator based on the options specified
  static KeyGenerator* factory(arangodb::velocypack::Slice const&);

  virtual bool trackKeys() const = 0;

  /// @brief generate a key
  virtual std::string generate() = 0;

  /// @brief validate a key
  virtual int validate(char const* p, size_t length, bool isRestore);

  /// @brief track usage of a key
  virtual void track(char const* p, size_t length) = 0;

  /// @brief build a VelocyPack representation of the generator in the builder
  virtual void toVelocyPack(arangodb::velocypack::Builder&) const = 0;

  /// @brief check global key attributes
  int globalCheck(char const* p, size_t length, bool isRestore);
  
  /// @brief validate a key
  static bool validateKey(char const* key, size_t len);
 
  /// @brief maximum length of a key in a collection
  static constexpr size_t maxKeyLength = 254;

 protected:
  /// @brief whether or not the users can specify their own keys
  bool _allowUserKeys;
};

}

/// @brief validate a document id (collection name + / + document key)
bool TRI_ValidateDocumentIdKeyGenerator(char const*, size_t, size_t*);

#endif
