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

#include <cstddef>
#include <utility>

#include "Basics/debugging.h"

namespace arangodb::containers {

struct Helpers {
  /// @brief calculate capacity for the container for at least one more
  /// element.
  /// if this would exceed the container's capacity, use a growth factor of
  /// 1.5 to calculate the new capacity.
  template<typename T>
  static std::size_t nextCapacity(T const& container,
                                  std::size_t initialCapacity) {
    std::size_t capacity;
    if (container.empty()) {
      // reserve some initial space
      capacity = std::max(std::size_t(1), initialCapacity);
    } else {
      TRI_ASSERT(container.capacity() > 0);
      // minimum requirement is that we have room for at least one more element.
      capacity = container.size() + 1;
      if (capacity > container.capacity()) {
        // inspired by facebook/folly
        // (https://github.com/facebook/folly/blob/master/folly/memory/Malloc.h):
        constexpr size_t jemallocMinInPlaceExpandable = 4096;
        if (container.capacity() <
            jemallocMinInPlaceExpandable /
                std::max(sizeof(typename T::value_type),
                         alignof(typename T::value_type))) {
          capacity = container.capacity() * 2;
        } else {
          // grow with a growth factor of 1.5
          capacity = (container.size() * 3 + 1) / 2;
        }
      }
    }
    TRI_ASSERT(capacity > container.size());
    return capacity;
  }

  /// @brief reserve space for at least one more element in the container.
  /// if this would exceed the container's capacity, use a growth factor of
  /// 1.5 to grow the container's memory.
  template<typename T>
  static void reserveSpace(T& container, std::size_t initialCapacity) {
    std::size_t capacity = nextCapacity(container, initialCapacity);

    // reserve space
    if (capacity > container.capacity()) {
      // if this fails, it will simply throw a bad_alloc exception,
      // and no harm has been done to the container
      container.reserve(capacity);
    }
  }

};  // Helpers

}  // namespace arangodb::containers
