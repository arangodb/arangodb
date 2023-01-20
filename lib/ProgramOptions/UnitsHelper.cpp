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

#include "UnitsHelper.h"

#include <array>
#include <cstdint>
#include <string_view>
#include <utility>

namespace arangodb::options::UnitsHelper {

std::pair<std::string_view, uint64_t> extractSuffix(std::string_view value) {
  constexpr uint64_t oneKiB = 1'024;
  constexpr uint64_t oneKB = 1'000;

  // note: longer strings must come first in this array!
  constexpr std::array<std::pair<std::string_view, uint64_t>, 30> units = {{
      // three letter units
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
      // two letter units
      {std::string_view{"kb"}, oneKB},
      {std::string_view{"KB"}, oneKB},
      {std::string_view{"mb"}, oneKB * oneKB},
      {std::string_view{"MB"}, oneKB * oneKB},
      {std::string_view{"gb"}, oneKB * oneKB * oneKB},
      {std::string_view{"GB"}, oneKB * oneKB * oneKB},
      {std::string_view{"tb"}, oneKB * oneKB * oneKB * oneKB},
      {std::string_view{"TB"}, oneKB * oneKB * oneKB * oneKB},
      // single letter units
      {std::string_view{"k"}, oneKB},
      {std::string_view{"K"}, oneKB},
      {std::string_view{"m"}, oneKB * oneKB},
      {std::string_view{"M"}, oneKB * oneKB},
      {std::string_view{"g"}, oneKB * oneKB * oneKB},
      {std::string_view{"G"}, oneKB * oneKB * oneKB},
      {std::string_view{"t"}, oneKB * oneKB * oneKB * oneKB},
      {std::string_view{"T"}, oneKB * oneKB * oneKB * oneKB},
      {std::string_view{"b"}, 1},
      {std::string_view{"B"}, 1},
  }};

  // handle unit suffixes
  for (auto const& unit : units) {
    if (value.ends_with(unit.first)) {
      return {unit.first, unit.second};
    }
  }
  return {"", 1};
}

}  // namespace arangodb::options::UnitsHelper
