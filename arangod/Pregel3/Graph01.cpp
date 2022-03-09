//
// Created by roman on 25.02.22.
//

#include "Graph01.h"
#include "velocypack/Value.h"

using namespace arangodb;
using namespace arangodb::pregel3;
using namespace arangodb::velocypack;

template<class VertexProperties>
void Vertex<VertexProperties>::toVelocyPack(VPackBuilder& builder,
                                            std::string_view id, size_t idx) {
  VPackObjectBuilder ob(&builder);

  builder.add("id", VPackValue(id));
  builder.add("idx", VPackValue(idx));
  builder.add(VPackValue("outEdges"));
  {
    VPackArrayBuilder ab(&builder);
    for (auto const& e : outEdges) {
      builder.add(VPackValue(e));
    }
  }
  builder.add(VPackValue("inEdges"));
  {
    VPackArrayBuilder ab(&builder);
    for (auto const& e : inEdges) {
      builder.add(VPackValue(e));
    }
  }
  builder.add(VPackValue("props"));
  { props.toVelocyPack(builder); }
}

template<class EdgeProperties>
void Edge<EdgeProperties>::toVelocyPack(VPackBuilder& builder) {
  VPackObjectBuilder ob(&builder);
  builder.add("from", VPackValue(from));
  builder.add("to", VPackValue(to));
  props.toVelocyPack(builder);
}

void MinCutVertex::toVelocyPack(VPackBuilder& builder, std::string_view id,
                                size_t idx) {
  VPackObjectBuilder ob(&builder);
  builder.add("id", VPackValue(id));
  builder.add("idx", VPackValue(idx));
  builder.add("label", VPackValue(label));
  builder.add("excess", VPackValue(excess));
  builder.add("isLeaf", VPackValue(isLeaf));
}
void MinCutEdge::toVelocyPack(VPackBuilder& builder) {
  VPackObjectBuilder ob(&builder);
  builder.add("from", VPackValue(from));
  builder.add("to", VPackValue(to));
  builder.add("capacity", VPackValue(capacity));
  builder.add("flow", VPackValue(flow));
  if (edgeRev.has_value()) {
    builder.add("edgeRev", VPackValue(edgeRev.value()));
  }
}
void EmptyVertexProperties::toVelocyPack(VPackBuilder& builder,
                                         std::string_view id, size_t idx) {
  VPackObjectBuilder ob(&builder);
}

void EmptyEdgeProperties::toVelocyPack(VPackBuilder& builder) {
  VPackObjectBuilder ob(&builder);
}