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
/// @author Markus Pfeiffer
////////////////////////////////////////////////////////////////////////////////

#include "GraphStorer.h"
#include <cstdint>

#include "Pregel/GraphFormat.h"
#include "Pregel/Algos/ColorPropagation/ColorPropagationValue.h"
#include "Pregel/Algos/DMID/DMIDValue.h"
#include "Pregel/Algos/EffectiveCloseness/ECValue.h"
#include "Pregel/Algos/HITS/HITSValue.h"
#include "Pregel/Algos/HITSKleinberg/HITSKleinbergValue.h"
#include "Pregel/Algos/LabelPropagation/LPValue.h"
#include "Pregel/Algos/SCC/SCCValue.h"
#include "Pregel/Algos/SLPA/SLPAValue.h"
#include "Pregel/Algos/WCC/WCCValue.h"

#include "Pregel/StatusMessages.h"
#include "Pregel/Worker/WorkerConfig.h"

#include "ApplicationFeatures/ApplicationServer.h"
#include "Basics/overload.h"
#include "Logger/LogMacros.h"
#include "Scheduler/SchedulerFeature.h"
#include "Transaction/StandaloneContext.h"
#include "Utils/OperationOptions.h"
#include "Utils/OperationResult.h"
#include "Utils/SingleCollectionTransaction.h"
#include "VocBase/vocbase.h"

#define LOG_PREGEL(logId, level) \
  LOG_TOPIC(logId, level, Logger::PREGEL) << "[job " << executionNumber << "] "

namespace arangodb::pregel {

template<typename V, typename E>
auto GraphStorer<V, E>::storeQuiver(std::shared_ptr<Quiver<V, E>> quiver)
    -> void {
  // transaction on one shard
  OperationOptions options;
  options.silent = true;
  options.waitForSync = false;

  std::unique_ptr<arangodb::SingleCollectionTransaction> trx;

  ShardID shard;
  PregelShard currentShard = InvalidPregelShard;
  Result res;

  VPackBuilder builder;
  uint64_t numDocs = 0;

  auto commitTransaction = [&]() {
    if (trx) {
      builder.close();

      OperationResult opRes = trx->update(shard, builder.slice(), options);
      if (!opRes.countErrorCodes.empty()) {
        auto code = ErrorCode{opRes.countErrorCodes.begin()->first};
        if (opRes.countErrorCodes.size() > 1) {
          // more than a single error code. let's just fail this
          THROW_ARANGO_EXCEPTION(code);
        }
        // got only a single error code. now let's use it, whatever it is.
        opRes.result.reset(code);
      }

      if (opRes.fail() && opRes.isNot(TRI_ERROR_ARANGO_DOCUMENT_NOT_FOUND) &&
          opRes.isNot(TRI_ERROR_ARANGO_CONFLICT)) {
        THROW_ARANGO_EXCEPTION(opRes.result);
      }
      if (opRes.is(TRI_ERROR_ARANGO_CONFLICT)) {
        LOG_PREGEL("4e632", WARN)
            << "conflict while storing " << builder.toJson();
      }

      res = trx->finish(res);
      if (!res.ok()) {
        THROW_ARANGO_EXCEPTION(res);
      }

      if (vocbaseGuard.database().server().isStopping()) {
        LOG_PREGEL("73ec2", WARN) << "Storing data was canceled prematurely";
        THROW_ARANGO_EXCEPTION(TRI_ERROR_SHUTTING_DOWN);
      }

      numDocs = 0;
    }

    builder.clear();
    builder.openArray(true);
  };

  // loop over vertices
  // This loop will fill a buffer of vertices until we run into a new
  // collection
  // or there are no more vertices for to store (or the buffer is full)
  for (auto& vertex : *quiver) {
    if (vertex.shard() != currentShard || numDocs >= 1000) {
      commitTransaction();
      currentShard = vertex.shard();
      shard = graphSerdeConfig.shardID(currentShard);

      auto ctx =
          transaction::StandaloneContext::Create(vocbaseGuard.database());
      trx = std::make_unique<SingleCollectionTransaction>(
          ctx, shard, AccessMode::Type::WRITE);
      trx->addHint(transaction::Hints::Hint::INTERMEDIATE_COMMITS);

      res = trx->begin();
      if (!res.ok()) {
        THROW_ARANGO_EXCEPTION(res);
      }
    }

    std::string_view const key = vertex.key();

    builder.openObject(true);
    builder.add(StaticStrings::KeyString,
                VPackValuePair(key.data(), key.size(), VPackValueType::String));
    V const& data = vertex.data();
    if (auto result = graphFormat->buildVertexDocument(builder, &data);
        !result) {
      LOG_PREGEL("143af", DEBUG) << "Failed to build vertex document";
    }
    builder.close();
    ++numDocs;
    if (numDocs % Utils::batchOfVerticesStoredBeforeUpdatingStatus == 0) {
      std::visit(overload{[&](ActorStoringUpdate const& update) {
                            update.fn(message::GraphStoringUpdate{
                                .verticesStored = 0  // TODO
                            });
                          },
                          [](OldStoringUpdate const& update) {
                            SchedulerFeature::SCHEDULER->queue(
                                RequestLane::INTERNAL_LOW, update.fn);
                          }},
                 updateCallback);
    }
  }

  std::visit(overload{[&](ActorStoringUpdate const& update) {
                        update.fn(message::GraphStoringUpdate{
                            .verticesStored = 0  // TODO
                        });
                      },
                      [](OldStoringUpdate const& update) {
                        SchedulerFeature::SCHEDULER->queue(
                            RequestLane::INTERNAL_LOW, update.fn);
                      }},
             updateCallback);

  // commit the remainders in our buffer
  // will throw if it fails
  commitTransaction();
}

template<typename V, typename E>
auto GraphStorer<V, E>::store(Magazine<V, E> magazine)
    -> futures::Future<futures::Unit> {
  auto futures = std::vector<futures::Future<futures::Unit>>{};
  auto self = this->shared_from_this();

  auto quiverIdx = std::make_shared<std::atomic<size_t>>(0);

  for (auto futureN = size_t{0}; futureN < parallelism; ++futureN) {
    futures.emplace_back(SchedulerFeature::SCHEDULER->queueWithFuture(
        RequestLane::INTERNAL_LOW, [this, self, quiverIdx, magazine] {
          while (true) {
            auto myCurrentQuiverIdx = quiverIdx->fetch_add(1);
            if (myCurrentQuiverIdx >= magazine.size()) {
              break;
            }

            storeQuiver(magazine.quivers.at(myCurrentQuiverIdx));
          }

          return futures::Unit{};
        }));
  }
  return futures::collectAll(futures).thenValue(
      [](auto&& units) { return futures::Unit{}; });
}
}  // namespace arangodb::pregel

template struct arangodb::pregel::GraphStorer<int64_t, int64_t>;
template struct arangodb::pregel::GraphStorer<uint64_t, uint64_t>;
template struct arangodb::pregel::GraphStorer<uint64_t, uint8_t>;
template struct arangodb::pregel::GraphStorer<float, float>;
template struct arangodb::pregel::GraphStorer<double, float>;
template struct arangodb::pregel::GraphStorer<double, double>;
template struct arangodb::pregel::GraphStorer<float, uint8_t>;

// specific algo combos
template struct arangodb::pregel::GraphStorer<arangodb::pregel::algos::WCCValue,
                                              uint64_t>;
template struct arangodb::pregel::GraphStorer<arangodb::pregel::algos::SCCValue,
                                              int8_t>;
template struct arangodb::pregel::GraphStorer<arangodb::pregel::algos::ECValue,
                                              int8_t>;
template struct arangodb::pregel::GraphStorer<
    arangodb::pregel::algos::HITSValue, int8_t>;
template struct arangodb::pregel::GraphStorer<
    arangodb::pregel::algos::HITSKleinbergValue, int8_t>;
template struct arangodb::pregel::GraphStorer<
    arangodb::pregel::algos::DMIDValue, float>;
template struct arangodb::pregel::GraphStorer<arangodb::pregel::algos::LPValue,
                                              int8_t>;
template struct arangodb::pregel::GraphStorer<
    arangodb::pregel::algos::SLPAValue, int8_t>;
template struct arangodb::pregel::GraphStorer<
    arangodb::pregel::algos::ColorPropagationValue, int8_t>;
