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
///
/// @author Jan Steemann
////////////////////////////////////////////////////////////////////////////////

#pragma once

#include <algorithm>
#include <limits>

namespace arangodb {

template<typename T>
bool isUnsafeAddition(T l, T r) {
  return ((r > 0 && l > (std::numeric_limits<T>::max)() - r) ||
          (r < 0 && l < (std::numeric_limits<T>::min)() - r));
}

template<typename T>
bool isUnsafeSubtraction(T l, T r) {
  return ((r > 0 && l < (std::numeric_limits<T>::min)() + r) ||
          (r < 0 && l > (std::numeric_limits<T>::max)() + r));
}

template<typename T>
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

template<typename T>
bool isUnsafeDivision(T l, T r) {
  // note: the caller still has to check whether r is zero (division by zero)
  return (l == (std::numeric_limits<T>::min)() && r == -1);
}

}  // namespace arangodb
