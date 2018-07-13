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
#include "Logger/Logger.h"
#include "ProgramOptions/ProgramOptions.h"
#include "ProgramOptions/Section.h"
#include "RestServer/DatabaseFeature.h"
#include "Statistics/ConnectionStatistics.h"
#include "Statistics/Descriptions.h"
#include "Statistics/RequestStatistics.h"
#include "Statistics/ServerStatistics.h"
#include "Statistics/StatisticsWorker.h"
#include "VocBase/vocbase.h"

#include <thread>
#include <chrono>

using namespace arangodb;
using namespace arangodb::application_features;
using namespace arangodb::basics;
using namespace arangodb::options;

// -----------------------------------------------------------------------------
// --SECTION--                                                  global variables
// -----------------------------------------------------------------------------

namespace arangodb {
namespace basics {
StatisticsCounter TRI_AsyncRequestsStatistics;
StatisticsCounter TRI_HttpConnectionsStatistics;
StatisticsCounter TRI_TotalRequestsStatistics;
StatisticsDistribution* TRI_BytesReceivedDistributionStatistics;
StatisticsDistribution* TRI_BytesSentDistributionStatistics;
StatisticsDistribution* TRI_ConnectionTimeDistributionStatistics;
StatisticsDistribution* TRI_IoTimeDistributionStatistics;
StatisticsDistribution* TRI_QueueTimeDistributionStatistics;
StatisticsDistribution* TRI_RequestTimeDistributionStatistics;
StatisticsDistribution* TRI_TotalTimeDistributionStatistics;
StatisticsVector TRI_BytesReceivedDistributionVectorStatistics;
StatisticsVector TRI_BytesSentDistributionVectorStatistics;
StatisticsVector TRI_ConnectionTimeDistributionVectorStatistics;
StatisticsVector TRI_RequestTimeDistributionVectorStatistics;
std::vector<StatisticsCounter> TRI_MethodRequestsStatistics;
}
}

// -----------------------------------------------------------------------------
// --SECTION--                                                  StatisticsThread
// -----------------------------------------------------------------------------

class arangodb::StatisticsThread final : public Thread {
 public:
  StatisticsThread() : Thread("Statistics") {}
  ~StatisticsThread() { shutdown(); }

 public:
  void run() override {
    uint64_t const MAX_SLEEP_TIME = 250 * 1000;

    uint64_t sleepTime = 100 * 1000;
    int nothingHappened = 0;

    while (!isStopping() && StatisticsFeature::enabled()) {
      size_t count = RequestStatistics::processAll();

      if (count == 0) {
        if (++nothingHappened == 10 * 30) {
          // increase sleep time every 30 seconds
          nothingHappened = 0;
          sleepTime += 50 * 1000;

          if (sleepTime > MAX_SLEEP_TIME) {
            sleepTime = MAX_SLEEP_TIME;
          }
        }

        std::this_thread::sleep_for(std::chrono::microseconds(sleepTime));
      } else {
        nothingHappened = 0;

        if (count < 10) {
          std::this_thread::sleep_for(std::chrono::microseconds(10 * 1000));
        } else if (count < 100) {
          std::this_thread::sleep_for(std::chrono::microseconds(1 * 1000));
        }
      }
    }

    RequestStatistics::shutdown();
    ConnectionStatistics::shutdown();
    ServerStatistics::shutdown();
  }
};

// -----------------------------------------------------------------------------
// --SECTION--                                                 StatisticsFeature
// -----------------------------------------------------------------------------

StatisticsFeature* StatisticsFeature::STATISTICS = nullptr;

StatisticsFeature::StatisticsFeature(
    application_features::ApplicationServer* server)
    : ApplicationFeature(server, "Statistics"),
      _statistics(true),
      _descriptions(new stats::Descriptions()) {
  startsAfter("AQLPhase");
}

void StatisticsFeature::collectOptions(
    std::shared_ptr<ProgramOptions> options) {
  options->addOldOption("server.disable-statistics", "server.statistics");

  options->addSection("server", "Server features");

  options->addHiddenOption("--server.statistics",
                           "turn statistics gathering on or off",
                           new BooleanParameter(&_statistics));
}

void StatisticsFeature::prepare() {
  // initialize counters for all HTTP request types
  TRI_MethodRequestsStatistics.clear();

  for (int i = 0; i < ((int)arangodb::rest::RequestType::ILLEGAL) + 1; ++i) {
    StatisticsCounter c;
    TRI_MethodRequestsStatistics.emplace_back(c);
  }

  TRI_ConnectionTimeDistributionVectorStatistics << (0.1) << (1.0) << (60.0);

  TRI_BytesSentDistributionVectorStatistics << (250) << (1000) << (2 * 1000)
                                            << (5 * 1000) << (10 * 1000);
  TRI_BytesReceivedDistributionVectorStatistics << (250) << (1000) << (2 * 1000)
                                                << (5 * 1000) << (10 * 1000);
  TRI_RequestTimeDistributionVectorStatistics << (0.01) << (0.05) << (0.1)
                                              << (0.2) << (0.5) << (1.0);

  TRI_ConnectionTimeDistributionStatistics = new StatisticsDistribution(
      TRI_ConnectionTimeDistributionVectorStatistics);
  TRI_TotalTimeDistributionStatistics =
      new StatisticsDistribution(TRI_RequestTimeDistributionVectorStatistics);
  TRI_RequestTimeDistributionStatistics =
      new StatisticsDistribution(TRI_RequestTimeDistributionVectorStatistics);
  TRI_QueueTimeDistributionStatistics =
      new StatisticsDistribution(TRI_RequestTimeDistributionVectorStatistics);
  TRI_IoTimeDistributionStatistics =
      new StatisticsDistribution(TRI_RequestTimeDistributionVectorStatistics);
  TRI_BytesSentDistributionStatistics =
      new StatisticsDistribution(TRI_BytesSentDistributionVectorStatistics);
  TRI_BytesReceivedDistributionStatistics =
      new StatisticsDistribution(TRI_BytesReceivedDistributionVectorStatistics);

  STATISTICS = this;

  ServerStatistics::initialize();
  ConnectionStatistics::initialize();
  RequestStatistics::initialize();
}

void StatisticsFeature::start() {
  if (!_statistics) {
    return;
  }

  auto* databaseFeature = arangodb::application_features::ApplicationServer::lookupFeature<
    arangodb::DatabaseFeature
  >("Database");

  if (!databaseFeature) {
    LOG_TOPIC(FATAL, arangodb::Logger::STATISTICS) << "could not find feature 'Database'";
    FATAL_ERROR_EXIT();
  }

  auto* vocbase = databaseFeature->systemDatabase();

  if (!vocbase) {
    LOG_TOPIC(FATAL, arangodb::Logger::STATISTICS) << "could not find system database";
    FATAL_ERROR_EXIT();
  }

  _statisticsThread.reset(new StatisticsThread);
  _statisticsWorker.reset(new StatisticsWorker(*vocbase));

  if (!_statisticsThread->start()) {
    LOG_TOPIC(FATAL, arangodb::Logger::STATISTICS) << "could not start statistics thread";
    FATAL_ERROR_EXIT();
  }

  if (!_statisticsWorker->start()) {
    LOG_TOPIC(FATAL, arangodb::Logger::STATISTICS) << "could not start statistics worker";
    FATAL_ERROR_EXIT();
  }
}

void StatisticsFeature::unprepare() {
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

  delete TRI_ConnectionTimeDistributionStatistics;
  TRI_ConnectionTimeDistributionStatistics = nullptr;

  delete TRI_TotalTimeDistributionStatistics;
  TRI_TotalTimeDistributionStatistics = nullptr;

  delete TRI_RequestTimeDistributionStatistics;
  TRI_RequestTimeDistributionStatistics = nullptr;

  delete TRI_QueueTimeDistributionStatistics;
  TRI_QueueTimeDistributionStatistics = nullptr;

  delete TRI_IoTimeDistributionStatistics;
  TRI_IoTimeDistributionStatistics = nullptr;

  delete TRI_BytesSentDistributionStatistics;
  TRI_BytesSentDistributionStatistics = nullptr;

  delete TRI_BytesReceivedDistributionStatistics;
  TRI_BytesReceivedDistributionStatistics = nullptr;

  STATISTICS = nullptr;
}
