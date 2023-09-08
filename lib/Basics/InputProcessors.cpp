////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2014-2023 ArangoDB GmbH, Cologne, Germany
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

#include "InputProcessors.h"

#include "Assertions/Assert.h"

#include <cstdint>

namespace arangodb {

InputProcessorJSONL::InputProcessorJSONL(std::string_view data)
    : _data(data), _position(0), _parser(_builder) {
  consumeWhitespace();
}

InputProcessorJSONL::~InputProcessorJSONL() = default;

bool InputProcessorJSONL::valid() const noexcept {
  return _position < _data.size();
}

velocypack::Slice InputProcessorJSONL::value() {
  char const* p = _data.data() + _position;
  char const* e = _data.data() + _data.size();
  char const* q = p;
  TRI_ASSERT(p <= e);

  while (p < e && (*p != '\r' && *p != '\n')) {
    ++p;
  }

  _builder.clear();
  // will throw on error
  auto length = _parser.parse(_data.data() + _position, p - q);

  _position = p - q + length;
  consumeWhitespace();
  return _builder.slice();
}

void InputProcessorJSONL::consumeWhitespace() {
  char const* p = _data.data() + _position;
  char const* e = _data.data() + _data.size();
  TRI_ASSERT(p <= e);

  while (p < e && (*p == '\t' || *p == ' ')) {
    ++p;
  }

  _position = p - _data.data();
}

InputProcessorVPackArray::InputProcessorVPackArray(std::string_view data)
    : _data(reinterpret_cast<uint8_t const*>(data.data())), _iterator(_data) {}

InputProcessorVPackArray::~InputProcessorVPackArray() = default;

bool InputProcessorVPackArray::valid() const noexcept {
  return _iterator.valid();
}

velocypack::Slice InputProcessorVPackArray::value() {
  TRI_ASSERT(valid());

  auto result = _iterator.value();
  ++_iterator;
  return result;
}

}  // namespace arangodb
