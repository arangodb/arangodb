////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2014-2022 ArangoDB GmbH, Cologne, Germany
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
/// @author Jan Steemann
////////////////////////////////////////////////////////////////////////////////

#include "EdgeCache.h"

#include <iostream>
#include <memory>
#include <optional>
#include <stdexcept>

#include "Aql/Query.h"
#include "Aql/QueryString.h"
#include "ApplicationFeatures/ApplicationServer.h"
#include "Basics/StaticStrings.h"
#include "Basics/overload.h"
#include "Cache/Cache.h"
#include "Cache/Manager.h"
#include "Cache/CacheManagerFeature.h"
#include "RocksDBEngine/RocksDBEngine.h"
#include "StorageEngine/EngineSelectorFeature.h"
#include "Transaction/Manager.h"
#include "Transaction/ManagerFeature.h"
#include "Transaction/Methods.h"
#include "Transaction/StandaloneContext.h"
#include "Utils/SingleCollectionTransaction.h"
#include "Utils/OperationResult.h"
#include "velocypack/Builder.h"

#include "Execution.h"
#include "Server.h"

namespace arangodb::sepp::workloads {

auto EdgeCache::stoppingCriterion() const noexcept -> StoppingCriterion::type {
  return _options.stop;
}

auto EdgeCache::createThreads(Execution& exec, Server& server)
    -> WorkerThreadList {
  ThreadOptions defaultThread;
  defaultThread.stop = _options.stop;

  if (_options.defaultThreadOptions) {
    auto& defaultOpts = _options.defaultThreadOptions.value();
    defaultThread.collection = defaultOpts.collection;
    defaultThread.documentsPerTrx = defaultOpts.documentsPerTrx;
    defaultThread.edgesPerVertex = defaultOpts.edgesPerVertex;
    defaultThread.readsPerEdge = defaultOpts.readsPerEdge;
  }

  WorkerThreadList result;
  for (std::uint32_t i = 0; i < _options.threads; ++i) {
    result.emplace_back(
        std::make_unique<Thread>(defaultThread, i, exec, server));
  }
  return result;
}

EdgeCache::Thread::Thread(ThreadOptions options, std::uint32_t id,
                          Execution& exec, Server& server)
    : ExecutionThread(id, exec, server), _options(std::move(options)) {
  _prefix = _options.collection;
  _prefix.append("/testvalue-");
  _prefix.append(std::to_string(id));
  _prefix.push_back('-');
}

EdgeCache::Thread::~Thread() = default;

void EdgeCache::Thread::run() {
  auto startDocument = currentDocument;
  executeWriteTransaction();
  executeReadTransaction(startDocument);
}

void EdgeCache::Thread::executeWriteTransaction() {
  SingleCollectionTransaction trx(
      transaction::StandaloneContext::Create(*_server.vocbase()),
      _options.collection, AccessMode::Type::WRITE);

  auto res = trx.begin();
  if (!res.ok()) {
    throw std::runtime_error("Failed to begin trx: " +
                             std::string(res.errorMessage()));
  }

  velocypack::Builder builder;
  builder.openArray();
  std::uint32_t j = 0;
  std::uint64_t startDocument = currentDocument;
  std::uint32_t batchSize = 0;
  while (j < _options.documentsPerTrx) {
    builder.openObject();
    buildString(startDocument);
    builder.add(StaticStrings::FromString, VPackValue(scratch));
    buildString(currentDocument);
    builder.add(StaticStrings::ToString, VPackValue(scratch));
    builder.close();

    ++j;
    ++currentDocument;
    if (++batchSize == _options.edgesPerVertex) {
      batchSize = 0;
      startDocument = currentDocument;
    }
  }
  builder.close();

  auto r = trx.insert(_options.collection, builder.slice(), {});
  if (!r.ok()) {
    throw std::runtime_error("Failed to insert edges in trx: " +
                             std::string(r.errorMessage()));
  }

  res = trx.commit();
  if (!res.ok()) {
    throw std::runtime_error("Failed to commit trx: " +
                             std::string(res.errorMessage()));
  }
  _operations += _options.documentsPerTrx;
}

auto EdgeCache::Thread::report() const -> ThreadReport {
  velocypack::Builder data;

  if (id() != 0) {
    // make only one thread report
    data.openObject();
    data.close();
  } else {
    auto* manager =
        _server.vocbase()->server().getFeature<CacheManagerFeature>().manager();
    auto rates = manager->globalHitRates();
    auto stats = manager->memoryStats(cache::Cache::triesGuarantee);

    data.openObject();
    if (stats.has_value()) {
      data.add("peakMemoryUsage", VPackValue(stats->peakGlobalAllocation));
      data.add("peakSpareAllocation", VPackValue(stats->peakSpareAllocation));
      data.add("migrateTasks", VPackValue(stats->migrateTasks));
      data.add("freeMemoryTasks", VPackValue(stats->freeMemoryTasks));
      data.add("lifeTimeHitrate", VPackValue(rates.first));

      auto& engine = _server.vocbase()
                         ->server()
                         .getFeature<EngineSelectorFeature>()
                         .engine<RocksDBEngine>();
      auto [entriesSizeTotal, entriesSizeEffective, inserts, compressedInserts,
            emptyInserts] = engine.getCacheMetrics();
      data.add("inserts", VPackValue(inserts));
      data.add("compressedInserts", VPackValue(compressedInserts));
      data.add(
          "compressedInsertsRate",
          VPackValue(inserts > 0
                         ? 100.0 * (static_cast<double>(compressedInserts) /
                                    static_cast<double>(inserts))
                         : 0.0));
      data.add("emptyInserts", VPackValue(emptyInserts));
      data.add("payloadSizeBeforeCompression", VPackValue(entriesSizeTotal));
      data.add("payloadSizeAfterCompression", VPackValue(entriesSizeEffective));
      data.add(
          "payloadCompressionRate",
          VPackValue(
              entriesSizeTotal > 0
                  ? 100.0 * (1.0 - (static_cast<double>(entriesSizeEffective) /
                                    static_cast<double>(entriesSizeTotal)))
                  : 0.0));
    }
    data.close();
    std::cout << "cache stats: " << data.slice().toJson() << std::endl;
  }
  return {.data = std::move(data), .operations = _operations};
}

void EdgeCache::Thread::executeReadTransaction(std::uint64_t startDocument) {
  constexpr std::string_view qs =
      "FOR doc IN @@collection FILTER doc._from IN @values RETURN doc";

  auto bindVars = std::make_shared<VPackBuilder>();
  bindVars->openObject();
  bindVars->add("@collection", VPackValue(_options.collection));
  bindVars->add("values", VPackValue(VPackValueType::Array));
  std::uint32_t j = 0;
  std::uint64_t currentDocument = startDocument;
  std::uint32_t batchSize = 0;
  std::uint64_t lastStart = ~startDocument;
  while (j < _options.documentsPerTrx) {
    if (startDocument != lastStart) {
      buildString(startDocument);
      bindVars->add(VPackValue(scratch));
      lastStart = startDocument;
    }

    ++j;
    ++currentDocument;
    if (++batchSize == _options.edgesPerVertex) {
      batchSize = 0;
      startDocument = currentDocument;
    }
  }

  bindVars->close();
  bindVars->close();
  VPackSlice opts = VPackSlice::emptyObjectSlice();

  // query 2 times so that stuff is not only loaded from RocksDB into the cache,
  // but also queried from the cache
  for (std::uint32_t i = 0; i < _options.readsPerEdge; ++i) {
    auto query = aql::Query::create(
        transaction::StandaloneContext::Create(*_server.vocbase()),
        arangodb::aql::QueryString(qs), bindVars, aql::QueryOptions(opts));

    auto result = query->executeSync();
    if (result.fail()) {
      throw std::runtime_error("Failed to execute lookup query: " +
                               std::string(result.errorMessage()));
    }
  }
}

void EdgeCache::Thread::buildString(std::uint64_t currentDocument) {
  scratch.clear();
  scratch.append(_prefix);
  scratch.append(std::to_string(currentDocument));
}

auto EdgeCache::Thread::shouldStop() const noexcept -> bool {
  if (execution().stopped()) {
    return true;
  }

  using StopAfterOps = StoppingCriterion::NumberOfOperations;
  if (std::holds_alternative<StopAfterOps>(_options.stop)) {
    return _operations >= std::get<StopAfterOps>(_options.stop).count;
  }
  return false;
}

}  // namespace arangodb::sepp::workloads
