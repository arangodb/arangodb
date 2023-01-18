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

#include <cstdint>
#include <limits>
#include <numeric>
#include <stdexcept>
#include <string>
#include <string_view>

namespace arangodb::options::UnitsHelper {

std::pair<std::string_view, uint64_t> extractSuffix(std::string_view value);

// turns a number string with an optional unit suffix into a numeric
// value. will throw std::out_of_range for invalid values.
// it is required that any input value to this function is stripped of
// any leading and trailing whitespace characters. otherwise the
// function will throw an exception
template<typename T, typename internal>
inline T parseNumberWithUnit(std::string_view value, T base = 1) {
  // handle unit suffixes
  internal m = 1;
  auto [suffix, multiplier] = extractSuffix(value);
  if (!suffix.empty()) {
    m = static_cast<internal>(multiplier);
  }

  // handle % suffix
  internal d = 1;
  if (suffix.empty() && value.ends_with('%')) {
    suffix = "%";
    m = static_cast<internal>(base);
    d = 100;
  }

  bool valid = true;
  auto v = NumberUtils::atoi<T>(
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
