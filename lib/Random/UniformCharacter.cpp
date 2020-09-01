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
/// @author Dr. Frank Celler
////////////////////////////////////////////////////////////////////////////////

#include "UniformCharacter.h"

#include "Random/RandomGenerator.h"

using namespace arangodb;

UniformCharacter::UniformCharacter(size_t length)
    : _length(length),
      _characters(
          "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789") {}

UniformCharacter::UniformCharacter(std::string const& characters)
    : _length(1), _characters(characters) {}

UniformCharacter::UniformCharacter(size_t length, std::string const& characters)
    : _length(length), _characters(characters) {}

char UniformCharacter::randomChar() const {
  size_t r = RandomGenerator::interval((uint32_t)(_characters.size() - 1));

  return _characters[r];
}

std::string UniformCharacter::random() const { return random(_length); }

std::string UniformCharacter::random(size_t length) const {
  std::string buffer;
  buffer.reserve(length);

  for (size_t i = 0; i < length; ++i) {
    size_t r = RandomGenerator::interval((uint32_t)(_characters.size() - 1));

    buffer.push_back(_characters[r]);
  }

  return buffer;
}
