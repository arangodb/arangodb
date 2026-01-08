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
////////////////////////////////////////////////////////////////////////////////
#pragma once

#include <velocypack/HashedStringRef.h>

#include <compare>

#include "Basics/ResultT.h"

namespace arangodb::graph {

class VertexRef {
  using RefType = velocypack::HashedStringRef;

 public:
  VertexRef() : _vertex() {}
  explicit VertexRef(RefType v) : _vertex(std::move(v)) {}

  RefType const& getID() const noexcept { return _vertex; };

  // TODO: should this be in HashedStringRef?
  auto operator<=>(VertexRef const& other) const {
    if (_vertex == other._vertex) {
      return std::partial_ordering::equivalent;
    } else if (_vertex < other._vertex) {
      return std::partial_ordering::less;
    } else if (_vertex > other._vertex) {
      return std::partial_ordering::greater;
    }
    return std::partial_ordering::unordered;
  }
  auto operator==(VertexRef const& other) const {
    return _vertex == other._vertex;
  }

  friend std::ostream& operator<<(std::ostream& stream, VertexRef const& ref) {
    return stream << ref._vertex;
  }

  operator velocypack::HashedStringRef() { return _vertex; }

  [[nodiscard]] auto collectionName() const -> ResultT<std::string_view>;

 private:
  RefType _vertex;
};

}  // namespace arangodb::graph

namespace std {
template<>
struct hash<arangodb::graph::VertexRef> {
  size_t operator()(arangodb::graph::VertexRef const& value) const noexcept {
    return value.getID().hash();
  }
};

}  // namespace std
