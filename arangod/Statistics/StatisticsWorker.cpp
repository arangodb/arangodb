
#include "StatisticsFeature.h"
#include "StatisticsWorker.h"

#include "Aql/Query.h"
#include "Aql/QueryString.h"
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

void StatisticsWorker::run() {
  uint64_t seconds = 0;

  std::cout << "stat worker start run\n";

  while (!isStopping() && StatisticsFeature::enabled()) {
    // process every second
    
    std::cout << "1 done\n";

    // process every 15 seconds

    if (seconds % 8 == 0) {
      collectGarbage();
    }

    if (seconds % 15 == 0) {
      std::cout << "15 done\n";
    }

    seconds++;
    usleep(1000 * 1000);
  }
}
