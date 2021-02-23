////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2014-2021 ArangoDB GmbH, Cologne, Germany
/// Copyright 2004-2014 triAGENS GmbH, Cologne, Germany
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

#include "ApplicationFeatures/ApplicationServer.h"
#include "ApplicationFeatures/GreetingsFeaturePhase.h"
#include "Basics/application-exit.h"
#include "Logger/LogMacros.h"
#include "Logger/Logger.h"
#include "Logger/LoggerStream.h"
#include "ProgramOptions/ProgramOptions.h"
#include "ProgramOptions/Section.h"
#include "RestServer/Metrics.h"
#include "RocksDBEngine/RocksDBEngine.h"
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
// --SECTION--                                                 MetricsFeature
// -----------------------------------------------------------------------------


MetricsFeature::MetricsFeature(application_features::ApplicationServer& server)
  : ApplicationFeature(server, "Metrics"), 
    _export(true) , 
    _exportReadWriteMetrics(false) {
  setOptional(false);
  startsAfter<LoggerFeature>();
  startsBefore<GreetingsFeaturePhase>();
}

void MetricsFeature::collectOptions(std::shared_ptr<ProgramOptions> options) {
  _serverStatistics = std::make_unique<ServerStatistics>(
      *this, StatisticsFeature::time());

  options->addOption("--server.export-metrics-api",
                     "turn metrics API on or off",
                     new BooleanParameter(&_export),
                     arangodb::options::makeDefaultFlags(arangodb::options::Flags::Hidden))
                     .setIntroducedIn(30600);

  options->addOption("--server.export-read-write-metrics",
                     "turn metrics for document read/write metrics on or off",
                     new BooleanParameter(&_exportReadWriteMetrics),
                     arangodb::options::makeDefaultFlags(arangodb::options::Flags::Hidden))
                     .setIntroducedIn(30707);
}

bool MetricsFeature::exportAPI() const {
  return _export;
}

bool MetricsFeature::exportReadWriteMetrics() const {
  return _exportReadWriteMetrics;
}

void MetricsFeature::validateOptions(std::shared_ptr<ProgramOptions>) {
  if (_exportReadWriteMetrics) {
    serverStatistics().setupDocumentMetrics();
  }
}

void MetricsFeature::toPrometheus(std::string& result) const {

  // minimize reallocs
  result.reserve(32768);

  {
    std::lock_guard<std::recursive_mutex> guard(_lock);
    for (auto const& i : _registry) {
      i.second->toPrometheus(result);
    }
  }

  // StatisticsFeature
  auto& sf = server().getFeature<StatisticsFeature>();
  sf.toPrometheus(result, std::chrono::duration<double, std::milli>(
                    std::chrono::system_clock::now().time_since_epoch()).count());

  // RocksDBEngine
  auto& es = server().getFeature<EngineSelectorFeature>().engine();
  std::string const& engineName = es.typeName();
  if (engineName == RocksDBEngine::EngineName) {
    es.getStatistics(result);
  }
}

ServerStatistics& MetricsFeature::serverStatistics() {
  return *_serverStatistics;
}

metrics_key::metrics_key(std::string const& name, std::initializer_list<std::string> const& il) : name(name) {
  TRI_ASSERT(il.size() < 2);
  if (il.size() == 1) {
    labels = *(il.begin());
  }
  _hash = std::hash<std::string>{}(name + labels);
}

metrics_key::metrics_key(std::initializer_list<std::string> const& il) {
  TRI_ASSERT(il.size() > 0);
  TRI_ASSERT(il.size() < 3);
  name = *(il.begin());
  if (il.size() == 2) {
    labels = *(il.begin()+1);
  }
  _hash = std::hash<std::string>{}(name + labels);
}

metrics_key::metrics_key(std::string const& name) : name(name) {
  // the metric name should not include any spaces
  TRI_ASSERT(name.find(' ') == std::string::npos);
  _hash = std::hash<std::string>{}(name);
}

metrics_key::metrics_key(std::string const& name, std::string const& labels) :
  name(name), labels(labels) {
  // the metric name should not include any spaces
  TRI_ASSERT(name.find(' ') == std::string::npos);
  _hash = std::hash<std::string>{}(name + labels);
}

std::size_t metrics_key::hash() const noexcept {
  return _hash;
}

bool metrics_key::operator== (metrics_key const& other) const {
  return name == other.name && labels == other.labels;
}

namespace std {
std::size_t hash<arangodb::metrics_key>::operator()(arangodb::metrics_key const& m) const noexcept {
  return m.hash();
}
}
