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
/// @author Roman Rabinovich
////////////////////////////////////////////////////////////////////////////////

#pragma once

#include <vector>

class DisjointSet {
 public:
  explicit DisjointSet(size_t hintSize = 0);

  auto addSingleton(size_t element, size_t hintSize = 0) -> bool;

  /**
   * Get the representative of element, compress the path from element to the
   * representative.
   * @param element an element
   * @return the representative
   */
  [[nodiscard]] auto representative(size_t element) -> size_t;

  /**
   * Makes the set of first and the set of second be one set.
   * @param first an element
   * @param second another element
   * @return the representative of the united set
   */
  auto merge(size_t first, size_t second) -> size_t;

  [[nodiscard]] auto capacity() const -> size_t;
  [[nodiscard]] auto contains(size_t element) const -> bool;
  /**
   * Remove an element from its set. If the element is not in any set, do
   * nothing.
   * @param element
   */
  auto removeElement(size_t element) -> void;
  auto print() -> void;

  auto writeSCCIntoVertices() -> size_t;

 private:
  std::vector<size_t> _parent;
  // rank 0 means: element not in any set
  // rank 1 means: the root (the representative)
  std::vector<size_t> _rank;
};
