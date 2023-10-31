///////////////////////////////////////////////////////////////////////////////
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

#pragma once

#include "Pregel/GraphStore/Quiver.h"

/*
 * A magazine is a collection of quivers.
 *
 * This is a temporary measure as we store multiple Quivers per
 * worker.
 *
 */
namespace arangodb::pregel {
template<typename V, typename E>
struct Magazine {
  using Storage = std::vector<std::shared_ptr<Quiver<V, E>>>;
  using iterator = typename Storage::iterator;

  Storage quivers;

  auto emplace(std::shared_ptr<Quiver<V, E>>&& quiver) {
    return quivers.emplace_back(std::move(quiver));
  }

  auto size() const -> size_t { return quivers.size(); }

  auto begin() const { return std::begin(quivers); }
  auto end() const { return std::end(quivers); }

  auto begin() { return std::begin(quivers); }
  auto end() { return std::end(quivers); }

  auto numberOfVertices() const -> size_t {
    auto sum = size_t{0};
    for (auto const& quiver : quivers) {
      sum += quiver->numberOfVertices();
    }
    return sum;
  }
  auto numberOfEdges() const -> size_t {
    auto sum = size_t{0};
    for (auto const& quiver : quivers) {
      sum += quiver->numberOfEdges();
    }
    return sum;
  }
};

template<typename V, typename E, typename Inspector>
auto inspect(Inspector& f, Magazine<V, E>& s) {
  return f.object(s).fields(f.field("quivers", s.quivers));
}

}  // namespace arangodb::pregel
