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
/// @author Jan Steemann
////////////////////////////////////////////////////////////////////////////////

#include "ApplicationServerHelper.h"
#include "Cluster/ServerState.h"
#include "IResearch/IResearchAnalyzerCollectionFeature.h"
#include "IResearch/IResearchCommon.h"
#include "Logger/Logger.h"
#include "RestServer/DatabaseFeature.h"
#include "VocBase/Methods/Collections.h"

using namespace arangodb;

IResearchAnalyzerCollectionFeature::IResearchAnalyzerCollectionFeature(application_features::ApplicationServer& server)
    : ApplicationFeature(server, "ArangoSearchAnalyzerCollection") {
  setOptional(true);
  startsAfter("DatabasePhase");
  // should be relatively late in startup sequence
  startsAfter("ClusterPhase");
  startsAfter("ServerPhase");
  startsAfter("Bootstrap");
}

void IResearchAnalyzerCollectionFeature::start() {
  if (ServerState::instance()->isDBServer()) {
    // no need to execute this in DB server
    return;
  }

  DatabaseFeature* databaseFeature = DatabaseFeature::DATABASE;
  TRI_ASSERT(databaseFeature != nullptr);

  databaseFeature->enumerateDatabases([](TRI_vocbase_t& vocbase) {
    Result res = methods::Collections::lookup(vocbase, StaticStrings::AnalyzersCollection, [](std::shared_ptr<LogicalCollection> const&) {
    });

    if (res.is(TRI_ERROR_ARANGO_DATA_SOURCE_NOT_FOUND)) {
      // collection does not yet exist, so let's create it now
    auto res =  methods::Collections::createSystem(vocbase, StaticStrings::AnalyzersCollection, false);
      if (res.first.ok()) {
	        LOG_TOPIC("c2e33", DEBUG, arangodb::iresearch::TOPIC) << "successfully created '" << StaticStrings::AnalyzersCollection << "' collection in database '" << vocbase.name() << "'";
      } else if (res.first.fail() && !res.first.is(TRI_ERROR_ARANGO_CONFLICT)) {
      LOG_TOPIC("ecc23", WARN, arangodb::iresearch::TOPIC) << "unable to create '" << StaticStrings::AnalyzersCollection << "' collection: " << res.first.errorMessage();
        // don't abort startup here. the next startup may fix this
      }
    }
  });
}
