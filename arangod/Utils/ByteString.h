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
#pragma once

#include <cstddef>
#include <string>

namespace arangodb {

inline static std::byte operator"" _b(unsigned long long b) {
  return std::byte{(unsigned char)b};
}

using byte_string = std::basic_string<std::byte>;
using byte_string_view = std::basic_string_view<std::byte>;

byte_string operator"" _bs(const char* str, std::size_t len);
byte_string operator"" _bss(const char* str, std::size_t len);

}  // namespace arangodb

std::ostream& operator<<(std::ostream& ostream,
                         arangodb::byte_string const& string);
std::ostream& operator<<(std::ostream& ostream,
                         arangodb::byte_string_view string);
