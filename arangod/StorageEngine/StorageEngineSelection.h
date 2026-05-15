#pragma once

#include <cstddef>

namespace arangodb {
class StorageEngine;
class RocksDBEngine;
class ClusterEngine;

// Holds concrete references to both engines and a selection rule.
// At construction time, both engines must exist. The actual choice
// (rocks vs cluster) is deferred until select() is called, because
// ServerState::isCoordinator() may not be settled at construction.
class StorageEngineSelection {
 public:
  // Production: both engines must already be registered/constructed.
  StorageEngineSelection(RocksDBEngine& rocks, ClusterEngine& cluster) noexcept;

  // Tests / direct binding.
  explicit StorageEngineSelection(StorageEngine& engine) noexcept;

#ifdef ARANGODB_USE_GOOGLE_TESTS
  explicit StorageEngineSelection(std::nullptr_t) noexcept;
#endif

  // Apply the selection rule once. Side effects (rocks.disable()/enable(),
  // cluster.setActualEngine) happen here. Subsequent calls return the
  // cached choice.
  StorageEngine& select() noexcept;

 private:
  RocksDBEngine* _rocks = nullptr;
  ClusterEngine* _cluster = nullptr;
  StorageEngine* _direct = nullptr;     // tests / direct binding
  StorageEngine* _selected = nullptr;   // memoized result
};

}  // namespace arangodb