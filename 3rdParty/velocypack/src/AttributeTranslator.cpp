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

#include "velocypack/velocypack-common.h"
#include "velocypack/AttributeTranslator.h"
#include "velocypack/Builder.h"
#include "velocypack/Exception.h"
#include "velocypack/Iterator.h"
#include "velocypack/Value.h"

using namespace arangodb::velocypack;

AttributeTranslator::~AttributeTranslator() {
  delete _builder;
}

void AttributeTranslator::add(std::string const& key, uint64_t id) {
  if (_builder == nullptr) {
    _builder = new Builder;
    _builder->add(Value(ValueType::Object));
  }

  _builder->add(key, Value(id));
  _count++;
}

void AttributeTranslator::seal() {
  if (_builder == nullptr) {
    return;
  }

  _builder->close();

  Slice s(_builder->slice());

  ObjectIterator it(s);

  while (it.valid()) {
    Slice const key(it.key(false));
    VELOCYPACK_ASSERT(key.isString());

    // extract key value
    ValueLength len;
    char const* p = key.getString(len);
    // insert into string and char lookup maps
    _keyToIdString.emplace(key.copyString(), it.value().begin());
    _keyToIdStringRef.emplace(StringRef(p, len), it.value().begin());
    // insert into id to slice lookup map
    _idToKey.emplace(it.value().getUInt(), key.begin());
    it.next();
  }
}

// translate from string to id
uint8_t const* AttributeTranslator::translate(std::string const& key) const noexcept {
  auto it = _keyToIdString.find(key);

  if (it == _keyToIdString.end()) {
    return nullptr;
  }

  return (*it).second;
}

// translate from string to id
uint8_t const* AttributeTranslator::translate(char const* key,
                                              ValueLength length) const noexcept {
  auto it = _keyToIdStringRef.find(StringRef(key, length));

  if (it == _keyToIdStringRef.end()) {
    return nullptr;
  }

  return (*it).second;
}

// translate from id to string
uint8_t const* AttributeTranslator::translate(uint64_t id) const noexcept {
  auto it = _idToKey.find(id);

  if (it == _idToKey.end()) {
    return nullptr;
  }

  return (*it).second;
}
