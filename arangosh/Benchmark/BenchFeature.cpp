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
/// @author Jan Steemann
////////////////////////////////////////////////////////////////////////////////

#include "BenchFeature.h"

#include <ctime>
#include <iomanip>
#include <iostream>

#include "ApplicationFeatures/ApplicationServer.h"
#include "Basics/StringUtils.h"
#include "Benchmark/BenchmarkCounter.h"
#include "Benchmark/BenchmarkOperation.h"
#include "Benchmark/BenchmarkThread.h"
#include "ProgramOptions/ProgramOptions.h"
#include "ProgramOptions/Section.h"
#include "Shell/ClientFeature.h"
#include "SimpleHttpClient/SimpleHttpClient.h"
#include "SimpleHttpClient/SimpleHttpResult.h"

using namespace arangodb;
using namespace arangodb::arangobench;
using namespace arangodb::basics;
using namespace arangodb::httpclient;
using namespace arangodb::options;
using namespace arangodb::rest;

////////////////////////////////////////////////////////////////////////////////
/// @brief includes all the test cases
///
/// We use an evil global pointer here.
////////////////////////////////////////////////////////////////////////////////

BenchFeature* ARANGOBENCH;
#include "Benchmark/test-cases.h"

BenchFeature::BenchFeature(application_features::ApplicationServer* server,
                           int* result)
    : ApplicationFeature(server, "Bench"),
      _async(false),
      _concurreny(1),
      _operations(1000),
      _batchSize(0),
      _keepAlive(true),
      _collection("ArangoBenchmark"),
      _testCase("version"),
      _complexity(1),
      _delay(false),
      _progress(true),
      _verbose(false),
      _quiet(false),
      _runs(1),
      _junitReportFile(""),
      _replicationFactor(1),
      _numberOfShards(1),
      _waitForSync(false),
      _result(result) {
  requiresElevatedPrivileges(false);
  setOptional(false);
  startsAfter("Client");
  startsAfter("Config");
  startsAfter("Random");
  startsAfter("Logger");
}

void BenchFeature::collectOptions(std::shared_ptr<ProgramOptions> options) {
  options->addOption("--async", "send asynchronous requests",
                     new BooleanParameter(&_async));

  options->addOption("--concurrency", "number of parallel connections",
                     new UInt64Parameter(&_concurreny));

  options->addOption("--requests", "total number of operations",
                     new UInt64Parameter(&_operations));

  options->addOption("--batch-size",
                     "number of operations in one batch (0 disables batching)",
                     new UInt64Parameter(&_batchSize));

  options->addOption("--keep-alive", "use HTTP keep-alive",
                     new BooleanParameter(&_keepAlive));

  options->addOption("--collection", "collection name to use in tests",
                     new StringParameter(&_collection));

  options->addOption("--replication-factor",
                     "replication factor of created collections",
                     new UInt64Parameter(&_replicationFactor));

  options->addOption("--number-of-shards",
                     "number of shards of created collections",
                     new UInt64Parameter(&_numberOfShards));

  options->addOption("--wait-for-sync",
                     "use waitForSync for created collections",
                     new BooleanParameter(&_waitForSync));

  std::unordered_set<std::string> cases = {"version",
                                           "document",
                                           "collection",
                                           "import-document",
                                           "hash",
                                           "skiplist",
                                           "edge",
                                           "shapes",
                                           "shapes-append",
                                           "random-shapes",
                                           "crud",
                                           "crud-append",
                                           "crud-write-read",
                                           "aqltrx",
                                           "counttrx",
                                           "multitrx",
                                           "multi-collection",
                                           "aqlinsert",
                                           "aqlv8"};

  options->addOption(
      "--test-case", "test case to use",
      new DiscreteValuesParameter<StringParameter>(&_testCase, cases));

  options->addOption("--complexity", "complexity parameter for the test",
                     new UInt64Parameter(&_complexity));

  options->addOption("--delay",
                     "use a startup delay (necessary only when run in series)",
                     new BooleanParameter(&_delay));

  options->addOption("--junit-report-file",
                     "filename to write junit style report to",
                     new StringParameter(&_junitReportFile));

  options->addOption(
      "--runs", "run test n times (and calculate statistics based on median)",
      new UInt64Parameter(&_runs));

  options->addOption("--progress", "show progress",
                     new BooleanParameter(&_progress));

  options->addOption("--verbose",
                     "print out replies if the http-header indicates db-errors",
                     new BooleanParameter(&_verbose));

  options->addOption("--quiet", "supress status messages",
                     new BooleanParameter(&_quiet));
}

void BenchFeature::status(std::string const& value) {
  if (!_quiet) {
    std::cout << value << std::endl;
  }
}

std::atomic<int> BenchFeature::_started;

void BenchFeature::updateStartCounter() { ++_started; }

int BenchFeature::getStartCounter() { return _started; }

void BenchFeature::start() {
  ClientFeature* client =
      application_features::ApplicationServer::getFeature<ClientFeature>(
          "Client");
  client->setRetries(3);
  client->setWarn(true);

  int ret = EXIT_SUCCESS;

  *_result = ret;
  ARANGOBENCH = this;

  std::unique_ptr<BenchmarkOperation> benchmark(GetTestCase(_testCase));

  if (benchmark == nullptr) {
    ARANGOBENCH = nullptr;
    LOG_TOPIC(FATAL, arangodb::Logger::FIXME) << "invalid test case name '" << _testCase << "'";
    FATAL_ERROR_EXIT();
  }

  double const stepSize = (double)_operations / (double)_concurreny;
  int64_t realStep = (int64_t)stepSize;

  if (stepSize - (double)((int64_t)stepSize) > 0.0) {
    realStep++;
  }

  if (realStep % 1000 != 0) {
    realStep += 1000 - (realStep % 1000);
  }

  // add some more offset so we don't get into trouble with threads of different
  // speed
  realStep += 10000;

  std::vector<BenchmarkThread*> threads;

  bool ok = true;
  std::vector<BenchRunResult> results;
  for (uint64_t j = 0; j < _runs; j++) {
    status("starting threads...");
    BenchmarkCounter<unsigned long> operationsCounter(
        0, (unsigned long)_operations);
    ConditionVariable startCondition;
    // start client threads
    _started = 0;
    for (uint64_t i = 0; i < _concurreny; ++i) {
      BenchmarkThread* thread = new BenchmarkThread(
          benchmark.get(), &startCondition, &BenchFeature::updateStartCounter,
          static_cast<int>(i), (unsigned long)_batchSize, &operationsCounter,
          client, _keepAlive, _async, _verbose);
      thread->setOffset((size_t)(i * realStep));
      thread->start();
      threads.push_back(thread);
    }

    // give all threads a chance to start so they will not miss the broadcast
    while (getStartCounter() < (int)_concurreny) {
      usleep(5000);
    }

    if (_delay) {
      status("sleeping (startup delay)...");
      sleep(10);
    }

    status("executing tests...");
    double start = TRI_microtime();

    // broadcast the start signal to all threads
    {
      CONDITION_LOCKER(guard, startCondition);
      guard.broadcast();
    }

    size_t const stepValue = static_cast<size_t>(_operations / 20);
    size_t nextReportValue = stepValue;

    if (nextReportValue < 100) {
      nextReportValue = 100;
    }

    while (true) {
      size_t const numOperations = operationsCounter.getDone();

      if (numOperations >= (size_t)_operations) {
        break;
      }

      if (_progress && numOperations >= nextReportValue) {
        LOG_TOPIC(INFO, arangodb::Logger::FIXME) << "number of operations: " << nextReportValue;
        nextReportValue += stepValue;
      }

      usleep(10000);
    }

    double time = TRI_microtime() - start;
    double requestTime = 0.0;

    for (size_t i = 0; i < static_cast<size_t>(_concurreny); ++i) {
      requestTime += threads[i]->getTime();
    }

    if (operationsCounter.failures() > 0) {
      ok = false;
    }

    results.push_back({
        time, operationsCounter.failures(),
        operationsCounter.incompleteFailures(), requestTime,
    });
    for (size_t i = 0; i < static_cast<size_t>(_concurreny); ++i) {
      delete threads[i];
    }
    threads.clear();
  }
  std::cout << std::endl;

  report(client, results);
  if (!ok) {
    std::cout << "At least one of the runs produced failures!" << std::endl;
  }
  benchmark->tearDown();

  if (!ok) {
    ret = EXIT_FAILURE;
  }

  *_result = ret;
}

bool BenchFeature::report(ClientFeature* client,
                          std::vector<BenchRunResult> results) {
  std::cout << std::endl;

  std::cout << "Total number of operations: " << _operations
            << ", runs: " << _runs
            << ", keep alive: " << (_keepAlive ? "yes" : "no")
            << ", async: " << (_async ? "yes" : "no")
            << ", batch size: " << _batchSize
            << ", replication factor: " << _replicationFactor
            << ", number of shards: " << _numberOfShards
            << ", wait for sync: " << (_waitForSync ? "true" : "false")
            << ", concurrency level (threads): " << _concurreny << std::endl;

  std::cout << "Test case: " << _testCase << ", complexity: " << _complexity
            << ", database: '" << client->databaseName() << "', collection: '"
            << _collection << "'" << std::endl;

  std::sort(results.begin(), results.end(),
            [](BenchRunResult a, BenchRunResult b) { return a.time < b.time; });

  BenchRunResult output{0, 0, 0, 0};
  if (_runs > 1) {
    size_t size = results.size();
    std::cout << std::endl;
    std::cout << "Printing fastest result" << std::endl;
    std::cout << "=======================" << std::endl;
    printResult(results[0]);

    std::cout << "Printing slowest result" << std::endl;
    std::cout << "=======================" << std::endl;
    printResult(results[size - 1]);

    std::cout << "Printing median result" << std::endl;
    std::cout << "=======================" << std::endl;
    size_t mid = (size_t)size / 2;
    if (size % 2 == 0) {
      output.update(
          (results[mid - 1].time + results[mid].time) / 2,
          (results[mid - 1].failures + results[mid].failures) / 2,
          (results[mid - 1].incomplete + results[mid].incomplete) / 2,
          (results[mid - 1].requestTime + results[mid].requestTime) / 2);
    } else {
      output = results[mid];
    }
  } else if (_runs > 0) {
    output = results[0];
  }
  printResult(output);
  if (_junitReportFile.empty()) {
    return true;
  }

  return writeJunitReport(output);
}

bool BenchFeature::writeJunitReport(BenchRunResult const& result) {
  std::ofstream outfile(_junitReportFile, std::ofstream::binary);
  if (!outfile.is_open()) {
    std::cerr << "Could not open JUnit Report File: " << _junitReportFile
              << std::endl;
    return false;
  }

  // c++ shall die....not even bothered to provide proper alternatives
  // to this C dirt

  std::time_t t = std::time(nullptr);
  std::tm tm = *std::localtime(&t);

  char date[255];
  memset(date, 0, sizeof(date));
  strftime(date, sizeof(date) - 1, "%FT%T%z", &tm);

  char host[255];
  memset(host, 0, sizeof(host));
  gethostname(host, sizeof(host) - 1);

  std::string hostname(host);
  bool ok = false;
  try {
    outfile << "<?xml version=\"1.0\" encoding=\"UTF-8\"?>" << '\n'
            << "<testsuite name=\"arangobench\" tests=\"1\" skipped=\"0\" "
               "failures=\"0\" errors=\"0\" timestamp=\""
            << date << "\" hostname=\"" << hostname << "\" time=\""
            << std::fixed << result.time << "\">\n"
            << "<properties/>\n"
            << "<testcase name=\"" << testCase() << "\" classname=\"BenchTest\""
            << " time=\"" << std::fixed << result.time << "\"/>\n"
            << "</testsuite>\n";
    ok = true;
  } catch (...) {
    std::cerr << "Got an exception writing to junit report file "
              << _junitReportFile;
    ok = false;
  }
  outfile.close();
  return ok;
}

void BenchFeature::printResult(BenchRunResult const& result) {
  std::cout << "Total request/response duration (sum of all threads): "
            << std::fixed << result.requestTime << " s" << std::endl;

  std::cout << "Request/response duration (per thread): " << std::fixed
            << (result.requestTime / (double)_concurreny) << " s" << std::endl;

  std::cout << "Time needed per operation: " << std::fixed
            << (result.time / _operations) << " s" << std::endl;

  std::cout << "Time needed per operation per thread: " << std::fixed
            << (result.time / (double)_operations * (double)_concurreny) << " s"
            << std::endl;

  std::cout << "Operations per second rate: " << std::fixed
            << ((double)_operations / result.time) << std::endl;

  std::cout << "Elapsed time since start: " << std::fixed << result.time << " s"
            << std::endl
            << std::endl;

  if (result.failures > 0) {
    LOG_TOPIC(WARN, arangodb::Logger::FIXME) << result.failures << " arangobench request(s) failed!";
  }
  if (result.incomplete > 0) {
    LOG_TOPIC(WARN, arangodb::Logger::FIXME) << result.incomplete
              << " arangobench requests with incomplete results!";
  }
}

void BenchFeature::unprepare() { ARANGOBENCH = nullptr; }
