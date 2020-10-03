#ifndef ARANGODB_GRAPH_WEIGHTEDENUMERATOR_H
#define ARANGODB_GRAPH_WEIGHTEDENUMERATOR_H 1

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
    arangodb::velocypack::StringRef currentVertexId;
    double accumWeight;

   public:
    explicit PathStep(arangodb::velocypack::StringRef vertex);

    PathStep(size_t sourceIdx, graph::EdgeDocumentToken&& edge,
             arangodb::velocypack::StringRef vertex, double weight);

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
    arangodb::velocypack::StringRef forwardVertexId;

   private:
    NextEdge() = delete;

   public:
    explicit NextEdge(size_t fromIndex, double accumWeight, size_t depth, graph::EdgeDocumentToken forwardEdgeToken,
                      arangodb::velocypack::StringRef forwardVertexId)
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

  void setStartVertex(arangodb::velocypack::StringRef startVertex) override;

  /// @brief Get the next Path element from the traversal.
  bool next() override;

  aql::AqlValue lastVertexToAqlValue() override;
  aql::AqlValue lastEdgeToAqlValue() override;
  aql::AqlValue pathToAqlValue(arangodb::velocypack::Builder& result) override;

 private:
  bool pathContainsVertex(size_t index, arangodb::velocypack::StringRef vertex) const;
  bool pathContainsEdge(size_t index, graph::EdgeDocumentToken const& edge) const;

  aql::AqlValue vertexToAqlValue(size_t index);
  aql::AqlValue edgeToAqlValue(size_t index);
  aql::AqlValue pathToIndexToAqlValue(arangodb::velocypack::Builder& result, size_t index);
  velocypack::Slice pathToIndexToSlice(arangodb::velocypack::Builder& result, size_t index);

  bool shouldPrune();
  double weightEdge(arangodb::velocypack::Slice edge) const;

  bool expand();
  void expandVertex(size_t vertexIndex, size_t depth);
  bool expandEdge(NextEdge edge);

  static velocypack::StringRef getToVertex(velocypack::Slice edge, velocypack::StringRef from);
};
}  // namespace graph
}  // namespace arangodb

#endif
