////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2014-2024 ArangoDB GmbH, Cologne, Germany
/// Copyright 2004-2014 triAGENS GmbH, Cologne, Germany
///
/// Licensed under the Business Source License 1.1 (the "License");
/// you may not use this file except in compliance with the License.
/// You may obtain a copy of the License at
///
///     https://github.com/arangodb/arangodb/blob/devel/LICENSE
///
/// Unless required by applicable law or agreed to in writing, software
/// distributed under the License is distributed on an "AS IS" BASIS,
/// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
/// See the License for the specific language governing permissions and
/// limitations under the License.
///
/// Copyright holder is ArangoDB GmbH, Cologne, Germany
////////////////////////////////////////////////////////////////////////////////

#include "ByteString.h"
#include <stdexcept>

arangodb::byte_string operator"" _bs(const char* const str, std::size_t len) {
  using namespace std::string_literals;

  std::string normalizedInput{};
  normalizedInput.reserve(len);
  for (char const* p = str; *p != '\0'; ++p) {
    switch (*p) {
      case '0':
      case '1':
        normalizedInput += *p;
        break;
      case ' ':
      case '\'':
        // skip whitespace and single quotes
        break;
      default:
        throw std::invalid_argument{"Unexpected character "s + *p +
                                    " in byte string: " + str};
    }
  }

  if (normalizedInput.empty()) {
    throw std::invalid_argument{"Empty byte string"};
  }

  auto result = arangodb::byte_string{};

  char const* p = normalizedInput.c_str();
  for (auto bitIdx = 0; *p != '\0'; bitIdx = 0) {
    result += std::byte{0};
    for (; *p != '\0' && bitIdx < 8; ++bitIdx) {
      switch (*p) {
        case '0':
          break;
        case '1': {
          auto const bitPos = 7 - bitIdx;
          result.back() |= (std::byte{1} << bitPos);
          break;
        }
        default:
          throw std::invalid_argument{"Unexpected character "s + *p +
                                      " in byte string: " + str};
      }

      ++p;
      // skip whitespace and single quotes
      while (*p == ' ' || *p == '\'') {
        ++p;
      }
    }
  }

  return result;
}

arangodb::byte_string arangodb::operator"" _bss(const char* str,
                                                std::size_t len) {
  return byte_string{reinterpret_cast<const std::byte*>(str), len};
}
