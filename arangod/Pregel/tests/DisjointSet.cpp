#include <stdexcept>
#include "DisjointSet.h"

DisjointSet::DisjointSet(size_t hintSize) {
  if (hintSize != 0) {
    _parent.resize(hintSize);
    _rank.resize(hintSize, 0);
  }
}

auto DisjointSet::capacity() -> size_t { return _parent.size(); }

auto DisjointSet::contains(size_t element) -> bool {
  return element < capacity() and _rank[element] != 0;
}

auto DisjointSet::addSingleton(size_t element, size_t hintSize) -> bool {
  if (hintSize != 0 and element < hintSize and capacity() < hintSize) {
    _parent.resize(hintSize);
    _rank.resize(hintSize, 0);
  } else if (capacity() <= element) {
    _parent.resize(element + 1);
    _rank.resize(element + 1, 0);
  }
  if (contains(element)) {
    // already added
    return false;
  }
  _parent[element] = element;
  _rank[element] = 1; // 0 reserved for "not added"
  return true;
}

auto DisjointSet::representative(size_t element) -> size_t {
  if (element >= capacity() or not contains(element)) {
    throw std::runtime_error(
        "Asked for representative of an element that was not stored: " +
        std::to_string(element));
  }
  size_t running = element;
  // path splitting
  while (running != _parent[running]) {
    auto tmp = _parent[running];
    _parent[running] = _parent[_parent[running]];
    _rank[running] = (_rank[running] + 1) / 2;
    running = tmp;
  }
  return running;
}

auto DisjointSet::merge(size_t first, size_t second) -> void {
  size_t repr_first = representative(first);
  size_t repr_second = representative(second);
  if (repr_first == repr_second) {
    return;
  }
  if (_rank[repr_first] > _rank[repr_second]) {
    std::swap(repr_first, repr_second);
  }
  _parent[repr_first] = repr_second;
  if (_rank[repr_first] == _rank[repr_second]) {
    _rank[repr_second]++;
  }
}