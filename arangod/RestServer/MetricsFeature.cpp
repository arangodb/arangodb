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


MetricsFeature::MetricsFeature(application_features::ApplicationServer& server)
    : ApplicationFeature(server, "Metrics"),
      _enabled(true) {
  setOptional(false);
  startsAfter<LoggerFeature>();
}

void MetricsFeature::collectOptions(std::shared_ptr<ProgramOptions> options) {
}

void MetricsFeature::validateOptions(std::shared_ptr<ProgramOptions>) {
}

void MetricsFeature::prepare() {
  METRICS = this;
}

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

void MetricsFeature::toBuilder(VPackBuilder& builder) const {
  for (auto const& i : _help) {
    builder.add(i.first, VPackValue(_metrics.counter(i.first).load()));
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
