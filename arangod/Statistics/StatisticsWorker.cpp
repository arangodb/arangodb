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

#include "ConnectionStatistics.h"
#include "RequestStatistics.h"
#include "ServerStatistics.h"
#include "StatisticsFeature.h"
#include "StatisticsWorker.h"

#include "Aql/Query.h"
#include "Aql/QueryString.h"
#include "Basics/process-utils.h"
#include "Basics/VelocyPackHelper.h"
#include "Cluster/ClusterFeature.h"
#include "Cluster/ClusterInfo.h"
#include "Cluster/ServerState.h"
#include "Logger/Logger.h"
#include "RestServer/DatabaseFeature.h"
#include "RestServer/QueryRegistryFeature.h"
#include "Scheduler/Scheduler.h"
#include "Scheduler/SchedulerFeature.h"
#include "Transaction/StandaloneContext.h"
#include "Utils/OperationOptions.h"
#include "Utils/SingleCollectionTransaction.h"
#include "VocBase/Methods/Indexes.h"
#include "V8Server/V8DealerFeature.h"

#include <velocypack/Exception.h>
#include <velocypack/Iterator.h>
#include <velocypack/velocypack-aliases.h>

#include <iostream>


using namespace arangodb;
using namespace arangodb::basics;

StatisticsWorker::StatisticsWorker() : Thread("StatisticsWorker") {
  _bytesSentDistribution.openArray();
  for (auto const& val : TRI_BytesSentDistributionVectorStatistics._value) {
    _bytesSentDistribution.add(VPackValue(val));
  }
  _bytesSentDistribution.close();

  _bytesReceivedDistribution.openArray();
  for (auto const& val : TRI_BytesReceivedDistributionVectorStatistics._value) {
    _bytesReceivedDistribution.add(VPackValue(val));
  }
  _bytesReceivedDistribution.close();

  _requestTimeDistribution.openArray();
  for (auto const& val : TRI_RequestTimeDistributionVectorStatistics._value) {
    _requestTimeDistribution.add(VPackValue(val));
  }
  _requestTimeDistribution.close();
}

void StatisticsWorker::collectGarbage() const {

  auto time = TRI_microtime();

  _collectGarbage("_statistics", time - 3600); // 1 hour
  _collectGarbage("_statisticsRaw", time - 3600); // 1 hour
  _collectGarbage("_statistics15", time  - 30 * 86400);  // 30 days
}

void StatisticsWorker::_collectGarbage(std::string const& collectionName,
                                       double start) const {

  auto queryRegistryFeature =
       application_features::ApplicationServer::getFeature<
            QueryRegistryFeature>("QueryRegistry");
  auto _queryRegistry = queryRegistryFeature->queryRegistry();

  TRI_vocbase_t* vocbase = DatabaseFeature::DATABASE->systemDatabase();

  auto bindVars = std::make_shared<VPackBuilder>();
  bindVars->openObject();
  bindVars->add("@collection", VPackValue(collectionName));
  bindVars->add("start", VPackValue(start));
  bindVars->close();

  std::string const aql("FOR s in @@collection FILTER s.time < @start RETURN s._key");
    arangodb::aql::Query query(false, vocbase, arangodb::aql::QueryString(aql),
                               bindVars, nullptr, arangodb::aql::PART_MAIN);

    auto queryResult = query.execute(_queryRegistry);
    if (queryResult.code != TRI_ERROR_NO_ERROR) {
      THROW_ARANGO_EXCEPTION_MESSAGE(queryResult.code, queryResult.details);
    }

    VPackSlice keysToRemove = queryResult.result->slice();

    OperationOptions opOptions;
    opOptions.returnOld = false;
    opOptions.ignoreRevs = true;
    opOptions.waitForSync = false;
    opOptions.silent = false;

    auto transactionContext(transaction::StandaloneContext::Create(vocbase));

    SingleCollectionTransaction trx(transactionContext, collectionName,
                                    AccessMode::Type::WRITE);
    trx.addHint(transaction::Hints::Hint::SINGLE_OPERATION);

    Result res = trx.begin();

    if (!res.ok()) {
      return;
    }

    OperationResult result = trx.remove(collectionName, keysToRemove, opOptions);

    res = trx.finish(result.code);

    /* useless until we do something
    if (!res.ok()) {
      return;
    } */
}





void StatisticsWorker::historian() const {
  std::string clusterId = "";

  if (ServerState::instance()->isRunningInCluster()) {
    clusterId = ServerState::instance()->getId();
  }

  try {
    auto now = TRI_microtime();

    std::shared_ptr<arangodb::velocypack::Builder> prevRawBuilder = _lastEntry("_statisticsRaw", now - 2 * INTERVAL, clusterId);
    VPackSlice prevRaw = prevRawBuilder->slice();

    VPackBuilder rawBuilder = _generateRawStatistics(clusterId, now);

    _saveSlice(rawBuilder.slice(), "_statisticsRaw");

    // create the per-seconds statistics
    if (prevRaw.isArray() && prevRaw.length()) {
      VPackSlice slice = prevRaw.at(0).resolveExternals();
      VPackBuilder perSecsBuilder = _computePerSeconds(rawBuilder.slice(), slice, clusterId);
      VPackSlice perSecs = perSecsBuilder.slice();

      if (perSecs.length()) {
        _saveSlice(perSecs, "_statistics");
      }
    }
  } catch (...) {
    // errors on shutdown are expected. do not log them in case they occur
    // if (err.errorNum !== internal.errors.ERROR_SHUTTING_DOWN.code &&
    //     err.errorNum !== internal.errors.ERROR_ARANGO_CORRUPTED_COLLECTION.code &&
    //     err.errorNum !== internal.errors.ERROR_ARANGO_CORRUPTED_DATAFILE.code) {
    //   require('console').warn('catch error in historian: %s', err.stack);
  }
}



void StatisticsWorker::historianAverage() const {
  std::string clusterId = "";

  if (ServerState::instance()->isRunningInCluster()) {
    clusterId = ServerState::instance()->getId();
  }

  try {
    uint64_t now = static_cast<uint64_t>(TRI_microtime());
    uint64_t start;

    std::shared_ptr<arangodb::velocypack::Builder> prev15Builder = _lastEntry("_statistics15", now - 2 * HISTORY_INTERVAL, clusterId);
    VPackSlice prev15 = prev15Builder->slice();

    if (prev15.isArray() && prev15.length()) {
      VPackSlice slice = prev15.at(0).resolveExternals();
      start = slice.get("time").getNumber<uint64_t>();
    } else {
      start = now - HISTORY_INTERVAL;
    }

    VPackBuilder builder = _compute15Minute(start, clusterId);
    VPackSlice stat15 = builder.slice();

    if (stat15.length()) {
      _saveSlice(stat15, "_statistics15");
    }
  } catch(velocypack::Exception const& ex) {
    LOG_TOPIC(DEBUG, Logger::STATISTICS) << "vpack exception in historian average "
    << ex.what() << " " << ex.errorCode() << " " << ex;

  } catch(basics::Exception const& ex) {
    LOG_TOPIC(DEBUG, Logger::STATISTICS) << "Exception in historian average "
    << ex.what() << " " << ex.message() << " " << ex.code();

  } catch (...) {
    LOG_TOPIC(DEBUG, Logger::STATISTICS) << "Exception in historian average";
  }
}

std::shared_ptr<arangodb::velocypack::Builder> StatisticsWorker::_lastEntry(
  std::string const& collectionName, double start, std::string const& clusterId) const {
  std::string filter = "";

  auto queryRegistryFeature =
  application_features::ApplicationServer::getFeature<
      QueryRegistryFeature>("QueryRegistry");
  auto _queryRegistry = queryRegistryFeature->queryRegistry();

  TRI_vocbase_t* vocbase = DatabaseFeature::DATABASE->systemDatabase();

  auto bindVars = std::make_shared<VPackBuilder>();
  bindVars->openObject();
  bindVars->add("@collection", VPackValue(collectionName));
  bindVars->add("start", VPackValue(start));

  if (clusterId.length()) {
    filter = "FILTER s.clusterId == @clusterId";
    bindVars->add("clusterId", VPackValue(clusterId));
  }

  bindVars->close();

  std::string const aql("FOR s in @@collection FILTER s.time >= @start " + filter + " SORT s.time DESC LIMIT 1 RETURN s");
  arangodb::aql::Query query(false, vocbase, arangodb::aql::QueryString(aql),
                          bindVars, nullptr, arangodb::aql::PART_MAIN);

  auto queryResult = query.execute(_queryRegistry);
  if (queryResult.code != TRI_ERROR_NO_ERROR) {
    THROW_ARANGO_EXCEPTION_MESSAGE(queryResult.code, queryResult.details);
  }

  return queryResult.result;
}

VPackBuilder StatisticsWorker::_compute15Minute(double start, std::string const& clusterId) const {
  std::string filter = "";

  auto queryRegistryFeature =
  application_features::ApplicationServer::getFeature<
      QueryRegistryFeature>("QueryRegistry");
  auto _queryRegistry = queryRegistryFeature->queryRegistry();

  TRI_vocbase_t* vocbase = DatabaseFeature::DATABASE->systemDatabase();

  auto bindVars = std::make_shared<VPackBuilder>();
  bindVars->openObject();
  bindVars->add("start", VPackValue(start));

  if (clusterId.length()) {
    filter = "FILTER s.clusterId == @clusterId";
    bindVars->add("clusterId", VPackValue(clusterId));
  }

  bindVars->close();

  std::string const aql("FOR s in _statistics FILTER s.time >= @start " + filter + " SORT s.time RETURN s");
  arangodb::aql::Query query(false, vocbase, arangodb::aql::QueryString(aql),
                          bindVars, nullptr, arangodb::aql::PART_MAIN);

  auto queryResult = query.execute(_queryRegistry);
  if (queryResult.code != TRI_ERROR_NO_ERROR) {
    THROW_ARANGO_EXCEPTION_MESSAGE(queryResult.code, queryResult.details);
  }

  VPackSlice result = queryResult.result->slice();
  uint64_t count = result.length();

  if (count == 0) {
    VPackBuilder builder;
    builder.openObject();
    builder.close();
    return builder;
  }

  VPackSlice last = result.at(count-1).resolveExternals();

  float serverV8available = 0,
  serverV8busy = 0,
  serverV8dirty = 0,
  serverV8free = 0,
  serverV8max = 0,
  serverThreadsRunning = 0,
  serverThreadsWorking = 0,
  serverThreadsBlocked = 0,
  serverThreadsQueued = 0,

  systemMinorPageFaultsPerSecond = 0,
  systemMajorPageFaultsPerSecond = 0,
  systemUserTimePerSecond = 0,
  systemSystemTimePerSecond = 0,
  systemResidentSize = 0,
  systemVirtualSize = 0,
  systemNumberOfThreads = 0,

  httpRequestsTotalPerSecond = 0,
  httpRequestsAsyncPerSecond = 0,
  httpRequestsGetPerSecond = 0,
  httpRequestsHeadPerSecond = 0,
  httpRequestsPostPerSecond = 0,
  httpRequestsPutPerSecond = 0,
  httpRequestsPatchPerSecond = 0,
  httpRequestsDeletePerSecond = 0,
  httpRequestsOptionsPerSecond = 0,
  httpRequestsOtherPerSecond = 0,

  clientHttpConnections = 0,
  clientBytesSentPerSecond = 0,
  clientBytesReceivedPerSecond = 0,
  clientAvgTotalTime = 0,
  clientAvgRequestTime = 0,
  clientAvgQueueTime = 0,
  clientAvgIoTime = 0;

  for (auto const& vs : VPackArrayIterator(result)) {
    VPackSlice const& values = vs.resolveExternals();

    serverV8available += values.get("server").get("v8Context").get("availablePerSecond").getNumber<float>();
    serverV8busy += values.get("server").get("v8Context").get("busyPerSecond").getNumber<float>();
    serverV8dirty += values.get("server").get("v8Context").get("dirtyPerSecond").getNumber<float>();
    serverV8free += values.get("server").get("v8Context").get("freePerSecond").getNumber<float>();
    serverV8max += values.get("server").get("v8Context").get("maxPerSecond").getNumber<float>();

    serverThreadsRunning += values.get("server").get("threads").get("runningPerSecond").getNumber<float>();
    serverThreadsWorking += values.get("server").get("threads").get("workingPerSecond").getNumber<float>();
    serverThreadsBlocked += values.get("server").get("threads").get("blockedPerSecond").getNumber<float>();
    serverThreadsQueued += values.get("server").get("threads").get("queuedPerSecond").getNumber<float>();

    systemMinorPageFaultsPerSecond += values.get("system").get("minorPageFaultsPerSecond").getNumber<float>();
    systemMajorPageFaultsPerSecond += values.get("system").get("majorPageFaultsPerSecond").getNumber<float>();
    systemUserTimePerSecond += values.get("system").get("userTimePerSecond").getNumber<float>();
    systemSystemTimePerSecond += values.get("system").get("systemTimePerSecond").getNumber<float>();
    systemResidentSize += values.get("system").get("residentSize").getNumber<float>();
    systemVirtualSize += values.get("system").get("virtualSize").getNumber<float>();
    systemNumberOfThreads += values.get("system").get("numberOfThreads").getNumber<float>();

    httpRequestsTotalPerSecond += values.get("http").get("requestsTotalPerSecond").getNumber<float>();
    httpRequestsAsyncPerSecond += values.get("http").get("requestsAsyncPerSecond").getNumber<float>();
    httpRequestsGetPerSecond += values.get("http").get("requestsGetPerSecond").getNumber<float>();
    httpRequestsHeadPerSecond += values.get("http").get("requestsHeadPerSecond").getNumber<float>();
    httpRequestsPostPerSecond += values.get("http").get("requestsPostPerSecond").getNumber<float>();
    httpRequestsPutPerSecond += values.get("http").get("requestsPutPerSecond").getNumber<float>();
    httpRequestsPatchPerSecond += values.get("http").get("requestsPatchPerSecond").getNumber<float>();
    httpRequestsDeletePerSecond += values.get("http").get("requestsDeletePerSecond").getNumber<float>();
    httpRequestsOptionsPerSecond += values.get("http").get("requestsOptionsPerSecond").getNumber<float>();
    httpRequestsOtherPerSecond += values.get("http").get("requestsOtherPerSecond").getNumber<float>();

    clientHttpConnections += values.get("client").get("httpConnections").getNumber<float>();
    clientBytesSentPerSecond += values.get("client").get("bytesSentPerSecond").getNumber<float>();
    clientBytesReceivedPerSecond += values.get("client").get("bytesReceivedPerSecond").getNumber<float>();
    clientAvgTotalTime += values.get("client").get("avgTotalTime").getNumber<float>();
    clientAvgRequestTime += values.get("client").get("avgRequestTime").getNumber<float>();
    clientAvgQueueTime += values.get("client").get("avgQueueTime").getNumber<float>();
    clientAvgIoTime += values.get("client").get("avgIoTime").getNumber<float>();
  }

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

  VPackBuilder builder;
  builder.openObject();

  builder.add("time", last.get("time"));

  if (clusterId.length()) {
    builder.add("clusterId", VPackValue(clusterId));
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

  return builder;
}

VPackBuilder StatisticsWorker::_computePerSeconds(
  VPackSlice const& current, VPackSlice const& prev, std::string const& clusterId) const {
  VPackBuilder result;
  result.openObject();

  if (prev.get("time").getNumber<double>() + INTERVAL * 1.5 < current.get("time").getNumber<double>()) {
    result.close();
    return result;
  }

  if (prev.get("server").get("uptime").getNumber<double>() > current.get("server").get("uptime").getNumber<double>()) {
      result.close();
      return result;
  }

  // compute differences and average per second
  auto dt = current.get("time").getNumber<double>() - prev.get("time").getNumber<double>();

  if (dt <= 0) {
    result.close();
    return result;
  }

  result.add("time", current.get("time"));

  result.add("system", VPackValue(VPackValueType::Object));
  result.add("minorPageFaultsPerSecond", VPackValue((current.get("system").get("minorPageFaults").getNumber<float>() -
                                                        prev.get("system").get("minorPageFaults").getNumber<float>()) / dt));
  result.add("majorPageFaultsPerSecond", VPackValue((current.get("system").get("majorPageFaults").getNumber<float>() -
                                                        prev.get("system").get("majorPageFaults").getNumber<float>()) / dt));
  result.add("userTimePerSecond", VPackValue((current.get("system").get("userTime").getNumber<float>() -
                                                 prev.get("system").get("userTime").getNumber<float>()) / dt));
  result.add("systemTimePerSecond", VPackValue((current.get("system").get("systemTime").getNumber<float>() -
                                                   prev.get("system").get("systemTime").getNumber<float>()) / dt));
  result.add("residentSize", current.get("system").get("residentSize"));
  result.add("residentSizePercent", current.get("system").get("residentSizePercent"));
  result.add("virtualSize", current.get("system").get("virtualSize"));
  result.add("numberOfThreads", current.get("system").get("numberOfThreads"));
  result.close();

  // _serverStatistics()
  result.add("server", VPackValue(VPackValueType::Object));
  result.add("physicalMemory", current.get("server").get("physicalMemory"));
  result.add("uptime", current.get("server").get("uptime"));
  result.add("v8Context", VPackValue(VPackValueType::Object));
  result.add("availablePerSecond", current.get("server").get("v8Context").get("available"));
  result.add("busyPerSecond", current.get("server").get("v8Context").get("busy"));
  result.add("dirtyPerSecond", current.get("server").get("v8Context").get("dirty"));
  result.add("freePerSecond", current.get("server").get("v8Context").get("free"));
  result.add("maxPerSecond", current.get("server").get("v8Context").get("max"));
  result.close();

  result.add("threads", VPackValue(VPackValueType::Object));
  result.add("runningPerSecond", current.get("server").get("threads").get("running"));
  result.add("workingPerSecond", current.get("server").get("threads").get("working"));
  result.add("blockedPerSecond", current.get("server").get("threads").get("blocked"));
  result.add("queuedPerSecond", current.get("server").get("threads").get("queued"));
  result.close();
  result.close();

  result.add("http", VPackValue(VPackValueType::Object));
  result.add("requestsTotalPerSecond", VPackValue((current.get("http").get("requestsTotal").getNumber<float>() -
                                                      prev.get("http").get("requestsTotal").getNumber<float>()) / dt));
  result.add("requestsAsyncPerSecond", VPackValue((current.get("http").get("requestsAsync").getNumber<float>() -
                                                      prev.get("http").get("requestsAsync").getNumber<float>()) / dt));
  result.add("requestsGetPerSecond", VPackValue((current.get("http").get("requestsGet").getNumber<float>() -
                                                    prev.get("http").get("requestsGet").getNumber<float>()) / dt));
  result.add("requestsHeadPerSecond", VPackValue((current.get("http").get("requestsHead").getNumber<float>() -
                                                     prev.get("http").get("requestsHead").getNumber<float>()) / dt));
  result.add("requestsPostPerSecond", VPackValue((current.get("http").get("requestsPost").getNumber<float>() -
                                                     prev.get("http").get("requestsPost").getNumber<float>()) / dt));
  result.add("requestsPutPerSecond", VPackValue((current.get("http").get("requestsPut").getNumber<float>() -
                                                    prev.get("http").get("requestsPut").getNumber<float>()) / dt));
  result.add("requestsPatchPerSecond", VPackValue((current.get("http").get("requestsPatch").getNumber<float>() -
                                                      prev.get("http").get("requestsPatch").getNumber<float>()) / dt));
  result.add("requestsDeletePerSecond", VPackValue((current.get("http").get("requestsDelete").getNumber<float>() -
                                                       prev.get("http").get("requestsDelete").getNumber<float>()) / dt));
  result.add("requestsOptionsPerSecond", VPackValue((current.get("http").get("requestsOptions").getNumber<float>() -
                                                        prev.get("http").get("requestsOptions").getNumber<float>()) / dt));
  result.add("requestsOtherPerSecond", VPackValue((current.get("http").get("requestsOther").getNumber<float>() -
                                                      prev.get("http").get("requestsOther").getNumber<float>()) / dt));
  result.close();

  result.add("client", VPackValue(VPackValueType::Object));
  result.add("httpConnections", current.get("client").get("httpConnections"));

  // bytes sent
  result.add("bytesSentPerSecond", VPackValue((current.get("client").get("bytesSent").get("sum").getNumber<float>() -
                                                  prev.get("client").get("bytesSent").get("sum").getNumber<float>()) / dt));

    VPackBuilder tmp = _avgPercentDistributon(current.get("client").get("bytesSent"), prev.get("client").get("bytesSent"), _bytesSentDistribution);
    result.add("bytesSentPercent", tmp.slice());

    // bytes received
    result.add("bytesReceivedPerSecond", VPackValue((current.get("client").get("bytesReceived").get("sum").getNumber<float>() -
                                                        prev.get("client").get("bytesReceived").get("sum").getNumber<float>()) / dt));

    tmp = _avgPercentDistributon(current.get("client").get("bytesReceived"), prev.get("client").get("bytesReceived"), _bytesReceivedDistribution);
    result.add("bytesReceivedPercent", tmp.slice());

    // total time
    auto d1 = current.get("client").get("totalTime").get("count").getNumber<float>() - prev.get("client").get("totalTime").get("count").getNumber<float>();

    if (d1 == 0) {
      result.add("avgTotalTime", VPackValue(0));
    } else {
      result.add("avgTotalTime", VPackValue((current.get("client").get("totalTime").get("sum").getNumber<float>() -
                                                prev.get("client").get("totalTime").get("sum").getNumber<float>()) / d1));
    }

    tmp = _avgPercentDistributon(current.get("client").get("totalTime"), prev.get("client").get("totalTime"), _requestTimeDistribution);
    result.add("totalTimePercent", tmp.slice());

  // request time
  d1 = current.get("client").get("requestTime").get("count").getNumber<float>() - prev.get("client").get("requestTime").get("count").getNumber<float>();

  if (d1 == 0) {
    result.add("avgRequestTime", VPackValue(0));
  } else {
    result.add("avgRequestTime", VPackValue((current.get("client").get("requestTime").get("sum").getNumber<float>() -
                                                prev.get("client").get("requestTime").get("sum").getNumber<float>()) / d1));
  }
  tmp = _avgPercentDistributon(current.get("client").get("requestTime"), prev.get("client").get("requestTime"), _requestTimeDistribution);
  result.add("requestTimePercent", tmp.slice());

  // queue time
  d1 = current.get("client").get("queueTime").get("count").getNumber<float>() - prev.get("client").get("queueTime").get("count").getNumber<float>();

  if (d1 == 0) {
    result.add("avgQueueTime", VPackValue(0));
  } else {
    result.add("avgQueueTime", VPackValue((current.get("client").get("queueTime").get("sum").getNumber<float>() -
                                              prev.get("client").get("queueTime").get("sum").getNumber<float>()) / d1));
  }

  tmp = _avgPercentDistributon(current.get("client").get("queueTime"), prev.get("client").get("queueTime"), _requestTimeDistribution);
  result.add("queueTimePercent", tmp.slice());

  // io time
  d1 = current.get("client").get("ioTime").get("count").getNumber<float>() - prev.get("client").get("ioTime").get("count").getNumber<float>();

  if (d1 == 0) {
    result.add("avgIoTime", VPackValue(0));
  } else {
    result.add("avgIoTime", VPackValue((current.get("client").get("ioTime").get("sum").getNumber<float>() -
                                           prev.get("client").get("ioTime").get("sum").getNumber<float>()) / d1));
  }

  tmp = _avgPercentDistributon(current.get("client").get("ioTime"), prev.get("client").get("ioTime"), _requestTimeDistribution);
  result.add("ioTimePercent", tmp.slice());

  result.close();

  if (clusterId.length()) {
    result.add("clusterId", VPackValue(clusterId));
  }

  result.close();

  return result;
}

VPackBuilder StatisticsWorker::_avgPercentDistributon(
  VPackSlice const& now, VPackSlice const& last, VPackBuilder const& cuts) const {
  VPackBuilder builder;

  uint32_t n = cuts.slice().length() + 1;
  uint32_t count = 0;
  std::vector<float> result(n, 0);

  if (last.hasKey("count")) {
    count = now.get("count").getNumber<float>() - last.get("count").getNumber<float>();
  } else {
    count = now.get("count").getNumber<float>();
  }

  if ( count != 0) {
    VPackSlice counts = now.get("counts");
    VPackSlice lastCounts = last.get("counts");
    for (uint32_t i = 0;  i < n;  i++) {
      result[i] = (counts.at(i).getNumber<float>() - lastCounts.at(i).getNumber<float>()) / count;
    }
  }

  builder.openObject();
  builder.add("values", VPackValue(VPackValueType::Array));
  for (auto const& n : result) {
    builder.add(VPackValue(n));
  }
  builder.close();

  builder.add("cuts", cuts.slice());

  builder.close();

  return builder;
}

VPackBuilder StatisticsWorker::_generateRawStatistics(std::string const& clusterId, double const& now) const {
  ProcessInfo info = TRI_ProcessInfoSelf();
  double rss = static_cast<double>(info._residentSize);
  double rssp = 0;

  if (TRI_PhysicalMemory != 0) {
    rssp = rss / TRI_PhysicalMemory;
  }

  StatisticsCounter httpConnections;
  StatisticsCounter totalRequests;
  std::vector<StatisticsCounter> methodRequests;
  StatisticsCounter asyncRequests;
  StatisticsDistribution connectionTime;

  ConnectionStatistics::fill(httpConnections, totalRequests, methodRequests,
                             asyncRequests, connectionTime);

  StatisticsDistribution totalTime;
  StatisticsDistribution requestTime;
  StatisticsDistribution queueTime;
  StatisticsDistribution ioTime;
  StatisticsDistribution bytesSent;
  StatisticsDistribution bytesReceived;

  RequestStatistics::fill(totalTime, requestTime, queueTime, ioTime, bytesSent,
                          bytesReceived);

  ServerStatistics serverInfo = ServerStatistics::statistics();

  V8DealerFeature* dealer =
  application_features::ApplicationServer::getFeature<V8DealerFeature>("V8Dealer");
  auto v8Counters = dealer->getCurrentContextNumbers();

  auto threadCounters = SchedulerFeature::SCHEDULER->getCounters();

  VPackBuilder builder;
  builder.openObject();

  if (clusterId.length()) {
    builder.add("clusterId", VPackValue(clusterId));
  }

  builder.add("time", VPackValue(now));

  // processStatistics()
  builder.add("system", VPackValue(VPackValueType::Object));
  builder.add("minorPageFaults", VPackValue(info._minorPageFaults));
  builder.add("majorPageFaults", VPackValue(info._majorPageFaults));
  builder.add("userTime", VPackValue(static_cast<float>(info._userTime) / static_cast<float>(info._scClkTck)));
  builder.add("systemTime", VPackValue(static_cast<float>(info._systemTime) / static_cast<float>(info._scClkTck)));
  builder.add("numberOfThreads", VPackValue(info._numberThreads));
  builder.add("residentSize", VPackValue(rss));
  builder.add("residentSizePercent", VPackValue(rssp));
  builder.add("virtualSize", VPackValue(info._virtualSize));
  builder.close();

  // _clientStatistics()
  builder.add("client", VPackValue(VPackValueType::Object));
  builder.add("httpConnections", VPackValue(httpConnections._count));

  VPackBuilder tmp = _fillDistribution(connectionTime);
  builder.add("connectionTime", tmp.slice());

  tmp = _fillDistribution(totalTime);
  builder.add("totalTime", tmp.slice());

  tmp = _fillDistribution(requestTime);
  builder.add("requestTime", tmp.slice());

  tmp = _fillDistribution(queueTime);
  builder.add("queueTime", tmp.slice());

  tmp = _fillDistribution(ioTime);
  builder.add("ioTime", tmp.slice());

  tmp = _fillDistribution(bytesSent);
  builder.add("bytesSent", tmp.slice());

  tmp = _fillDistribution(bytesReceived);
  builder.add("bytesReceived", tmp.slice());
  builder.close();

  // _httpStatistics()
  builder.add("http", VPackValue(VPackValueType::Object));
  builder.add("requestsTotal", VPackValue(totalRequests._count));
  builder.add("requestsAsync", VPackValue(asyncRequests._count));
  builder.add("requestsGet", VPackValue(methodRequests[(int)rest::RequestType::GET]._count));
  builder.add("requestsHead", VPackValue(methodRequests[(int)rest::RequestType::HEAD]._count));
  builder.add("requestsPost", VPackValue(methodRequests[(int)rest::RequestType::POST]._count));
  builder.add("requestsPut", VPackValue(methodRequests[(int)rest::RequestType::PUT]._count));
  builder.add("requestsPatch", VPackValue(methodRequests[(int)rest::RequestType::PATCH]._count));
  builder.add("requestsDelete", VPackValue(methodRequests[(int)rest::RequestType::DELETE_REQ]._count));
  builder.add("requestsOptions", VPackValue(methodRequests[(int)rest::RequestType::OPTIONS]._count));
  builder.add("requestsOther", VPackValue(methodRequests[(int)rest::RequestType::ILLEGAL]._count));
  builder.close();

  // _serverStatistics()
  builder.add("server", VPackValue(VPackValueType::Object));
  builder.add("uptime", VPackValue(serverInfo._uptime));
  builder.add("physicalMemory", VPackValue(TRI_PhysicalMemory));

  builder.add("v8Context", VPackValue(VPackValueType::Object));
  builder.add("available", VPackValue(v8Counters.available));
  builder.add("busy", VPackValue(v8Counters.busy));
  builder.add("dirty", VPackValue(v8Counters.dirty));
  builder.add("free", VPackValue(v8Counters.free));
  builder.add("max", VPackValue(v8Counters.max));
  builder.close();

  builder.add("threads", VPackValue(VPackValueType::Object));
  builder.add("running", VPackValue(rest::Scheduler::numRunning(threadCounters)));
  builder.add("working", VPackValue(rest::Scheduler::numWorking(threadCounters)));
  builder.add("blocked", VPackValue(rest::Scheduler::numBlocked(threadCounters)));
  builder.add("queued",  VPackValue(SchedulerFeature::SCHEDULER->numQueued()));
  builder.close();

  builder.close();

  builder.close();

  return builder;
}

VPackBuilder StatisticsWorker::_fillDistribution(StatisticsDistribution const& dist) const {
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

void StatisticsWorker::_saveSlice(VPackSlice const& slice,
                                  std::string const& collection) const {
  if (isStopping()) {
    return;
  }

  TRI_vocbase_t* vocbase = DatabaseFeature::DATABASE->systemDatabase();
  arangodb::OperationOptions opOptions;
  opOptions.isRestore = false, opOptions.waitForSync = false;
  opOptions.returnNew = false;
  opOptions.silent = false;

  // find and load collection given by name or identifier
  auto transactionContext(transaction::StandaloneContext::Create(vocbase));
  SingleCollectionTransaction trx(transactionContext, collection,
                                  AccessMode::Type::WRITE);
  trx.addHint(transaction::Hints::Hint::SINGLE_OPERATION);

  Result res = trx.begin();

  if (!res.ok()) {
    return;
  }

  arangodb::OperationResult result =
      trx.insert(collection, slice, opOptions);

  // Will commit if no error occured.
  // or abort if an error occured.
  // result stays valid!
  res = trx.finish(result.code);

  /* useless until we do something
  if (result.failed()) {
    return;
  }

  if (!res.ok()) {
    return;
  }*/
}

void StatisticsWorker::createCollections() const {
  if (!ServerState::instance()->isRunningInCluster() ||
       ServerState::instance()->isCoordinator()) {
         _createCollection("_statisticsRaw");
         _createCollection("_statistics");
         _createCollection("_statistics15");
  } else {
    // wait for collection creation cluster db server
    while (true) {
      usleep(500*1000);

      TRI_vocbase_t* vocbase = DatabaseFeature::DATABASE->systemDatabase();

      LogicalCollection* coll = vocbase->lookupCollection("_statisticsRaw");
      if (coll == nullptr) continue;

      coll = vocbase->lookupCollection("_statistics");
      if (coll == nullptr) continue;

      coll = vocbase->lookupCollection("_statistics15");
      if (coll == nullptr) continue;

      break;
    }
  }
}

void StatisticsWorker::_createCollection(std::string const& collection) const {
  VPackBuilder s;
  s.openObject();
  s.add("name", VPackValue(collection));
  s.add("isSystem", VPackValue(true));
  s.add("waitForSync", VPackValue(false));

  if (ServerState::instance()->isRunningInCluster() &&
      ServerState::instance()->isCoordinator()) {
    auto clusterFeature = application_features::ApplicationServer::getFeature<ClusterFeature>("Cluster");
    s.add("replicationFactor", VPackValue(clusterFeature->systemReplicationFactor()));
    s.add("distributeShardsLike", VPackValue("_graphs"));
  }

  s.add("journalSize", VPackValue(8 * 1024 * 1024));
  s.close();

  TRI_vocbase_t* vocbase = DatabaseFeature::DATABASE->systemDatabase();

  int res = TRI_ERROR_NO_ERROR;
  try {
    vocbase->createCollection(s.slice());
  } catch (basics::Exception const& ex) {
    res = ex.code();
  } catch (...) {
    res = TRI_ERROR_INTERNAL;
  }

  if (res != TRI_ERROR_ARANGO_DUPLICATE_NAME && res != TRI_ERROR_NO_ERROR) {
    LOG_TOPIC(ERR, Logger::STATISTICS) << "Can't create statistics collection " << collection;
  }

  std::shared_ptr<LogicalCollection> col = nullptr;
  LogicalCollection* coll = nullptr;

  if (ServerState::instance()->isCoordinator()) {
    try {
      col = ClusterInfo::instance()->getCollection(vocbase->name(), collection);
      if (col) {
        coll = col.get();
      }
    } catch (...) {
    }
  } else {
    coll = vocbase->lookupCollection(collection);
  }

  VPackBuilder t;
  t.openObject();
  t.add("collection", VPackValue(collection));
  t.add("type", VPackValue("skiplist"));
  t.add("unique", VPackValue(false));
  t.add("sparse", VPackValue(false));

  t.add("fields", VPackValue(VPackValueType::Array));
  t.add(VPackValue("time"));
  t.close();

  t.close();

  VPackBuilder output;
  Result idxRes = methods::Indexes::ensureIndex(coll, t.slice(), true, output);
  if (!idxRes.ok()) {
    LOG_TOPIC(WARN, Logger::STATISTICS) << "Can't create the skiplist index for collection "
    << collection << " please create it manually; Error code: " << idxRes.errorNumber();
  }
}



void StatisticsWorker::run() {
  uint64_t seconds = 0;

  createCollections();

  //

  while (!isStopping() && StatisticsFeature::enabled()) {
    historian();

    if (seconds % 8 == 0) {
      collectGarbage();
    }

    // process every 15 seconds
    if (seconds % HISTORY_INTERVAL == 0) {
      historianAverage();
    }

    seconds++;
    usleep(1000 * 1000);
  }
}
