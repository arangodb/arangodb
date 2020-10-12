////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2014-2020 ArangoDB GmbH, Cologne, Germany
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
  : ApplicationFeature(server, "Metrics"), _export(true) {
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
}

bool MetricsFeature::exportAPI() const {
  return _export;
}

void MetricsFeature::validateOptions(std::shared_ptr<ProgramOptions>) {}

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
  if (sf.enabled()) {
    sf.toPrometheus(result, std::chrono::duration<double,std::milli>(
                      std::chrono::system_clock::now().time_since_epoch()).count());
  }

  // RocksDBEngine
  auto& es = server().getFeature<EngineSelectorFeature>().engine();
  std::string const& engineName = es.typeName();
  if (engineName == RocksDBEngine::EngineName) {
    es.getStatistics(result);
  }
}

Counter& MetricsFeature::counter(
  std::initializer_list<std::string> const& key, uint64_t const& val,
  std::string const& help) {
  return counter(metrics_key(key), val, help);
}

Counter& MetricsFeature::counter(
  metrics_key const& mk, uint64_t const& val,
  std::string const& help) {

  std::string labels = mk.labels;
  if (ServerState::instance() != nullptr &&
      ServerState::instance()->getRole() != ServerState::ROLE_UNDEFINED) {
    if (!labels.empty()) {
      labels += ",";
    }
    labels += "role=\"" + ServerState::roleToString(ServerState::instance()->getRole()) +
      "\",shortname=\"" + ServerState::instance()->getShortName() + "\"";
  }
  auto metric = std::make_shared<Counter>(val, mk.name, help, labels);
  bool success = false;
  {
    std::lock_guard<std::recursive_mutex> guard(_lock);
    success = _registry.emplace(mk, std::dynamic_pointer_cast<Metric>(metric)).second;
  }
  if (!success) {
    THROW_ARANGO_EXCEPTION_MESSAGE(
      TRI_ERROR_INTERNAL, std::string("counter ") + mk.name + " already exists");
  }
  return *metric;
}

Counter& MetricsFeature::counter(
  std::string const& name, uint64_t const& val, std::string const& help) {
  return counter(metrics_key(name), val, help);
}

ServerStatistics& MetricsFeature::serverStatistics() {
  return *_serverStatistics;
}

Counter& MetricsFeature::counter(std::initializer_list<std::string> const& key) {
  metrics_key mk(key);
  std::shared_ptr<Counter> metric = nullptr;
  std::string error;
  {
    std::lock_guard<std::recursive_mutex> guard(_lock);
    registry_type::const_iterator it = _registry.find(mk);
    if (it == _registry.end()) {
      it = _registry.find(mk.name);
      if (it == _registry.end()) {
        THROW_ARANGO_EXCEPTION_MESSAGE(
          TRI_ERROR_INTERNAL, std::string("No counter booked as ") + mk.name);
      } else {
        auto tmp = std::dynamic_pointer_cast<Counter>(it->second);
        return counter(mk, 0, tmp->help());
      }
    }
    try {
      metric = std::dynamic_pointer_cast<Counter>(it->second);
      if (metric == nullptr) {
        error = std::string("Failed to retrieve counter ") + mk.name;
      }
    } catch (std::exception const& e) {
      error = std::string("Failed to retrieve counter ") + mk.name +  ": " + e.what();
    }
  }
  if (!error.empty()) {
    THROW_ARANGO_EXCEPTION_MESSAGE(TRI_ERROR_INTERNAL, error);
  }
  return *metric;
}

Counter& MetricsFeature::counter(std::string const& name) {
  return counter({name});
}

metrics_key::metrics_key(std::initializer_list<std::string> const& il) {
  TRI_ASSERT(il.size() > 0);
  TRI_ASSERT(il.size() < 3);
  name = *il.begin();
  if (il.size() == 2) {
    labels = *(il.begin()+1);
  }
  _hash = std::hash<std::string>{}(name + labels);
}

metrics_key::metrics_key(std::string const& name) : name(name) {
  _hash = std::hash<std::string>{}(name);
}

metrics_key::metrics_key(std::string const& name, std::string const& labels) :
  name(name), labels(labels) {
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
