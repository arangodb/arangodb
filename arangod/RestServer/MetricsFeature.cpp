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

template<typename T> struct printer;
template<> struct printer<double> {
  static std::string const print(
    std::string const name, Metrics::hist_type const& hist,
    std::pair<std::any,std::any> const& lims) {
    double low = std::any_cast<const double>(std::get<0>(lims));
    double high = std::any_cast<const double>(std::get<1>(lims));
    size_t buckets = hist.size();
    uint64_t sum = 0;
    double div = (high - low) / (double)hist.size();
    std::string ret;
    double le = low;
    for (size_t i = 0; i < buckets; ++i) {
      uint64_t n = hist.load(i);
      sum += n;
      ret += name + "_bucket{le=\"" + std::to_string(le) + "\"} " + std::to_string(hist.load(i)) + "\n";
      le += div;
    }
    ret += name + "_count " + std::to_string(sum) + "\n";
    return ret;
  }
};

char const* _HELP ="#HELP ";
char const* _TYPE ="#TYPE ";

void MetricsFeature::toPrometheus(std::string& result) const {
  std::lock_guard<std::mutex> guard(_lock);
  for (auto const& i : _help) {
    if (!i.second.empty()) {
      result += _HELP + i.first + " " + i.second + "\n";
    }
    auto const it = _params.find(i.first);
    if (it != _params.end()) {
      result += _TYPE + i.first + " histogram\n";
      std::string const tname = std::get<0>(it->second).type().name();
      if (tname == typeid(double(0.)).name()) {
        result += printer<double>::print(
          i.first, _metrics.histogram(i.first), it->second);
      }
    } else {
      result += _TYPE + i.first + " counter\n";
      result +=
        i.first + " " + std::to_string(_metrics.counter(i.first).load()) + "\n";
    }
    result += "\n";
  }



}
  

void MetricsFeature::toBuilder(VPackBuilder& builder) const {
  std::lock_guard<std::mutex> guard(_lock);
  for (auto const& i : _help) {
    builder.add(VPackValue(i.first));
    VPackObjectBuilder o(&builder);
    builder.add("help", VPackValue(i.second));
    auto const it = _params.find(i.first);
    if (it != _params.end()) {
      builder.add(
        "type", VPackValue(std::get<0>(it->second).type().name()));
      builder.add(VPackValue("value"));
      VPackArrayBuilder a(&builder);
      auto const& hist = _metrics.histogram(i.first);
      for (size_t i = 0; i < hist.size(); ++i) {
        builder.add(VPackValue(hist.load(i)));
      }
    } else {
      builder.add("value", VPackValue(_metrics.counter(i.first).load()));
    }
  }
}

Counter MetricsFeature::counter (std::string const& name, std::string const& help) {
  std::lock_guard<std::mutex> guard(_lock);
  auto const it = _help.find(name);
  if (it == _help.end()) {
    _help[name] = help;
  }
  return _metrics.registerCounter(name);
};


ServerStatistics& MetricsFeature::serverStatistics() {
  return *_serverStatistics;
}
