////////////////////////////////////////////////////////////////////////////////
/// @brief arango benchmark tool
///
/// @file
///
/// DISCLAIMER
///
/// Copyright 2014 ArangoDB GmbH, Cologne, Germany
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
/// @author Copyright 2014, ArangoDB GmbH, Cologne, Germany
/// @author Copyright 2011-2013, triAGENS GmbH, Cologne, Germany
////////////////////////////////////////////////////////////////////////////////

#include "Basics/Common.h"

#include "ArangoShell/ArangoClient.h"
#include "Basics/Mutex.h"
#include "Basics/MutexLocker.h"
#include "Basics/ProgramOptions.h"
#include "Basics/ProgramOptionsDescription.h"
#include "Basics/StringUtils.h"
#include "Basics/init.h"
#include "BasicsC/logging.h"
#include "BasicsC/random.h"
#include "BasicsC/tri-strings.h"
#include "BasicsC/string-buffer.h"
#include "BasicsC/terminal-utils.h"
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
/// @brief base class for clients
////////////////////////////////////////////////////////////////////////////////

ArangoClient BaseClient;

////////////////////////////////////////////////////////////////////////////////
/// @brief started counter
////////////////////////////////////////////////////////////////////////////////

static atomic<int> Started;

////////////////////////////////////////////////////////////////////////////////
/// @brief mutex for start counter
////////////////////////////////////////////////////////////////////////////////

Mutex StartMutex;

////////////////////////////////////////////////////////////////////////////////
/// @brief send asychronous requests
////////////////////////////////////////////////////////////////////////////////

static bool Async = false;

////////////////////////////////////////////////////////////////////////////////
/// @brief number of operations in one batch
////////////////////////////////////////////////////////////////////////////////

static int BatchSize = 0;

////////////////////////////////////////////////////////////////////////////////
/// @brief collection to use
////////////////////////////////////////////////////////////////////////////////

static string Collection = "ArangoBenchmark";

////////////////////////////////////////////////////////////////////////////////
/// @brief complexity parameter for tests
////////////////////////////////////////////////////////////////////////////////

static uint64_t Complexity = 1;

////////////////////////////////////////////////////////////////////////////////
/// @brief concurrency
////////////////////////////////////////////////////////////////////////////////

static int Concurrency = 1;

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

static string TestCase = "version";

////////////////////////////////////////////////////////////////////////////////
/// @brief includes all the test cases
////////////////////////////////////////////////////////////////////////////////

#include "Benchmark/test-cases.h"

// -----------------------------------------------------------------------------
// --SECTION--                                                 private functions
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @brief update the number of ready threads. this is a callback function
/// that is called by each thread after it is created
////////////////////////////////////////////////////////////////////////////////

static void UpdateStartCounter () {
  ++Started;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief get the value of the number of started threads counter
////////////////////////////////////////////////////////////////////////////////

static int GetStartCounter () {
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
    ("async", &Async, "send asychronous requests")
    ("concurrency", &Concurrency, "number of parallel connections")
    ("requests", &Operations, "total number of operations")
    ("batch-size", &BatchSize, "number of operations in one batch (0 disables batching)")
    ("keep-alive", &KeepAlive, "use HTTP keep-alive")
    ("collection", &Collection, "collection name to use in tests")
    ("test-case", &TestCase, "test case to use (possible values: version, document, collection, import-document, hash, skiplist, edge, shapes, shapes-append, random-shapes, crud, crud-append, counttrx, multitrx, aqltrx, aqlinsert)")
    ("complexity", &Complexity, "complexity parameter for the test")
    ("delay", &Delay, "use a startup delay (necessary only when run in series)")
    ("progress", &Progress, "show progress")
  ;

  BaseClient.setupGeneral(description);
  BaseClient.setupServer(description);

  vector<string> arguments;
  description.arguments(&arguments);

  ProgramOptions options;
  BaseClient.parse(options, description, "--concurrency <concurrency> --requests <request> --test-case <case> ...", argc, argv, "arangob.conf");
}

// -----------------------------------------------------------------------------
// --SECTION--                                                  public functions
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @brief startup and exit functions
////////////////////////////////////////////////////////////////////////////////

static void arangobEntryFunction ();
static void arangobExitFunction (int, void*);

#ifdef _WIN32

// .............................................................................
// Call this function to do various initialisations for windows only
// .............................................................................

void arangobEntryFunction () {
  int maxOpenFiles = 1024;
  int res = 0;

  // ...........................................................................
  // Uncomment this to call this for extended debug information.
  // If you familiar with valgrind ... then this is not like that, however
  // you do get some similar functionality.
  // ...........................................................................
  //res = initialiseWindows(TRI_WIN_INITIAL_SET_DEBUG_FLAG, 0);

  res = initialiseWindows(TRI_WIN_INITIAL_SET_INVALID_HANLE_HANDLER, 0);
  if (res != 0) {
    _exit(1);
  }

  res = initialiseWindows(TRI_WIN_INITIAL_SET_MAX_STD_IO,(const char*)(&maxOpenFiles));
  if (res != 0) {
    _exit(1);
  }

  res = initialiseWindows(TRI_WIN_INITIAL_WSASTARTUP_FUNCTION_CALL, 0);
  if (res != 0) {
    _exit(1);
  }

  TRI_Application_Exit_SetExit(arangobExitFunction);
}

static void arangobExitFunction (int exitCode, void* data) {
  int res = 0;
  // ...........................................................................
  // TODO: need a terminate function for windows to be called and cleanup
  // any windows specific stuff.
  // ...........................................................................

  res = finaliseWindows(TRI_WIN_FINAL_WSASTARTUP_FUNCTION_CALL, 0);

  if (res != 0) {
    exit(1);
  }

  exit(exitCode);
}
#else

static void arangobEntryFunction () {
}

static void arangobExitFunction (int exitCode, void* data) {
}

#endif

////////////////////////////////////////////////////////////////////////////////
/// @brief main
////////////////////////////////////////////////////////////////////////////////

int main (int argc, char* argv[]) {
  int ret = EXIT_SUCCESS;

  arangobEntryFunction();

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
    LOG_FATAL_AND_EXIT("invalid value for --server.endpoint ('%s')", BaseClient.endpointString().c_str());
  }

  BenchmarkOperation* testCase = GetTestCase(TestCase);

  if (testCase == 0) {
    LOG_FATAL_AND_EXIT("invalid test case name '%s'", TestCase.c_str());
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
  // add some more offset so we don't get into trouble with threads of different speed
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
        BaseClient.databaseName(),
        BaseClient.username(),
        BaseClient.password(),
        BaseClient.requestTimeout(),
        BaseClient.connectTimeout(),
        BaseClient.sslProtocol(),
        KeepAlive,
        Async);

    threads.push_back(thread);
    thread->setOffset((size_t) (i * realStep));
    thread->start();
  }

  // give all threads a chance to start so they will not miss the broadcast
  while (GetStartCounter() < Concurrency) {
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
    ConditionLocker guard(&startCondition);
    guard.broadcast();
  }

  const size_t stepValue = (Operations / 20);
  size_t nextReportValue = stepValue;

  if (nextReportValue < 100) {
    nextReportValue = 100;
  }

  while (1) {
    const size_t numOperations = operationsCounter.getValue();

    if (numOperations >= (size_t) Operations) {
      break;
    }

    if (Progress && numOperations >= nextReportValue) {
      LOG_INFO("number of operations: %d", (int) nextReportValue);
      nextReportValue += stepValue;
    }

    usleep(20000);
  }

  double time = TRI_microtime() - start;
  double requestTime = 0.0;

  for (int i = 0; i < Concurrency; ++i) {
    requestTime += threads[i]->getTime();
  }

  size_t failures = operationsCounter.failures();

  cout << endl;
  cout << "Total number of operations: " << Operations <<
          ", keep alive: " << (KeepAlive ? "yes" : "no") <<
          ", async: " << (Async ? "yes" : "no")  <<
          ", batch size: " << BatchSize <<
          ", concurrency level (threads): " << Concurrency <<
          endl;

  cout << "Test case: " << TestCase <<
          ", complexity: " << Complexity <<
          ", database: '" << BaseClient.databaseName() <<
          "', collection: '" << Collection << "'" <<
          endl;

  cout << "Total request/response duration (sum of all threads): " << fixed << requestTime << " s" << endl;
  cout << "Request/response duration (per thread): " << fixed << (requestTime / (double) Concurrency) << " s" << endl;
  cout << "Time needed per operation: " << fixed << (time / Operations) << " s" << endl;
  cout << "Time needed per operation per thread: " << fixed << (time / (double) Operations * (double) Concurrency) << " s" << endl;
  cout << "Operations per second rate: " << fixed << ((double) Operations / time) << endl;
  cout << "Elapsed time since start: " << fixed << time << " s" << endl << endl;

  if (failures > 0) {
    cerr << "WARNING: " << failures << " arangob request(s) failed!!" << endl;
  }

  testCase->tearDown();

  for (int i = 0; i < Concurrency; ++i) {
    threads[i]->join();
    delete threads[i];
    delete endpoints[i];
  }

  delete testCase;

  TRIAGENS_REST_SHUTDOWN;

  if (failures > 0) {
    ret = EXIT_FAILURE;
  }

  arangobExitFunction(ret, NULL);

  return ret;
}

// -----------------------------------------------------------------------------
// --SECTION--                                                       END-OF-FILE
// -----------------------------------------------------------------------------

// Local Variables:
// mode: outline-minor
// outline-regexp: "/// @brief\\|/// {@inheritDoc}\\|/// @page\\|// --SECTION--\\|/// @\\}"
// End:
