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

#pragma once

#include "utils/utf8_character_tables.hpp"

namespace irs::utf8_utils {

// Returns true if a specified character 'c' is a whitespace according to
// unicode standard, for details see https://unicode.org/reports
constexpr bool CharIsWhiteSpace(uint32_t c) noexcept {
  return kWhiteSpaceTable.count(c) != 0;
}

constexpr uint16_t CharGeneralCategory(uint32_t c) noexcept {
  const auto it = frozen::bits::lower_bound<kGeneralCategoryTable.size()>(
    kGeneralCategoryTable.begin(), c, kGeneralCategoryTable.key_comp());

  if (it != kGeneralCategoryTable.begin() && it->first != c) {
    return std::prev(it)->second;
  }
  return it->second;
}

constexpr char CharPrimaryCategory(uint32_t c) noexcept {
  return static_cast<char>(CharGeneralCategory(c) >> 8U);
}

constexpr bool CharIsAlphanumeric(uint32_t c) noexcept {
  const auto g = CharPrimaryCategory(c);

  return g == 'L' || g == 'N';
}

}  // namespace irs::utf8_utils
