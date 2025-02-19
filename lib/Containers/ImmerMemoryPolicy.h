////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2014-2024 ArangoDB GmbH, Cologne, Germany
/// Copyright 2004-2014 triAGENS GmbH, Cologne, Germany
///
/// Licensed under the Business Source License 1.1 (the "License");
/// you may not use this file except in compliance with the License.
/// You may obtain a copy of the License at
///
///     https://github.com/arangodb/arangodb/blob/devel/LICENSE
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

#include <immer/heap/cpp_heap.hpp>
#include <immer/heap/heap_policy.hpp>
#include <immer/memory_policy.hpp>

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
    ::immer::memory_policy<arango_heap_policy, ::immer::default_refcount_policy,
                           ::immer::default_lock_policy>;

}  // namespace arangodb::immer
