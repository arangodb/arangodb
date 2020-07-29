#include <iostream>

#include "Pregel/Algos/AIR/Greenspun/Primitives.h"

struct MyEvalContext : PrimEvalContext {
  std::string const& getThisId() const override { std::abort(); }

  EvalResult getAccumulatorValue(std::string_view id, VPackBuilder& result) const override {
    std::abort();
  }

  EvalResult updateAccumulator(std::string_view accumId, std::string_view vertexId,
                         VPackSlice value) override {
    std::abort();
  }

  EvalResult setAccumulator(std::string_view accumId, VPackSlice value) override {
    std::abort();
  }

  EvalResult enumerateEdges(std::function<EvalResult(VPackSlice edge)> cb) const override {
    std::abort();
  }
};
