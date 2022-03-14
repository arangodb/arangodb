//
// Created by roman on 11.03.22.
//

#include "Edge02.h"

using namespace arangodb::pregel3;

void MinCutEdge::toVelocyPack(VPackBuilder& builder) {
  VPackObjectBuilder ob(&builder);
  builder.add("from", VPackValue(from));
  builder.add("to", VPackValue(to));
  builder.add("capacity", VPackValue(capacity));
  builder.add("flow", VPackValue(flow));
}

void EmptyEdgeProperties::toVelocyPack(VPackBuilder& builder) {
  VPackObjectBuilder ob(&builder);
}