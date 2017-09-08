
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
