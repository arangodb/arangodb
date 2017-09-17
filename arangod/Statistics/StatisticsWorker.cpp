
#include "StatisticsFeature.h"
#include "StatisticsWorker.h"

#include "Aql/Query.h"
#include "Aql/QueryString.h"
#include "Basics/VelocyPackHelper.h"
#include "Cluster/ServerState.h"
#include "RestServer/DatabaseFeature.h"
#include "RestServer/QueryRegistryFeature.h"
#include "Transaction/StandaloneContext.h"
#include "Utils/OperationOptions.h"
#include "Utils/SingleCollectionTransaction.h"

#include <velocypack/Exception.h>
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

  std::string clusterId = "";

  if (ServerState::instance()->isRunningInCluster()) {
    clusterId = ServerState::instance()->getId();
  }

  std::cout << "clusterId is " << clusterId << std::endl;

  uint64_t x = 0;

  try {
    uint64_t now = static_cast<uint64_t>(TRI_microtime());
    uint64_t start;

    // check if need to create a new 15 min interval

    std::cout << x++ << std::endl;
    std::shared_ptr<arangodb::velocypack::Builder> prev15Builder = _lastEntry("_statistics15", now - 2 * HISTORY_INTERVAL, 0);
    VPackSlice prev15 = prev15Builder->slice();

    std::cout << "lastEntry hexType " << prev15.hexType() << std::endl;
    std::cout << "lastEntry isArray " << prev15.isArray() << std::endl;
    std::cout << "lastEntry length "  << prev15.length() << std::endl;

    std::cout << x++ << std::endl;

    if (prev15.isArray() && prev15.length()) {
      start = prev15.getNthValue(0).get("time").getUInt();
    } else {
      start = now - HISTORY_INTERVAL;
    }

    std::cout << x++ << std::endl;

    VPackBuilder builder = _compute15Minute(start, clusterId);
    VPackSlice stat15 = builder.slice();

    std::cout << "hextype " << stat15.hexType() << std::endl;

    if (stat15.length() != 0) {

      std::cout << "save stat15 document\n";

      std::cout << "hextype " << stat15.hexType() << std::endl;


      std::cout << x++ << std::endl;

      TRI_vocbase_t* vocbase = DatabaseFeature::DATABASE->systemDatabase();
      std::cout << x++ << std::endl;

      arangodb::OperationOptions opOptions;
      opOptions.isRestore = false,
      opOptions.waitForSync = false;
      opOptions.returnNew = false;
      opOptions.silent = false;

      // find and load collection given by name or identifier
      auto transactionContext(transaction::StandaloneContext::Create(vocbase));
      std::cout << x++ << std::endl;
      SingleCollectionTransaction trx(transactionContext, "_statistics15",
                                      AccessMode::Type::WRITE);
      trx.addHint(transaction::Hints::Hint::SINGLE_OPERATION);
      std::cout << x++ << std::endl;
      Result res = trx.begin();
      std::cout << x++ << std::endl;
      if (!res.ok()) {
        // ?
        return;
      }
      std::cout << x++ << std::endl;
      arangodb::OperationResult result = trx.insert("_statistics15", stat15, opOptions);
      std::cout << x++ << std::endl;
      // Will commit if no error occured.
      // or abort if an error occured.
      // result stays valid!
      res = trx.finish(result.code);
      std::cout << x++ << std::endl;
      if (result.failed()) {
        // ?
        return;
      }
      std::cout << x++ << std::endl;

      if (!res.ok()) {
        // ?
      }
      std::cout << x++ << std::endl;
    } // if !null
  } catch(arangodb::velocypack::Exception const& ex) {
    std::cout << ex.what() << " " << ex.errorCode() << std::endl;
    std::cout << ex << std::endl;

  } catch(arangodb::basics::Exception const& ex) {
    std::cout << ex.what() << std::endl;
    std::cout << ex.message() << std::endl;
    std::cout << ex.code() << std::endl;

  } catch (...) {
    std::cout << "catch :S\n";
    // we don't want this error to appear every x seconds
    // require("console").warn("catch error in historianAverage: %s", err)
  }
}

std::shared_ptr<arangodb::velocypack::Builder> StatisticsWorker::_lastEntry(std::string const& collectionName, uint64_t start, std::string const& clusterId) {
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

VPackBuilder StatisticsWorker::_compute15Minute(uint64_t start, std::string const& clusterId) {
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

  std::cout << "_compute15Minute count is " << count << std::endl;

  if (count == 0) {
    VPackBuilder builder;
    builder.openObject();
    builder.close();
    return builder; //  arangodb::basics::VelocyPackHelper::NullValue();
  }

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
  clientAvgIoTime = 0,

  fTime;

  for (auto const& vs : VPackArrayIterator(result)) {
    VPackSlice const& values = vs.resolveExternals();

    std::cout << "value type is " << values.hexType() << std::endl;

    fTime = values.get("time").getNumber<float>();

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

  if (count != 0) {
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
  }

  VPackBuilder builder;
  builder.openObject();

  builder.add("time", VPackValue(fTime));

  if (clusterId.length()) {
    builder.add("clusterId", VPackValue(clusterId));
  }

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







void StatisticsWorker::run() {
  uint64_t seconds = 0;

  // create collections

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
