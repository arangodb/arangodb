////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2016 ArangoDB GmbH, Cologne, Germany
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
/// @author Dr. Frank Celler
////////////////////////////////////////////////////////////////////////////////

#include "QueryRegistryFeature.h"

#include "Aql/QueryCache.h"
#include "Aql/QueryRegistry.h"
#include "ProgramOptions/ProgramOptions.h"
#include "ProgramOptions/Section.h"

using namespace arangodb;
using namespace arangodb::application_features;
using namespace arangodb::basics;
using namespace arangodb::options;

aql::QueryRegistry* QueryRegistryFeature::QUERY_REGISTRY = nullptr;

QueryRegistryFeature::QueryRegistryFeature(ApplicationServer* server)
    : ApplicationFeature(server, "QueryRegistry"),
      _queryTracking(true),
      _slowThreshold(10.0),
      _queryCacheMode("off"),
      _queryCacheEntries(128) {
  setOptional(false);
  requiresElevatedPrivileges(false);
  startsAfter("DatabasePath");
  startsAfter("Database");
  startsAfter("LogfileManager");
}

void QueryRegistryFeature::collectOptions(
    std::shared_ptr<ProgramOptions> options) {
  options->addSection("query", "Configure queries");
  
  options->addOldOption("database.query-cache-mode", "query.cache-mode");
  options->addOldOption("database.query-cache-max-results", "query.cache-entries");
  options->addOldOption("database.disable-query-tracking", "query.tracking");

  options->addOption("--query.tracking", "whether to track slow AQL queries",
                     new BooleanParameter(&_queryTracking));
  
  options->addOption("--query.slow-threshold", "threshold for slow AQL queries (in seconds)",
                     new DoubleParameter(&_slowThreshold));

  options->addOption("--query.cache-mode",
                     "mode for the AQL query result cache (on, off, demand)",
                     new StringParameter(&_queryCacheMode));

  options->addOption("--query.cache-entries",
                     "maximum number of results in query result cache per database",
                     new UInt64Parameter(&_queryCacheEntries));
}

void QueryRegistryFeature::validateOptions(
    std::shared_ptr<ProgramOptions> options) {
}

void QueryRegistryFeature::prepare() {
  // set global query tracking flag
  arangodb::aql::Query::DisableQueryTracking(!_queryTracking);
  
  // set global threshold value for slow queries  
  arangodb::aql::Query::SlowQueryThreshold(_slowThreshold);

  // configure the query cache
  std::pair<std::string, size_t> cacheProperties{_queryCacheMode,
                                                 _queryCacheEntries};
  arangodb::aql::QueryCache::instance()->setProperties(cacheProperties);
  
  // create the query registery
  _queryRegistry.reset(new aql::QueryRegistry());
  QUERY_REGISTRY = _queryRegistry.get();
}

void QueryRegistryFeature::start() {
}

void QueryRegistryFeature::unprepare() {
  // clear the query registery
  QUERY_REGISTRY = nullptr;
}
