////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2014-2016 ArangoDB GmbH, Cologne, Germany
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

#include "Arithmetic.h"

#include <algorithm>
#include <limits>

namespace {

/// @brief returns whether or not the string is empty
bool isEmptyString(char const* p, size_t length) noexcept {
  char const* e = p + length;

  while (p < e) {
    if (*p != ' ' && *p != '\t' && *p != '\r' && *p != '\n' && *p != '\f' && *p != '\b') {
      return false;
    }
    ++p;
  }

  return true;
}

}  // namespace

namespace arangodb {
namespace aql {

double stringToNumber(std::string const& value, bool& failed) noexcept {
  failed = false;
  try {
    size_t behind = 0;
    double v = std::stod(value, &behind);
    while (behind < value.size()) {
      char c = value[behind];
      if (c != ' ' && c != '\t' && c != '\r' && c != '\n' && c != '\f') {
        failed = true;
        return 0.0;
      }
      ++behind;
    }
    return v;
  } catch (...) {
    if (!isEmptyString(value.data(), value.size())) {
      failed = true;
    }
  }
  return 0.0;
}

template <typename T>
bool isUnsafeAddition(T l, T r) {
  return ((r > 0 && l > (std::numeric_limits<T>::max)() - r) ||
          (r < 0 && l < (std::numeric_limits<T>::min)() - r));
}

template <typename T>
bool isUnsafeSubtraction(T l, T r) {
  return ((r > 0 && l < (std::numeric_limits<T>::min)() + r) ||
          (r < 0 && l > (std::numeric_limits<T>::max)() + r));
}

template <typename T>
bool isUnsafeMultiplication(T l, T r) {
  if (l > 0) {
    if (r > 0) {
      if (l > ((std::numeric_limits<T>::max)() / r)) {
        return true;
      }
    } else {
      if (r < ((std::numeric_limits<T>::min)() / l)) {
        return true;
      }
    }
  } else {
    if (r > 0) {
      if (l < ((std::numeric_limits<T>::min)() / r)) {
        return true;
      }
    } else {
      if ((l != 0) && (r < ((std::numeric_limits<T>::max)() / l))) {
        return true;
      }
    }
  }

  return false;
}

template <typename T>
bool isUnsafeDivision(T l, T r) {
  // note: the caller still has to check whether r is zero (division by zero)
  return (l == (std::numeric_limits<T>::min)() && r == -1);
}

template bool isUnsafeAddition<int64_t>(int64_t l, int64_t r);
template bool isUnsafeSubtraction<int64_t>(int64_t l, int64_t r);
template bool isUnsafeMultiplication<int64_t>(int64_t l, int64_t r);
template bool isUnsafeDivision<int64_t>(int64_t l, int64_t r);

}  // namespace aql
}  // namespace arangodb
