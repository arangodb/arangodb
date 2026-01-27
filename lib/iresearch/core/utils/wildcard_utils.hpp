////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2019 ArangoDB GmbH, Cologne, Germany
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
/// @author Andrey Abramov
////////////////////////////////////////////////////////////////////////////////

#pragma once

#include "automaton.hpp"
#include "string.hpp"

namespace irs {

enum class WildcardType {
  kTermEscaped = 0,  // f\%o
  kTerm,             // foo
  kPrefixEscaped,    // fo\%
  kPrefix,           // foo%
  kWildcard,         // f_o%
};

WildcardType ComputeWildcardType(bytes_view pattern) noexcept;

enum WildcardMatch : uint8_t {
  kAnyStr = '%',   // match any number of arbitrary characters
  kAnyChr = '_',   // match a single arbitrary character
  kEscape = '\\',  // escape control symbol, e.g. "\%" issues literal "%"
};

////////////////////////////////////////////////////////////////////////////////
/// @brief instantiates minimal DFA from a specified UTF-8 encoded wildcard
///        sequence
/// @returns DFA accpeting a specified wildcard expression
/// @note control symbols are WildcardMatch
/// @note non UTF-8 bytes transition instatiated as bytes
/// @note invalid last escape, instatiate transition as byte
////////////////////////////////////////////////////////////////////////////////
automaton FromWildcard(bytes_view expr);

inline automaton FromWildcard(std::string_view expr) {
  return FromWildcard(ViewCast<byte_type>(expr));
}

}  // namespace irs
