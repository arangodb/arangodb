
#pragma once

#include "velocypack/Builder.h"
class AlgorithmResult {
 public:
  virtual ~AlgorithmResult() = default;
  virtual void toVelocyPack(VPackBuilder& builder) = 0;
};
