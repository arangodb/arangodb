#include "Graph/Enumerators/OneSidedEnumeratorInterface.h"

namespace arangodb::graph::experimental {
struct SingleServerTraversalEnumerator : TraversalEnumerator {
  SingleServerTraversalEnumerator() {}
  ~SingleServerTraversalEnumerator() {}
  void clear(bool keepPathStore) override { TRI_ASSERT(false); }
  [[nodiscard]] bool isDone() const override { return false; }

  void reset(VertexRef source, size_t depth = 0, double weight = 0.0,
             bool keepPathStore = false) override {
    // does not validate that vertex exists on graph
  }
  void resetManyStartVertices(
      std::vector<VertexDescription> const& vertices) override {
    TRI_ASSERT(false);
  };

  auto prepareIndexExpressions(aql::Ast* ast) -> void override {
    TRI_ASSERT(false);
  };
  auto getNextPath() -> std::unique_ptr<PathResultInterface> override {
    TRI_ASSERT(false);
    return nullptr;
  };
#ifdef USE_ENTERPRISE
  auto smartSearch(size_t amountOfExpansions, velocypack::Builder& result)
      -> void override {
    TRI_ASSERT(false);
  }
#endif
  bool skipPath() override {
    TRI_ASSERT(false);
    return false;
  }
  auto destroyEngines() -> void override { TRI_ASSERT(false); };

  auto stealStats() -> aql::TraversalStats override {
    TRI_ASSERT(false); /*TODO*/
  };

  auto validatorUsesPrune() const -> bool override {
    TRI_ASSERT(false);
    return false;
  }
  auto validatorUsesPostFilter() const -> bool override {
    TRI_ASSERT(false);
    return false;
  }
  auto setValidatorContext(aql::InputAqlItemRow& inputRow) -> void override {
    TRI_ASSERT(false);
  }
  auto unprepareValidatorContext() -> void override { TRI_ASSERT(false); }
};
}  // namespace arangodb::graph::experimental
