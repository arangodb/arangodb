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



#ifndef S2_UTIL_GTL_SUBTLE_CONTAINER_MEMORY_H_
#define S2_UTIL_GTL_SUBTLE_CONTAINER_MEMORY_H_

#include <cassert>
#include <cstddef>
#include <memory>
#include <tuple>
#include <utility>

#include "s2/third_party/absl/memory/memory.h"
#include "s2/third_party/absl/utility/utility.h"

namespace gtl {
namespace subtle {

// Allocates at least n bytes aligned to the specified alignment.
// Alignment must be a power of 2. It must be positive.
//
// Note that many allocators don't honor alignment requirements above certain
// threshold (usually either alignof(std::max_align_t) or alignof(void*)).
// Allocate() doesn't apply alignment corrections. If the underlying allocator
// returns insufficiently alignment pointer, that's what you are going to get.
template <size_t Alignment, class Alloc>
void* Allocate(Alloc* alloc, size_t n) {
  static_assert(Alignment > 0, "");
  assert(n && "n must be positive");
  struct alignas(Alignment) M {};
  using A = typename absl::allocator_traits<Alloc>::template rebind_alloc<M>;
  using AT = typename absl::allocator_traits<Alloc>::template rebind_traits<M>;
  A mem_alloc(*alloc);
  void* p = AT::allocate(mem_alloc, (n + sizeof(M) - 1) / sizeof(M));
  assert(reinterpret_cast<uintptr_t>(p) % Alignment == 0 &&
         "allocator does not respect alignment");
  return p;
}

// The pointer must have been previously obtained by calling
// Allocate<Alignment>(alloc, n).
template <size_t Alignment, class Alloc>
void Deallocate(Alloc* alloc, void* p, size_t n) {
  static_assert(Alignment > 0, "");
  assert(n && "n must be positive");
  struct alignas(Alignment) M {};
  using A = typename absl::allocator_traits<Alloc>::template rebind_alloc<M>;
  using AT = typename absl::allocator_traits<Alloc>::template rebind_traits<M>;
  A mem_alloc(*alloc);
  AT::deallocate(mem_alloc, static_cast<M*>(p),
                 (n + sizeof(M) - 1) / sizeof(M));
}

namespace internal_memory {

// Constructs T into uninitialized storage pointed by `ptr` using the args
// specified in the tuple.
template <class Alloc, class T, class Tuple, size_t... I>
void ConstructFromTupleImpl(Alloc* alloc, T* ptr, Tuple&& t,
                            absl::index_sequence<I...>) {
  absl::allocator_traits<Alloc>::construct(
      *alloc, ptr, std::get<I>(std::forward<Tuple>(t))...);
}

template <class T, class F>
struct WithConstructedImplF {
  template <class... Args>
  decltype(std::declval<F>()(std::declval<T>())) operator()(
      Args&&... args) const {
    return std::forward<F>(f)(T(std::forward<Args>(args)...));
  }
  F&& f;
};

template <class T, class Tuple, size_t... Is, class F>
decltype(std::declval<F>()(std::declval<T>())) WithConstructedImpl(
    Tuple&& t, absl::index_sequence<Is...>, F&& f) {
  return WithConstructedImplF<T, F>{std::forward<F>(f)}(
      std::get<Is>(std::forward<Tuple>(t))...);
}

template <class T, size_t... Is>
auto TupleRefImpl(T&& t, absl::index_sequence<Is...>)
    -> decltype(std::forward_as_tuple(std::get<Is>(std::forward<T>(t))...)) {
  return std::forward_as_tuple(std::get<Is>(std::forward<T>(t))...);
}

// Returns a tuple of references to the elements of the input tuple. T must be a
// tuple.
template <class T>
auto TupleRef(T&& t) -> decltype(
    TupleRefImpl(std::forward<T>(t),
                 absl::make_index_sequence<
                     std::tuple_size<typename std::decay<T>::type>::value>())) {
  return TupleRefImpl(
      std::forward<T>(t),
      absl::make_index_sequence<
          std::tuple_size<typename std::decay<T>::type>::value>());
}

template <class ContainerKey, class Hash, class Eq, class PassedKey>
using RequireUsableKey = std::pair<
    decltype(std::declval<const Hash&>()(std::declval<const PassedKey&>())),
    decltype(std::declval<const Eq&>()(std::declval<const ContainerKey&>(),
                                       std::declval<const PassedKey&>()))>;

template <class Key, class Hash, class Eq, class F, class K, class V,
          class = RequireUsableKey<Key, Hash, Eq, K>>
decltype(std::declval<F>()(std::declval<const K&>(), std::piecewise_construct,
                           std::declval<std::tuple<K>>(), std::declval<V>()))
DecomposePairImpl(F&& f, std::pair<std::tuple<K>, V> p) {
  const auto& key = std::get<0>(p.first);
  return std::forward<F>(f)(key, std::piecewise_construct, std::move(p.first),
                            std::move(p.second));
}

}  // namespace internal_memory

// Constructs T into uninitialized storage pointed by `ptr` using the args
// specified in the tuple.
template <class Alloc, class T, class Tuple>
void ConstructFromTuple(Alloc* alloc, T* ptr, Tuple&& t) {
  internal_memory::ConstructFromTupleImpl(
      alloc, ptr, std::forward<Tuple>(t),
      absl::make_index_sequence<
          std::tuple_size<typename std::decay<Tuple>::type>::value>());
}

// Constructs T using the args specified in the tuple and calls F with the
// constructed value.
template <class T, class Tuple, class F>
decltype(std::declval<F>()(std::declval<T>())) WithConstructed(
    Tuple&& t, F&& f) {
  return internal_memory::WithConstructedImpl<T>(
      std::forward<Tuple>(t),
      absl::make_index_sequence<
          std::tuple_size<typename std::decay<Tuple>::type>::value>(),
      std::forward<F>(f));
}

// Given arguments of an std::pair's consructor, PairArgs() returns a pair of
// tuples with references to the passed arguments. The tuples contain
// constructor arguments for the first and the second elements of the pair.
//
// The following two snippets are equivalent.
//
// 1. std::pair<F, S> p(args...);
//
// 2. auto a = PairArgs(args...);
//    std::pair<F, S> p(std::piecewise_construct,
//                      std::move(p.first), std::move(p.second));
inline std::pair<std::tuple<>, std::tuple<>> PairArgs() { return {}; }
template <class F, class S>
std::pair<std::tuple<F&&>, std::tuple<S&&>> PairArgs(F&& f, S&& s) {
  return {std::piecewise_construct, std::forward_as_tuple(std::forward<F>(f)),
          std::forward_as_tuple(std::forward<S>(s))};
}
template <class F, class S>
std::pair<std::tuple<const F&>, std::tuple<const S&>> PairArgs(
    const std::pair<F, S>& p) {
  return PairArgs(p.first, p.second);
}
template <class F, class S>
std::pair<std::tuple<F&&>, std::tuple<S&&>> PairArgs(std::pair<F, S>&& p) {
  return PairArgs(std::forward<F>(p.first), std::forward<S>(p.second));
}
template <class F, class S>
auto PairArgs(std::piecewise_construct_t, F&& f, S&& s)
    -> decltype(std::make_pair(internal_memory::TupleRef(std::forward<F>(f)),
                               internal_memory::TupleRef(std::forward<S>(s)))) {
  return std::make_pair(internal_memory::TupleRef(std::forward<F>(f)),
                        internal_memory::TupleRef(std::forward<S>(s)));
}

// A helper function for implementing apply() in map policies.
template <class Key, class Hash, class Eq, class F, class... Args>
auto DecomposePair(F&& f, Args&&... args)
    -> decltype(internal_memory::DecomposePairImpl<Key, Hash, Eq>(
        std::forward<F>(f), PairArgs(std::forward<Args>(args)...))) {
  return internal_memory::DecomposePairImpl<Key, Hash, Eq>(
      std::forward<F>(f), PairArgs(std::forward<Args>(args)...));
}

// A helper function for implementing apply() in set policies.
template <class Key, class Hash, class Eq, class F, class Arg,
          class = internal_memory::RequireUsableKey<Key, Hash, Eq, Arg>>
decltype(std::declval<F>()(std::declval<const Arg&>(), std::declval<Arg>()))
DecomposeValue(F&& f, Arg&& arg) {
  const auto& key = arg;
  return std::forward<F>(f)(key, std::forward<Arg>(arg));
}

}  // namespace subtle
}  // namespace gtl

#endif  // S2_UTIL_GTL_SUBTLE_CONTAINER_MEMORY_H_
