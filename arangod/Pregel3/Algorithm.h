
#pragma once

#include <string>
#include <memory>
#include "AlgorithmResult.h"
class Algorithm {
 public:
  virtual std::unique_ptr<AlgorithmResult> run() = 0;
  virtual ~Algorithm() = default;
};
