#pragma once

#include <vector>

class DisjointSet {
 public:
  DisjointSet(size_t hintSize = 0);

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
   */
  auto merge(size_t first, size_t second) -> void;

  [[nodiscard]] auto capacity() -> size_t;
  [[nodiscard]] auto contains(size_t element) -> bool;
  auto print() -> void;

 private:
  std::vector<size_t> _parent;
  // rank 0 means: element not in any set
  // rank 1 means: the root (the representative)
  std::vector<size_t> _rank;
};
