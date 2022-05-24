////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2014-2022 ArangoDB GmbH, Cologne, Germany
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
/// @author Manuel Pöter
////////////////////////////////////////////////////////////////////////////////

#include "RandomStringGenerator.h"

#include <cmath>
#include <cstdint>
#include <string>
#include <string_view>

#include "Basics/debugging.h"
#include "velocypack/Builder.h"

namespace {
constexpr std::string_view charset =
    "abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789.,";

}

namespace arangodb::sepp::generators {

RandomStringGenerator::RandomStringGenerator(std::uint32_t size) : _size(size) {
  // TODO - seed properly (make seed configurable and use different but
  // deterministic seeds for different threads)
  _prng.seed(0, 0xdeadbeefdeadbeefULL);
}

void RandomStringGenerator::apply(velocypack::Builder& builder) {
  std::string result;
  result.reserve(_size);

  constexpr std::uint32_t charsetSize = 64;
  static_assert(charset.size() == charsetSize);
  constexpr std::uint32_t bitsPerChar = 6;
  constexpr std::uint32_t charsPerRound = 64u / bitsPerChar;

  auto rounds = _size / charsPerRound;
  for (unsigned i = 0; i < rounds; ++i) {
    auto v = _prng.next();
    for (unsigned j = 0; j < charsPerRound; ++j) {
      result.push_back(charset[v % charsetSize]);
      v >>= bitsPerChar;
    }
  }

  TRI_ASSERT(_size - result.size() < charsPerRound);
  auto v = _prng.next();
  while (result.size() < _size) {
    result.push_back(charset[v % charsetSize]);
    v >>= bitsPerChar;
  }

  builder.add(VPackValue(result));
}

}  // namespace arangodb::sepp::generators
