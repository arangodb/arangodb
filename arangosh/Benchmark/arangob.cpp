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
/// @addtogroup V8Shell
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
/// @}
////////////////////////////////////////////////////////////////////////////////

// -----------------------------------------------------------------------------
// --SECTION--                                              benchmark test cases
// -----------------------------------------------------------------------------

// -----------------------------------------------------------------------------
// --SECTION--                                            version retrieval test
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @addtogroup V8Shell
/// @{
////////////////////////////////////////////////////////////////////////////////

struct VersionTest : public BenchmarkOperation {
  VersionTest ()
    : BenchmarkOperation () {
  }

  ~VersionTest () {
  }

  string collectionName () {
    return "";
  }

  const bool useCollection () const {
    return false;
  }

  const string& url () {
    static string url = "/_api/version";

    return url;
  }

  const HttpRequest::HttpRequestType type () {
    return HttpRequest::HTTP_REQUEST_GET;
  }

  const char* payload (size_t* length, const size_t counter, bool* mustFree) {
    static const char* payload = "";

    *mustFree = false;
    *length = 0;
    return payload;
  }

  const map<string, string>& headers () {
    static const map<string, string> headers;
    return headers;
  }
};

////////////////////////////////////////////////////////////////////////////////
/// @}
////////////////////////////////////////////////////////////////////////////////

// -----------------------------------------------------------------------------
// --SECTION--                                            document creation test
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @addtogroup V8Shell
/// @{
////////////////////////////////////////////////////////////////////////////////

struct DocumentCreationTest : public BenchmarkOperation {
  DocumentCreationTest ()
    : BenchmarkOperation (),
      _url(),
      _buffer(0) {
    _url = "/_api/document?collection=" + Collection + "&createCollection=true";

    const size_t n = Complexity;

    _buffer = TRI_CreateSizedStringBuffer(TRI_UNKNOWN_MEM_ZONE, 4096);
    TRI_AppendCharStringBuffer(_buffer, '{');

    for (size_t i = 1; i <= n; ++i) {
      TRI_AppendStringStringBuffer(_buffer, "\"test");
      TRI_AppendUInt32StringBuffer(_buffer, (uint32_t) i);
      TRI_AppendStringStringBuffer(_buffer, "\":\"some test value\"");
      if (i != n) {
        TRI_AppendCharStringBuffer(_buffer, ',');
      }
    }

    TRI_AppendCharStringBuffer(_buffer, '}');

    _length = TRI_LengthStringBuffer(_buffer);
  }

  ~DocumentCreationTest () {
    TRI_FreeStringBuffer(TRI_UNKNOWN_MEM_ZONE, _buffer);
  }

  string collectionName () {
    return Collection;
  }

  const bool useCollection () const {
    return true;
  }

  const string& url () {
    return _url;
  }

  const HttpRequest::HttpRequestType type () {
    return HttpRequest::HTTP_REQUEST_POST;
  }

  const char* payload (size_t* length, const size_t counter, bool* mustFree) {
    *mustFree = false;
    *length = _length;
    return (const char*) _buffer->_buffer;
  }

  const map<string, string>& headers () {
    static const map<string, string> headers;
    return headers;
  }

  string _url;

  TRI_string_buffer_t* _buffer;

  size_t _length;
};

////////////////////////////////////////////////////////////////////////////////
/// @}
////////////////////////////////////////////////////////////////////////////////

// -----------------------------------------------------------------------------
// --SECTION--                                            document creation test
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @addtogroup V8Shell
/// @{
////////////////////////////////////////////////////////////////////////////////

struct CollectionCreationTest : public BenchmarkOperation {
  CollectionCreationTest ()
    : BenchmarkOperation (),
      _url() {
    _url = "/_api/collection";

  }

  ~CollectionCreationTest () {
  }

  BenchmarkCounter<uint64_t>* getSharedCounter () {
    if (_counter == 0) {
      _counter = new BenchmarkCounter<uint64_t>(0, 1024 * 1024);
    }

    return _counter;
  }

  string collectionName () {
    return "";
  }

  const bool useCollection () const {
    return false;
  }

  const string& url () {
    return _url;
  }

  const HttpRequest::HttpRequestType type () {
    return HttpRequest::HTTP_REQUEST_POST;
  }

  const char* payload (size_t* length, const size_t counter, bool* mustFree) {
    BenchmarkCounter<uint64_t>* ctr = getSharedCounter();
    TRI_string_buffer_t* buffer;
    char* data;

    ctr->next(1);
    buffer = TRI_CreateSizedStringBuffer(TRI_UNKNOWN_MEM_ZONE, 64);
    if (buffer == 0) {
      return 0;
    }
    TRI_AppendStringStringBuffer(buffer, "{\"name\":\"");
    TRI_AppendStringStringBuffer(buffer, Collection.c_str());
    TRI_AppendUInt64StringBuffer(buffer, ctr->getValue());
    TRI_AppendStringStringBuffer(buffer, "\"}");

    *length = TRI_LengthStringBuffer(buffer);

    // this will free the string buffer frame, but not the string
    data = buffer->_buffer;
    TRI_Free(TRI_UNKNOWN_MEM_ZONE, buffer);

    *mustFree = true;
    return (const char*) data;
  }

  const map<string, string>& headers () {
    static const map<string, string> headers;
    return headers;
  }

  static BenchmarkCounter<uint64_t>* _counter;

  string _url;
};

BenchmarkCounter<uint64_t>* CollectionCreationTest::_counter = 0;

////////////////////////////////////////////////////////////////////////////////
/// @}
////////////////////////////////////////////////////////////////////////////////


// -----------------------------------------------------------------------------
// --SECTION--                                                 private functions
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @addtogroup V8Shell
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

  BenchmarkOperation* testCase;

  if (TestCase == "version") {
    testCase = new VersionTest();
  }
  else if (TestCase == "document") {
    testCase = new DocumentCreationTest();
  }
  else if (TestCase == "collection") {
    testCase = new CollectionCreationTest();
  }
  else {
    LOGGER_FATAL_AND_EXIT("invalid test case name " << TestCase);
    return EXIT_FAILURE; // will not be reached
  }

  Status("starting threads...");

  BenchmarkCounter<unsigned long> operationsCounter(0, (unsigned long) Operations);
  ConditionVariable startCondition;


  vector<Endpoint*> endpoints;
  vector<BenchmarkThread*> threads;

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
        BaseClient.password());

    threads.push_back(thread);
    thread->setOffset(i * (Operations / Concurrency));
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

  while (1) {
    size_t numOperations = operationsCounter.getValue();

    if (numOperations >= (size_t) Operations) {
      break;
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
