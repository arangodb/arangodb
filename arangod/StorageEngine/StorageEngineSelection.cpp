#include "StorageEngineSelection.h"

#include "Basics/debugging.h"
#include "Cluster/ServerState.h"
#include "ClusterEngine/ClusterEngine.h"
#include "RocksDBEngine/RocksDBEngine.h"
#include "StorageEngine/StorageEngine.h"

namespace arangodb {

StorageEngineSelection::StorageEngineSelection(
    RocksDBEngine& rocks, ClusterEngine& cluster) noexcept
    : _rocks(&rocks), _cluster(&cluster) {}

StorageEngineSelection::StorageEngineSelection(StorageEngine& engine) noexcept
    : _direct(&engine) {}

#ifdef ARANGODB_USE_GOOGLE_TESTS
StorageEngineSelection::StorageEngineSelection(std::nullptr_t) noexcept {}
#endif

StorageEngine& StorageEngineSelection::select() noexcept {
  if (_selected != nullptr) {
    return *_selected;
  }
  if (_direct != nullptr) {
    _selected = _direct;
    return *_selected;
  }
  TRI_ASSERT(_rocks != nullptr && _cluster != nullptr);
  if (ServerState::instance()->isCoordinator()) {
    _rocks->disable();
    _cluster->setActualEngine(_rocks);
    _selected = _cluster;
  } else {
    _rocks->enable();
    _selected = _rocks;
  }
  return *_selected;
}

}  // namespace arangodb