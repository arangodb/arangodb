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
#include "Basics/ProgramOptions.h"
#include "Basics/ProgramOptionsDescription.h"
#include "Basics/StringUtils.h"
#include "BasicsC/init.h"
#include "BasicsC/logging.h"
#include "BasicsC/strings.h"
#include "BasicsC/terminal-utils.h"
#include "ImportHelper.h"
#include "Logger/Logger.h"
#include "Rest/Endpoint.h"
#include "Rest/Initialise.h"
#include "SimpleHttpClient/SimpleHttpClient.h"
#include "SimpleHttpClient/SimpleHttpResult.h"
#include "V8Client/BenchmarkThread.h"
#include "V8Client/SharedCounter.h"

using namespace std;
using namespace triagens::basics;
using namespace triagens::httpclient;
using namespace triagens::rest;
using namespace triagens::v8client;
using namespace triagens::arango;

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

static int BatchSize = 1;

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
/// @brief parses the program options
////////////////////////////////////////////////////////////////////////////////

static void ParseProgramOptions (int argc, char* argv[]) {
  ProgramOptionsDescription description("STANDARD options");
  
  description
    ("concurrency", &Concurrency, "number of parallel connections")
    ("requests", &Operations, "total number of operations")
    ("batch-size", &BatchSize, "number of operations in one batch")
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

BenchmarkRequest VersionFunc () {
  map<string, string> params;
  BenchmarkRequest r("/_api/version", params, "", PB_NO_CONTENT, SimpleHttpClient::GET);

  return r;
}

BenchmarkRequest InsertFunc () {
  map<string, string> params;
  params["createCollection"] = "true";
  params["collection"] = "BenchmarkInsert";

  BenchmarkRequest r("/_api/document", params, "{\"some value\" : 1}", PB_NO_CONTENT, SimpleHttpClient::POST);

  return r;
}

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


  SharedCounter<unsigned long> operationsCounter(0, (unsigned long) Operations);
  ConditionVariable startCondition;

  vector<Endpoint*> endpoints;
  vector<BenchmarkThread*> threads;
  for (int i = 0; i < Concurrency; ++i) {
    Endpoint* endpoint = Endpoint::clientFactory(BaseClient.endpointString());
    endpoints.push_back(endpoint);

    BenchmarkThread* thread = new BenchmarkThread(&InsertFunc,
        &startCondition, 
        (unsigned long) BatchSize,
        &operationsCounter,
        endpoint,
        BaseClient.username(), 
        BaseClient.password());

    threads.push_back(thread);
    thread->start();
  }

  usleep(500000);


  Timing timer(Timing::TI_WALLCLOCK);

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

  cout << "Total number of operations: " << Operations << ", batch size: " << BatchSize << ", concurrency level: " << Concurrency << endl;
  cout << "Total duration: " << fixed << time << " s" << endl;
  cout << "Duration per operation: " << fixed << (time / Operations) << " s" << endl;
  cout << "Duration per operation per thread: " << fixed << (time / (double) Operations * (double) Concurrency) << " s" << endl << endl;

  if (operationsCounter.failures() > 0) {
    cout << "WARNING: " << operationsCounter.failures() << " request(s) failed!!" << endl << endl;
  }

  for (int i = 0; i < Concurrency; ++i) {
    threads[i]->join();
    delete threads[i];
    delete endpoints[i];
  }

  TRIAGENS_REST_SHUTDOWN;

  return EXIT_SUCCESS;
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
