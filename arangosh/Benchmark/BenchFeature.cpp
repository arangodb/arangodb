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
  std::vector<std::string> casesVector(cases.begin(), cases.end());
  std::string casesJoined = StringUtils::join(casesVector, ", ");

  options->addOption(
      "--test-case", "test case to use (possible values: " + casesJoined + ")",
      new DiscreteValuesParameter<StringParameter>(&_testCase, cases));

  options->addOption("--complexity", "complexity parameter for the test",
                     new UInt64Parameter(&_complexity));

  options->addOption("--delay",
                     "use a startup delay (necessary only when run in series)",
                     new BooleanParameter(&_delay));

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
  ClientFeature* client = application_features::ApplicationServer::getFeature<ClientFeature>("Client");
  client->setRetries(3);
  client->setWarn(true);

  int ret = EXIT_SUCCESS;

  *_result = ret;
  ARANGOBENCH = this;

  std::unique_ptr<BenchmarkOperation> benchmark(GetTestCase(_testCase));

  if (benchmark == nullptr) {
    ARANGOBENCH = nullptr;
    LOG(FATAL) << "invalid test case name '" << _testCase << "'";
    FATAL_ERROR_EXIT();
  }

  status("starting threads...");

  BenchmarkCounter<unsigned long> operationsCounter(0,
                                                    (unsigned long)_operations);
  ConditionVariable startCondition;

  std::vector<Endpoint*> endpoints;
  std::vector<BenchmarkThread*> threads;

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

  // start client threads
  for (uint64_t i = 0; i < _concurreny; ++i) {
    Endpoint* endpoint = Endpoint::clientFactory(client->endpoint());
    endpoints.push_back(endpoint);

    BenchmarkThread* thread = new BenchmarkThread(
        benchmark.get(), &startCondition, &BenchFeature::updateStartCounter,
        static_cast<int>(i), (unsigned long)_batchSize, &operationsCounter, client, _keepAlive,
        _async, _verbose);

    threads.push_back(thread);
    thread->setOffset((size_t)(i * realStep));
    thread->start();
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
      LOG(INFO) << "number of operations: " << nextReportValue;
      nextReportValue += stepValue;
    }

    usleep(10000);
  }

  double time = TRI_microtime() - start;
  double requestTime = 0.0;

  for (size_t i = 0; i < static_cast<size_t>(_concurreny); ++i) {
    requestTime += threads[i]->getTime();
  }

  size_t failures = operationsCounter.failures();
  size_t incomplete = operationsCounter.incompleteFailures();

  std::cout << std::endl;

  std::cout << "Total number of operations: " << _operations
            << ", keep alive: " << (_keepAlive ? "yes" : "no")
            << ", async: " << (_async ? "yes" : "no")
            << ", batch size: " << _batchSize
            << ", concurrency level (threads): " << _concurreny << std::endl;

  std::cout << "Test case: " << _testCase << ", complexity: " << _complexity
            << ", database: '" << client->databaseName() << "', collection: '"
            << _collection << "'" << std::endl;

  std::cout << "Total request/response duration (sum of all threads): "
            << std::fixed << requestTime << " s" << std::endl;

  std::cout << "Request/response duration (per thread): " << std::fixed
            << (requestTime / (double)_concurreny) << " s" << std::endl;

  std::cout << "Time needed per operation: " << std::fixed
            << (time / _operations) << " s" << std::endl;

  std::cout << "Time needed per operation per thread: " << std::fixed
            << (time / (double)_operations * (double)_concurreny) << " s"
            << std::endl;

  std::cout << "Operations per second rate: " << std::fixed
            << ((double)_operations / time) << std::endl;

  std::cout << "Elapsed time since start: " << std::fixed << time << " s"
            << std::endl
            << std::endl;

  if (failures > 0) {
    LOG(WARN) << "WARNING: " << failures << " arangobench request(s) failed!";
  }
  if (incomplete > 0) {
    LOG(WARN) << "WARNING: " << incomplete
              << " arangobench requests with incomplete results!";
  }

  benchmark->tearDown();

  for (size_t i = 0; i < static_cast<size_t>(_concurreny); ++i) {
    delete threads[i];
    delete endpoints[i];
  }

  if (failures > 0) {
    ret = EXIT_FAILURE;
  }

  *_result = ret;
}

void BenchFeature::unprepare() {
  ARANGOBENCH = nullptr;
}
