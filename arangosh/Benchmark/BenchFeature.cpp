////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2014-2021 ArangoDB GmbH, Cologne, Germany
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
/// @author Jan Steemann
////////////////////////////////////////////////////////////////////////////////

#include "BenchFeature.h"

#include <velocypack/Builder.h>
#include <velocypack/Iterator.h>
#include <velocypack/Slice.h>
#include <velocypack/velocypack-aliases.h>

#include <ctime>
#include <fstream>
#include <iomanip>
#include <iostream>

#ifdef TRI_HAVE_UNISTD_H
#include <unistd.h>
#endif

#include "ApplicationFeatures/ApplicationServer.h"
#include "Basics/FileUtils.h"
#include "Basics/NumberOfCores.h"
#include "Basics/StaticStrings.h"
#include "Basics/StringUtils.h"
#include "Basics/Utf8Helper.h"
#include "Basics/application-exit.h"
#include "Basics/files.h"
#include "Basics/system-functions.h"
#include "Benchmark/BenchmarkCounter.h"
#include "Benchmark/BenchmarkOperation.h"
#include "Benchmark/BenchmarkStats.h"
#include "Benchmark/BenchmarkThread.h"
#include "FeaturePhases/BasicFeaturePhaseClient.h"
#include "Logger/LogMacros.h"
#include "Logger/Logger.h"
#include "ProgramOptions/ProgramOptions.h"
#include "ProgramOptions/Section.h"
#include "Shell/ClientFeature.h"
#include "SimpleHttpClient/HttpResponseChecker.h"
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

#include "Benchmark/test-cases.h"

BenchFeature::BenchFeature(application_features::ApplicationServer& server,
                           int* result)
    : ApplicationFeature(server, "Bench"),
      _concurrency(NumberOfCores::getValue()),
      _operations(1000),
      _realOperations(0),
      _batchSize(0),
      _duration(0),
      _collection("ArangoBenchmark"),
      _testCase("version"),
      _complexity(1),
      _async(false),
      _keepAlive(true),
      _createDatabase(false),
      _delay(false),
      _progress(true),
      _quiet(false),
      _waitForSync(false),
      _runs(1),
      _junitReportFile(""),
      _jsonReportFile(""),
      _replicationFactor(1),
      _numberOfShards(1),
      _result(result),
      _histogramNumIntervals(1000),
      _histogramIntervalSize(0.0),
      _percentiles({50.0, 80.0, 85.0, 90.0, 95.0, 99.0, 99.99}) {
  requiresElevatedPrivileges(false);
  setOptional(false);
  startsAfter<application_features::BasicFeaturePhaseClient>();

  // the following is not awesome, as all test classes need to be repeated here.
  // however, it works portably across different compilers.
  AqlInsertTest::registerTestcase();
  AqlV8Test::registerTestcase();
  CollectionCreationTest::registerTestcase();
  CustomQueryTest::registerTestcase();
  DocumentCreationTest::registerTestcase();
  DocumentCrudAppendTest::registerTestcase();
  DocumentCrudTest::registerTestcase();
  DocumentCrudWriteReadTest::registerTestcase();
  DocumentImportTest::registerTestcase();
  EdgeCrudTest::registerTestcase();
  HashTest::registerTestcase();
  RandomShapesTest::registerTestcase();
  ShapesAppendTest::registerTestcase();
  ShapesTest::registerTestcase();
  SkiplistTest::registerTestcase();
  StreamCursorTest::registerTestcase();
  TransactionAqlTest::registerTestcase();
  TransactionCountTest::registerTestcase();
  TransactionDeadlockTest::registerTestcase();
  TransactionMultiCollectionTest::registerTestcase();
  TransactionMultiTest::registerTestcase();
  VersionTest::registerTestcase();
}

void BenchFeature::collectOptions(std::shared_ptr<ProgramOptions> options) {
  options->addSection("histogram", "Benchmark statistics configuration");
  options->addOption(
      "--histogram.interval-size",
      "bucket width, dynamically calculated by default: "
      "(first measured time * 20) / num-intervals",
      new DoubleParameter(&_histogramIntervalSize),
      arangodb::options::makeDefaultFlags(options::Flags::Dynamic));
  options->addOption("--histogram.num-intervals",
                     "number of buckets (resolution)",
                     new UInt64Parameter(&_histogramNumIntervals));
  options->addOption(
      "--histogram.percentiles", "which percentiles to calculate",
      new VectorParameter<DoubleParameter>(&_percentiles),
      arangodb::options::makeDefaultFlags(options::Flags::FlushOnFirst));

  options->addOption("--async", "send asynchronous requests",
                     new BooleanParameter(&_async));

  options->addOption("--concurrency",
                     "number of parallel threads and connections",
                     new UInt64Parameter(&_concurrency));

  options->addOption("--requests", "total number of operations",
                     new UInt64Parameter(&_operations));

  options->addOption("--batch-size",
                     "number of operations in one batch (0 disables batching)",
                     new UInt64Parameter(&_batchSize));

  options->addOption("--keep-alive", "use HTTP keep-alive",
                     new BooleanParameter(&_keepAlive));

  options->addOption(
      "--collection",
      "collection name to use in tests (if they involve collections)",
      new StringParameter(&_collection));

  options->addOption("--replication-factor",
                     "replication factor of created collections (cluster only)",
                     new UInt64Parameter(&_replicationFactor));

  options->addOption("--number-of-shards",
                     "number of shards of created collections (cluster only)",
                     new UInt64Parameter(&_numberOfShards));

  options->addOption("--wait-for-sync",
                     "use waitForSync for created collections",
                     new BooleanParameter(&_waitForSync));

  options->addOption("--create-database",
                     "whether we should create the database specified via the "
                     "server connection",
                     new BooleanParameter(&_createDatabase));

  options->addOption("--duration",
                     "test for duration seconds instead of a fixed test count",
                     new UInt64Parameter(&_duration));

  std::unordered_set<std::string> cases;
  for (auto& [name, _] : BenchmarkOperation::allBenchmarks()) {
    cases.emplace(name);
  }
  options->addOption(
      "--test-case", "test case to use",
      new DiscreteValuesParameter<StringParameter>(&_testCase, cases));

  options->addOption(
      "--complexity",
      "complexity parameter for the test (meaning depends on test case)",
      new UInt64Parameter(&_complexity));

  options->addOption("--delay",
                     "use a startup delay (necessary only when run in series)",
                     new BooleanParameter(&_delay));

  options->addOption("--junit-report-file",
                     "filename to write junit style report to",
                     new StringParameter(&_junitReportFile));

  options->addOption("--json-report-file",
                     "filename to write a report in JSON format to",
                     new StringParameter(&_jsonReportFile));

  options->addOption(
      "--runs", "run test n times (and calculate statistics based on median)",
      new UInt64Parameter(&_runs));

  options->addOption("--progress", "log intermediate progress",
                     new BooleanParameter(&_progress));

  options
      ->addOption("--custom-query",
                  "the query to be used in the 'custom-query' testcase",
                  new StringParameter(&_customQuery))
      .setIntroducedIn(30800);

  options
      ->addOption(
          "--custom-query-file",
          "path to a file with the query to be used in the 'custom-query' "
          "testcase. "
          "If --custom-query is specified as well, it has higher priority.",
          new StringParameter(&_customQueryFile))
      .setIntroducedIn(30800);

  options->addOption("--quiet", "suppress status messages",
                     new BooleanParameter(&_quiet));

  options->addObsoleteOption(
      "--verbose", "print out replies if the HTTP header indicates DB errors",
      false);
}

void BenchFeature::status(std::string const& value) {
  if (!_quiet) {
    LOG_TOPIC("a6905", INFO, arangodb::Logger::BENCH) << value;
  }
}

std::atomic<int> BenchFeature::_started;

void BenchFeature::updateStartCounter() { ++_started; }

int BenchFeature::getStartCounter() { return _started; }

void BenchFeature::start() {
  std::sort(_percentiles.begin(), _percentiles.end());

  if (!_jsonReportFile.empty() && FileUtils::exists(_jsonReportFile)) {
    LOG_TOPIC("ee2a4", FATAL, arangodb::Logger::BENCH)
        << "file already exists: '" << _jsonReportFile
        << "' - won't overwrite it.";
    FATAL_ERROR_EXIT();
  }
  ClientFeature& client =
      server().getFeature<HttpEndpointProvider, ClientFeature>();
  client.setRetries(3);
  client.setWarn(true);

  if (_createDatabase) {
    auto connectDB = client.databaseName();
    client.setDatabaseName(StaticStrings::SystemDatabase);
    auto createDbClient = client.createHttpClient();
    createDbClient->params().setUserNamePassword("/", client.username(),
                                                 client.password());
    VPackBuilder b;
    b.openObject();
    b.add("name", VPackValue(normalizeUtf8ToNFC(connectDB)));
    b.close();

    std::unordered_map<std::string, std::string> headers;
    headers.emplace(StaticStrings::ContentTypeHeader,
                    StaticStrings::MimeTypeVPack);

    std::unique_ptr<SimpleHttpResult> result(createDbClient->request(
        RequestType::POST, "/_api/database", b.slice().startAs<char>(),
        b.slice().byteSize(), headers));

    auto check = arangodb::HttpResponseChecker::check(
        createDbClient->getErrorMessage(), result.get());
    if (check.fail()) {
      LOG_TOPIC("5cda8", FATAL, arangodb::Logger::BENCH)
          << "failed to create the specified database: "
          << check.errorMessage();
      FATAL_ERROR_EXIT();
    }

    client.setDatabaseName(connectDB);
  }
  int ret = EXIT_SUCCESS;

  *_result = ret;

  std::unique_ptr<BenchmarkOperation> benchmark =
      BenchmarkOperation::createBenchmark(_testCase, *this);

  if (benchmark == nullptr) {
    LOG_TOPIC("ee2a5", FATAL, arangodb::Logger::BENCH)
        << "invalid test case name '" << _testCase << "'";
    FATAL_ERROR_EXIT();
  }

  LOG_TOPIC("69091", INFO, arangodb::Logger::BENCH)
      << "Running test case '" << _testCase
      << "': " << benchmark->getDescription();
  if (benchmark->isDeprecated()) {
    LOG_TOPIC("caf8a", WARN, arangodb::Logger::BENCH)
        << "Please note: this test case is deprecated and will be removed in a "
           "future version.";
  }

  if (_duration != 0) {
    _operations = std::numeric_limits<uint64_t>::max();
  } else {
    _realOperations = _operations;
  }
  double const stepSize = (double)_operations / (double)_concurrency;
  uint64_t realStep = static_cast<uint64_t>(stepSize);

  if (stepSize - static_cast<double>(static_cast<uint64_t>(stepSize)) > 0.0) {
    realStep++;
  }

  if (realStep % 1000 != 0) {
    realStep += 1000 - (realStep % 1000);
  }

  // add some more offset so we don't get into trouble with threads of different
  // speed
  realStep += 10000;

  // aggregated stats for all runs
  BenchmarkStats totalStats;

  auto builder = std::make_shared<VPackBuilder>();
  builder->openObject();
  builder->add("histogram", VPackValue(VPackValueType::Object));
  std::vector<std::unique_ptr<BenchmarkThread>> threads;
  bool ok = true;
  std::vector<BenchRunResult> results;
  std::stringstream pp;
  pp << "Interval/Percentile:";
  for (auto percentile : _percentiles) {
    pp << std::fixed << std::right << std::setw(12) << std::setprecision(2)
       << percentile << "%";
  }
  pp << std::endl;

  for (uint64_t j = 0; j < _runs; j++) {
    status("starting threads...");
    double runUntil = 0.0;

    if (_duration != 0) {
      runUntil = TRI_microtime() + _duration;
    }

    BenchmarkCounter<uint64_t> operationsCounter(0, _operations, runUntil);
    ConditionVariable startCondition;

    // start client threads
    _started = 0;

    for (uint64_t i = 0; i < _concurrency; ++i) {
      auto thread = std::make_unique<BenchmarkThread>(
          server(), benchmark.get(), &startCondition,
          &BenchFeature::updateStartCounter, static_cast<int>(i), _batchSize,
          &operationsCounter, client, _keepAlive, _async,
          _histogramIntervalSize, _histogramNumIntervals);
      thread->setOffset(i * realStep);
      thread->start();
      threads.push_back(std::move(thread));
    }

    // give all threads a chance to start so they will not miss the broadcast
    while (getStartCounter() < static_cast<int>(_concurrency)) {
      std::this_thread::sleep_for(std::chrono::milliseconds(5));
    }

    if (_delay) {
      status("sleeping (startup delay)...");
      std::this_thread::sleep_for(std::chrono::seconds(10));
    }

    status("executing tests...");
    double start = TRI_microtime();

    // broadcast the start signal to all threads
    {
      CONDITION_LOCKER(guard, startCondition);
      guard.broadcast();
    }

    uint64_t const stepValue = _operations / 20;
    uint64_t nextReportValue = stepValue;

    if (nextReportValue < 100) {
      nextReportValue = 100;
    }

    while (true) {
      uint64_t const numOperations = operationsCounter.getDone();

      if (numOperations >= _operations) {
        break;
      }

      if (_progress && numOperations >= nextReportValue) {
        LOG_TOPIC("c3604", INFO, arangodb::Logger::BENCH)
            << "number of operations: " << nextReportValue;
        nextReportValue += stepValue;
      }

      std::this_thread::sleep_for(std::chrono::milliseconds(5));
    }

    double time = TRI_microtime() - start;

    // sum up times of all threads
    double requestTime = 0.0;
    for (size_t i = 0; i < static_cast<size_t>(_concurrency); ++i) {
      requestTime += threads[i]->stats().total;
    }

    if (operationsCounter.failures() > 0) {
      ok = false;
    }

    results.push_back({
        time,
        operationsCounter.failures(),
        operationsCounter.incompleteFailures(),
        requestTime,
    });

    for (size_t i = 0; i < static_cast<size_t>(_concurrency); ++i) {
      if (_duration != 0) {
        _realOperations += threads[i]->_counter;
      }

      totalStats.add(threads[i]->stats());

      double scope;
      auto res = threads[i]->getPercentiles(_percentiles, scope);

      builder->add(std::to_string(i), VPackValue(VPackValueType::Object));
      size_t j = 0;

      pp << " " << std::left << std::setfill('0') << std::fixed << std::setw(8)
         << std::setprecision(6) << (threads[i]->_histogramIntervalSize * 1000)
         << std::setw(0) << "ms         ";

      builder->add("IntervalSize",
                   VPackValue(threads[i]->_histogramIntervalSize));

      for (auto time : res) {
        builder->add(std::to_string(_percentiles[j]), VPackValue(time));
        pp << "   " << std::left << std::setfill('0') << std::fixed
           << std::setw(8) << std::setprecision(6) << (time * 1000)
           << std::setw(0) << "ms";
        j++;
      }

      builder->close();
      pp << std::endl;
    }
    threads.clear();
  }

  std::cout << std::endl;
  builder->close();

  report(client, results, totalStats, pp.str(), *builder);

  if (!ok) {
    std::cout << "At least one of the runs produced failures!" << std::endl;
  }

  benchmark->tearDown();

  if (!ok) {
    ret = EXIT_FAILURE;
  }

  *_result = ret;
}

bool BenchFeature::report(ClientFeature& client,
                          std::vector<BenchRunResult> results,
                          BenchmarkStats const& stats,
                          std::string const& histogram, VPackBuilder& builder) {
  std::cout << std::endl;

  std::cout << "Total number of operations: " << _realOperations
            << ", runs: " << _runs
            << ", keep alive: " << (_keepAlive ? "yes" : "no")
            << ", async: " << (_async ? "yes" : "no")
            << ", batch size: " << _batchSize
            << ", replication factor: " << _replicationFactor
            << ", number of shards: " << _numberOfShards
            << ", wait for sync: " << (_waitForSync ? "true" : "false")
            << ", concurrency level (threads): " << _concurrency << std::endl;

  std::cout << "Test case: " << _testCase << ", complexity: " << _complexity
            << ", database: '" << client.databaseName() << "', collection: '"
            << _collection << "'" << std::endl;

  builder.add("totalNumberOfOperations", VPackValue(_realOperations));
  builder.add("runs", VPackValue(_runs));
  builder.add("keepAlive", VPackValue(_keepAlive));
  builder.add("async", VPackValue(_async));
  builder.add("batchSize", VPackValue(_batchSize));
  builder.add("replicationFactor", VPackValue(_replicationFactor));
  builder.add("numberOfShards", VPackValue(_numberOfShards));
  builder.add("waitForSync", VPackValue(_waitForSync));
  builder.add("concurrencyLevel", VPackValue(_concurrency));
  builder.add("testCase", VPackValue(_testCase));
  builder.add("complexity", VPackValue(_complexity));
  builder.add("database", VPackValue(client.databaseName()));
  builder.add("collection", VPackValue(_collection));

  std::sort(
      results.begin(), results.end(),
      [](BenchRunResult a, BenchRunResult b) { return a._time < b._time; });

  BenchRunResult output{0, 0, 0, 0};
  if (_runs > 1) {
    size_t size = results.size();

    std::cout << std::endl;
    std::cout << "Printing fastest result" << std::endl;
    std::cout << "=======================" << std::endl;

    builder.add("fastestResults", VPackValue(VPackValueType::Object));
    printResult(results[0], builder);
    builder.close();

    std::cout << "Printing slowest result" << std::endl;
    std::cout << "=======================" << std::endl;

    builder.add("slowestResults", VPackValue(VPackValueType::Object));
    printResult(results[size - 1], builder);
    builder.close();

    std::cout << "Printing median result" << std::endl;
    std::cout << "=======================" << std::endl;

    size_t mid = (size_t)size / 2;

    if (size % 2 == 0) {
      output.update(
          (results[mid - 1]._time + results[mid]._time) / 2,
          (results[mid - 1]._failures + results[mid]._failures) / 2,
          (results[mid - 1]._incomplete + results[mid]._incomplete) / 2,
          (results[mid - 1]._requestTime + results[mid]._requestTime) / 2);
    } else {
      output = results[mid];
    }
  } else if (_runs > 0) {
    output = results[0];
  }

  builder.add("results", VPackValue(VPackValueType::Object));
  printResult(output, builder);
  builder.close();

  std::cout << "Min request time: " << std::setprecision(6)
            << (stats.min * 1000) << "ms" << std::endl
            << "Avg request time: " << std::setprecision(6)
            << (stats.avg() * 1000) << "ms" << std::endl
            << "Max request time: " << std::setprecision(6)
            << (stats.max * 1000) << "ms" << std::endl
            << std::endl;

  std::cout << histogram;

  builder.add("min", VPackValue(stats.min));
  builder.add("avg", VPackValue(stats.avg()));
  builder.add("max", VPackValue(stats.max));
  builder.close();

  if (!_jsonReportFile.empty()) {
    auto json = builder.toJson();
    TRI_WriteFile(_jsonReportFile.c_str(), json.c_str(), json.length());
  }

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

  std::string hostname = utilities::hostname();
  bool ok = false;
  try {
    outfile << "<?xml version=\"1.0\" encoding=\"UTF-8\"?>" << '\n'
            << "<testsuite name=\"arangobench\" tests=\"1\" skipped=\"0\" "
               "failures=\"0\" errors=\"0\" timestamp=\""
            << date << "\" hostname=\"" << hostname << "\" time=\""
            << std::fixed << result._time << "\">\n"
            << "<properties/>\n"
            << "<testcase name=\"" << testCase() << "\" classname=\"BenchTest\""
            << " time=\"" << std::fixed << result._time << "\"/>\n"
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

void BenchFeature::printResult(BenchRunResult const& result,
                               VPackBuilder& builder) {
  std::cout << "Total request/response duration (sum of all threads): "
            << std::fixed << result._requestTime << " s" << std::endl;
  builder.add("requestTime", VPackValue(result._requestTime));
  std::cout << "Request/response duration (per thread): " << std::fixed
            << (result._requestTime / (double)_concurrency) << " s"
            << std::endl;
  builder.add("requestResponseDurationPerThread",
              VPackValue(result._requestTime / (double)_concurrency));

  std::cout << "Time needed per operation: " << std::fixed
            << (result._time / _realOperations) << " s" << std::endl;
  builder.add("timeNeededPerOperation",
              VPackValue(result._time / _realOperations));

  std::cout << "Time needed per operation per thread: " << std::fixed
            << (result._time / (double)_realOperations * (double)_concurrency)
            << " s" << std::endl;
  builder.add("timeNeededPerOperationPerThread",
              VPackValue(result._time / (double)_realOperations *
                         (double)_concurrency));

  std::cout << "Operations per second rate: " << std::fixed
            << ((double)_realOperations / result._time) << std::endl;
  builder.add("operationsPerSecondRate",
              VPackValue((double)_realOperations / result._time));

  std::cout << "Elapsed time since start: " << std::fixed << result._time
            << " s" << std::endl
            << std::endl;
  builder.add("timeSinceStart", VPackValue(result._time));

  builder.add("failures", VPackValue(result._failures));
  if (result._failures > 0) {
    LOG_TOPIC("a826b", WARN, arangodb::Logger::BENCH)
        << result._failures << " arangobench request(s) failed!";
  }
  builder.add("incompleteResults", VPackValue(result._incomplete));
  if (result._incomplete > 0) {
    LOG_TOPIC("41006", WARN, arangodb::Logger::BENCH)
        << result._incomplete
        << " arangobench requests with incomplete results!";
  }
}
