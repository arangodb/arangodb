////////////////////////////////////////////////////////////////////////////////
/// @brief Library to build up VPack documents.
///
/// DISCLAIMER
///
/// Copyright 2015 ArangoDB GmbH, Cologne, Germany
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
/// @author Max Neunhoeffer
/// @author Jan Steemann
/// @author Copyright 2015, ArangoDB GmbH, Cologne, Germany
////////////////////////////////////////////////////////////////////////////////

#ifndef VELOCYPACK_ATTRIBUTETRANSLATOR_H
#define VELOCYPACK_ATTRIBUTETRANSLATOR_H 1

#include <cstdint>
#include <string>
#include <unordered_map>
#include <memory>

#include "velocypack/velocypack-common.h"
#include "velocypack/Options.h"
#include "velocypack/Slice.h"

namespace arangodb {
namespace velocypack {
class Builder;

class AttributeTranslator {
 public:
  AttributeTranslator(AttributeTranslator const&) = delete;
  AttributeTranslator& operator=(AttributeTranslator const&) = delete;

  AttributeTranslator() : _builder(nullptr), _count(0) {}

  ~AttributeTranslator();

  size_t count() const { return _count; }

  void add(std::string const& key, uint64_t id);

  void seal();

  Builder* builder() { return _builder; }

  // translate from string to id
  uint8_t const* translate(std::string const& key) const;

  // translate from string to id
  uint8_t const* translate(char const* key, ValueLength length) const;

  // translate from id to string
  uint8_t const* translate(uint64_t id) const;

 private:
  Builder* _builder;
  std::unordered_map<std::string, uint8_t const*> _keyToId;
  std::unordered_map<uint64_t, uint8_t const*> _idToKey;
  size_t _count;
};

class AttributeTranslatorScope {
 private:
  AttributeTranslatorScope(AttributeTranslatorScope const&) = delete;
  AttributeTranslatorScope& operator= (AttributeTranslatorScope const&) = delete;

 public:
  explicit AttributeTranslatorScope(AttributeTranslator* translator)
      : _old(Options::Defaults.attributeTranslator) {
    Options::Defaults.attributeTranslator = translator;
  }

  ~AttributeTranslatorScope() {
    revert();
  }

  // prematurely revert the change
  void revert() {
    Options::Defaults.attributeTranslator = _old;
  }

 private:
   AttributeTranslator* _old;
};

}  // namespace arangodb::velocypack
}  // namespace arangodb

#endif
