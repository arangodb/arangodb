////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2014-2023 ArangoDB GmbH, Cologne, Germany
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
/// @author Max Neunhoeffer
////////////////////////////////////////////////////////////////////////////////

#include "IOHeartbeatThread.h"

#include "ApplicationFeatures/ApplicationServer.h"
#include "Basics/ErrorCode.h"
#include "Basics/FileUtils.h"
#include "Logger/LogMacros.h"
#include "Metrics/CounterBuilder.h"
#include "Metrics/HistogramBuilder.h"
#include "Metrics/MetricsFeature.h"
#include "RestServer/DatabasePathFeature.h"

using namespace arangodb;

struct HeartbeatTimescale {
  static metrics::LogScale<double> scale() { return {10.0, 0.0, 1000000.0, 8}; }
};

DECLARE_HISTOGRAM(arangodb_ioheartbeat_duration, HeartbeatTimescale,
                  "Time to execute the io heartbeat once [us]");
DECLARE_COUNTER(arangodb_ioheartbeat_failures_total,
                "Total number of failures in IO heartbeat");
DECLARE_COUNTER(arangodb_ioheartbeat_delays_total,
                "Total number of delays in IO heartbeat");

/// IO check thread main loop
/// The purpose of this thread is to try to perform a simple IO write
/// operation on the database volume regularly. We need visibility in
/// production if IO is slow or not possible at all.
IOHeartbeatThread::IOHeartbeatThread(Server& server,
                                     metrics::MetricsFeature& metricsFeature)
    : ServerThread<ArangodServer>(server, "IOHeartbeat"),
      _exeTimeHistogram(metricsFeature.add(arangodb_ioheartbeat_duration{})),
      _failures(metricsFeature.add(arangodb_ioheartbeat_failures_total{})),
      _delays(metricsFeature.add(arangodb_ioheartbeat_delays_total{})) {}

IOHeartbeatThread::~IOHeartbeatThread() { shutdown(); }

void IOHeartbeatThread::run() {
  auto& databasePathFeature = server().getFeature<DatabasePathFeature>();
  std::string testFilePath = basics::FileUtils::buildFilename(
      databasePathFeature.directory(), "TestFileIOHeartbeat");
  std::string testFileContent = "This is just an I/O test.\n";

  LOG_TOPIC("66665", DEBUG, Logger::ENGINES) << "IOHeartbeatThread: running...";
  // File might be left if previous run has crashed.
  // That will trigger an error in spit.
  std::ignore = basics::FileUtils::remove(testFilePath);
  while (true) {
    try {  // protect thread against any exceptions
      if (isStopping()) {
        // done
        break;
      }

      LOG_TOPIC("66659", DEBUG, Logger::ENGINES)
          << "IOHeartbeat: testing to write/read/remove " << testFilePath;
      // We simply write a file and sync it to disk in the database
      // directory and then read it and then delete it again:
      auto start1 = std::chrono::steady_clock::now();
      bool trouble = false;
      try {
        basics::FileUtils::spit(testFilePath, testFileContent, true);
      } catch (std::exception const& exc) {
        ++_failures;
        LOG_TOPIC("66663", INFO, Logger::ENGINES)
            << "IOHeartbeat: exception when writing test file: " << exc.what();
        trouble = true;
      }
      auto finish = std::chrono::steady_clock::now();
      std::chrono::duration<double> dur = finish - start1;
      bool delayed = dur > std::chrono::seconds(1);
      if (trouble || delayed) {
        if (delayed) {
          ++_delays;
        }
        LOG_TOPIC("66662", INFO, Logger::ENGINES)
            << "IOHeartbeat: trying to write test file took "
            << std::chrono::duration_cast<std::chrono::microseconds>(dur)
                   .count()
            << " microseconds.";
      }

      // Read the file if we can reasonably assume it is there:
      if (!trouble) {
        auto start = std::chrono::steady_clock::now();
        try {
          std::string content = basics::FileUtils::slurp(testFilePath);
          if (content != testFileContent) {
            LOG_TOPIC("66660", INFO, Logger::ENGINES)
                << "IOHeartbeat: read content of test file was not as "
                   "expected, found:'"
                << content << "', expected: '" << testFileContent << "'";
            trouble = true;
            ++_failures;
          }
        } catch (std::exception const& exc) {
          ++_failures;
          LOG_TOPIC("66661", INFO, Logger::ENGINES)
              << "IOHeartbeat: exception when reading test file: "
              << exc.what();
          trouble = true;
        }
        auto finish = std::chrono::steady_clock::now();
        std::chrono::duration<double> dur = finish - start;
        bool delayed = dur > std::chrono::seconds(1);
        if (trouble || delayed) {
          if (delayed) {
            ++_delays;
          }
          LOG_TOPIC("66669", INFO, Logger::ENGINES)
              << "IOHeartbeat: trying to read test file took "
              << std::chrono::duration_cast<std::chrono::microseconds>(dur)
                     .count()
              << " microseconds.";
        }

        // And remove it again:
        start = std::chrono::steady_clock::now();
        ErrorCode err = basics::FileUtils::remove(testFilePath);
        if (err != TRI_ERROR_NO_ERROR) {
          ++_failures;
          LOG_TOPIC("66670", INFO, Logger::ENGINES)
              << "IOHeartbeat: error when removing test file: " << err;
          trouble = true;
        }
        finish = std::chrono::steady_clock::now();
        dur = finish - start;
        delayed = dur > std::chrono::seconds(1);
        if (trouble || delayed) {
          if (delayed) {
            ++_delays;
          }
          LOG_TOPIC("66671", INFO, Logger::ENGINES)
              << "IOHeartbeat: trying to remove test file took "
              << std::chrono::duration_cast<std::chrono::microseconds>(dur)
                     .count()
              << " microseconds.";
        }
      }

      // Total duration and update histogram:
      dur = finish - start1;
      _exeTimeHistogram.count(static_cast<double>(
          std::chrono::duration_cast<std::chrono::microseconds>(dur).count()));

      std::unique_lock<std::mutex> guard(_mutex);
      if (trouble) {
        // In case of trouble, we retry more quickly, since we want to
        // have a record when the trouble has actually stopped!
        _cv.wait_for(guard, checkIntervalTrouble);
      } else {
        _cv.wait_for(guard, checkIntervalNormal);
      }
    } catch (...) {
    }
    // next iteration
  }
  LOG_TOPIC("66664", DEBUG, Logger::ENGINES) << "IOHeartbeatThread: stopped.";
}
