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

#include <omp.h>
#include <unordered_set>

#ifdef TRI_HAVE_SYS_PRCTL_H
#include <pthread.h>
#endif

namespace arangodb {

VectorIndexBuildCoordinator::VectorIndexBuildCoordinator(
    DatabaseFeature& dbFeature)
    : _dbFeature(dbFeature) {}

void VectorIndexBuildCoordinator::start(std::uint32_t maxOmpThreads) {
  _thread = std::jthread([this, maxOmpThreads](std::stop_token stopToken) {
    // Set OpenMP thread limit on the worker thread (ICV is per-thread).
    // If user didn't configure a value, default to max(4, numCores/4).
    auto const numThreads = maxOmpThreads > 0
                                ? static_cast<int>(maxOmpThreads)
                                : std::max(4, omp_get_num_procs() / 4);
    omp_set_num_threads(numThreads);
    run(stopToken);
  });
}

void VectorIndexBuildCoordinator::beginShutdown() { _thread.request_stop(); }

void VectorIndexBuildCoordinator::stop() {
  beginShutdown();

  if (_thread.joinable()) {
    _thread.join();
  }
}

bool VectorIndexBuildCoordinator::shouldSkipRetry(
    std::uint64_t objectId, std::int64_t currentDocCount) const {
  auto const it = _failedBuilds.find(objectId);
  if (it == _failedBuilds.end()) {
    return false;
  }
  auto const& info = it->second;
  bool backoffElapsed =
      std::chrono::steady_clock::now() - info.failedAt >= kRetryBackoff;
  bool docCountChanged = currentDocCount != info.documentCount;
  // Retry only if backoff elapsed AND document count changed.
  return !(backoffElapsed && docCountChanged);
}

void VectorIndexBuildCoordinator::recordFailure(std::uint64_t objectId,
                                                std::int64_t docCount) {
  _failedBuilds[objectId] = {std::chrono::steady_clock::now(), docCount};
}

void VectorIndexBuildCoordinator::clearFailure(std::uint64_t objectId) {
  _failedBuilds.erase(objectId);
}

void VectorIndexBuildCoordinator::run(std::stop_token stopToken) {
#ifdef TRI_HAVE_SYS_PRCTL_H
  pthread_setname_np(pthread_self(), "VecIdxBuild");
#endif

  while (!stopToken.stop_requested()) {
    auto const deadline = std::chrono::steady_clock::now() + kScanInterval;
    while (std::chrono::steady_clock::now() < deadline &&
           !stopToken.stop_requested()) {
      std::this_thread::sleep_for(kSleepGranularity);
    }

    try {
      scanAndBuild(stopToken);
    } catch (std::exception const& ex) {
      LOG_TOPIC("e170b", WARN, Logger::ENGINES)
          << "VectorIndexBuildCoordinator scan error: " << ex.what();
    }
  }
}

void VectorIndexBuildCoordinator::scanAndBuild(
    std::stop_token const& stopToken) {
  // Collect objectIds seen this scan to prune stale entries from
  // _failedBuilds (e.g. indexes that were dropped since the last scan).
  std::unordered_set<std::uint64_t> seenObjectIds;

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
        if (vecIdx.trainingState() != VectorIndexTrainingState::kUntrained) {
          continue;
        }

        seenObjectIds.insert(vecIdx.objectId());

        auto const* rcoll =
            static_cast<RocksDBCollection*>(coll->getPhysical());
        auto const numDocs =
            static_cast<std::int64_t>(rcoll->meta().numberDocuments());
        if (numDocs < vecIdx.trainingThreshold()) {
          continue;
        }

        if (shouldSkipRetry(vecIdx.objectId(), numDocs)) {
          continue;
        }

        LOG_TOPIC("e171b", INFO, Logger::ENGINES)
            << "[shard=" << vecIdx.collection().name()
            << ", index=" << vecIdx.id().id()
            << "] Training threshold reached (" << vecIdx.trainingThreshold()
            << " documents). Starting deferred training.";

        vector::VectorIndexBuildManager builder(vecIdx);
        if (auto const res = builder.build(stopToken); res.fail()) {
          LOG_TOPIC("e164b", ERR, Logger::ENGINES)
              << "[index=" << vecIdx.id().id()
              << "] Vector build failed: " << res.errorMessage();
          recordFailure(vecIdx.objectId(), numDocs);
          continue;
        }
        clearFailure(vecIdx.objectId());

        // Built one index — return to the scan loop so we sleep
        // before starting the next one.
        return;
      }
    }
  });

  // Prune failed build entries for indexes that no longer exist.
  std::erase_if(_failedBuilds, [&](auto const& entry) {
    return !seenObjectIds.contains(entry.first);
  });
}

}  // namespace arangodb
