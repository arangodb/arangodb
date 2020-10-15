////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2017 ArangoDB GmbH, Cologne, Germany
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
/// @author Manuel Baesler
////////////////////////////////////////////////////////////////////////////////

#include "StatisticsWorker.h"
#include "ConnectionStatistics.h"
#include "RequestStatistics.h"
#include "ServerStatistics.h"
#include "StatisticsFeature.h"

#include "ApplicationFeatures/ApplicationServer.h"
#include "Aql/Query.h"
#include "Aql/QueryString.h"
#include "Basics/ConditionLocker.h"
#include "Basics/PhysicalMemory.h"
#include "Basics/VelocyPackHelper.h"
#include "Basics/process-utils.h"
#include "Cluster/ClusterFeature.h"
#include "Cluster/ClusterInfo.h"
#include "Cluster/ServerState.h"
#include "Logger/LogMacros.h"
#include "Logger/Logger.h"
#include "Logger/LoggerStream.h"
#include "RestServer/MetricsFeature.h"
#include "RestServer/QueryRegistryFeature.h"
#include "RestServer/TtlFeature.h"
#include "Scheduler/Scheduler.h"
#include "Scheduler/SchedulerFeature.h"
#include "Transaction/StandaloneContext.h"
#include "Utils/OperationOptions.h"
#include "Utils/SingleCollectionTransaction.h"
#include "V8Server/V8DealerFeature.h"
#include "VocBase/LogicalCollection.h"
#include "VocBase/Methods/Collections.h"
#include "VocBase/Methods/Indexes.h"

#include <velocypack/Exception.h>
#include <velocypack/Iterator.h>
#include <velocypack/velocypack-aliases.h>

namespace {
static std::string const statisticsCollection("_statistics");
static std::string const statistics15Collection("_statistics15");
static std::string const statisticsRawCollection("_statisticsRaw");

static std::string const garbageCollectionQuery(
    "FOR s in @@collection FILTER s.time < @start RETURN s._key");

static std::string const lastEntryQuery(
    "FOR s in @@collection FILTER s.time >= @start SORT s.time DESC LIMIT 1 "
    "RETURN s");
static std::string const filteredLastEntryQuery(
    "FOR s in @@collection FILTER s.time >= @start FILTER s.clusterId == "
    "@clusterId SORT s.time DESC LIMIT 1 RETURN s");

static std::string const fifteenMinuteQuery(
    "FOR s in _statistics FILTER s.time >= @start SORT s.time RETURN s");
static std::string const filteredFifteenMinuteQuery(
    "FOR s in _statistics FILTER s.time >= @start FILTER s.clusterId == "
    "@clusterId SORT s.time RETURN s");

double extractNumber(VPackSlice slice, char const* attribute) {
  if (!slice.isObject()) {
    return 0.0;
  }
  slice = slice.get(attribute);
  if (!slice.isNumber()) {
    return 0.0;
  }
  return slice.getNumber<double>();
}

}  // namespace

using namespace arangodb;
using namespace arangodb::statistics;

StatisticsWorker::StatisticsWorker(TRI_vocbase_t& vocbase)
    : Thread(vocbase.server(), "StatisticsWorker"), _gcTask(GC_STATS), _vocbase(vocbase) {
  _bytesSentDistribution.openArray();

  for (auto const& val : BytesSentDistributionCuts) {
    _bytesSentDistribution.add(VPackValue(val));
  }

  _bytesSentDistribution.close();

  _bytesReceivedDistribution.openArray();

  for (auto const& val : BytesReceivedDistributionCuts) {
    _bytesReceivedDistribution.add(VPackValue(val));
  }

  _bytesReceivedDistribution.close();

  _requestTimeDistribution.openArray();

  for (auto const& val : RequestTimeDistributionCuts) {
    _requestTimeDistribution.add(VPackValue(val));
  }

  _requestTimeDistribution.close();

  _bindVars = std::make_shared<VPackBuilder>();
}

void StatisticsWorker::collectGarbage() {
  // use _gcTask to separate the different garbage collection operations,
  // and not have them execute all at once sequentially (potential load spike),
  // but only one task at a time. this should spread the load more evenly
  auto time = TRI_microtime();

  try {
    if (_gcTask == GC_STATS) {
      collectGarbage(statisticsCollection, time - 3600.0);  // 1 hour
      _gcTask = GC_STATS_RAW;
    } else if (_gcTask == GC_STATS_RAW) {
      collectGarbage(statisticsRawCollection, time - 3600.0);  // 1 hour
      _gcTask = GC_STATS_15;
    } else if (_gcTask == GC_STATS_15) {
      collectGarbage(statistics15Collection, time - 30.0 * 86400.0);  // 30 days
      _gcTask = GC_STATS;
    }
  } catch (basics::Exception const& ex) {
    if (ex.code() != TRI_ERROR_ARANGO_DATA_SOURCE_NOT_FOUND) {
      // if the underlying collection does not exist, it does not matter
      // that the garbage collection query failed
      throw;
    }
  }
}

void StatisticsWorker::collectGarbage(std::string const& name, double start) const {
  auto bindVars = _bindVars.get();

  bindVars->clear();
  bindVars->openObject();
  bindVars->add("@collection", VPackValue(name));
  bindVars->add("start", VPackValue(start));
  bindVars->close();

  arangodb::aql::Query query(transaction::StandaloneContext::Create(_vocbase),
                             arangodb::aql::QueryString(garbageCollectionQuery),
                             _bindVars, nullptr);

  query.queryOptions().cache = false;

  aql::QueryResult queryResult = query.executeSync();

  if (queryResult.result.fail()) {
    THROW_ARANGO_EXCEPTION(queryResult.result);
  }

  VPackSlice keysToRemove = queryResult.data->slice();
  OperationOptions opOptions;

  opOptions.ignoreRevs = true;
  opOptions.waitForSync = false;
  opOptions.silent = true;

  auto ctx = transaction::StandaloneContext::Create(_vocbase);
  SingleCollectionTransaction trx(ctx, name, AccessMode::Type::WRITE);
  Result res = trx.begin();

  if (!res.ok()) {
    return;
  }

  OperationResult result = trx.remove(name, keysToRemove, opOptions);

  res = trx.finish(result.result);

  if (res.fail()) {
    LOG_TOPIC("14fa9", WARN, Logger::STATISTICS)
        << "removing outdated statistics failed: " << res.errorMessage();
  }
}

void StatisticsWorker::historian() {
  try {
    double now = TRI_microtime();
    std::shared_ptr<arangodb::velocypack::Builder> prevRawBuilder =
        lastEntry(statisticsRawCollection, now - 2.0 * INTERVAL);
    VPackSlice prevRaw = prevRawBuilder->slice();

    _rawBuilder.clear();
    generateRawStatistics(_rawBuilder, now);

    saveSlice(_rawBuilder.slice(), statisticsRawCollection);

    // create the per-seconds statistics
    if (prevRaw.isArray() && prevRaw.length()) {
      VPackSlice slice = prevRaw.at(0).resolveExternals();
      _tempBuilder.clear();
      computePerSeconds(_tempBuilder, _rawBuilder.slice(), slice);
      VPackSlice perSecs = _tempBuilder.slice();

      if (perSecs.length()) {
        saveSlice(perSecs, statisticsCollection);
      }
    }
  } catch (...) {
    // errors on shutdown are expected. do not log them in case they occur
    // if (err.errorNum !== internal.errors.ERROR_SHUTTING_DOWN.code &&
    //     err.errorNum !==
    //     internal.errors.ERROR_ARANGO_CORRUPTED_COLLECTION.code &&
    //     err.errorNum !==
    //     internal.errors.ERROR_ARANGO_CORRUPTED_DATAFILE.code) {
    //   require('console').warn('catch error in historian: %s', err.stack);
  }
}

void StatisticsWorker::historianAverage() {
  try {
    double now = TRI_microtime();

    std::shared_ptr<arangodb::velocypack::Builder> prev15Builder =
        lastEntry(statistics15Collection, now - 2.0 * HISTORY_INTERVAL);
    VPackSlice prev15 = prev15Builder->slice();

    double start;
    if (prev15.isArray() && prev15.length()) {
      VPackSlice slice = prev15.at(0).resolveExternals();
      start = slice.get("time").getNumber<double>();
    } else {
      start = now - HISTORY_INTERVAL;
    }

    _tempBuilder.clear();
    compute15Minute(_tempBuilder, start);
    VPackSlice stat15 = _tempBuilder.slice();

    if (stat15.length()) {
      saveSlice(stat15, statistics15Collection);
    }
  } catch (velocypack::Exception const& ex) {
    LOG_TOPIC("1c429", DEBUG, Logger::STATISTICS)
        << "vpack exception in historian average: " << ex.what();
  } catch (basics::Exception const& ex) {
    LOG_TOPIC("40480", DEBUG, Logger::STATISTICS)
        << "exception in historian average: " << ex.what();
  } catch (...) {
    LOG_TOPIC("570d6", DEBUG, Logger::STATISTICS)
        << "unknown exception in historian average";
  }
}

std::shared_ptr<arangodb::velocypack::Builder> StatisticsWorker::lastEntry(
    std::string const& collectionName, double start) const {
  auto bindVars = _bindVars.get();

  bindVars->clear();
  bindVars->openObject();
  bindVars->add("@collection", VPackValue(collectionName));
  bindVars->add("start", VPackValue(start));

  if (!_clusterId.empty()) {
    bindVars->add("clusterId", VPackValue(_clusterId));
  }

  bindVars->close();

  arangodb::aql::Query query(transaction::StandaloneContext::Create(_vocbase),
                             arangodb::aql::QueryString(_clusterId.empty() ? lastEntryQuery : filteredLastEntryQuery),
                             _bindVars, nullptr);

  query.queryOptions().cache = false;

  aql::QueryResult queryResult = query.executeSync();

  if (queryResult.result.fail()) {
    THROW_ARANGO_EXCEPTION(queryResult.result);
  }

  return queryResult.data;
}

void StatisticsWorker::compute15Minute(VPackBuilder& builder, double start) {
  auto bindVars = _bindVars.get();

  bindVars->clear();
  bindVars->openObject();
  bindVars->add("start", VPackValue(start));

  if (!_clusterId.empty()) {
    bindVars->add("clusterId", VPackValue(_clusterId));
  }

  bindVars->close();

  arangodb::aql::Query query(transaction::StandaloneContext::Create(_vocbase),
                             arangodb::aql::QueryString(
                                 _clusterId.empty() ? fifteenMinuteQuery : filteredFifteenMinuteQuery),
                             _bindVars, nullptr);

  query.queryOptions().cache = false;

  aql::QueryResult queryResult = query.executeSync();

  if (queryResult.result.fail()) {
    THROW_ARANGO_EXCEPTION(queryResult.result);
  }

  VPackSlice result = queryResult.data->slice();
  uint64_t count = result.length();

  builder.clear();
  if (count == 0) {
    builder.openObject();
    builder.close();
    return;
  }

  VPackSlice last = result.at(count - 1).resolveExternals();

  double serverV8available = 0, serverV8busy = 0, serverV8dirty = 0,
         serverV8free = 0, serverV8max = 0, serverThreadsRunning = 0,
         serverThreadsWorking = 0, serverThreadsBlocked = 0, serverThreadsQueued = 0,

         systemMinorPageFaultsPerSecond = 0, systemMajorPageFaultsPerSecond = 0,
         systemUserTimePerSecond = 0, systemSystemTimePerSecond = 0,
         systemResidentSize = 0, systemVirtualSize = 0, systemNumberOfThreads = 0,

         httpRequestsTotalPerSecond = 0, httpRequestsAsyncPerSecond = 0,
         httpRequestsGetPerSecond = 0, httpRequestsHeadPerSecond = 0,
         httpRequestsPostPerSecond = 0, httpRequestsPutPerSecond = 0,
         httpRequestsPatchPerSecond = 0, httpRequestsDeletePerSecond = 0,
         httpRequestsOptionsPerSecond = 0, httpRequestsOtherPerSecond = 0,

         clientHttpConnections = 0, clientBytesSentPerSecond = 0,
         clientBytesReceivedPerSecond = 0, clientAvgTotalTime = 0,
         clientAvgRequestTime = 0, clientAvgQueueTime = 0, clientAvgIoTime = 0;

  for (auto const& vs : VPackArrayIterator(result)) {
    VPackSlice const values = vs.resolveExternals();

    if (!values.isObject()) {
      // oops
      continue;
    }

    VPackSlice server = values.get("server");

    try {
      // in an environment that is mixing 3.4 and previous versions, the
      // following attributes may not be present. we don't want the statistics
      // to give up in this case, but simply ignore these errors
      try {
        VPackSlice v8Contexts = server.get("v8Context");
        serverV8available += extractNumber(v8Contexts, "availablePerSecond");
        serverV8busy += extractNumber(v8Contexts, "busyPerSecond");
        serverV8dirty += extractNumber(v8Contexts, "dirtyPerSecond");
        serverV8free += extractNumber(v8Contexts, "freePerSecond");
        serverV8max += extractNumber(v8Contexts, "maxPerSecond");
      } catch (...) {
        // if attribute "v8Context" is not present, simply do not count
      }

      try {
        VPackSlice threads = server.get("threads");
        serverThreadsRunning += extractNumber(threads, "runningPerSecond");
        serverThreadsWorking += extractNumber(threads, "workingPerSecond");
        serverThreadsBlocked += extractNumber(threads, "blockedPerSecond");
        serverThreadsQueued += extractNumber(threads, "queuedPerSecond");
      } catch (...) {
        // if attribute "threads" is not present, simply do not count
      }

      try {
        VPackSlice system = values.get("system");
        systemMinorPageFaultsPerSecond +=
            extractNumber(system, "minorPageFaultsPerSecond");
        systemMajorPageFaultsPerSecond +=
            extractNumber(system, "majorPageFaultsPerSecond");
        systemUserTimePerSecond += extractNumber(system, "userTimePerSecond");
        systemSystemTimePerSecond +=
            extractNumber(system, "systemTimePerSecond");
        systemResidentSize += extractNumber(system, "residentSize");
        systemVirtualSize += extractNumber(system, "virtualSize");
        systemNumberOfThreads += extractNumber(system, "numberOfThreads");
      } catch (...) {
        // if attribute "system" is not present, simply do not count
      }

      try {
        VPackSlice http = values.get("http");
        httpRequestsTotalPerSecond +=
            extractNumber(http, "requestsTotalPerSecond");
        httpRequestsAsyncPerSecond +=
            extractNumber(http, "requestsAsyncPerSecond");
        httpRequestsGetPerSecond += extractNumber(http, "requestsGetPerSecond");
        httpRequestsHeadPerSecond +=
            extractNumber(http, "requestsHeadPerSecond");
        httpRequestsPostPerSecond +=
            extractNumber(http, "requestsPostPerSecond");
        httpRequestsPutPerSecond += extractNumber(http, "requestsPutPerSecond");
        httpRequestsPatchPerSecond +=
            extractNumber(http, "requestsPatchPerSecond");
        httpRequestsDeletePerSecond +=
            extractNumber(http, "requestsDeletePerSecond");
        httpRequestsOptionsPerSecond +=
            extractNumber(http, "requestsOptionsPerSecond");
        httpRequestsOtherPerSecond +=
            extractNumber(http, "requestsOtherPerSecond");
      } catch (...) {
        // if attribute "http" is not present, simply do not count
      }

      try {
        VPackSlice client = values.get("client");
        clientHttpConnections += extractNumber(client, "httpConnections");
        clientBytesSentPerSecond += extractNumber(client, "bytesSentPerSecond");
        clientBytesReceivedPerSecond +=
            extractNumber(client, "bytesReceivedPerSecond");
        clientAvgTotalTime += extractNumber(client, "avgTotalTime");
        clientAvgRequestTime += extractNumber(client, "avgRequestTime");
        clientAvgQueueTime += extractNumber(client, "avgQueueTime");
        clientAvgIoTime += extractNumber(client, "avgIoTime");
      } catch (...) {
        // if attribute "client" is not present, simply do not count
      }
    } catch (std::exception const& ex) {
      // should almost never happen now
      LOG_TOPIC("e4102", WARN, Logger::STATISTICS)
          << "caught exception during statistics processing: " << ex.what();
    }
  }

  TRI_ASSERT(count > 0);

  serverV8available /= count;
  serverV8busy /= count;
  serverV8dirty /= count;
  serverV8free /= count;
  serverV8max /= count;

  serverThreadsRunning /= count;
  serverThreadsWorking /= count;
  serverThreadsBlocked /= count;
  serverThreadsQueued /= count;

  systemMinorPageFaultsPerSecond /= count;
  systemMajorPageFaultsPerSecond /= count;
  systemUserTimePerSecond /= count;
  systemSystemTimePerSecond /= count;
  systemResidentSize /= count;
  systemVirtualSize /= count;
  systemNumberOfThreads /= count;

  httpRequestsTotalPerSecond /= count;
  httpRequestsAsyncPerSecond /= count;
  httpRequestsGetPerSecond /= count;
  httpRequestsHeadPerSecond /= count;
  httpRequestsPostPerSecond /= count;
  httpRequestsPutPerSecond /= count;
  httpRequestsPatchPerSecond /= count;
  httpRequestsDeletePerSecond /= count;
  httpRequestsOptionsPerSecond /= count;
  httpRequestsOtherPerSecond /= count;

  clientHttpConnections /= count;
  clientBytesSentPerSecond /= count;
  clientBytesReceivedPerSecond /= count;
  clientAvgTotalTime /= count;
  clientAvgRequestTime /= count;
  clientAvgQueueTime /= count;
  clientAvgIoTime /= count;

  builder.openObject();

  builder.add("time", last.get("time"));

  if (!_clusterId.empty()) {
    builder.add("clusterId", VPackValue(_clusterId));
  }

  builder.add("server", VPackValue(VPackValueType::Object));
  builder.add("physicalMemory", last.get("server").get("physicalMemory"));
  builder.add("uptime", last.get("server").get("uptime"));

  builder.add("v8Context", VPackValue(VPackValueType::Object));
  builder.add("availablePerSecond", VPackValue(serverV8available));
  builder.add("busyPerSecond", VPackValue(serverV8busy));
  builder.add("dirtyPerSecond", VPackValue(serverV8dirty));
  builder.add("freePerSecond", VPackValue(serverV8free));
  builder.add("maxPerSecond", VPackValue(serverV8max));
  builder.close();

  builder.add("threads", VPackValue(VPackValueType::Object));
  builder.add("runningPerSecond", VPackValue(serverThreadsRunning));
  builder.add("workingPerSecond", VPackValue(serverThreadsWorking));
  builder.add("blockedPerSecond", VPackValue(serverThreadsBlocked));
  builder.add("queuedPerSecond", VPackValue(serverThreadsQueued));
  builder.close();
  builder.close();

  builder.add("system", VPackValue(VPackValueType::Object));
  builder.add("minorPageFaultsPerSecond", VPackValue(systemMinorPageFaultsPerSecond));
  builder.add("majorPageFaultsPerSecond", VPackValue(systemMajorPageFaultsPerSecond));
  builder.add("userTimePerSecond", VPackValue(systemUserTimePerSecond));
  builder.add("systemTimePerSecond", VPackValue(systemSystemTimePerSecond));
  builder.add("residentSize", VPackValue(systemResidentSize));
  builder.add("virtualSize", VPackValue(systemVirtualSize));
  builder.add("numberOfThreads", VPackValue(systemNumberOfThreads));
  builder.close();

  builder.add("http", VPackValue(VPackValueType::Object));
  builder.add("requestsTotalPerSecond", VPackValue(httpRequestsTotalPerSecond));
  builder.add("requestsAsyncPerSecond", VPackValue(httpRequestsAsyncPerSecond));
  builder.add("requestsGetPerSecond", VPackValue(httpRequestsGetPerSecond));
  builder.add("requestsHeadPerSecond", VPackValue(httpRequestsHeadPerSecond));
  builder.add("requestsPostPerSecond", VPackValue(httpRequestsPostPerSecond));
  builder.add("requestsPutPerSecond", VPackValue(httpRequestsPutPerSecond));
  builder.add("requestsPatchPerSecond", VPackValue(httpRequestsPatchPerSecond));
  builder.add("requestsDeletePerSecond", VPackValue(httpRequestsDeletePerSecond));
  builder.add("requestsOptionsPerSecond", VPackValue(httpRequestsOptionsPerSecond));
  builder.add("requestsOtherPerSecond", VPackValue(httpRequestsOtherPerSecond));
  builder.close();

  builder.add("client", VPackValue(VPackValueType::Object));
  builder.add("httpConnections", VPackValue(clientHttpConnections));
  builder.add("bytesSentPerSecond", VPackValue(clientBytesSentPerSecond));
  builder.add("bytesReceivedPerSecond", VPackValue(clientBytesReceivedPerSecond));
  builder.add("avgTotalTime", VPackValue(clientAvgTotalTime));
  builder.add("avgRequestTime", VPackValue(clientAvgRequestTime));
  builder.add("avgQueueTime", VPackValue(clientAvgQueueTime));
  builder.add("avgIoTime", VPackValue(clientAvgIoTime));
  builder.close();

  builder.close();
}

void StatisticsWorker::computePerSeconds(VPackBuilder& result, VPackSlice const& current,
                                         VPackSlice const& prev) {
  result.clear();
  result.openObject();

  if (prev.get("time").getNumber<double>() + INTERVAL * 1.5 <
      current.get("time").getNumber<double>()) {
    result.close();
    return;
  }

  if (prev.get("server").get("uptime").getNumber<double>() >
      current.get("server").get("uptime").getNumber<double>()) {
    result.close();
    return;
  }

  // compute differences and average per second
  auto dt = current.get("time").getNumber<double>() - prev.get("time").getNumber<double>();

  if (dt <= 0) {
    result.close();
    return;
  }

  result.add("time", current.get("time"));

  VPackSlice currentSystem = current.get("system");
  VPackSlice prevSystem = prev.get("system");
  result.add("system", VPackValue(VPackValueType::Object));
  result.add("minorPageFaultsPerSecond",
             VPackValue((currentSystem.get("minorPageFaults").getNumber<double>() -
                         prevSystem.get("minorPageFaults").getNumber<double>()) /
                        dt));
  result.add("majorPageFaultsPerSecond",
             VPackValue((currentSystem.get("majorPageFaults").getNumber<double>() -
                         prevSystem.get("majorPageFaults").getNumber<double>()) /
                        dt));
  result.add("userTimePerSecond",
             VPackValue((currentSystem.get("userTime").getNumber<double>() -
                         prevSystem.get("userTime").getNumber<double>()) /
                        dt));
  result.add("systemTimePerSecond",
             VPackValue((currentSystem.get("systemTime").getNumber<double>() -
                         prevSystem.get("systemTime").getNumber<double>()) /
                        dt));
  result.add("residentSize", currentSystem.get("residentSize"));
  result.add("residentSizePercent", currentSystem.get("residentSizePercent"));
  result.add("virtualSize", currentSystem.get("virtualSize"));
  result.add("numberOfThreads", currentSystem.get("numberOfThreads"));
  result.close();

  // _serverStatistics()
  VPackSlice currentServer = current.get("server");
  result.add("server", VPackValue(VPackValueType::Object));
  result.add("physicalMemory", currentServer.get("physicalMemory"));
  result.add("uptime", currentServer.get("uptime"));

  VPackSlice currentV8Context = currentServer.get("v8Context");
  result.add("v8Context", VPackValue(VPackValueType::Object));
  if (currentV8Context.isObject()) {
    result.add("availablePerSecond", currentV8Context.get("available"));
    result.add("busyPerSecond", currentV8Context.get("busy"));
    result.add("dirtyPerSecond", currentV8Context.get("dirty"));
    result.add("freePerSecond", currentV8Context.get("free"));
    result.add("maxPerSecond", currentV8Context.get("max"));
  } else {
    // note: V8 may be turned off entirely on some servers
    result.add("availablePerSecond", VPackValue(0));
    result.add("busyPerSecond", VPackValue(0));
    result.add("dirtyPerSecond", VPackValue(0));
    result.add("freePerSecond", VPackValue(0));
    result.add("maxPerSecond", VPackValue(0));
  }
  result.close();

  VPackSlice currentThreads = currentServer.get("threads");
  result.add("threads", VPackValue(VPackValueType::Object));
  result.add("runningPerSecond", currentThreads.get("scheduler-threads"));
  result.add("workingPerSecond", currentThreads.get("in-progress"));
  result.add("blockedPerSecond", currentThreads.get("blocked"));
  result.add("queuedPerSecond", currentThreads.get("queued"));
  result.close();
  result.close();

  VPackSlice currentHttp = current.get("http");
  VPackSlice prevHttp = prev.get("http");
  result.add("http", VPackValue(VPackValueType::Object));
  result.add("requestsTotalPerSecond",
             VPackValue((currentHttp.get("requestsTotal").getNumber<double>() -
                         prevHttp.get("requestsTotal").getNumber<double>()) /
                        dt));
  result.add("requestsAsyncPerSecond",
             VPackValue((currentHttp.get("requestsAsync").getNumber<double>() -
                         prevHttp.get("requestsAsync").getNumber<double>()) /
                        dt));
  result.add("requestsGetPerSecond",
             VPackValue((currentHttp.get("requestsGet").getNumber<double>() -
                         prevHttp.get("requestsGet").getNumber<double>()) /
                        dt));
  result.add("requestsHeadPerSecond",
             VPackValue((currentHttp.get("requestsHead").getNumber<double>() -
                         prevHttp.get("requestsHead").getNumber<double>()) /
                        dt));
  result.add("requestsPostPerSecond",
             VPackValue((currentHttp.get("requestsPost").getNumber<double>() -
                         prevHttp.get("requestsPost").getNumber<double>()) /
                        dt));
  result.add("requestsPutPerSecond",
             VPackValue((currentHttp.get("requestsPut").getNumber<double>() -
                         prevHttp.get("requestsPut").getNumber<double>()) /
                        dt));
  result.add("requestsPatchPerSecond",
             VPackValue((currentHttp.get("requestsPatch").getNumber<double>() -
                         prevHttp.get("requestsPatch").getNumber<double>()) /
                        dt));
  result.add("requestsDeletePerSecond",
             VPackValue((currentHttp.get("requestsDelete").getNumber<double>() -
                         prevHttp.get("requestsDelete").getNumber<double>()) /
                        dt));
  result.add("requestsOptionsPerSecond",
             VPackValue((currentHttp.get("requestsOptions").getNumber<double>() -
                         prevHttp.get("requestsOptions").getNumber<double>()) /
                        dt));
  result.add("requestsOtherPerSecond",
             VPackValue((currentHttp.get("requestsOther").getNumber<double>() -
                         prevHttp.get("requestsOther").getNumber<double>()) /
                        dt));
  result.close();

  VPackSlice currentClient = current.get("client");
  VPackSlice prevClient = prev.get("client");
  result.add("client", VPackValue(VPackValueType::Object));
  result.add("httpConnections", currentClient.get("httpConnections"));

  // bytes sent
  result.add("bytesSentPerSecond",
             VPackValue((currentClient.get("bytesSent").get("sum").getNumber<double>() -
                         prevClient.get("bytesSent").get("sum").getNumber<double>()) /
                        dt));

  result.add(VPackValue("bytesSentPercent"));
  avgPercentDistributon(result, currentClient.get("bytesSent"),
                        prevClient.get("bytesSent"), _bytesSentDistribution);

  // bytes received
  result.add("bytesReceivedPerSecond",
             VPackValue((currentClient.get("bytesReceived").get("sum").getNumber<double>() -
                         prevClient.get("bytesReceived").get("sum").getNumber<double>()) /
                        dt));

  result.add(VPackValue("bytesReceivedPercent"));
  avgPercentDistributon(result, currentClient.get("bytesReceived"),
                        prevClient.get("bytesReceived"), _bytesReceivedDistribution);

  // total time
  auto d1 = currentClient.get("totalTime").get("count").getNumber<double>() -
            prevClient.get("totalTime").get("count").getNumber<double>();

  if (d1 == 0) {
    result.add("avgTotalTime", VPackValue(0));
  } else {
    result.add("avgTotalTime",
               VPackValue((currentClient.get("totalTime").get("sum").getNumber<double>() -
                           prevClient.get("totalTime").get("sum").getNumber<double>()) /
                          d1));
  }

  result.add(VPackValue("totalTimePercent"));
  avgPercentDistributon(result, currentClient.get("totalTime"),
                        prevClient.get("totalTime"), _requestTimeDistribution);

  // request time
  d1 = currentClient.get("requestTime").get("count").getNumber<double>() -
       prevClient.get("requestTime").get("count").getNumber<double>();

  if (d1 == 0) {
    result.add("avgRequestTime", VPackValue(0));
  } else {
    result.add("avgRequestTime",
               VPackValue((currentClient.get("requestTime").get("sum").getNumber<double>() -
                           prevClient.get("requestTime").get("sum").getNumber<double>()) /
                          d1));
  }

  result.add(VPackValue("requestTimePercent"));
  avgPercentDistributon(result, currentClient.get("requestTime"),
                        prevClient.get("requestTime"), _requestTimeDistribution);

  // queue time
  d1 = currentClient.get("queueTime").get("count").getNumber<double>() -
       prevClient.get("queueTime").get("count").getNumber<double>();

  if (d1 == 0) {
    result.add("avgQueueTime", VPackValue(0));
  } else {
    result.add("avgQueueTime",
               VPackValue((currentClient.get("queueTime").get("sum").getNumber<double>() -
                           prevClient.get("queueTime").get("sum").getNumber<double>()) /
                          d1));
  }

  result.add(VPackValue("queueTimePercent"));
  avgPercentDistributon(result, currentClient.get("queueTime"),
                        prevClient.get("queueTime"), _requestTimeDistribution);

  // io time
  d1 = currentClient.get("ioTime").get("count").getNumber<double>() -
       prevClient.get("ioTime").get("count").getNumber<double>();

  if (d1 == 0) {
    result.add("avgIoTime", VPackValue(0));
  } else {
    result.add("avgIoTime",
               VPackValue((currentClient.get("ioTime").get("sum").getNumber<double>() -
                           prevClient.get("ioTime").get("sum").getNumber<double>()) /
                          d1));
  }

  result.add(VPackValue("ioTimePercent"));
  avgPercentDistributon(result, currentClient.get("ioTime"),
                        prevClient.get("ioTime"), _requestTimeDistribution);

  result.close();

  if (!_clusterId.empty()) {
    result.add("clusterId", VPackValue(_clusterId));
  }

  result.close();
}

void StatisticsWorker::avgPercentDistributon(VPackBuilder& builder, VPackSlice const& now,
                                             VPackSlice const& last,
                                             VPackBuilder const& cuts) const {
  uint32_t n = static_cast<uint32_t>(cuts.slice().length() + 1);
  double count = 0;
  std::vector<double> result(n, 0);

  if (last.hasKey("count")) {
    count = now.get("count").getNumber<double>() - last.get("count").getNumber<double>();
  } else {
    count = now.get("count").getNumber<double>();
  }

  if (count > 0.0) {
    VPackSlice counts = now.get("counts");
    VPackSlice lastCounts = last.get("counts");
    for (uint32_t i = 0; i < n; i++) {
      result[i] =
          (counts.at(i).getNumber<double>() - lastCounts.at(i).getNumber<double>()) / count;
    }
  }

  builder.openObject();
  builder.add("values", VPackValue(VPackValueType::Array));
  for (auto const& i : result) {
    builder.add(VPackValue(i));
  }
  builder.close();

  builder.add("cuts", cuts.slice());

  builder.close();
}

// local_name: {"prometheus_name", "type", "help"}
std::map<std::string, std::vector<std::string>> statStrings{
  {"bytesReceived",
   {"arangodb_client_connection_statistics_bytes_received_bucket", "gauge",
    "Bytes received for a request"}},
  {"bytesReceivedCount",
   {"arangodb_client_connection_statistics_bytes_received_count", "gauge",
    "Bytes received for a request"}},
  {"bytesReceivedSum",
   {"arangodb_client_connection_statistics_bytes_received_sum", "gauge",
    "Bytes received for a request"}},
  {"bytesSent",
   {"arangodb_client_connection_statistics_bytes_sent_bucket", "gauge",
    "Bytes sent for a request"}},
  {"bytesSentCount",
   {"arangodb_client_connection_statistics_bytes_sent_count", "gauge",
    "Bytes sent for a request"}},
  {"bytesSentSum",
   {"arangodb_client_connection_statistics_bytes_sent_sum", "gauge",
    "Bytes sent for a request"}},
  {"minorPageFaults",
   {"arangodb_process_statistics_minor_page_faults", "gauge",
    "The number of minor faults the process has made which have not required loading a memory page from disk. This figure is not reported on Windows"}},
  {"majorPageFaults",
   {"arangodb_process_statistics_major_page_faults", "gauge",
    "On Windows, this figure contains the total number of page faults. On other system, this figure contains the number of major faults the process has made which have required loading a memory page from disk"}},
  {"bytesReceived",
   {"arangodb_client_connection_statistics_bytes_received_bucket", "gauge",
    "Bytes received for a request"}},
  {"userTime",
   {"arangodb_process_statistics_user_time", "gauge",
    "Amount of time that this process has been scheduled in user mode, measured in seconds"}},
  {"systemTime",
   {"arangodb_process_statistics_system_time", "gauge",
    "Amount of time that this process has been scheduled in kernel mode, measured in seconds"}},
  {"numberOfThreads",
   {"arangodb_process_statistics_number_of_threads", "gauge",
    "Number of threads in the arangod process"}},
  {"residentSize",
   {"arangodb_process_statistics_resident_set_size", "gauge", "The total size of the number of pages the process has in real memory. This is just the pages which count toward text, data, or stack space. This does not include pages which have not been demand-loaded in, or which are swapped out. The resident set size is reported in bytes"}},
  {"residentSizePercent",
   {"arangodb_process_statistics_resident_set_size_percent", "gauge", "The relative size of the number of pages the process has in real memory compared to system memory. This is just the pages which count toward text, data, or stack space. This does not include pages which have not been demand-loaded in, or which are swapped out. The value is a ratio between 0.00 and 1.00"}},
  {"virtualSize",
   {"arangodb_process_statistics_virtual_memory_size", "gauge", "On Windows, this figure contains the total amount of memory that the memory manager has committed for the arangod process. On other systems, this figure contains The size of the virtual memory the process is using"}},
  {"clientHttpConnections",
   {"arangodb_client_connection_statistics_client_connections", "gauge",
    "The number of client connections that are currently open"}},
  {"connectionTime",
   {"arangodb_client_connection_statistics_connection_time_bucket", "gauge",
    "Total connection time of a client"}},
  {"connectionTimeCount",
   {"arangodb_client_connection_statistics_connection_time_count", "gauge",
    "Total connection time of a client"}},
  {"connectionTimeSum",
   {"arangodb_client_connection_statistics_connection_time_sum", "gauge",
    "Total connection time of a client"}},
  {"totalTime",
   {"arangodb_client_connection_statistics_total_time_bucket", "gauge",
    "Total time needed to answer a request"}},
  {"totalTimeCount",
   {"arangodb_client_connection_statistics_total_time_count", "gauge",
    "Total time needed to answer a request"}},
  {"totalTimeSum",
   {"arangodb_client_connection_statistics_total_time_sum", "gauge",
    "Total time needed to answer a request"}},
  {"requestTime",
   {"arangodb_client_connection_statistics_request_time_bucket", "gauge",
    "Request time needed to answer a request"}},
  {"requestTimeCount",
   {"arangodb_client_connection_statistics_request_time_count", "gauge",
    "Request time needed to answer a request"}},
  {"requestTimeSum",
   {"arangodb_client_connection_statistics_request_time_sum", "gauge",
    "Request time needed to answer a request"}},
  {"queueTime",
   {"arangodb_client_connection_statistics_queue_time_bucket", "gauge",
    "Request time needed to answer a request"}},
  {"queueTimeCount",
   {"arangodb_client_connection_statistics_queue_time_count", "gauge",
    "Request time needed to answer a request"}},
  {"queueTimeSum",
   {"arangodb_client_connection_statistics_queue_time_sum", "gauge",
    "Request time needed to answer a request"}},
  {"ioTime",
   {"arangodb_client_connection_statistics_io_time_bucket", "gauge",
    "Request time needed to answer a request"}},
  {"ioTimeCount",
   {"arangodb_client_connection_statistics_io_time_count", "gauge",
    "Request time needed to answer a request"}},
  {"ioTimeSum",
   {"arangodb_client_connection_statistics_io_time_sum", "gauge",
    "Request time needed to answer a request"}},
  {"httpReqsTotal",
   {"arangodb_http_request_statistics_total_requests", "gauge",
    "Total number of HTTP requests"}},
  {"httpReqsSuperuser",
   {"arangodb_http_request_statistics_superuser_requests", "gauge",
    "Total number of HTTP requests executed by superuser/JWT"}},
  {"httpReqsUser",
   {"arangodb_http_request_statistics_user_requests", "gauge",
    "Total number of HTTP requests executed by clients"}},
  {"httpReqsAsync",
   {"arangodb_http_request_statistics_async_requests", "gauge",
    "Number of asynchronously executed HTTP requests"}},
  {"httpReqsDelete",
   {"arangodb_http_request_statistics_http_delete_requests", "gauge",
    "Number of HTTP DELETE requests"}},
  {"httpReqsGet",
   {"arangodb_http_request_statistics_http_get_requests", "gauge",
    "Number of HTTP GET requests"}},
  {"httpReqsHead",
   {"arangodb_http_request_statistics_http_head_requests", "gauge",
    "Number of HTTP HEAD requests"}},
  {"httpReqsOptions",
   {"arangodb_http_request_statistics_http_options_requests", "gauge",
    "Number of HTTP OPTIONS requests"}},
  {"httpReqsPatch",
   {"arangodb_http_request_statistics_http_patch_requests", "gauge",
    "Number of HTTP PATCH requests"}},
  {"httpReqsPost",
   {"arangodb_http_request_statistics_http_post_requests", "gauge",
    "Number of HTTP POST requests"}},
  {"httpReqsPut",
   {"arangodb_http_request_statistics_http_put_requests", "gauge",
    "Number of HTTP PUT requests"}},
  {"httpReqsOther",
   {"arangodb_http_request_statistics_other_http_requests", "gauge",
    "Number of other HTTP requests"}},
  {"uptime",
   {"arangodb_server_statistics_server_uptime", "gauge",
    "Number of seconds elapsed since server start"}},
  {"physicalSize",
   {"arangodb_server_statistics_physical_memory", "gauge",
    "Physical memory in bytes"}},
  {"v8ContextAvailable",
   {"arangodb_v8_context_alive", "gauge",
    "Number of V8 contexts currently alive"}},
  {"v8ContextBusy",
   {"arangodb_v8_context_busy", "gauge",
    "Number of V8 contexts currently busy"}},
  {"v8ContextDirty",
   {"arangodb_v8_context_dirty", "gauge",
    "Number of V8 contexts currently dirty"}},
  {"v8ContextFree",
   {"arangodb_v8_context_free", "gauge",
    "Number of V8 contexts currently free"}},
  {"v8ContextMax",
   {"arangodb_v8_context_max", "gauge",
    "Maximum number of concurrent V8 contexts"}},
  {"v8ContextMin",
   {"arangodb_v8_context_min", "gauge",
    "Minimum number of concurrent V8 contexts"}},
};

void StatisticsWorker::generateRawStatistics(std::string& result, double const& now) {
  ProcessInfo info = TRI_ProcessInfoSelf();
  uint64_t rss = static_cast<uint64_t>(info._residentSize);
  double rssp = 0;

  if (PhysicalMemory::getValue() != 0) {
    rssp = static_cast<double>(rss) / static_cast<double>(PhysicalMemory::getValue());
  }

  ConnectionStatistics::Snapshot connectionStats;
  ConnectionStatistics::getSnapshot(connectionStats);

  RequestStatistics::Snapshot requestStats;
  RequestStatistics::getSnapshot(requestStats, stats::RequestStatisticsSource::ALL);

  ServerStatistics const& serverInfo =
      _vocbase.server().getFeature<MetricsFeature>().serverStatistics();

  // processStatistics()
  appendMetric(result, std::to_string(info._minorPageFaults), "minorPageFaults");
  appendMetric(result, std::to_string(info._majorPageFaults), "majorPageFaults");
  if (info._scClkTck != 0) {  // prevent division by zero
    appendMetric(
      result, std::to_string(
        static_cast<double>(info._userTime) / static_cast<double>(info._scClkTck)), "userTime");
    appendMetric(
      result, std::to_string(
        static_cast<double>(info._systemTime) / static_cast<double>(info._scClkTck)), "systemTime");
  }
  appendMetric(result, std::to_string(info._numberThreads), "numberOfThreads");
  appendMetric(result, std::to_string(rss), "residentSize");
  appendMetric(result, std::to_string(rssp), "residentSizePercent");
  appendMetric(result, std::to_string(info._virtualSize), "virtualSize");
  appendMetric(result, std::to_string(PhysicalMemory::getValue()), "physicalSize");
  appendMetric(result, std::to_string(serverInfo.uptime()), "uptime");

  // _clientStatistics()
  appendMetric(result, std::to_string(connectionStats.httpConnections.get()), "clientHttpConnections");
  appendHistogram(result, connectionStats.connectionTime, "connectionTime", {"0.01", "1.0", "60.0", "+Inf"});
  appendHistogram(result, requestStats.totalTime, "totalTime", {"0.01", "0.05", "0.1", "0.2", "0.5", "1.0", "+Inf"});
  appendHistogram(result, requestStats.requestTime, "requestTime", {"0.01", "0.05", "0.1", "0.2", "0.5", "1.0", "+Inf"});
  appendHistogram(result, requestStats.queueTime, "queueTime", {"0.01", "0.05", "0.1", "0.2", "0.5", "1.0", "+Inf"});
  appendHistogram(result, requestStats.ioTime, "ioTime", {"0.01", "0.05", "0.1", "0.2", "0.5", "1.0", "+Inf"});
  appendHistogram(result, requestStats.bytesSent, "bytesSent", {"250", "1000", "2000", "5000", "10000", "+Inf"});
  appendHistogram(result, requestStats.bytesReceived, "bytesReceived", {"250", "1000", "2000", "5000", "10000", "+Inf"});

  // _httpStatistics()
  using rest::RequestType;
  appendMetric(result, std::to_string(connectionStats.asyncRequests.get()), "httpReqsAsync");
  appendMetric(result, std::to_string(connectionStats.methodRequests[(int)RequestType::DELETE_REQ].get()), "httpReqsDelete");
  appendMetric(result, std::to_string(connectionStats.methodRequests[(int)RequestType::GET].get()), "httpReqsGet");
  appendMetric(result, std::to_string(connectionStats.methodRequests[(int)RequestType::HEAD].get()), "httpReqsHead");
  appendMetric(result, std::to_string(connectionStats.methodRequests[(int)RequestType::OPTIONS].get()), "httpReqsOptions");
  appendMetric(result, std::to_string(connectionStats.methodRequests[(int)RequestType::PATCH].get()), "httpReqsPatch");
  appendMetric(result, std::to_string(connectionStats.methodRequests[(int)RequestType::POST].get()), "httpReqsPost");
  appendMetric(result, std::to_string(connectionStats.methodRequests[(int)RequestType::PUT].get()), "httpReqsPut");
  appendMetric(result, std::to_string(connectionStats.methodRequests[(int)RequestType::ILLEGAL].get()), "httpReqsOther");
  appendMetric(result, std::to_string(connectionStats.totalRequests.get()), "httpReqsTotal");
  appendMetric(result, std::to_string(connectionStats.totalRequestsSuperuser.get()), "httpReqsSuperuser");
  appendMetric(result, std::to_string(connectionStats.totalRequestsUser.get()), "httpReqsUser");

  V8DealerFeature::Statistics v8Counters{};
  if (_server.hasFeature<V8DealerFeature>()) {
    V8DealerFeature& dealer = _server.getFeature<V8DealerFeature>();
    if (dealer.isEnabled()) {
      v8Counters = dealer.getCurrentContextNumbers();
    }
  }
  appendMetric(result, std::to_string(v8Counters.available), "v8ContextAvailable");
  appendMetric(result, std::to_string(v8Counters.busy), "v8ContextBusy");
  appendMetric(result, std::to_string(v8Counters.dirty), "v8ContextDirty");
  appendMetric(result, std::to_string(v8Counters.free), "v8ContextFree");
  appendMetric(result, std::to_string(v8Counters.min), "v8ContextMin");
  appendMetric(result, std::to_string(v8Counters.max), "v8ContextMax");
  result += "\n";
}

void StatisticsWorker::appendMetric(std::string& result, std::string const& val, std::string const& label) const {
  auto const& stat = statStrings.at(label); 
  std::string const& name = stat.at(0);

  result +=
    "\n#TYPE " + name + " " + stat[1] +
    "\n#HELP " + name + " " + stat[2] + 
    '\n' + name + " " + val + '\n';
}

void StatisticsWorker::appendHistogram(
  std::string& result, Distribution const& dist,
  std::string const& label, std::initializer_list<std::string> const& les) const {

  auto const countLabel = label + "Count";
  auto const countSum = label + "Sum";
  VPackBuilder tmp = fillDistribution(dist);
  VPackSlice slc = tmp.slice();
  VPackSlice counts = slc.get("counts");
  
  auto const& stat = statStrings.at(label); 
  std::string const& name = stat.at(0);

  result +=
    "\n#TYPE " + name + " " + stat[1] + 
    "\n#HELP " + name + " " + stat[2] + '\n';

  TRI_ASSERT(les.size() == counts.length());
  size_t i = 0;
  for (auto const& le : les) {
    result +=
      name + "{le=\"" + le + "\"}"  + " " +
      std::to_string(counts.at(i++).getNumber<uint64_t>()) + '\n';
  }

}

void StatisticsWorker::generateRawStatistics(VPackBuilder& builder, double const& now) {
  ProcessInfo info = TRI_ProcessInfoSelf();
  uint64_t rss = static_cast<uint64_t>(info._residentSize);
  double rssp = 0;

  if (PhysicalMemory::getValue() != 0) {
    rssp = static_cast<double>(rss) / static_cast<double>(PhysicalMemory::getValue());
  }

  ConnectionStatistics::Snapshot connectionStats;
  ConnectionStatistics::getSnapshot(connectionStats);

  RequestStatistics::Snapshot requestStats;
  RequestStatistics::getSnapshot(requestStats, stats::RequestStatisticsSource::ALL);

  ServerStatistics const& serverInfo =
      _vocbase.server().getFeature<MetricsFeature>().serverStatistics();

  builder.openObject();
  if (!_clusterId.empty()) {
    builder.add("clusterId", VPackValue(_clusterId));
  }

  builder.add("time", VPackValue(now));

  // processStatistics()
  builder.add("system", VPackValue(VPackValueType::Object));
  builder.add("minorPageFaults", VPackValue(info._minorPageFaults));
  builder.add("majorPageFaults", VPackValue(info._majorPageFaults));
  if (info._scClkTck != 0) {
    // prevent division by zero
    builder.add("userTime", VPackValue(
                  static_cast<double>(info._userTime) / static_cast<double>(info._scClkTck)));
    builder.add("systemTime", VPackValue(
                  static_cast<double>(info._systemTime) / static_cast<double>(info._scClkTck)));
  }
  builder.add("numberOfThreads", VPackValue(info._numberThreads));
  builder.add("residentSize", VPackValue(rss));
  builder.add("residentSizePercent", VPackValue(rssp));
  builder.add("virtualSize", VPackValue(info._virtualSize));
  builder.close();

  // _clientStatistics()
  builder.add("client", VPackValue(VPackValueType::Object));
  builder.add("httpConnections", VPackValue(connectionStats.httpConnections.get()));

  VPackBuilder tmp = fillDistribution(connectionStats.connectionTime);
  builder.add("connectionTime", tmp.slice());

  tmp = fillDistribution(requestStats.totalTime);
  builder.add("totalTime", tmp.slice());

  tmp = fillDistribution(requestStats.requestTime);
  builder.add("requestTime", tmp.slice());

  tmp = fillDistribution(requestStats.queueTime);
  builder.add("queueTime", tmp.slice());

  tmp = fillDistribution(requestStats.ioTime);
  builder.add("ioTime", tmp.slice());

  tmp = fillDistribution(requestStats.bytesSent);
  builder.add("bytesSent", tmp.slice());

  tmp = fillDistribution(requestStats.bytesReceived);
  builder.add("bytesReceived", tmp.slice());
  builder.close();

  // _httpStatistics()
  using rest::RequestType;
  builder.add("http", VPackValue(VPackValueType::Object));
  builder.add("requestsTotal", VPackValue(connectionStats.totalRequests.get()));
  builder.add("requestsSuperuser", VPackValue(connectionStats.totalRequestsSuperuser.get()));
  builder.add("requestsUser", VPackValue(connectionStats.totalRequestsUser.get()));
  builder.add("requestsAsync", VPackValue(connectionStats.asyncRequests.get()));
  builder.add("requestsGet",
              VPackValue(connectionStats.methodRequests[(int)RequestType::GET].get()));
  builder.add("requestsHead",
              VPackValue(connectionStats.methodRequests[(int)RequestType::HEAD].get()));
  builder.add("requestsPost",
              VPackValue(connectionStats.methodRequests[(int)RequestType::POST].get()));
  builder.add("requestsPut",
              VPackValue(connectionStats.methodRequests[(int)RequestType::PUT].get()));
  builder.add("requestsPatch",
              VPackValue(connectionStats.methodRequests[(int)RequestType::PATCH].get()));
  builder.add("requestsDelete",
              VPackValue(connectionStats.methodRequests[(int)RequestType::DELETE_REQ].get()));
  builder.add("requestsOptions",
              VPackValue(connectionStats.methodRequests[(int)RequestType::OPTIONS].get()));
  builder.add("requestsOther",
              VPackValue(connectionStats.methodRequests[(int)RequestType::ILLEGAL].get()));
  builder.close();

  // _serverStatistics()
  builder.add("server", VPackValue(VPackValueType::Object));
  builder.add("uptime", VPackValue(serverInfo.uptime()));
  builder.add("physicalMemory", VPackValue(PhysicalMemory::getValue()));
  builder.add("transactions", VPackValue(VPackValueType::Object));
  builder.add("started", VPackValue(serverInfo._transactionsStatistics._transactionsStarted.load()));
  builder.add("aborted", VPackValue(serverInfo._transactionsStatistics._transactionsAborted.load()));
  builder.add("committed", VPackValue(serverInfo._transactionsStatistics._transactionsCommitted.load()));
  builder.add("intermediateCommits", VPackValue(serverInfo._transactionsStatistics._intermediateCommits.load()));
  builder.close();

  // export v8 statistics
  builder.add("v8Context", VPackValue(VPackValueType::Object));
  V8DealerFeature::Statistics v8Counters{};
  // std::vector<V8DealerFeature::MemoryStatistics> memoryStatistics;
  // V8 may be turned off on a server
  if (_server.hasFeature<V8DealerFeature>()) {
    V8DealerFeature& dealer = _server.getFeature<V8DealerFeature>();
    if (dealer.isEnabled()) {
      v8Counters = dealer.getCurrentContextNumbers();
      // see below: memoryStatistics = dealer.getCurrentMemoryDetails();
    }
  }
  builder.add("available", VPackValue(v8Counters.available));
  builder.add("busy", VPackValue(v8Counters.busy));
  builder.add("dirty", VPackValue(v8Counters.dirty));
  builder.add("free", VPackValue(v8Counters.free));
  builder.add("min", VPackValue(v8Counters.min));
  builder.add("max", VPackValue(v8Counters.max));
  /* at the time being we don't want to write this into the database so the data volume doesn't increase.
  {
    builder.add("memory", VPackValue(VPackValueType::Array));
    for (auto memStatistic : memoryStatistics) {
      builder.add(VPackValue(VPackValueType::Object));
      builder.add("contextId", VPackValue(memStatistic.id));
      builder.add("tMax", VPackValue(memStatistic.tMax));
      builder.add("countOfTimes", VPackValue(memStatistic.countOfTimes));
      builder.add("heapMax", VPackValue(memStatistic.heapMax));
      builder.add("heapMin", VPackValue(memStatistic.heapMin));
      builder.close();
    }
    builder.close();
  }
  */
  builder.close();

  // export threads statistics
  builder.add("threads", VPackValue(VPackValueType::Object));
  SchedulerFeature::SCHEDULER->toVelocyPack(builder);
  builder.close();

  // export ttl statistics
  TtlFeature& ttlFeature = _server.getFeature<TtlFeature>();
  builder.add(VPackValue("ttl"));
  ttlFeature.statsToVelocyPack(builder);

  builder.close();

  builder.close();
}

VPackBuilder StatisticsWorker::fillDistribution(Distribution const& dist) const {
  VPackBuilder builder;
  builder.openObject();

  builder.add("sum", VPackValue(dist._total));
  builder.add("count", VPackValue(dist._count));

  builder.add("counts", VPackValue(VPackValueType::Array));
  for (auto const& val : dist._counts) {
    builder.add(VPackValue(val));
  }
  builder.close();

  builder.close();

  return builder;
}

void StatisticsWorker::saveSlice(VPackSlice const& slice, std::string const& collection) const {
  if (isStopping()) {
    return;
  }

  arangodb::OperationOptions opOptions;

  opOptions.waitForSync = false;
  opOptions.silent = true;

  // find and load collection given by name or identifier
  auto ctx = transaction::StandaloneContext::Create(_vocbase);
  SingleCollectionTransaction trx(ctx, collection, AccessMode::Type::WRITE);

  trx.addHint(transaction::Hints::Hint::SINGLE_OPERATION);

  Result res = trx.begin();

  if (!res.ok()) {
    LOG_TOPIC("ecdb9", WARN, Logger::STATISTICS) << "could not start transaction on "
                                        << collection << ": " << res.errorMessage();
    return;
  }

  arangodb::OperationResult result = trx.insert(collection, slice, opOptions);

  // Will commit if no error occured.
  // or abort if an error occured.
  // result stays valid!
  res = trx.finish(result.result);
  if (res.fail()) {
    LOG_TOPIC("82af5", WARN, Logger::STATISTICS) << "could not commit stats to " << collection
                                        << ": " << res.errorMessage();
  }
}

void StatisticsWorker::beginShutdown() {
  Thread::beginShutdown();

  // wake up
  CONDITION_LOCKER(guard, _cv);
  guard.signal();
}

void StatisticsWorker::run() {
  while (ServerState::isMaintenance()) {
    if (isStopping()) {
      // startup aborted
      return;
    }
    std::this_thread::sleep_for(std::chrono::milliseconds(100));
  }

  try {
    if (ServerState::instance()->isRunningInCluster()) {
      // compute cluster id just once
      _clusterId = ServerState::instance()->getId();
    }
  } catch (...) {
    // do not fail hard here, as we are inside a thread!
  }

  uint64_t seconds = 0;
  while (!isStopping() && StatisticsFeature::enabled()) {
    seconds++;
    try {
      if (seconds % STATISTICS_INTERVAL == 0) {
        // new stats are produced every 10 seconds
        historian();
      }

      if (seconds % GC_INTERVAL == 0) {
        collectGarbage();
      }

      // process every 15 seconds
      if (seconds % HISTORY_INTERVAL == 0) {
        historianAverage();
      }
    } catch (std::exception const& ex) {
      LOG_TOPIC("92a40", WARN, Logger::STATISTICS)
          << "caught exception in StatisticsWorker: " << ex.what();
    } catch (...) {
      LOG_TOPIC("9a4f9", WARN, Logger::STATISTICS)
          << "caught unknown exception in StatisticsWorker";
    }

    CONDITION_LOCKER(guard, _cv);
    guard.wait(1000 * 1000);
  }
}
