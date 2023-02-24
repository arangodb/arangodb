////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2014-2023 ArangoDB GmbH, Cologne, Germany
/// Copyright 2004-2014 triAGENS GmbH, Cologne, Germany
///
/// Licensed under the Apache License, Version 2.0 (the "License");
/// you may not use this file except in compliance with the License.
/// You may obtain a copy of the License at
///
///     http://www.apache.org/licenses/LICENSE-2.0
///
/// Unless required by applicable law or agreed to in writing, software
/// distributed under the License is distributed on an "AS IS" BASIS,
/// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
/// See the License for the specific language governing permissions and
/// limitations under the License.
///
/// Copyright holder is ArangoDB GmbH, Cologne, Germany
///
/// @author Julia Volmer
/// @author Markus Pfeiffer
////////////////////////////////////////////////////////////////////////////////

#include "Pregel/GraphStore/Loader.h"

namespace arangodb::pregel {
template<typename V, typename E>
void GraphStore<V, E>::loadShards(
    WorkerConfig* config, std::function<void()> const& statusUpdateCallback,
    std::function<void()> const& finishedLoadingCallback) {
  _config = config;
  TRI_ASSERT(_runningThreads == 0);

  LOG_PREGEL("27f1e", DEBUG)
      << "Using up to " << _config->parallelism()
      << " threads to load data. memory-mapping is turned "
      << (config->useMemoryMaps() ? "on" : "off");

  // hold the current position where the ith vertex shard can
  // start to write its data. At the end the offset should equal the
  // sum of the counts of all ith edge shards

  // Contains the shards located on this db server in the right order
  // assuming edges are sharded after _from, vertices after _key
  // then every ith vertex shard has the corresponding edges in
  // the ith edge shard
  std::map<CollectionID, std::vector<ShardID>> const& vertexCollMap =
      _config->vertexCollectionShards();
  std::map<CollectionID, std::vector<ShardID>> const& edgeCollMap =
      _config->edgeCollectionShards();
  size_t numShards = SIZE_MAX;

  auto poster = [](std::function<void()> fn) -> void {
    return SchedulerFeature::SCHEDULER->queue(RequestLane::INTERNAL_LOW,
                                              std::move(fn));
  };
  auto queue = std::make_shared<basics::LocalTaskQueue>(
      _vocbaseGuard.database().server(), poster);
  queue->setConcurrency(_config->parallelism());

  for (auto const& pair : vertexCollMap) {
    std::vector<ShardID> const& vertexShards = pair.second;
    if (numShards == SIZE_MAX) {
      numShards = vertexShards.size();
    } else if (numShards != vertexShards.size()) {
      THROW_ARANGO_EXCEPTION_MESSAGE(TRI_ERROR_BAD_PARAMETER, shardError);
    }

    for (size_t i = 0; i < vertexShards.size(); i++) {
      ShardID const& vertexShard = vertexShards[i];

      auto const& edgeCollectionRestrictions =
          _config->edgeCollectionRestrictions(vertexShard);

      // distributeshardslike should cause the edges for a vertex to be
      // in the same shard index. x in vertexShard2 => E(x) in edgeShard2
      std::vector<ShardID> edges;
      for (auto const& pair2 : edgeCollMap) {
        std::vector<ShardID> const& edgeShards = pair2.second;
        if (vertexShards.size() != edgeShards.size()) {
          THROW_ARANGO_EXCEPTION_MESSAGE(TRI_ERROR_BAD_PARAMETER, shardError);
        }

        // optionally restrict edge collections to a positive list
        if (edgeCollectionRestrictions.empty() ||
            std::find(edgeCollectionRestrictions.begin(),
                      edgeCollectionRestrictions.end(),
                      edgeShards[i]) != edgeCollectionRestrictions.end()) {
          edges.emplace_back(edgeShards[i]);
        }
      }

      try {
        // we might have already loaded these shards
        if (!_loadedShards.emplace(vertexShard).second) {
          continue;
        }
        auto task = std::make_shared<basics::LambdaTask>(
            queue,
            [this, vertexShard, edges, statusUpdateCallback]() -> Result {
              if (_vocbaseGuard.database().server().isStopping()) {
                LOG_PREGEL("4355b", WARN) << "Aborting graph loading";
                return {TRI_ERROR_SHUTTING_DOWN};
              }
              try {
                loadVertices(vertexShard, edges, statusUpdateCallback);
                return Result();
              } catch (basics::Exception const& ex) {
                LOG_PREGEL("8682a", WARN)
                    << "caught exception while loading pregel graph: "
                    << ex.what();
                return Result(ex.code(), ex.what());
              } catch (std::exception const& ex) {
                LOG_PREGEL("c87c9", WARN)
                    << "caught exception while loading pregel graph: "
                    << ex.what();
                return Result(TRI_ERROR_INTERNAL, ex.what());
              } catch (...) {
                LOG_PREGEL("c7240", WARN)
                    << "caught unknown exception while loading pregel graph";
                return Result(TRI_ERROR_INTERNAL,
                              "unknown exception while loading pregel graph");
              }
            });
        queue->enqueue(task);
      } catch (basics::Exception const& ex) {
        LOG_PREGEL("3f283", WARN) << "unhandled exception while "
                                  << "loading pregel graph: " << ex.what();
      } catch (...) {
        LOG_PREGEL("3f282", WARN) << "unhandled exception while "
                                  << "loading pregel graph";
      }
    }
  }
  queue->dispatchAndWait();
  if (queue->status().fail() && !queue->status().is(TRI_ERROR_SHUTTING_DOWN)) {
    THROW_ARANGO_EXCEPTION(queue->status());
  }

  SchedulerFeature::SCHEDULER->queue(RequestLane::INTERNAL_LOW,
                                     statusUpdateCallback);

  SchedulerFeature::SCHEDULER->queue(RequestLane::INTERNAL_LOW,
                                     finishedLoadingCallback);
}

template<typename V, typename E>
void GraphStore<V, E>::loadDocument(WorkerConfig* config,
                                    std::string const& documentID) {
  // figure out if we got this vertex locally
  VertexID _id = config->documentIdToPregel(documentID);
  if (config->isLocalVertexShard(_id.shard)) {
    loadDocument(config, _id.shard, std::string_view(_id.key));
  }
}

template<typename V, typename E>
void GraphStore<V, E>::loadDocument(WorkerConfig* config,
                                    PregelShard sourceShard,
                                    std::string_view key) {
  // Apparently this code has not been implemented yet; find out whether it's
  // needed at all or remove
  TRI_ASSERT(false);
}

}  // namespace arangodb::pregel
