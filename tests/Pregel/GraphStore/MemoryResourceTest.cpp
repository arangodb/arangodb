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
/// @author Markus Pfeiffer
////////////////////////////////////////////////////////////////////////////////
#include <gtest/gtest.h>

#include "experimental/vector"
#include "experimental/string"

#include "Pregel/GraphStore/MemoryResource.h"
#include "Pregel/GraphStore/Vertex.h"
#include "Pregel/GraphStore/Edge.h"
#include "Pregel/GraphStore/Quiver.h"
#include "Pregel/GraphStore/Magazine.h"

using namespace arangodb;
using namespace arangodb::pregel;

TEST(PregelMemoryResource, memory_resource) {
    auto mr = std::make_unique<MemoryResource>(std::experimental::pmr::new_delete_resource());
    using alloc = std::experimental::pmr::polymorphic_allocator<std::experimental::pmr::string>;

    auto v = std::experimental::pmr::vector<std::experimental::pmr::string>(alloc(mr.get()));

    v.emplace_back("Hello");
    v.emplace_back("cruel");
    v.emplace_back("world abc 123 v44444 fooo");

    ASSERT_EQ(mr->bytesAllocated, 160);
    ASSERT_EQ(mr->numberAllocations, 4);
}


namespace {
struct V {};
struct E {};
}

TEST(PregelMemoryResource, vertex) {
    auto mr = std::make_unique<MemoryResource>(std::experimental::pmr::new_delete_resource());
    auto v = Vertex<V, E>(mr.get());
    v.addEdge(Edge<E>({}, {}));
    v.addEdge(Edge<E>({}, {}));
    v.addEdge(Edge<E>({}, {}));
    v.addEdge(Edge<E>({}, {}));
}

TEST(PregelMemoryResource, quiver) {
    auto mr = std::make_unique<MemoryResource>(std::experimental::pmr::new_delete_resource());
    auto q = Quiver<V, E>(mr.get());

    auto v = Vertex<V,E>(mr.get());

    v.addEdge(Edge<E>({}, {}));

    // q.emplace(Vertex<V,E>(mr.get()));
    // q.emplace(std::move(v));
}

TEST(PregelMemoryResource, magazine) {
    auto mr = std::make_unique<MemoryResource>(std::experimental::pmr::new_delete_resource());
  //  using alloc = std::experimental::pmr::polymorphic_allocator<std::experimental::pmr::string>;

    auto v = Magazine<V, E>();
}
