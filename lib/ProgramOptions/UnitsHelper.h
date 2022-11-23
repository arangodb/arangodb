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
/// @author Jan Steemann
////////////////////////////////////////////////////////////////////////////////

#pragma once

#include "Basics/Arithmetic.h"
#include "Basics/NumberUtils.h"

#include <array>
#include <cstdint>
#include <limits>
#include <numeric>
#include <stdexcept>
#include <string>
#include <string_view>

namespace arangodb::options::UnitsHelper {

// turns a number string with an optional unit suffix into a numeric
// value. will throw std::out_of_range for invalid values
template<typename T, typename internal>
inline T parseNumberWithUnit(std::string_view value, T base = 1) {
  constexpr internal oneKiB = 1'024;
  constexpr internal oneKB = 1'000;

  constexpr std::array<std::pair<std::string_view, internal>, 28> units = {{
      {std::string_view{"kib"}, oneKiB},
      {std::string_view{"KiB"}, oneKiB},
      {std::string_view{"KIB"}, oneKiB},
      {std::string_view{"mib"}, oneKiB * oneKiB},
      {std::string_view{"MiB"}, oneKiB * oneKiB},
      {std::string_view{"MIB"}, oneKiB * oneKiB},
      {std::string_view{"gib"}, oneKiB * oneKiB * oneKiB},
      {std::string_view{"GiB"}, oneKiB * oneKiB * oneKiB},
      {std::string_view{"GIB"}, oneKiB * oneKiB * oneKiB},
      {std::string_view{"tib"}, oneKiB * oneKiB * oneKiB * oneKiB},
      {std::string_view{"TiB"}, oneKiB * oneKiB * oneKiB * oneKiB},
      {std::string_view{"TIB"}, oneKiB * oneKiB * oneKiB * oneKiB},
      {std::string_view{"kb"}, oneKB},
      {std::string_view{"KB"}, oneKB},
      {std::string_view{"mb"}, oneKB * oneKB},
      {std::string_view{"MB"}, oneKB * oneKB},
      {std::string_view{"gb"}, oneKB * oneKB * oneKB},
      {std::string_view{"GB"}, oneKB * oneKB * oneKB},
      {std::string_view{"tb"}, oneKB * oneKB * oneKB * oneKB},
      {std::string_view{"TB"}, oneKB * oneKB * oneKB * oneKB},
      {std::string_view{"k"}, oneKB},
      {std::string_view{"K"}, oneKB},
      {std::string_view{"m"}, oneKB * oneKB},
      {std::string_view{"M"}, oneKB * oneKB},
      {std::string_view{"g"}, oneKB * oneKB * oneKB},
      {std::string_view{"G"}, oneKB * oneKB * oneKB},
      {std::string_view{"t"}, oneKB * oneKB * oneKB * oneKB},
      {std::string_view{"T"}, oneKB * oneKB * oneKB * oneKB},
  }};

  // handle unit suffixes
  internal m = 1;
  std::string_view suffix;
  for (auto const& unit : units) {
    if (value.ends_with(unit.first)) {
      suffix = unit.first;
      m = unit.second;
      break;
    }
  }

  // handle % suffix
  internal d = 1;
  if (suffix.empty() && value.ends_with('%')) {
    suffix = "%";
    m = static_cast<internal>(base);
    d = 100;
  }

  bool valid = true;
  auto v = arangodb::NumberUtils::atoi<T>(
      value.data(), value.data() + value.size() - suffix.size(), valid);
  if (!valid) {
    throw std::out_of_range(std::string{value});
  }
  if (isUnsafeMultiplication(static_cast<internal>(v), m)) {
    throw std::out_of_range(std::string{value});
  }
  internal r = static_cast<internal>(v) * m;
  if (r < std::numeric_limits<T>::min() || r > std::numeric_limits<T>::max()) {
    throw std::out_of_range(std::string{value});
  }
  return static_cast<T>(v * m / d);
}

}  // namespace arangodb::options::UnitsHelper
