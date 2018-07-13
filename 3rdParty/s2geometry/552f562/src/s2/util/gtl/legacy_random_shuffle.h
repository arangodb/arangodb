// Copyright 2018 Google Inc. All Rights Reserved.
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//     http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS-IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.
//

// gtl::legacy_random_shuffle is similar in API and behavior to
// std::random_shuffle, which was removed in C++17.
//
// When built for Linux production targets using crosstool 18,
// these APIs produce the same results as std::random_shuffle.
//
// Otherwise, the specification for these functions reverts to that
// of std::random_shuffle as specified in C++11. In particular,
// these functions do not promise to produce the same shuffle
// sequences forever.
//
// These are deprecated, and intended to be used only for legacy
// code that must move off std::random_shuffle simply because the
// function is not part of C++17.
#ifndef S2_UTIL_GTL_LEGACY_RANDOM_SHUFFLE_H_
#define S2_UTIL_GTL_LEGACY_RANDOM_SHUFFLE_H_

#include <algorithm>
#include <cstdlib>
#include <iterator>

#include "s2/third_party/absl/base/macros.h"

namespace gtl {

// Reorders the elements in the range `[begin, last)` randomly.  The
// random number generator `rnd` must be a function object returning a
// randomly chosen value of type convertible to and from
// `std::iterator_traits<RandomIt>::difference_type` in the interval
// `[0,n)` if invoked as `r(n)`.
//
// This function is deprecated.  See the file comment above for
// additional details.
template <class RandomIt, class RandomFunc>
ABSL_DEPRECATED("Use std::shuffle instead; see go/nors-legacy-api")
void legacy_random_shuffle(const RandomIt begin, const RandomIt end,
                           RandomFunc&& rnd) {
  auto size = std::distance(begin, end);
  for (decltype(size) i = 1; i < size; ++i) {
    // Loop invariant: elements below i are uniformly shuffled.
    std::iter_swap(begin + i, begin + rnd(i + 1));
  }
}

// Reorders the elements in the range `[begin, last)` randomly.  The
// random number generator is `std::rand()`.
//
// This function is deprecated.  See the file comment above for
// additional details.
template <class RandomIt>
ABSL_DEPRECATED("Use std::shuffle instead; see go/nors-legacy-api")
void legacy_random_shuffle(RandomIt begin, RandomIt end) {
  legacy_random_shuffle(
      begin, end,
      [](typename std::iterator_traits<RandomIt>::difference_type i) {
        return std::rand() % i;
      });
}

}  // namespace gtl

#endif  // S2_UTIL_GTL_LEGACY_RANDOM_SHUFFLE_H_
