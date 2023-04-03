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

#include "Pregel/Worker/WorkerConfig.h"

#include "Logger/LogMacros.h"

#include "Scheduler/SchedulerFeature.h"
#include "Transaction/StandaloneContext.h"
#include "Utils/OperationOptions.h"
#include "Utils/OperationResult.h"
#include "Utils/SingleCollectionTransaction.h"
#include "VocBase/vocbase.h"

#include "ApplicationFeatures/ApplicationServer.h"

#define LOG_PREGEL(logId, level)          \
  LOG_TOPIC(logId, level, Logger::PREGEL) \
      << "[job " << config->executionNumber() << "] "

namespace arangodb::pregel {

template<typename V, typename E>
auto GraphStorer<V, E>::store(
    std::vector<std::shared_ptr<Quiver<V, E>>> quivers) -> Result {
  // TODO: I do no like this approach. Currently the interface is using a vector
  // of quivers, but this concept does not fit this approach here. If we write
  // into a VPackBuilder, we need all quivers at once. If we want to write
  // parallel per quiver, we only need a single quiver here. Rework needed.
  TRI_ASSERT(quivers.size() == 1);
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

      if (config->vocbase()->server().isStopping()) {
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
  auto quiver = quivers.at(0);
  for (auto& vertex : *quiver) {
    if (vertex.shard() != currentShard || numDocs >= 1000) {
      commitTransaction();

      currentShard = vertex.shard();
      shard = globalShards[currentShard.value];

      auto ctx = transaction::StandaloneContext::Create(*config->vocbase());
      trx = std::make_unique<SingleCollectionTransaction>(
          ctx, shard, AccessMode::Type::WRITE);
      trx->addHint(transaction::Hints::Hint::INTERMEDIATE_COMMITS);

      res = trx->begin();
      if (!res.ok()) {
        return res;
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
      SchedulerFeature::SCHEDULER->queue(RequestLane::INTERNAL_LOW,
                                         statusUpdateCallback);
    }
  }

  SchedulerFeature::SCHEDULER->queue(RequestLane::INTERNAL_LOW,
                                     statusUpdateCallback);

  // commit the remainders in our buffer
  // will throw if it fails
  commitTransaction();
  return {TRI_ERROR_NO_ERROR};
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
