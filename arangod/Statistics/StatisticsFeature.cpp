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
/// @author Dr. Frank Celler
////////////////////////////////////////////////////////////////////////////////

#include "StatisticsFeature.h"

#include "ApplicationFeatures/ApplicationServer.h"
#include "Basics/application-exit.h"
#include "Cluster/ServerState.h"
#include "FeaturePhases/AqlFeaturePhase.h"
#include "Logger/LogMacros.h"
#include "Logger/Logger.h"
#include "Logger/LoggerStream.h"
#include "ProgramOptions/ProgramOptions.h"
#include "ProgramOptions/Section.h"
#include "RestServer/DatabaseFeature.h"
#include "RestServer/MetricsFeature.h"
#include "RestServer/SystemDatabaseFeature.h"
#include "Statistics/ConnectionStatistics.h"
#include "Statistics/Descriptions.h"
#include "Statistics/RequestStatistics.h"
#include "Statistics/ServerStatistics.h"
#include "Statistics/StatisticsWorker.h"
#include "VocBase/vocbase.h"

#include <chrono>
#include <thread>

using namespace arangodb;
using namespace arangodb::application_features;
using namespace arangodb::basics;
using namespace arangodb::options;

// -----------------------------------------------------------------------------
// --SECTION--                                                  global variables
// -----------------------------------------------------------------------------

namespace arangodb {
namespace statistics {

std::initializer_list<double> const BytesReceivedDistributionCuts{250, 1000, 2000, 5000, 10000};
std::initializer_list<double> const BytesSentDistributionCuts{250, 1000, 2000, 5000, 10000};
std::initializer_list<double> const ConnectionTimeDistributionCuts{0.1, 1.0, 60.0};
std::initializer_list<double> const RequestTimeDistributionCuts{0.01, 0.05, 0.1, 0.2, 0.5, 1.0};

Counter AsyncRequests;
Counter HttpConnections;
Counter TotalRequests;
Counter TotalRequestsSuperuser;
Counter TotalRequestsUser;
MethodRequestCounters MethodRequests;
Distribution ConnectionTimeDistribution(ConnectionTimeDistributionCuts);

RequestFigures::RequestFigures() :
  bytesReceivedDistribution(BytesReceivedDistributionCuts),
  bytesSentDistribution(BytesSentDistributionCuts),
  ioTimeDistribution(RequestTimeDistributionCuts),
  queueTimeDistribution(RequestTimeDistributionCuts),
  requestTimeDistribution(RequestTimeDistributionCuts),
  totalTimeDistribution(RequestTimeDistributionCuts) {}

RequestFigures SuperuserRequestFigures;
RequestFigures UserRequestFigures;
}  // namespace statistics
}  // namespace arangodb

// -----------------------------------------------------------------------------
// --SECTION--                                                  StatisticsThread
// -----------------------------------------------------------------------------

class StatisticsThread final : public Thread {
 public:
  explicit StatisticsThread(ApplicationServer& server) 
    : Thread(server, "Statistics") {}
  ~StatisticsThread() { shutdown(); }

 public:
  void run() override {
    auto& databaseFeature = server().getFeature<arangodb::DatabaseFeature>();
    if (databaseFeature.upgrade()) {
      // don't start the thread when we are running an upgrade
      return;
    }

    uint64_t const MAX_SLEEP_TIME = 250;

    uint64_t sleepTime = 100;
    int nothingHappened = 0;

    while (!isStopping() && StatisticsFeature::enabled()) {
      size_t count = RequestStatistics::processAll();

      if (count == 0) {
        if (++nothingHappened == 10 * 30) {
          // increase sleep time every 30 seconds
          nothingHappened = 0;
          sleepTime += 50;

          if (sleepTime > MAX_SLEEP_TIME) {
            sleepTime = MAX_SLEEP_TIME;
          }
        }

        std::this_thread::sleep_for(std::chrono::milliseconds(sleepTime));

      } else {
        nothingHappened = 0;

        if (count < 10) {
          std::this_thread::sleep_for(std::chrono::milliseconds(10));
        } else if (count < 100) {
          std::this_thread::sleep_for(std::chrono::milliseconds(1));
        }
      }
    }
  }
};

// -----------------------------------------------------------------------------
// --SECTION--                                                 StatisticsFeature
// -----------------------------------------------------------------------------

StatisticsFeature* StatisticsFeature::STATISTICS = nullptr;

StatisticsFeature::StatisticsFeature(application_features::ApplicationServer& server)
    : ApplicationFeature(server, "Statistics"),
      _statistics(true),
      _statisticsHistory(true),
      _statisticsHistoryTouched(false),
      _descriptions(new stats::Descriptions(server)) {
  setOptional(true);
  startsAfter<AqlFeaturePhase>();
}

StatisticsFeature::~StatisticsFeature() = default;

void StatisticsFeature::collectOptions(std::shared_ptr<ProgramOptions> options) {
  options->addOldOption("server.disable-statistics", "server.statistics");

  options->addSection("server", "Server features");

  options->addOption("--server.statistics",
                     "turn statistics gathering on or off",
                     new BooleanParameter(&_statistics),
                     arangodb::options::makeDefaultFlags(arangodb::options::Flags::Hidden));
  options->addOption("--server.statistics-history",
                     "turn storing statistics in database on or off",
                     new BooleanParameter(&_statisticsHistory),
                     arangodb::options::makeDefaultFlags(arangodb::options::Flags::Hidden))
    .setIntroducedIn(30409)
    .setIntroducedIn(30501);
}

void StatisticsFeature::validateOptions(std::shared_ptr<ProgramOptions> options) {
  if (!_statistics) {
    // turn ourselves off
    disable();
  }

  _statisticsHistoryTouched = options->processingResult().touched("--server.statistics-history");

}

void StatisticsFeature::prepare() {
  // initialize counters for all HTTP request types

  STATISTICS = this;

  ConnectionStatistics::initialize();
  RequestStatistics::initialize();
}

void StatisticsFeature::start() {
  TRI_ASSERT(isEnabled());

  if (!server().hasFeature<arangodb::SystemDatabaseFeature>()) {
    LOG_TOPIC("9b551", FATAL, arangodb::Logger::STATISTICS)
        << "could not find feature 'SystemDatabase'";
    FATAL_ERROR_EXIT();
  }
  auto& sysDbFeature = server().getFeature<arangodb::SystemDatabaseFeature>();

  auto vocbase = sysDbFeature.use();

  if (!vocbase) {
    LOG_TOPIC("cff56", FATAL, arangodb::Logger::STATISTICS)
        << "could not find system database";
    FATAL_ERROR_EXIT();
  }

  _statisticsThread.reset(new StatisticsThread(server()));

  if (!_statisticsThread->start()) {
    LOG_TOPIC("46b0c", FATAL, arangodb::Logger::STATISTICS)
        << "could not start statistics thread";
    FATAL_ERROR_EXIT();
  }

  // force history disable on Agents
  if (arangodb::ServerState::instance()->isAgent() && !_statisticsHistoryTouched) {
    _statisticsHistory = false;
  } // if

  if (_statisticsHistory) {
    _statisticsWorker.reset(new StatisticsWorker(*vocbase));

    if (!_statisticsWorker->start()) {
      LOG_TOPIC("6ecdc", FATAL, arangodb::Logger::STATISTICS)
        << "could not start statistics worker";
      FATAL_ERROR_EXIT();
    }
  } // if
}

void StatisticsFeature::stop() {
  if (_statisticsThread != nullptr) {
    _statisticsThread->beginShutdown();

    while (_statisticsThread->isRunning()) {
      std::this_thread::sleep_for(std::chrono::microseconds(10000));
    }
  }

  if (_statisticsWorker != nullptr) {
    _statisticsWorker->beginShutdown();

    while (_statisticsWorker->isRunning()) {
      std::this_thread::sleep_for(std::chrono::microseconds(10000));
    }
  }

  _statisticsThread.reset();
  _statisticsWorker.reset();

  STATISTICS = nullptr;
}

void StatisticsFeature::toPrometheus(std::string& result, double const& now) {
  if (_statisticsWorker != nullptr) {
    _statisticsWorker->generateRawStatistics(result, now);
  }
}
