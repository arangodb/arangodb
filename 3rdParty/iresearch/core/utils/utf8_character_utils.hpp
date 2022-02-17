////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2021 ArangoDB GmbH, Cologne, Germany
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

#ifndef IRESEARCH_UTF8_CHARACTER_UTILS_H
#define IRESEARCH_UTF8_CHARACTER_UTILS_H

#include "utils/utf8_character_tables.hpp"

namespace iresearch {
namespace utf8_utils {

////////////////////////////////////////////////////////////////////////////////
/// @returns true if a specified character 'c' is a whitespace according to
/// unicode standard, 
/// @note for details see https://unicode.org/reports
////////////////////////////////////////////////////////////////////////////////
constexpr bool char_is_white_space(uint32_t c) noexcept {
  return 0 != WHITE_SPACE_TABLE.count(c);
}

struct GeneralCategory {
  static constexpr auto Cc = std::make_pair('C','c');  // Other: Control
  static constexpr auto Cf = std::make_pair('C','f');  // Other: Format
  static constexpr auto Cn = std::make_pair('C','n');  // Other: Unassigned
  static constexpr auto Co = std::make_pair('C','o');  // Other: Private use
  static constexpr auto Cs = std::make_pair('C','s');  // Other: Surrogate
  static constexpr auto Ll = std::make_pair('L','l');  // Letter: Lowercase letter
  static constexpr auto Lm = std::make_pair('L','m');  // Letter: Modifier letter
  static constexpr auto Lo = std::make_pair('L','o');  // Letter: Other letter
  static constexpr auto Lt = std::make_pair('L','t');  // Letter: Titlecase letter
  static constexpr auto Lu = std::make_pair('L','u');  // Letter: Uppercase letter
  static constexpr auto Mc = std::make_pair('M','c');  // Mark: Spacing mark
  static constexpr auto Me = std::make_pair('M','e');  // Mark: Enclosing mark
  static constexpr auto Mn = std::make_pair('M','n');  // Mark: Nonspacing mark
  static constexpr auto Nd = std::make_pair('N','d');  // Number: Decimal number
  static constexpr auto Nl = std::make_pair('N','l');  // Number: Letter number
  static constexpr auto No = std::make_pair('N','o');  // Number: Other number
  static constexpr auto Pc = std::make_pair('P','c');  // Punctuation: Connector punctuation
  static constexpr auto Pd = std::make_pair('P','d');  // Punctuation: Dash punctuation
  static constexpr auto Pe = std::make_pair('P','e');  // Punctuation: Close punctuation
  static constexpr auto Pf = std::make_pair('P','f');  // Punctuation: Final punctuation
  static constexpr auto Pi = std::make_pair('P','i');  // Punctuation: Initial punctuation
  static constexpr auto Po = std::make_pair('P','o');  // Punctuation: Other punctuation
  static constexpr auto Ps = std::make_pair('P','s');  // Punctuation: Open punctuation
  static constexpr auto Sc = std::make_pair('S','c');  // Symbol: Currency symbol
  static constexpr auto Sk = std::make_pair('S','k');  // Symbol: Modifier symbol
  static constexpr auto Sm = std::make_pair('S','m');  // Symbol: Math symbol
  static constexpr auto So = std::make_pair('S','o');  // Symbol: Other symbol
  static constexpr auto Zl = std::make_pair('Z','l');  // Separator: Line separator
  static constexpr auto Zp = std::make_pair('Z','p');  // Separator: Paragraph separator
  static constexpr auto Zs = std::make_pair('Z','s');  // Separator: Space separator

  GeneralCategory() = delete;
};

constexpr uint16_t char_general_category(uint32_t c) noexcept {
  const auto it = frozen::bits::lower_bound<GENERAL_CATEGORY_TABLE.size()>(
    GENERAL_CATEGORY_TABLE.begin(), c, GENERAL_CATEGORY_TABLE.key_comp());

  if (it != GENERAL_CATEGORY_TABLE.begin() && it->first != c) {
    return std::prev(it)->second;
  }
  return it->second;
}

constexpr char char_primary_category(uint32_t c) noexcept {
  return char(char_general_category(c) >> 8);
}

constexpr bool char_is_alphanumeric(uint32_t c) noexcept {
  const auto g = char_primary_category(c);

  return g == 'L' || g == 'N';
}

} // utf8_utils
} // iresearch

#endif // IRESEARCH_UTF8_CHARACTER_UTILS_H
