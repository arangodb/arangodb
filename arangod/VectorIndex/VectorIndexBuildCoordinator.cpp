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
/// @author Jure Bajic
////////////////////////////////////////////////////////////////////////////////

#include "VectorIndex/VectorIndexBuildCoordinator.h"

#include "Indexes/Index.h"
#include "Logger/LogMacros.h"
#include "RestServer/DatabaseFeature.h"
#include "RocksDBEngine/RocksDBCollection.h"
#include "RocksDBEngine/RocksDBVectorIndex.h"
#include "RocksDBEngine/RocksDBVectorIndexBuilder.h"
#include "StorageEngine/PhysicalCollection.h"
#include "VocBase/LogicalCollection.h"
#include "VocBase/vocbase.h"

#ifdef TRI_HAVE_SYS_PRCTL_H
#include <pthread.h>
#endif

namespace arangodb {

VectorIndexBuildCoordinator::VectorIndexBuildCoordinator(
    DatabaseFeature& dbFeature)
    : _dbFeature(dbFeature) {}

void VectorIndexBuildCoordinator::start() {
  _thread =
      std::jthread([this](std::stop_token stopToken) { run(stopToken); });
}

void VectorIndexBuildCoordinator::beginShutdown() {
  _thread.request_stop();
}

void VectorIndexBuildCoordinator::stop() {
  beginShutdown();

  if (_thread.joinable()) {
    _thread.join();
  }
}

void VectorIndexBuildCoordinator::run(std::stop_token stopToken) {
#ifdef TRI_HAVE_SYS_PRCTL_H
  pthread_setname_np(pthread_self(), "VecIdxBuild");
#endif

  while (!stopToken.stop_requested()) {
    // Sleep between scans, checking stop frequently.
    auto const deadline = std::chrono::steady_clock::now() + kScanInterval;
    while (std::chrono::steady_clock::now() < deadline &&
           !stopToken.stop_requested()) {
      std::this_thread::sleep_for(kSleepGranularity);
    }

    // Scan all databases/collections for untrained vector indexes.
    try {
      _dbFeature.enumerateDatabases([&](TRI_vocbase_t& vocbase) {
        if (stopToken.stop_requested()) {
          return;
        }

        auto const collections = vocbase.collections(false);
        for (auto const& coll : collections) {
          auto const indexes = coll->getPhysical()->getReadyIndexes();
          for (auto const& idx : indexes) {
            if (idx->type() != Index::TRI_IDX_TYPE_VECTOR_INDEX) {
              continue;
            }
            auto& vecIdx = static_cast<RocksDBVectorIndex&>(*idx);
            if (vecIdx.trainingState() !=
                VectorIndexTrainingState::kUntrained) {
              continue;
            }

            auto const* rcoll = static_cast<RocksDBCollection*>(
                coll->getPhysical());
            auto numDocs =
                static_cast<std::int64_t>(rcoll->meta().numberDocuments());
            // In case of sparse indexes this is not enough, since they might not have enough vectors to train on.
            // Therefore the training and building process will fail and we will retry them in 10min or maybe when the
            // number of dcuments changes a "lot"
            if (numDocs < vecIdx.trainingThreshold()) {
              continue;
            }

            LOG_TOPIC("e171b", INFO, Logger::ENGINES)
                << "[shard=" << vecIdx.collection().name()
                << ", index=" << vecIdx.id().id()
                << "] Training threshold reached ("
                << vecIdx.trainingThreshold()
                << " documents). Starting deferred training.";

            try {
              vector::VectorIndexBuildManager builder(vecIdx);
              auto const res = builder.build(stopToken);
              if (res.fail()) {
                LOG_TOPIC("e164b", ERR, Logger::ENGINES)
                    << "[index=" << vecIdx.id().id()
                    << "] Vector build failed: " << res.errorMessage();
              }
            } catch (std::exception const& ex) {
              LOG_TOPIC("e164c", ERR, Logger::ENGINES)
                  << "[index=" << vecIdx.id().id()
                  << "] Vector build exception: " << ex.what();
            } catch (...) {
              LOG_TOPIC("e164d", ERR, Logger::ENGINES)
                  << "[index=" << vecIdx.id().id()
                  << "] Vector build unknown exception";
            }
            return;
          }
        }
      });
    } catch (std::exception const& ex) {
      LOG_TOPIC("e170b", WARN, Logger::ENGINES)
          << "VectorIndexBuildCoordinator scan error: " << ex.what();
    }
  }
}

}  // namespace arangodb
