////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2014-2016 ArangoDB GmbH, Cologne, Germany
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

#include "Basics/Common.h"

#include <iostream>

#include "ArangoShell/ArangoClient.h"
#include "Basics/Logger.h"
#include "Basics/Mutex.h"
#include "Basics/ProgramOptions.h"
#include "Basics/ProgramOptionsDescription.h"
#include "Basics/StringBuffer.h"
#include "Basics/StringUtils.h"
#include "Basics/init.h"
#include "Basics/random.h"
#include "Basics/terminal-utils.h"
#include "Basics/tri-strings.h"
#include "Benchmark/BenchmarkCounter.h"
#include "Benchmark/BenchmarkOperation.h"
#include "Benchmark/BenchmarkThread.h"
#include "Rest/Endpoint.h"
#include "Rest/InitializeRest.h"
#include "SimpleHttpClient/SimpleHttpClient.h"
#include "SimpleHttpClient/SimpleHttpResult.h"

using namespace arangodb;
using namespace arangodb::basics;
using namespace arangodb::httpclient;
using namespace arangodb::rest;
using namespace arangodb::arangob;

////////////////////////////////////////////////////////////////////////////////
/// @brief base class for clients
////////////////////////////////////////////////////////////////////////////////

ArangoClient BaseClient("arangob");

////////////////////////////////////////////////////////////////////////////////
/// @brief started counter
////////////////////////////////////////////////////////////////////////////////

static std::atomic<int> Started;

////////////////////////////////////////////////////////////////////////////////
/// @brief mutex for start counter
////////////////////////////////////////////////////////////////////////////////

Mutex StartMutex;

////////////////////////////////////////////////////////////////////////////////
/// @brief send asynchronous requests
////////////////////////////////////////////////////////////////////////////////

static bool Async = false;

////////////////////////////////////////////////////////////////////////////////
/// @brief number of operations in one batch
////////////////////////////////////////////////////////////////////////////////

static int BatchSize = 0;

////////////////////////////////////////////////////////////////////////////////
/// @brief collection to use
////////////////////////////////////////////////////////////////////////////////

static std::string Collection = "ArangoBenchmark";

////////////////////////////////////////////////////////////////////////////////
/// @brief complexity parameter for tests
////////////////////////////////////////////////////////////////////////////////

static uint64_t Complexity = 1;

////////////////////////////////////////////////////////////////////////////////
/// @brief concurrency
////////////////////////////////////////////////////////////////////////////////

static int ThreadConcurrency = 1;

////////////////////////////////////////////////////////////////////////////////
/// @brief use a startup delay
////////////////////////////////////////////////////////////////////////////////

static bool Delay = false;

////////////////////////////////////////////////////////////////////////////////
/// @brief use HTTP keep-alive
////////////////////////////////////////////////////////////////////////////////

static bool KeepAlive = true;

////////////////////////////////////////////////////////////////////////////////
/// @brief number of operations to perform
////////////////////////////////////////////////////////////////////////////////

static int Operations = 1000;

////////////////////////////////////////////////////////////////////////////////
/// @brief display progress
////////////////////////////////////////////////////////////////////////////////

static bool Progress = true;

////////////////////////////////////////////////////////////////////////////////
/// @brief test case to use
////////////////////////////////////////////////////////////////////////////////

static std::string TestCase = "version";

////////////////////////////////////////////////////////////////////////////////
/// @brief print out replies on error
////////////////////////////////////////////////////////////////////////////////

static bool verbose = false;

////////////////////////////////////////////////////////////////////////////////
/// @brief includes all the test cases
////////////////////////////////////////////////////////////////////////////////

#include "Benchmark/test-cases.h"

////////////////////////////////////////////////////////////////////////////////
/// @brief update the number of ready threads. this is a callback function
/// that is called by each thread after it is created
////////////////////////////////////////////////////////////////////////////////

static void UpdateStartCounter() { ++Started; }

////////////////////////////////////////////////////////////////////////////////
/// @brief get the value of the number of started threads counter
////////////////////////////////////////////////////////////////////////////////

static int GetStartCounter() { return Started; }

////////////////////////////////////////////////////////////////////////////////
/// @brief print a status line (if ! quiet)
////////////////////////////////////////////////////////////////////////////////

static void Status(std::string const& value) {
  if (!BaseClient.quiet()) {
    std::cout << value << std::endl;
  }
}

////////////////////////////////////////////////////////////////////////////////
/// @brief parses the program options
////////////////////////////////////////////////////////////////////////////////

static void ParseProgramOptions(int argc, char* argv[]) {
  ProgramOptionsDescription description("STANDARD options");

  description("async", &Async, "send asynchronous requests")(
      "concurrency", &ThreadConcurrency, "number of parallel connections")(
      "requests", &Operations, "total number of operations")(
      "batch-size", &BatchSize,
      "number of operations in one batch (0 disables batching)")(
      "keep-alive", &KeepAlive, "use HTTP keep-alive")(
      "collection", &Collection, "collection name to use in tests")(
      "test-case", &TestCase,
      "test case to use (possible values: version, document, collection, "
      "import-document, hash, skiplist, edge, shapes, shapes-append, "
      "random-shapes, crud, crud-append, crud-write-read, aqltrx, counttrx, "
      "multitrx, multi-collection, aqlinsert, aqlv8)")(
      "complexity", &Complexity, "complexity parameter for the test")(
      "delay", &Delay,
      "use a startup delay (necessary only when run in series)")(
      "progress", &Progress, "show progress")(
      "verbose", &verbose,
      "print out replies if the http-header indicates db-errors");

  BaseClient.setupGeneral(description);
  BaseClient.setupServer(description);

  std::vector<std::string> arguments;
  description.arguments(&arguments);

  ProgramOptions options;
  BaseClient.parse(
      options, description,
      "--concurrency <concurrency> --requests <request> --test-case <case> ...",
      argc, argv, "arangob.conf");
}

////////////////////////////////////////////////////////////////////////////////
/// @brief startup and exit functions
////////////////////////////////////////////////////////////////////////////////

static void arangobEntryFunction();
static void arangobExitFunction(int, void*);

#ifdef _WIN32

// .............................................................................
// Call this function to do various initializations for windows only
// .............................................................................

void arangobEntryFunction() {
  int maxOpenFiles = 1024;
  int res = 0;

  // ...........................................................................
  // Uncomment this to call this for extended debug information.
  // If you familiar with valgrind ... then this is not like that, however
  // you do get some similar functionality.
  // ...........................................................................
  // res = initializeWindows(TRI_WIN_INITIAL_SET_DEBUG_FLAG, 0);

  res = initializeWindows(TRI_WIN_INITIAL_SET_INVALID_HANLE_HANDLER, 0);
  if (res != 0) {
    _exit(1);
  }

  res = initializeWindows(TRI_WIN_INITIAL_SET_MAX_STD_IO,
                          (char const*)(&maxOpenFiles));
  if (res != 0) {
    _exit(1);
  }

  res = initializeWindows(TRI_WIN_INITIAL_WSASTARTUP_FUNCTION_CALL, 0);
  if (res != 0) {
    _exit(1);
  }

  TRI_Application_Exit_SetExit(arangobExitFunction);
}

static void arangobExitFunction(int exitCode, void* data) {
  int res = finalizeWindows(TRI_WIN_FINAL_WSASTARTUP_FUNCTION_CALL, 0);

  if (res != 0) {
    exit(1);
  }

  exit(exitCode);
}
#else

static void arangobEntryFunction() {}

static void arangobExitFunction(int exitCode, void* data) {}

#endif

////////////////////////////////////////////////////////////////////////////////
/// @brief main
////////////////////////////////////////////////////////////////////////////////

int main(int argc, char* argv[]) {
  int ret = EXIT_SUCCESS;

  arangobEntryFunction();

  TRIAGENS_C_INITIALIZE(argc, argv);
  TRIAGENS_REST_INITIALIZE(argc, argv);

  Logger::initialize(false);

  BaseClient.setEndpointString(Endpoint::getDefaultEndpoint());

  // .............................................................................
  // parse the program options
  // .............................................................................

  ParseProgramOptions(argc, argv);

  // .............................................................................
  // set-up client connection
  // .............................................................................

  BaseClient.createEndpoint();

  if (BaseClient.endpointServer() == nullptr) {
    std::string endpointString = BaseClient.endpointString();
    LOG(FATAL) << "invalid value for --server.endpoint ('" << endpointString.c_str() << "')"; FATAL_ERROR_EXIT();
  }

  BenchmarkOperation* testCase = GetTestCase(TestCase);

  if (testCase == nullptr) {
    LOG(FATAL) << "invalid test case name '" << TestCase.c_str() << "'"; FATAL_ERROR_EXIT();
    return EXIT_FAILURE;  // will not be reached
  }

  Status("starting threads...");

  BenchmarkCounter<unsigned long> operationsCounter(0,
                                                    (unsigned long)Operations);
  ConditionVariable startCondition;

  std::vector<Endpoint*> endpoints;
  std::vector<BenchmarkThread*> threads;

  double const stepSize = (double)Operations / (double)ThreadConcurrency;
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
  for (int i = 0; i < ThreadConcurrency; ++i) {
    Endpoint* endpoint = Endpoint::clientFactory(BaseClient.endpointString());
    endpoints.push_back(endpoint);

    BenchmarkThread* thread = new BenchmarkThread(
        testCase, &startCondition, &UpdateStartCounter, i,
        (unsigned long)BatchSize, &operationsCounter, endpoint,
        BaseClient.databaseName(), BaseClient.username(), BaseClient.password(),
        BaseClient.requestTimeout(), BaseClient.connectTimeout(),
        BaseClient.sslProtocol(), KeepAlive, Async, verbose);

    threads.push_back(thread);
    thread->setOffset((size_t)(i * realStep));
    thread->start();
  }

  // give all threads a chance to start so they will not miss the broadcast
  while (GetStartCounter() < ThreadConcurrency) {
    usleep(5000);
  }

  if (Delay) {
    Status("sleeping (startup delay)...");
    sleep(10);
  }
  Status("executing tests...");

  double start = TRI_microtime();

  // broadcast the start signal to all threads
  {
    CONDITION_LOCKER(guard, startCondition);
    guard.broadcast();
  }

  size_t const stepValue = (Operations / 20);
  size_t nextReportValue = stepValue;

  if (nextReportValue < 100) {
    nextReportValue = 100;
  }

  while (1) {
    size_t const numOperations = operationsCounter.getDone();

    if (numOperations >= (size_t)Operations) {
      break;
    }

    if (Progress && numOperations >= nextReportValue) {
      LOG(INFO) << "number of operations: " << nextReportValue;
      nextReportValue += stepValue;
    }

    usleep(20000);
  }

  double time = TRI_microtime() - start;
  double requestTime = 0.0;

  for (int i = 0; i < ThreadConcurrency; ++i) {
    requestTime += threads[i]->getTime();
  }

  size_t failures = operationsCounter.failures();
  size_t incomplete = operationsCounter.incompleteFailures();

  std::cout << std::endl;
  std::cout << "Total number of operations: " << Operations
            << ", keep alive: " << (KeepAlive ? "yes" : "no")
            << ", async: " << (Async ? "yes" : "no")
            << ", batch size: " << BatchSize
            << ", concurrency level (threads): " << ThreadConcurrency
            << std::endl;

  std::cout << "Test case: " << TestCase << ", complexity: " << Complexity
            << ", database: '" << BaseClient.databaseName()
            << "', collection: '" << Collection << "'" << std::endl;

  std::cout << "Total request/response duration (sum of all threads): "
            << std::fixed << requestTime << " s" << std::endl;
  std::cout << "Request/response duration (per thread): " << std::fixed
            << (requestTime / (double)ThreadConcurrency) << " s" << std::endl;
  std::cout << "Time needed per operation: " << std::fixed
            << (time / Operations) << " s" << std::endl;
  std::cout << "Time needed per operation per thread: " << std::fixed
            << (time / (double)Operations * (double)ThreadConcurrency) << " s"
            << std::endl;
  std::cout << "Operations per second rate: " << std::fixed
            << ((double)Operations / time) << std::endl;
  std::cout << "Elapsed time since start: " << std::fixed << time << " s"
            << std::endl
            << std::endl;

  if (failures > 0) {
    std::cerr << "WARNING: " << failures << " arangob request(s) failed!!"
              << std::endl;
  }
  if (incomplete > 0) {
    std::cerr << "WARNING: " << incomplete
              << " arangob requests with incomplete results!!" << std::endl;
  }

  testCase->tearDown();

  for (int i = 0; i < ThreadConcurrency; ++i) {
    threads[i]->join();
    delete threads[i];
    delete endpoints[i];
  }

  delete testCase;

  TRIAGENS_REST_SHUTDOWN;

  if (failures > 0) {
    ret = EXIT_FAILURE;
  }

  arangobExitFunction(ret, nullptr);

  return ret;
}
