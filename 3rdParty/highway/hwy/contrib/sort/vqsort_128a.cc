// Copyright 2021 Google LLC
// SPDX-License-Identifier: Apache-2.0
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//      http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

#include "hwy/contrib/sort/vqsort.h"

#undef HWY_TARGET_INCLUDE
#define HWY_TARGET_INCLUDE "hwy/contrib/sort/vqsort_128a.cc"
#include "hwy/foreach_target.h"  // IWYU pragma: keep

// After foreach_target
#include "hwy/contrib/sort/traits128-inl.h"
#include "hwy/contrib/sort/vqsort-inl.h"

HWY_BEFORE_NAMESPACE();
namespace hwy {
namespace HWY_NAMESPACE {

void Sort128Asc(uint64_t* HWY_RESTRICT keys, size_t num) {
#if VQSORT_ENABLED
  SortTag<uint64_t> d;
  detail::SharedTraits<detail::Traits128<detail::OrderAscending128>> st;
  Sort(d, st, keys, num);
#else
  (void)keys;
  (void)num;
  HWY_ASSERT(0);
#endif
}

// NOLINTNEXTLINE(google-readability-namespace-comments)
}  // namespace HWY_NAMESPACE
}  // namespace hwy
HWY_AFTER_NAMESPACE();

#if HWY_ONCE
namespace hwy {
namespace {
HWY_EXPORT(Sort128Asc);
}  // namespace

void VQSort(uint128_t* HWY_RESTRICT keys, size_t n, SortAscending) {
  HWY_DYNAMIC_DISPATCH(Sort128Asc)
  (reinterpret_cast<uint64_t*>(keys), n * 2);
}

}  // namespace hwy
#endif  // HWY_ONCE
