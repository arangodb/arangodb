////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2020-2021 ArangoDB GmbH, Cologne, Germany
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
/// @author Tobias Gödderz
////////////////////////////////////////////////////////////////////////////////

#pragma once

#include <algorithm>
#include <cstdint>

namespace arangodb {

// @brief  Build an iterator based on an index and an accessor function (which
// must know its container).
// This is rudimentary and maybe not complete.
template <typename F, typename R = std::invoke_result_t<F, std::size_t>>
class IndexIter : public std::iterator<std::random_access_iterator_tag, std::remove_reference_t<R>> {
 public:
  explicit IndexIter(F accessor, std::size_t i)
      : _get(std::move(accessor)), _idx(i) {}

  auto operator++() -> IndexIter& {
    ++_idx;
    return *this;
  }
  auto operator++(int) -> IndexIter {
    IndexIter ret = *this;
    ++(*this);
    return ret;
  }

  auto operator==(IndexIter other) const -> bool { return _idx == other._idx; }
  auto operator!=(IndexIter other) const -> bool { return !(*this == other); }

  auto operator*() const -> R { return _get(_idx); }

 private:
  F _get;
  std::size_t _idx{};
};

template <typename F>
IndexIter(F, std::size_t) -> IndexIter<F>;

// @brief Create an accessor for a container that accesses via operator[].
constexpr auto accessByBrackets = [](auto& container) {
  using R = decltype(container.at(std::declval<std::size_t>()));
  return [&container](std::size_t i) -> R { return container.at(i); };
};

// @brief We often need a (begin, end) pair of iterators: This is a convenience
// function constructing both.
template <typename F>
auto makeIndexIterPair(F const& accessor, std::size_t i, std::size_t k)
    -> std::pair<IndexIter<F>, IndexIter<F>> {
  return std::make_pair(IndexIter(accessor, i), IndexIter(accessor, k));
}

}  // namespace arangodb
