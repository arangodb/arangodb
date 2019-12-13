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
/// @author Kaveh Vahedipour
////////////////////////////////////////////////////////////////////////////////

#include "MetricsFeature.h"
#include "ApplicationFeatures/GreetingsFeaturePhase.h"
#include "Basics/application-exit.h"
#include "Logger/LogMacros.h"
#include "Logger/Logger.h"
#include "Logger/LoggerStream.h"
#include "ProgramOptions/ProgramOptions.h"
#include "ProgramOptions/Section.h"
#include "RestServer/Metrics.h"
#include "RocksDBEngine/RocksDBEngine.h"
#include "MMFiles/MMFilesEngine.h"
#include "Statistics/StatisticsFeature.h"
#include "StorageEngine/EngineSelectorFeature.h"
#include "StorageEngine/StorageEngine.h"
#include "StorageEngine/StorageEngineFeature.h"

#include <chrono>
#include <thread>

using namespace arangodb;
using namespace arangodb::application_features;
using namespace arangodb::basics;
using namespace arangodb::options;

// -----------------------------------------------------------------------------
// --SECTION--                                                  global variables
// -----------------------------------------------------------------------------


// -----------------------------------------------------------------------------
// --SECTION--                                                 MetricsFeature
// -----------------------------------------------------------------------------

MetricsFeature::MetricsFeature(application_features::ApplicationServer& server)
  : ApplicationFeature(server, "Metrics"), _export(true) {
  setOptional(false);
  startsAfter<LoggerFeature>();
  startsBefore<GreetingsFeaturePhase>();
}

void MetricsFeature::collectOptions(std::shared_ptr<ProgramOptions> options) {
  _serverStatistics = std::make_unique<ServerStatistics>(
    std::chrono::duration<double>(
      std::chrono::system_clock::now().time_since_epoch()).count());
  options->addOption("--server.export-metrics-api",
                     "turn metrics API on or off",
                     new BooleanParameter(&_export),
                     arangodb::options::makeFlags(arangodb::options::Flags::Hidden));
}

bool MetricsFeature::exportAPI() const {
  return _export;
}

void MetricsFeature::validateOptions(std::shared_ptr<ProgramOptions>) {}

void MetricsFeature::toPrometheus(std::string& result) const {
  {
    std::lock_guard<std::mutex> guard(_lock);
    for (auto const& i : _registry) {
      i.second->toPrometheus(result);
    }
  }

  // StatisticsFeature
  auto& sf = server().getFeature<StatisticsFeature>();
  if (sf.enabled()) {
    sf.toPrometheus(result, std::chrono::duration<double,std::milli>(
                      std::chrono::system_clock::now().time_since_epoch()).count());
  }

  // RocksDBEngine
  auto es = EngineSelectorFeature::ENGINE;
  if (es != nullptr) {
    std::string const& engineName = es->typeName();
    if (engineName == RocksDBEngine::EngineName) {
      es->getStatistics(result);
    }
  }
}

Counter& MetricsFeature::counter (
  std::string const& name, uint64_t const& val, std::string const& help) {

  auto metric = std::make_shared<Counter>(val, name, help);
  bool success = false;
  {
    std::lock_guard<std::mutex> guard(_lock);
    success = _registry.emplace(name, std::dynamic_pointer_cast<Metric>(metric)).second;
  }
  if (!success) {
    THROW_ARANGO_EXCEPTION_MESSAGE(
      TRI_ERROR_INTERNAL, std::string("counter ") + name + " alredy exists");
  }
  return *metric;
}

ServerStatistics& MetricsFeature::serverStatistics() {
  _serverStatistics->_uptime = StatisticsFeature::time() - _serverStatistics->_startTime;
  return *_serverStatistics;
}
