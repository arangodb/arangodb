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

#include "StatisticsFeature.h"

#include "ProgramOptions/ProgramOptions.h"
#include "ProgramOptions/Section.h"
#include "Statistics/statistics.h"

using namespace arangodb;
using namespace arangodb::application_features;
using namespace arangodb::options;

StatisticsFeature* StatisticsFeature::STATISTICS = nullptr;

StatisticsFeature::StatisticsFeature(application_features::ApplicationServer* server)
  : ApplicationFeature(server, "Statistics"),
    _statistics(true) {
  startsAfter("Logger");
}

void StatisticsFeature::collectOptions(std::shared_ptr<ProgramOptions> options) {
  options->addOldOption("server.disable-statistics", "server.statistics");
  
  options->addSection("server", "Server features");

  options->addHiddenOption(
      "--server.statistics",
      "turn statistics gathering on or off",
      new BooleanParameter(&_statistics));
}

void StatisticsFeature::start() {
  STATISTICS = this;
  TRI_InitializeStatistics();
  
  _statisticsThread.reset(new StatisticsThread);
  if (!_statisticsThread->start()) {
    LOG(FATAL) << "could not start statistics thread";
    FATAL_ERROR_EXIT();
  }
}

void StatisticsFeature::unprepare() {
  if (_statisticsThread != nullptr) {
    _statisticsThread->beginShutdown();
    while (_statisticsThread->isRunning()) {
      usleep(10000);
    }
  }
  _statisticsThread.reset();

  STATISTICS = nullptr;
}
