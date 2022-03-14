//
// Created by roman on 11.03.22.
//

#include "Vertex02.h"

using namespace arangodb::pregel3;

template<class VertexProperties, class E>
void Vertex<VertexProperties, E>::toVelocyPack(VPackBuilder& builder,
                                               std::string_view id,
                                               size_t idx) {
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
}

void EmptyVertexProperties::toVelocyPack(VPackBuilder& builder,
                                         std::string_view id, size_t idx) {
  VPackObjectBuilder ob(&builder);
}