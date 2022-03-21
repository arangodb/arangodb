//
// Created by roman on 11.03.22.
//

#pragma once

#include <vector>
#include "velocypack/Builder.h"
#include "Basics/debugging.h"

namespace arangodb::pregel3 {

template<class EdgeProperties>
struct Edge {
  size_t from;
  size_t to;
  size_t idx;  // used only for quick deletion of edges
  EdgeProperties props;
  Edge() = delete;
  Edge(size_t from, size_t to, size_t idx) : from(from), to(to), idx(idx) {}

  void toVelocyPack(VPackBuilder& builder);
};

template<class EdgeProperties>
struct MultiEdge {
  size_t from;
  size_t to;
  std::vector<size_t> edgeIdxs;

  void toVelocyPack(VPackBuilder& builder);
};

struct EmptyEdgeProperties {
  virtual ~EmptyEdgeProperties() = default;
  static void toVelocyPack(VPackBuilder& builder);
};
struct EdgeWithEmptyProps : Edge<EmptyEdgeProperties> {
  EdgeWithEmptyProps(size_t fromIdx, size_t toIdx, size_t idx)
      : Edge(fromIdx, toIdx, idx){};
};

struct MinCutEdge : public Edge<EmptyEdgeProperties> {
  double const capacity;
  double flow = 0.0;

  MinCutEdge(size_t from, size_t to, size_t idx, double capacity)
      : Edge(from, to, idx), capacity(capacity){};

  bool operator==(MinCutEdge const& other) {
    return capacity == other.capacity && flow == other.flow;
  }

  void toVelocyPack(VPackBuilder& builder) const;

  double residual() const {
    TRI_ASSERT(capacity >= flow);
    return capacity - flow;
  }

  void decreaseFlow(double val) {
    TRI_ASSERT(val <= flow);
    flow -= val;
  }

  void increaseFlow(double val) {
    TRI_ASSERT(val <= residual());
    flow += val;
  }
};

}  // namespace arangodb::pregel3