////////////////////////////////////////////////////////////////////////////////
/// @brief arango benchmark tool
///
/// @file
///
/// DISCLAIMER
///
/// Copyright 2004-2012 triagens GmbH, Cologne, Germany
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
/// @author Copyright 2011-2012, triAGENS GmbH, Cologne, Germany
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
#include "Rest/Initialise.h"
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
  
  const char* payload (size_t* length, const size_t counter) {
    static const char* payload = "";
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
// --SECTION--                                      small document creation test
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @addtogroup V8Shell
/// @{
////////////////////////////////////////////////////////////////////////////////

struct SmallDocumentCreationTest : public BenchmarkOperation {
  SmallDocumentCreationTest ()
    : BenchmarkOperation(),
      _url() {
    _url = "/_api/document?collection=" + Collection + "&createCollection=true";
  }

  ~SmallDocumentCreationTest () {
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
  
  const char* payload (size_t* length, const size_t counter) {
    static const char* payload = "{\"test\":1}";
    *length = 10;
    return payload;
  }
  
  const map<string, string>& headers () {
    static const map<string, string> headers;
    return headers;
  }

  string _url;
};

// -----------------------------------------------------------------------------
// --SECTION--                                        big document creation test
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @addtogroup V8Shell
/// @{
////////////////////////////////////////////////////////////////////////////////

struct BigDocumentCreationTest : public BenchmarkOperation {
  BigDocumentCreationTest () 
    : BenchmarkOperation (),
      _url(),
      _buffer(0) {
    _url = "/_api/document?collection=" + Collection + "&createCollection=true";

    const size_t n = 100;

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

  ~BigDocumentCreationTest () {
    TRI_Free(TRI_UNKNOWN_MEM_ZONE, _buffer);
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
  
  const char* payload (size_t* length, const size_t counter) {
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

static int GetStartCounter () {
  MUTEX_LOCKER(StartMutex);
  return Started;
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
    cerr << "invalid value for --server.endpoint ('" << BaseClient.endpointString() << "')" << endl;
    exit(EXIT_FAILURE);
  }

  BenchmarkOperation* testCase;

  if (TestCase == "version") {
    testCase = new VersionTest();
  }
  else if (TestCase == "smalldoc") {
    testCase = new SmallDocumentCreationTest();
  }
  else if (TestCase == "bigdoc") {
    testCase = new BigDocumentCreationTest();
  }
  else {
    cerr << "invalid test case name " << TestCase << endl;
    exit(EXIT_FAILURE);
  }


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

  Timing timer(Timing::TI_WALLCLOCK);

  // broadcast the start signal to all threads
  {
    ConditionLocker guard(&startCondition);
    guard.broadcast();
  }

  while (1) {
    size_t numOperations = operationsCounter();

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
// outline-regexp: "^\\(/// @brief\\|/// {@inheritDoc}\\|/// @addtogroup\\|// --SECTION--\\|/// @\\}\\)"
// End:
