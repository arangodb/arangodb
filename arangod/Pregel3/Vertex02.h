//
// Created by roman on 11.03.22.
//

#pragma once

#include <unordered_map>
#include <string_view>
#include "velocypack/Builder.h"
#include "Edge02.h"

namespace arangodb::pregel3 {

template<class VertexProperties, class E>
struct Vertex {
  std::unordered_map<size_t, E*> outEdges;  // out-neighbour to edge
  std::unordered_map<size_t, E*> inEdges;   // in-neighbour to edge
  VertexProperties props;

  void toVelocyPack(VPackBuilder& builder, std::string_view id, size_t idx);

  size_t degree() const { return outEdges.size() + inEdges.size(); }

  size_t inDegree() const { return inEdges.size(); }

  size_t outDegree() const { return outEdges.size(); }
};

struct EmptyVertexProperties {
  virtual ~EmptyVertexProperties() = default;
  static void toVelocyPack(VPackBuilder& builder, std::string_view id,
                           size_t idx);
};
using VertexWithEmptyProps = Vertex<EmptyVertexProperties, EdgeWithEmptyProps>;

struct MinCutVertex : public Vertex<EmptyVertexProperties, MinCutEdge> {
  size_t label;
  double excess;

  MinCutVertex() = default;

  void toVelocyPack(VPackBuilder& builder, std::string_view id,
                    size_t idx) const;

  bool operator==(MinCutVertex const& other) {
    return label == other.label && excess == other.excess;
  }

  bool operator<(MinCutVertex const& other) {
    return label < other.label ||
           (label == other.label && excess < other.excess);
  }

  void increaseExcess(double val) {
    TRI_ASSERT(val > 0);
    excess += val;
  }

  void decreaseExcess(double val) {
    TRI_ASSERT(val > 0);
    excess -= val;
    TRI_ASSERT(excess >= 0);
  }
};

}  // namespace arangodb::pregel3