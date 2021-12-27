////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2021-2021 ArangoDB GmbH, Cologne, Germany
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

#if (_MSC_VER >= 1)
// suppress warnings:
#pragma warning(push)
// conversion from 'size_t' to 'immer::detail::rbts::count_t', possible loss of
// data
#pragma warning(disable : 4267)
// result of 32-bit shift implicitly converted to 64 bits (was 64-bit shift
// intended?)
#pragma warning(disable : 4334)
#endif
#include <immer/heap/cpp_heap.hpp>
#include <immer/heap/heap_policy.hpp>
#include <immer/memory_policy.hpp>
#if (_MSC_VER >= 1)
#pragma warning(pop)
#endif

namespace arangodb::immer {

// This is like free_list_heap_policy, but omits the global free_list_heap
// which sits there between the thread_local_free_list_heap and the Heap
// argument.
// We're using this because the free_list_heap is currently *not* thread safe,
// see https://github.com/arximboldi/immer/issues/182 for details.
template<typename Heap, std::size_t Limit = ::immer::default_free_list_size>
struct thread_local_free_list_heap_policy {
  using type = ::immer::debug_size_heap<Heap>;

  template<std::size_t Size>
  struct optimized {
    using type = ::immer::split_heap<
        Size,
        ::immer::with_free_list_node<::immer::thread_local_free_list_heap<
            Size, Limit, ::immer::debug_size_heap<Heap>>>,
        ::immer::debug_size_heap<Heap>>;
  };
};

using arango_heap_policy =
    thread_local_free_list_heap_policy<::immer::cpp_heap>;
using arango_memory_policy =
    ::immer::memory_policy<arango_heap_policy,
                           ::immer::default_refcount_policy>;

}  // namespace arangodb::immer
