// Copyright 2018 The Abseil Authors.
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//      https://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

// Implementation details for `absl::bind_front()`.

#ifndef IRESEARCH_ABSL_FUNCTIONAL_INTERNAL_FRONT_BINDER_H_
#define IRESEARCH_ABSL_FUNCTIONAL_INTERNAL_FRONT_BINDER_H_

#include <cstddef>
#include <type_traits>
#include <utility>

#include "absl/base/internal/invoke.h"
#include "absl/container/internal/compressed_tuple.h"
#include "absl/meta/type_traits.h"
#include "absl/utility/utility.h"

namespace iresearch_absl {
IRESEARCH_ABSL_NAMESPACE_BEGIN
namespace functional_internal {

// Invoke the method, expanding the tuple of bound arguments.
template <class R, class Tuple, size_t... Idx, class... Args>
R Apply(Tuple&& bound, iresearch_absl::index_sequence<Idx...>, Args&&... free) {
  return base_internal::invoke(
      iresearch_absl::forward<Tuple>(bound).template get<Idx>()...,
      iresearch_absl::forward<Args>(free)...);
}

template <class F, class... BoundArgs>
class FrontBinder {
  using BoundArgsT = iresearch_absl::container_internal::CompressedTuple<F, BoundArgs...>;
  using Idx = iresearch_absl::make_index_sequence<sizeof...(BoundArgs) + 1>;

  BoundArgsT bound_args_;

 public:
  template <class... Ts>
  constexpr explicit FrontBinder(iresearch_absl::in_place_t, Ts&&... ts)
      : bound_args_(iresearch_absl::forward<Ts>(ts)...) {}

  template <class... FreeArgs, class R = base_internal::invoke_result_t<
                                   F&, BoundArgs&..., FreeArgs&&...>>
  R operator()(FreeArgs&&... free_args) & {
    return functional_internal::Apply<R>(bound_args_, Idx(),
                                         iresearch_absl::forward<FreeArgs>(free_args)...);
  }

  template <class... FreeArgs,
            class R = base_internal::invoke_result_t<
                const F&, const BoundArgs&..., FreeArgs&&...>>
  R operator()(FreeArgs&&... free_args) const& {
    return functional_internal::Apply<R>(bound_args_, Idx(),
                                         iresearch_absl::forward<FreeArgs>(free_args)...);
  }

  template <class... FreeArgs, class R = base_internal::invoke_result_t<
                                   F&&, BoundArgs&&..., FreeArgs&&...>>
  R operator()(FreeArgs&&... free_args) && {
    // This overload is called when *this is an rvalue. If some of the bound
    // arguments are stored by value or rvalue reference, we move them.
    return functional_internal::Apply<R>(iresearch_absl::move(bound_args_), Idx(),
                                         iresearch_absl::forward<FreeArgs>(free_args)...);
  }

  template <class... FreeArgs,
            class R = base_internal::invoke_result_t<
                const F&&, const BoundArgs&&..., FreeArgs&&...>>
  R operator()(FreeArgs&&... free_args) const&& {
    // This overload is called when *this is an rvalue. If some of the bound
    // arguments are stored by value or rvalue reference, we move them.
    return functional_internal::Apply<R>(iresearch_absl::move(bound_args_), Idx(),
                                         iresearch_absl::forward<FreeArgs>(free_args)...);
  }
};

template <class F, class... BoundArgs>
using bind_front_t = FrontBinder<decay_t<F>, iresearch_absl::decay_t<BoundArgs>...>;

}  // namespace functional_internal
IRESEARCH_ABSL_NAMESPACE_END
}  // namespace absl

#endif  // IRESEARCH_ABSL_FUNCTIONAL_INTERNAL_FRONT_BINDER_H_
