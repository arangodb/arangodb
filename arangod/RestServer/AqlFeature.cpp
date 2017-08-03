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
/// @author Max Neunhoeffer
////////////////////////////////////////////////////////////////////////////////

#include "RestServer/AqlFeature.h"

#include "Aql/QueryList.h"
#include "Aql/QueryRegistry.h"
#include "Basics/MutexLocker.h"
#include "Cluster/TraverserEngineRegistry.h"
#include "Logger/Logger.h"
#include "RestServer/QueryRegistryFeature.h"
#include "RestServer/TraverserEngineRegistryFeature.h"

using namespace arangodb;
using namespace arangodb::application_features;

AqlFeature* AqlFeature::_AQL = nullptr;
Mutex AqlFeature::_aqlFeatureMutex;

AqlFeature::AqlFeature(
    application_features::ApplicationServer* server)
    : ApplicationFeature(server, "Aql"), _numberLeases(0), _isStopped(false) {
  setOptional(false);
  requiresElevatedPrivileges(false);
  startsAfter("CacheManager");
  startsAfter("Cluster");
  startsAfter("Database");
  startsAfter("QueryRegistry");
  startsAfter("Scheduler");
  startsAfter("V8Platform");
  startsAfter("WorkMonitor");
}

AqlFeature* AqlFeature::lease() {
  MUTEX_LOCKER(locker, AqlFeature::_aqlFeatureMutex);
  AqlFeature* aql = AqlFeature::_AQL;
  if (aql == nullptr) {
    return nullptr;
  }
  if (aql->_isStopped) {
    return nullptr;
  }
  ++aql->_numberLeases;
  return aql;
}

void AqlFeature::unlease() {
  MUTEX_LOCKER(locker, AqlFeature::_aqlFeatureMutex);
  AqlFeature* aql = AqlFeature::_AQL;
  TRI_ASSERT(aql != nullptr);
  --aql->_numberLeases;
}

void AqlFeature::start() {
  MUTEX_LOCKER(locker, AqlFeature::_aqlFeatureMutex);
  TRI_ASSERT(_AQL == nullptr);
  _AQL = this;
  LOG_TOPIC(DEBUG, Logger::QUERIES) << "AQL feature started";
}

void AqlFeature::stop() {
  {
    MUTEX_LOCKER(locker, AqlFeature::_aqlFeatureMutex);
    _isStopped = true;  // prevent new AQL queries from being launched
  }
  LOG_TOPIC(DEBUG, Logger::QUERIES) << "AQL feature stopped";

  // Wait until all AQL queries are done
  while (true) {
    try {
      QueryRegistryFeature::QUERY_REGISTRY->destroyAll();
      TraverserEngineRegistryFeature::TRAVERSER_ENGINE_REGISTRY->destroyAll();
    } catch (...) {
      // ignore errors here. if it fails, we'll try again in next round
    }

    size_t m, n, o;
    {
      MUTEX_LOCKER(locker, AqlFeature::_aqlFeatureMutex);
      m = _numberLeases;
      n = QueryRegistryFeature::QUERY_REGISTRY->numberRegisteredQueries();
      o = TraverserEngineRegistryFeature::TRAVERSER_ENGINE_REGISTRY
          ->numberRegisteredEngines();
    }
    if (n == 0 && m == 0 && o == 0) {
      break;
    }
    LOG_TOPIC(DEBUG, Logger::QUERIES) << "AQLFeature shutdown, waiting for "
      << o << " registered traverser engines to terminate and for "
      << n << " registered queries to terminate and for "
      << m << " feature leases to be released";
    usleep(500000);
  }
  MUTEX_LOCKER(locker, AqlFeature::_aqlFeatureMutex);
  AqlFeature::_AQL = nullptr;
}

