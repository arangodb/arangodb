////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2014-2026 ArangoDB GmbH, Cologne, Germany
/// Copyright 2004-2014 triAGENS GmbH, Cologne, Germany
///
/// Licensed under the Business Source License 1.1 (the "License");
/// you may not use this file except in compliance with the License.
/// You may obtain a copy of the License at
///
///     https://github.com/arangodb/arangodb/blob/devel/LICENSE
///
/// Unless required by applicable law or agreed to in writing, software
/// distributed under the License is distributed on an "AS IS" BASIS,
/// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
/// See the License for the specific language governing permissions and
/// limitations under the License.
///
/// Copyright holder is ArangoDB GmbH, Cologne, Germany
///
/// @author Jan Steemann
////////////////////////////////////////////////////////////////////////////////

#include "StorageEngineFeature.h"

#include "ApplicationFeatures/ApplicationServer.h"
#include "Cluster/ServerState.h"
#include "ClusterEngine/ClusterEngine.h"
#include "FeaturePhases/BasicFeaturePhaseServer.h"
#include "Logger/Logger.h"
#include "RocksDBEngine/RocksDBEngine.h"
#include "StorageEngine/StorageEngine.h"

namespace arangodb {

StorageEngineFeature::StorageEngineFeature(
    application_features::ApplicationServer& server)
    : ApplicationFeature(server, typeid(StorageEngineFeature), name()) {
  setOptional(false);
  startsAfter<application_features::BasicFeaturePhaseServer>();
}

template<typename As, typename std::enable_if<std::is_base_of<StorageEngine, As>::value, int>::type>
As& StorageEngineFeature::engine() {
    TRI_ASSERT(dynamic_cast<As*>(_engine) != nullptr);
    return *static_cast<As*>(_engine);
}

template ClusterEngine& StorageEngineFeature::engine<ClusterEngine>();
template RocksDBEngine& StorageEngineFeature::engine<RocksDBEngine>();

void StorageEngineFeature::prepare() {
#ifdef ARANGODB_USE_GOOGLE_TESTS
  if (_selected.load()) {
    // already set in the test code
    return;
  }
#endif

  if (ServerState::instance()->isCoordinator()) {
    auto& clusterEngine = server().getFeature<ClusterEngine>();
    auto& rocksDBEngine = server().getFeature<RocksDBEngine>();

    rocksDBEngine.disable();
    clusterEngine.setActualEngine(&rocksDBEngine);
    _engine = &clusterEngine;
  } else {
    auto& rocksDBEngine = server().getFeature<RocksDBEngine>();
    rocksDBEngine.enable();
    _engine = &rocksDBEngine;
  }

  TRI_ASSERT(_engine != nullptr);
  _selected.store(true);
}

void StorageEngineFeature::unprepare() {
  _selected.store(false);
  _engine = nullptr;
  if (ServerState::instance()->isCoordinator()) {
#ifdef ARANGODB_USE_GOOGLE_TESTS
    if (!arangodb::ClusterEngine::Mocking) {
#endif
      server().getFeature<ClusterEngine>().setActualEngine(nullptr);
#ifdef ARANGODB_USE_GOOGLE_TESTS
    }
#endif
  }
}

#ifdef ARANGODB_USE_GOOGLE_TESTS
void StorageEngineFeature::setEngineTesting(StorageEngine* input) {
    TRI_ASSERT((input == nullptr) != (_engine == nullptr));
    _selected.store(input != nullptr);
    _engine = input;
}
#endif

}  // namespace arangodb