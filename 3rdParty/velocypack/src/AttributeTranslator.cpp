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
    _keyToId.emplace(it.key(false).copyString(), it.value().begin());
    _idToKey.emplace(it.value().getUInt(), it.key(false).begin());
    it.next();
  }
}

// translate from string to id
uint8_t const* AttributeTranslator::translate(std::string const& key) const {
  auto it = _keyToId.find(key);

  if (it == _keyToId.end()) {
    return nullptr;
  }

  return (*it).second;
}

// translate from string to id
uint8_t const* AttributeTranslator::translate(char const* key,
                                              ValueLength length) const {
  auto it = _keyToId.find(std::string(key, checkOverflow(length)));

  if (it == _keyToId.end()) {
    return nullptr;
  }

  return (*it).second;
}

// translate from id to string
uint8_t const* AttributeTranslator::translate(uint64_t id) const {
  auto it = _idToKey.find(id);

  if (it == _idToKey.end()) {
    return nullptr;
  }

  return (*it).second;
}
