#pragma once

#include <string>
#include "velocypack/Slice.h"
#include "velocypack/Builder.h"
#include "Basics/ResultT.h"

namespace arangodb::pregel3 {

struct AlgorithmSpecification {
  using AlgName = std::string;

  AlgName algName;

  std::string capacityProp;
  std::optional<double> defaultCapacity;
  std::string sourceVertexId;
  std::string targetVertexId;

  static auto fromVelocyPack(VPackSlice slice)
      -> ResultT<AlgorithmSpecification>;

  void toVelocyPack(VPackBuilder&);
};
}  // namespace arangodb::pregel3