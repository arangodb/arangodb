////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2014-2021 ArangoDB GmbH, Cologne, Germany
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
/// @author Lars Maier
////////////////////////////////////////////////////////////////////////////////

#pragma once

#include "Basics/Common.h"
#include "Graph/PathEnumerator.h"

#include <memory>
#include <queue>
#include <vector>

namespace arangodb {

namespace traverser {
class Traverser;
struct TraverserOptions;
}  // namespace traverser

namespace graph {
class EdgeCursor;

class WeightedEnumerator final : public arangodb::traverser::PathEnumerator {
 private:
  struct PathStep {
    size_t fromIndex;
    graph::EdgeDocumentToken fromEdgeToken;
    std::string_view currentVertexId;
    double accumWeight;

   public:
    explicit PathStep(std::string_view vertex);

    PathStep(size_t sourceIdx, graph::EdgeDocumentToken&& edge,
             std::string_view vertex, double weight);

    ~PathStep() = default;

    PathStep(PathStep const& other) = default;
    PathStep& operator=(PathStep const& other) = delete;
  };

  /// @brief Struct to hold all information required to get the list of
  ///        connected edges
  struct NextEdge {
    size_t fromIndex;
    double accumWeight;
    size_t depth;

    graph::EdgeDocumentToken forwardEdgeToken;
    std::string_view forwardVertexId;

   private:
    NextEdge() = delete;

   public:
    explicit NextEdge(size_t fromIndex, double accumWeight, size_t depth, graph::EdgeDocumentToken forwardEdgeToken,
                      std::string_view forwardVertexId)
        : fromIndex(fromIndex),
          accumWeight(accumWeight),
          depth(depth),
          forwardEdgeToken(std::move(forwardEdgeToken)),
          forwardVertexId(std::move(forwardVertexId)) {}

    bool operator<(NextEdge const& other) const {
      if (accumWeight == other.accumWeight) {
        return depth < other.depth;
      }
      return accumWeight < other.accumWeight;
    }
    bool operator>(NextEdge const& other) const { return other < *this; }
  };

  /// @brief schreier vector to store the visited vertices
  std::vector<PathStep> _schreier;

  /// @brief Next free index in schreier vector.
  size_t _schreierIndex;

  /// @brief Position of the last returned value in the schreier vector
  size_t _lastReturned;

  template <typename T>
  using min_heap = std::priority_queue<T, std::vector<T>, std::greater<T>>;

  template <typename T>
  struct clearable_min_heap : min_heap<T> {
    using min_heap<T>::min_heap;

    void clear() { this->c.clear(); }
    T popTop() {
      T top = std::move(this->c.front());
      this->pop();
      return top;
    }
  };

  /// @brief Queue to store where to continue search on next depth
  clearable_min_heap<NextEdge> _queue;

  /// @brief helper vector that is used temporarily when building the path
  /// output. We hold this as member to keep underlying memory.
  std::vector<size_t> _tempPathHelper;

 public:
  WeightedEnumerator(arangodb::traverser::Traverser* traverser,
                     arangodb::traverser::TraverserOptions* opts);

  ~WeightedEnumerator() = default;

  void clear() final;

  void setStartVertex(std::string_view startVertex) override;

  /// @brief Get the next Path element from the traversal.
  bool next() override;

  aql::AqlValue lastVertexToAqlValue() override;
  aql::AqlValue lastEdgeToAqlValue() override;
  aql::AqlValue pathToAqlValue(arangodb::velocypack::Builder& result) override;

 private:
  bool pathContainsVertex(size_t index, std::string_view vertex) const;
  bool pathContainsEdge(size_t index, graph::EdgeDocumentToken const& edge) const;

  aql::AqlValue vertexToAqlValue(size_t index);
  aql::AqlValue edgeToAqlValue(size_t index);
  aql::AqlValue pathToIndexToAqlValue(arangodb::velocypack::Builder& result, size_t index);
  velocypack::Slice pathToIndexToSlice(arangodb::velocypack::Builder& result, size_t index, bool fromPrune);

  bool shouldPrune();
  double weightEdge(arangodb::velocypack::Slice edge) const;

  bool expand();
  void expandVertex(size_t vertexIndex, size_t depth);
  bool expandEdge(NextEdge edge);

  static std::string_view getToVertex(velocypack::Slice edge, std::string_view from);
  bool validDisjointPath(size_t index, std::string_view vertex) const;
};
}  // namespace graph
}  // namespace arangodb
