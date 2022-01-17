////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2014-2020 ArangoDB GmbH, Cologne, Germany
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
/// @author Max Neunhoeffer
/// @author Jan Steemann
////////////////////////////////////////////////////////////////////////////////

#pragma once

#include <cstdint>
#include <memory>
#include <string_view>
#include <unordered_map>

#include "velocypack/velocypack-common.h"

namespace arangodb::velocypack {
class Builder;

class AttributeTranslator {
 public:
  AttributeTranslator(AttributeTranslator const&) = delete;
  AttributeTranslator& operator=(AttributeTranslator const&) = delete;

  AttributeTranslator(); 

  ~AttributeTranslator();

  std::size_t count() const { return _count; }

  void add(std::string_view key, uint64_t id);

  void seal();

  Builder* builder() const { return _builder.get(); }
  
  // translate from string to id
  uint8_t const* translate(std::string_view key) const noexcept {
    auto it = _keyToId.find(key);

    if (it == _keyToId.end()) {
      return nullptr;
    }

    return (*it).second;
  }
  
  // translate from string to id
  [[deprecated]] inline uint8_t const* translate(char const* key, ValueLength length) const noexcept {
    return translate(std::string_view(key, length));
  }

  // translate from id to string
  uint8_t const* translate(uint64_t id) const noexcept {
    auto it = _idToKey.find(id);

    if (it == _idToKey.end()) {
      return nullptr;
    }

    return (*it).second;
  }

 private:
  std::unique_ptr<Builder> _builder;
  std::unordered_map<std::string_view, uint8_t const*> _keyToId;
  std::unordered_map<uint64_t, uint8_t const*> _idToKey;
  std::size_t _count;
};

class AttributeTranslatorScope {
 private:
  AttributeTranslatorScope(AttributeTranslatorScope const&) = delete;
  AttributeTranslatorScope& operator= (AttributeTranslatorScope const&) = delete;

 public:
  explicit AttributeTranslatorScope(AttributeTranslator* translator);
  ~AttributeTranslatorScope();

  void revert() noexcept;

 private:
  AttributeTranslator* _old;
};

}  // namespace arangodb::velocypack

using VPackAttributeTranslator = arangodb::velocypack::AttributeTranslator;
