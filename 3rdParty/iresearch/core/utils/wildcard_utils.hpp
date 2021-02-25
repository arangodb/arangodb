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

#ifndef IRESEARCH_WILDCARD_UTILS_H
#define IRESEARCH_WILDCARD_UTILS_H

#include "automaton.hpp"

namespace iresearch {

enum class WildcardType {
  INVALID  = 0,   // invalid input sequence
  TERM_ESCAPED,   // f\*o
  TERM,           // foo
  MATCH_ALL,      // *
  PREFIX_ESCAPED, // fo\*
  PREFIX,         // foo*
  WILDCARD        // f_o*
};

IRESEARCH_API WildcardType wildcard_type(const bytes_ref& pattern) noexcept;

enum WildcardMatch : byte_type {
  ANY_STRING = '%',
  ANY_CHAR = '_',
  ESCAPE = '\\'
};

////////////////////////////////////////////////////////////////////////////////
/// @brief instantiates minimal DFA from a specified UTF-8 encoded wildcard
///        sequence
/// @param expr valid UTF-8 encoded string
/// @returns DFA accpeting a specified wildcard expression
/// @note control symbols are
///       '%' - match any number of arbitrary characters
///       '_' - match a single arbitrary character
///       '\' - escape control symbol, e.g. '\%' issues literal '%'
/// @note if an input expression is incorrect UTF-8 sequence, function returns
///       empty automaton
////////////////////////////////////////////////////////////////////////////////
IRESEARCH_API automaton from_wildcard(const bytes_ref& expr);

inline automaton from_wildcard(const string_ref& expr) {
  return from_wildcard(ref_cast<byte_type>(expr));
}

}

#endif // IRESEARCH_WILDCARD_UTILS_H

