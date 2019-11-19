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
#include "Statistics/StatisticsFeature.h"

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

MetricsFeature* MetricsFeature::METRICS = nullptr;

#include <iostream>
MetricsFeature::MetricsFeature(application_features::ApplicationServer& server)
    : ApplicationFeature(server, "Metrics"),
      _enabled(true) {
  METRICS = this;
  _serverStatistics = new
    ServerStatistics(std::chrono::duration<double>(
                       std::chrono::system_clock::now().time_since_epoch()).count());
  setOptional(false);
  startsAfter<LoggerFeature>();
  startsBefore<GreetingsFeaturePhase>();
}

void MetricsFeature::collectOptions(std::shared_ptr<ProgramOptions> options) {}

void MetricsFeature::validateOptions(std::shared_ptr<ProgramOptions>) {}

void MetricsFeature::prepare() {}

void MetricsFeature::start() {
  TRI_ASSERT(isEnabled());
}

void MetricsFeature::stop() {
  METRICS = nullptr;
}

double time() {
  return std::chrono::duration<double>( // time since epoch in seconds
    std::chrono::system_clock::now().time_since_epoch())
    .count();
}

void MetricsFeature::toPrometheus(std::string& result) const {
  {
    std::lock_guard<std::mutex> guard(_lock);
    for (auto const& i : _registry) {
      i.second->toPrometheus(result);
    }
  }

  auto& sf = server().getFeature<StatisticsFeature>();
  sf.toPrometheus(result, std::chrono::duration<double,std::milli>(std::chrono::system_clock::now().time_since_epoch()).count());

}

Counter& MetricsFeature::counter (
  std::string const& name, uint64_t const& val, std::string const& help) {
  std::lock_guard<std::mutex> guard(_lock);
  auto const it = _registry.find(name);
  if (it != _registry.end()) {
    LOG_TOPIC("8532d", ERR, Logger::STATISTICS) << "Failed to retrieve histogram " << name;
    TRI_ASSERT(false);
  }
  auto c = std::make_shared<Counter>(val, name, help);
  _registry.emplace(name, std::dynamic_pointer_cast<HistBase>(c));
  return *c;
};

Counter& MetricsFeature::counter (std::string const& name) {
  std::lock_guard<std::mutex> guard(_lock);
  auto it = _registry.find(name);
  if (it == _registry.end()) {
    LOG_TOPIC("32d85", ERR, Logger::STATISTICS)
      << "Failed to retrieve counter " << name;
    TRI_ASSERT(false);
    throw std::exception();
  } 
  std::shared_ptr<Counter> h;
  try {
    h = std::dynamic_pointer_cast<Counter>(it->second);
  } catch (std::exception const& e) {
    LOG_TOPIC("8532d", ERR, Logger::STATISTICS)
      << "Failed to retrieve counter " << name;
    TRI_ASSERT(false);
    throw(e);
  }
  return *h;
};


ServerStatistics& MetricsFeature::serverStatistics() {
  return *_serverStatistics;
}
