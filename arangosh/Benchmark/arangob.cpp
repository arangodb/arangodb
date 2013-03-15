////////////////////////////////////////////////////////////////////////////////
/// @brief arango benchmark tool
///
/// @file
///
/// DISCLAIMER
///
/// Copyright 2004-2013 triAGENS GmbH, Cologne, Germany
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
/// Copyright holder is triAGENS GmbH, Cologne, Germany
///
/// @author Dr. Frank Celler
/// @author Copyright 2011-2013, triAGENS GmbH, Cologne, Germany
////////////////////////////////////////////////////////////////////////////////

#include <stdio.h>
#include <iomanip>

#include "build.h"

#include "ArangoShell/ArangoClient.h"
#include "Basics/Mutex.h"
#include "Basics/MutexLocker.h"
#include "Basics/ProgramOptions.h"
#include "Basics/ProgramOptionsDescription.h"
#include "Basics/StringUtils.h"
#include "BasicsC/init.h"
#include "BasicsC/logging.h"
#include "BasicsC/strings.h"
#include "BasicsC/string-buffer.h"
#include "BasicsC/terminal-utils.h"
#include "Logger/Logger.h"
#include "Rest/Endpoint.h"
#include "Rest/HttpRequest.h"
#include "Rest/InitialiseRest.h"
#include "SimpleHttpClient/SimpleHttpClient.h"
#include "SimpleHttpClient/SimpleHttpResult.h"
#include "Benchmark/BenchmarkCounter.h"
#include "Benchmark/BenchmarkOperation.h"
#include "Benchmark/BenchmarkThread.h"

using namespace std;
using namespace triagens::basics;
using namespace triagens::httpclient;
using namespace triagens::rest;
using namespace triagens::arango;
using namespace triagens::arangob;

// -----------------------------------------------------------------------------
// --SECTION--                                                 private variables
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @addtogroup Benchmark
/// @{
////////////////////////////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////////////////////////
/// @brief base class for clients
////////////////////////////////////////////////////////////////////////////////

ArangoClient BaseClient;

////////////////////////////////////////////////////////////////////////////////
/// @brief started counter
////////////////////////////////////////////////////////////////////////////////

static volatile int Started = 0;

////////////////////////////////////////////////////////////////////////////////
/// @brief mutex for start counter
////////////////////////////////////////////////////////////////////////////////

Mutex StartMutex;

////////////////////////////////////////////////////////////////////////////////
/// @brief use a startup delay
////////////////////////////////////////////////////////////////////////////////

static bool Delay = false;

////////////////////////////////////////////////////////////////////////////////
/// @brief concurrency
////////////////////////////////////////////////////////////////////////////////

static int Concurrency = 1;

////////////////////////////////////////////////////////////////////////////////
/// @brief number of operations to perform
////////////////////////////////////////////////////////////////////////////////

static int Operations = 1000;

////////////////////////////////////////////////////////////////////////////////
/// @brief number of operations in one batch
////////////////////////////////////////////////////////////////////////////////

static int BatchSize = 0;

////////////////////////////////////////////////////////////////////////////////
/// @brief complexity parameter for tests
////////////////////////////////////////////////////////////////////////////////

static uint64_t Complexity = 1;

////////////////////////////////////////////////////////////////////////////////
/// @brief collection to use
////////////////////////////////////////////////////////////////////////////////

static string Collection = "ArangoBenchmark";

////////////////////////////////////////////////////////////////////////////////
/// @brief test case to use
////////////////////////////////////////////////////////////////////////////////

static string TestCase = "version";

////////////////////////////////////////////////////////////////////////////////
/// @brief display progress
////////////////////////////////////////////////////////////////////////////////

static bool Progress = false;

////////////////////////////////////////////////////////////////////////////////
/// @}
////////////////////////////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////////////////////////
/// @brief includes all the test cases
////////////////////////////////////////////////////////////////////////////////

#include "Benchmark/test-cases.h"

// -----------------------------------------------------------------------------
// --SECTION--                                                 private functions
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @addtogroup Benchmark
/// @{
////////////////////////////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////////////////////////
/// @brief update the number of ready threads. this is a callback function
/// that is called by each thread after it is created
////////////////////////////////////////////////////////////////////////////////

static void UpdateStartCounter () {
  MUTEX_LOCKER(StartMutex);
  ++Started;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief get the value of the number of started threads counter
////////////////////////////////////////////////////////////////////////////////

static int GetStartCounter () {
  MUTEX_LOCKER(StartMutex);
  return Started;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief print a status line (if ! quiet)
////////////////////////////////////////////////////////////////////////////////

static void Status (const string& value) {
  if (! BaseClient.quiet()) {
    cout << value << endl;
  }
}

////////////////////////////////////////////////////////////////////////////////
/// @brief parses the program options
////////////////////////////////////////////////////////////////////////////////

static void ParseProgramOptions (int argc, char* argv[]) {
  ProgramOptionsDescription description("STANDARD options");

  description
    ("concurrency", &Concurrency, "number of parallel connections")
    ("requests", &Operations, "total number of operations")
    ("batch-size", &BatchSize, "number of operations in one batch (0 disables batching")
    ("collection", &Collection, "collection name to use in tests")
    ("test-case", &TestCase, "test case to use")
    ("complexity", &Complexity, "complexity parameter for the test")
    ("delay", &Delay, "use a startup delay (necessary only when run in series)")
    ("progress", &Progress, "show progress")
  ;

  BaseClient.setupGeneral(description);
  BaseClient.setupServer(description);

  vector<string> arguments;
  description.arguments(&arguments);

  ProgramOptions options;
  BaseClient.parse(options, description, argc, argv, "arangob.conf");
}

////////////////////////////////////////////////////////////////////////////////
/// @}
////////////////////////////////////////////////////////////////////////////////

// -----------------------------------------------------------------------------
// --SECTION--                                                  public functions
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @addtogroup arangoimp
/// @{
////////////////////////////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////////////////////////
/// @brief main
////////////////////////////////////////////////////////////////////////////////

int main (int argc, char* argv[]) {
  TRIAGENS_C_INITIALISE(argc, argv);
  TRIAGENS_REST_INITIALISE(argc, argv);

  TRI_InitialiseLogging(false);

  BaseClient.setEndpointString(Endpoint::getDefaultEndpoint());

  // .............................................................................
  // parse the program options
  // .............................................................................

  ParseProgramOptions(argc, argv);

  // .............................................................................
  // set-up client connection
  // .............................................................................

  BaseClient.createEndpoint();

  if (BaseClient.endpointServer() == 0) {
    LOGGER_FATAL_AND_EXIT("invalid value for --server.endpoint ('" << BaseClient.endpointString() << "')");
  }

  BenchmarkOperation* testCase = GetTestCase(TestCase);

  if (testCase == 0) {
    LOGGER_FATAL_AND_EXIT("invalid test case name " << TestCase);
    return EXIT_FAILURE; // will not be reached
  }

  Status("starting threads...");

  BenchmarkCounter<unsigned long> operationsCounter(0, (unsigned long) Operations);
  ConditionVariable startCondition;


  vector<Endpoint*> endpoints;
  vector<BenchmarkThread*> threads;

  const double stepSize = (double) Operations / (double) Concurrency;
  int64_t realStep = (int64_t) stepSize;
  if (stepSize - (double) ((int64_t) stepSize) > 0.0) {
    realStep++;
  }
  if (realStep % 1000 != 0) {
    realStep += 1000 - (realStep % 1000);
  }
  // add some more offset we don't get into trouble with threads of different speed
  realStep += 10000;

  // start client threads
  for (int i = 0; i < Concurrency; ++i) {
    Endpoint* endpoint = Endpoint::clientFactory(BaseClient.endpointString());
    endpoints.push_back(endpoint);

    BenchmarkThread* thread = new BenchmarkThread(testCase,
        &startCondition,
        &UpdateStartCounter,
        i,
        (unsigned long) BatchSize,
        &operationsCounter,
        endpoint,
        BaseClient.username(),
        BaseClient.password(),
        BaseClient.requestTimeout(),
        BaseClient.connectTimeout());

    threads.push_back(thread);
    thread->setOffset(i * realStep);
    thread->start();
  }

  // give all threads a chance to start so they will not miss the broadcast
  while (GetStartCounter() < Concurrency) {
    usleep(5000);
  }

  if (Delay) {
    Status("sleeping (startup delay)...");
    sleep(15);
  }
  Status("executing tests...");

  Timing timer(Timing::TI_WALLCLOCK);

  // broadcast the start signal to all threads
  {
    ConditionLocker guard(&startCondition);
    guard.broadcast();
  }

  const size_t stepValue = (Operations / 20);
  size_t lastReportValue = stepValue;

  while (1) {
    size_t numOperations = operationsCounter.getValue();

    if (numOperations >= (size_t) Operations) {
      break;
    }

    if (Progress && numOperations > lastReportValue) {
      LOGGER_INFO("number of operations: " << numOperations);
      lastReportValue = numOperations + stepValue;
    }

    usleep(50000);
  }

  double time = ((double) timer.time()) / 1000000.0;
  double requestTime = 0.0;

  for (int i = 0; i < Concurrency; ++i) {
    requestTime += threads[i]->getTime();
  }

  size_t failures = operationsCounter.failures();

  if (! BaseClient.quiet()) {
    cout << endl;
    cout << "Total number of operations: " << Operations << ", batch size: " << BatchSize << ", concurrency level (threads): " << Concurrency << endl;
    cout << "Total request/response duration (sum of all threads): " << fixed << requestTime << " s" << endl;
    cout << "Request/response duration (per thread): " << fixed << (requestTime / (double) Concurrency) << " s" << endl;
    cout << "Time needed per operation: " << fixed << (time / Operations) << " s" << endl;
    cout << "Time needed per operation per thread: " << fixed << (time / (double) Operations * (double) Concurrency) << " s" << endl;
    cout << "Operations per second rate: " << fixed << ((double) Operations / time) << endl;
    cout << "Elapsed time since start: " << fixed << time << " s" << endl;

    cout << endl;

    if (failures > 0) {
      cerr << "WARNING: " << failures << " request(s) failed!!" << endl << endl;
    }
  }
  else {
    if (failures > 0) {
      cerr << "WARNING: " << failures << " arangob request(s) failed!!" << endl;
    }
  }

  testCase->tearDown();

  for (int i = 0; i < Concurrency; ++i) {
    threads[i]->join();
    delete threads[i];
    delete endpoints[i];
  }

  delete testCase;

  TRIAGENS_REST_SHUTDOWN;

  return (failures == 0) ? EXIT_SUCCESS : 2;
}

////////////////////////////////////////////////////////////////////////////////
/// @}
////////////////////////////////////////////////////////////////////////////////

// -----------------------------------------------------------------------------
// --SECTION--                                                       END-OF-FILE
// -----------------------------------------------------------------------------

// Local Variables:
// mode: outline-minor
// outline-regexp: "/// @brief\\|/// {@inheritDoc}\\|/// @addtogroup\\|/// @page\\|// --SECTION--\\|/// @\\}"
// End:
