
#include "StatisticsFeature.h"
#include "StatisticsWorker.h"

#include "Aql/Query.h"
#include "Aql/QueryString.h"
#include "Basics/VelocyPackHelper.h"
#include "RestServer/DatabaseFeature.h"
#include "RestServer/QueryRegistryFeature.h"
#include "Transaction/StandaloneContext.h"
#include "Utils/OperationOptions.h"
#include "Utils/SingleCollectionTransaction.h"

#include <velocypack/Iterator.h>
#include <velocypack/velocypack-aliases.h>

#include <iostream>


using namespace arangodb;

void StatisticsWorker::collectGarbage() {

  uint64_t time = static_cast<uint64_t>(TRI_microtime());

  _collectGarbage("_statistics", time - 3600); // 1 hour
  _collectGarbage("_statisticsRaw", time - 3600); // 1 hour
  _collectGarbage("_statistics15", time  - 30 * 86400);  // 30 days
}

void StatisticsWorker::_collectGarbage(std::string const& collectionName,
                                       uint64_t start) {

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

    VPackSlice result = queryResult.result->slice();

    std::cout << result.length() << " " <<  result.isArray() <<  std::endl;

    for (auto const& key : VPackArrayIterator(result)) {
      // std::string key = key.copyString();

      OperationOptions opOptions;
      opOptions.returnOld = false;
      opOptions.ignoreRevs = true;
      opOptions.waitForSync = false;
      opOptions.silent = false;

      auto transactionContext(transaction::StandaloneContext::Create(vocbase));

      /*VPackBuilder builder;
      VPackSlice search;

      VPackObjectBuilder guard(&builder);
      builder.add(StaticStrings::KeyString, key); // VPackValue(key));
      search = builder.slice();*/

      SingleCollectionTransaction trx(transactionContext, collectionName,
                                      AccessMode::Type::WRITE);
      trx.addHint(transaction::Hints::Hint::SINGLE_OPERATION);

      Result res = trx.begin();

      if (!res.ok()) {
        std::cout << "cant start trx\n";
        continue;
      }

      OperationResult result = trx.remove(collectionName, key, opOptions);

      res = trx.finish(result.code);

      if (!result.successful()) {
        std::cout << "bad result\n";
      }

      if (!res.ok()) {
        std::cout << "bad res " << collectionName << std::endl;
      }
    }
}






void StatisticsWorker::historianAverage() {
  // var stats15m = db._statistics15;
  // var clusterId = cluster.isCluster() && global.ArangoServerState.id();
  // if (cluster.isCluster()) {
  // }

  uint64_t clusterId = 0;

  try {
    uint64_t now = static_cast<uint64_t>(TRI_microtime());
    uint64_t start;

    // check if need to create a new 15 min interval

    VPackSlice prev15 = _lastEntry("_statistics15", now - 2 * HISTORY_INTERVAL, 0);

    if (prev15.isNull()) {
      start = now - HISTORY_INTERVAL;
    } else {
      start = prev15.get("time").getUInt();
    }

    VPackSlice stat15 = _compute15Minute(start, clusterId);

    if (!stat15.isNull()) {
      if (clusterId) {
        // append clusterId to slice
      }

      // save stat15 to _statistics15 collection
    }
  } catch (...) {
    // we don't want this error to appear every x seconds
    // require("console").warn("catch error in historianAverage: %s", err)
  }
}

VPackSlice StatisticsWorker::_lastEntry(std::string const& collectionName, uint64_t start, uint64_t clusterId) {
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

  if (clusterId) {
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

  VPackSlice result = queryResult.result->slice();

  if (result.isArray() && result.length()) {
    return result.getNthValue(0);
  }

  return arangodb::basics::VelocyPackHelper::NullValue();
}

VPackSlice StatisticsWorker::_compute15Minute(uint64_t start, uint64_t clusterId) {
  std::string filter = "";

/*
  var filter = '';
  var bindVars = { start: start };

  if (clusterId) {
    filter = ' FILTER s.clusterId == @clusterId ';
    bindVars.clusterId = clusterId;
  }

  var values = db._query(
    'FOR s in _statistics '
    + '  FILTER s.time >= @start '
    + filter
    + '  SORT s.time '
    + '  RETURN s', bindVars);
  */

  uint64_t count = 0; // = result.length();

  float systemMinorPageFaultsPerSecond = 0,
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

  /*

  while (values.hasNext()) {
    var raw = values.next();

    result.time = raw.time;

    result.system.minorPageFaultsPerSecond += raw.system.minorPageFaultsPerSecond;
    result.system.majorPageFaultsPerSecond += raw.system.majorPageFaultsPerSecond;
    result.system.userTimePerSecond += raw.system.userTimePerSecond;
    result.system.systemTimePerSecond += raw.system.systemTimePerSecond;
    result.system.residentSize += raw.system.residentSize;
    result.system.virtualSize += raw.system.virtualSize;
    result.system.numberOfThreads += raw.system.numberOfThreads;

    result.http.requestsTotalPerSecond += raw.http.requestsTotalPerSecond;
    result.http.requestsAsyncPerSecond += raw.http.requestsAsyncPerSecond;
    result.http.requestsGetPerSecond += raw.http.requestsGetPerSecond;
    result.http.requestsHeadPerSecond += raw.http.requestsHeadPerSecond;
    result.http.requestsPostPerSecond += raw.http.requestsPostPerSecond;
    result.http.requestsPutPerSecond += raw.http.requestsPutPerSecond;
    result.http.requestsPatchPerSecond += raw.http.requestsPatchPerSecond;
    result.http.requestsDeletePerSecond += raw.http.requestsDeletePerSecond;
    result.http.requestsOptionsPerSecond += raw.http.requestsOptionsPerSecond;
    result.http.requestsOtherPerSecond += raw.http.requestsOtherPerSecond;

    result.client.httpConnections += raw.client.httpConnections;
    result.client.bytesSentPerSecond += raw.client.bytesSentPerSecond;
    result.client.bytesReceivedPerSecond += raw.client.bytesReceivedPerSecond;
    result.client.avgTotalTime += raw.client.avgTotalTime;
    result.client.avgRequestTime += raw.client.avgRequestTime;
    result.client.avgQueueTime += raw.client.avgQueueTime;
    result.client.avgIoTime += raw.client.avgIoTime;

    count++;
  }

  if (count !== 0) {
    result.system.minorPageFaultsPerSecond /= count;
    result.system.majorPageFaultsPerSecond /= count;
    result.system.userTimePerSecond /= count;
    result.system.systemTimePerSecond /= count;
    result.system.residentSize /= count;
    result.system.virtualSize /= count;
    result.system.numberOfThreads /= count;

    result.http.requestsTotalPerSecond /= count;
    result.http.requestsAsyncPerSecond /= count;
    result.http.requestsGetPerSecond /= count;
    result.http.requestsHeadPerSecond /= count;
    result.http.requestsPostPerSecond /= count;
    result.http.requestsPutPerSecond /= count;
    result.http.requestsPatchPerSecond /= count;
    result.http.requestsDeletePerSecond /= count;
    result.http.requestsOptionsPerSecond /= count;
    result.http.requestsOtherPerSecond /= count;

    result.client.httpConnections /= count;
  }

  return result;
*/







  return arangodb::basics::VelocyPackHelper::NullValue();
}







void StatisticsWorker::run() {
  uint64_t seconds = 0;

  while (!isStopping() && StatisticsFeature::enabled()) {
    // process every second

    std::cout << "1 done\n";

    if (seconds % 8 == 0) {
      collectGarbage();
    }


    // process every 15 seconds
    if (seconds % HISTORY_INTERVAL == 0) {
      historianAverage();
      std::cout << "15 done\n";
    }

    seconds++;
    usleep(1000 * 1000);
  }
}
