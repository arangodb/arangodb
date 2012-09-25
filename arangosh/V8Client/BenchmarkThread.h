////////////////////////////////////////////////////////////////////////////////
/// @brief benchmark thread
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
/// @author Jan Steemann
/// @author Copyright 2012, triAGENS GmbH, Cologne, Germany
////////////////////////////////////////////////////////////////////////////////

#ifndef TRIAGENS_V8_CLIENT_BENCHMARK_THREAD_H
#define TRIAGENS_V8_CLIENT_BENCHMARK_THREAD_H 1

#include "Basics/Common.h"

#include "BasicsC/hashes.h"

#include "Basics/ConditionLocker.h"
#include "Basics/ConditionVariable.h"
#include "Basics/Thread.h"
#include "SimpleHttpClient/SimpleClient.h"
#include "SimpleHttpClient/SimpleHttpClient.h"
#include "SimpleHttpClient/GeneralClientConnection.h"
#include "V8Client/BenchmarkCounter.h"
#include "V8Client/BenchmarkOperation.h"

using namespace std;
using namespace triagens::basics;
using namespace triagens::httpclient;
using namespace triagens::rest;

namespace triagens {
  namespace v8client {
  
// -----------------------------------------------------------------------------
// --SECTION--                                             class BenchmarkThread
// -----------------------------------------------------------------------------

// -----------------------------------------------------------------------------
// --SECTION--                                        constructors / destructors
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @addtogroup Threading
/// @{
////////////////////////////////////////////////////////////////////////////////

    class BenchmarkThread : public Thread {

      public:

////////////////////////////////////////////////////////////////////////////////
/// @brief construct the benchmark thread
////////////////////////////////////////////////////////////////////////////////

        BenchmarkThread (BenchmarkOperation* operation,
                         ConditionVariable* condition, 
                         const unsigned long batchSize,
                         BenchmarkCounter<unsigned long>* operationsCounter,
                         Endpoint* endpoint, 
                         const string& username, 
                         const string& password) 
          : Thread("arangob"), 
            _operation(operation),
            _startCondition(condition),
            _batchSize(batchSize),
            _operationsCounter(operationsCounter),
            _endpoint(endpoint),
            _username(username),
            _password(password),
            _client(0),
            _connection(0),
            _offset(0),
            _counter(0),
            _time(0.0) {
        }

////////////////////////////////////////////////////////////////////////////////
/// @brief destroy the benchmark thread
////////////////////////////////////////////////////////////////////////////////
        
        ~BenchmarkThread () {
          if (_client != 0) {
            delete _client;
          }
          
          if (_connection != 0) {
            delete _connection;
          }
        }

////////////////////////////////////////////////////////////////////////////////
/// @}
////////////////////////////////////////////////////////////////////////////////

// -----------------------------------------------------------------------------
// --SECTION--                                         virtual protected methods
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @addtogroup Threading
/// @{
////////////////////////////////////////////////////////////////////////////////

      protected:

////////////////////////////////////////////////////////////////////////////////
/// @brief the thread program
////////////////////////////////////////////////////////////////////////////////

        virtual void run () {
          allowAsynchronousCancelation();

          _connection = GeneralClientConnection::factory(_endpoint, 5.0, 10.0, 3);
          if (_connection == 0) {
            cerr << "out of memory" << endl;
            exit(EXIT_FAILURE);
          }

          _client = new SimpleHttpClient(_connection, 10.0, true);
          _client->setUserNamePassword("/", _username, _password);
          map<string, string> headerFields;
          SimpleHttpResult* result = _client->request(HttpRequest::HTTP_REQUEST_GET, 
                                                      "/_api/version", 
                                                      0, 
                                                      0, 
                                                      headerFields);
  
          if (! result || ! result->isComplete()) {
            if (result) {
              delete result;
            }
            cerr << "could not connect to server" << endl;
            exit(EXIT_FAILURE);
          }

          delete result;
 
          // wait for start condition to be broadcasted 
          {
            ConditionLocker guard(_startCondition);
            guard.wait();
          }

          while (1) {
            unsigned long numOps = _operationsCounter->next(_batchSize);

            if (numOps == 0) {
              break;
            }

            if (_batchSize < 1) {
              executeSingleRequest();
            } 
            else {
              executeBatchRequest(numOps);
            }
          }
        }

////////////////////////////////////////////////////////////////////////////////
/// @}
////////////////////////////////////////////////////////////////////////////////

// -----------------------------------------------------------------------------
// --SECTION--                                                   private methods
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @addtogroup V8Client
/// @{
////////////////////////////////////////////////////////////////////////////////

      private:

////////////////////////////////////////////////////////////////////////////////
/// @brief execute a batch request with numOperations parts
////////////////////////////////////////////////////////////////////////////////

        void executeBatchRequest (const unsigned long numOperations) {
          const string boundary = "XXXarangob-benchmarkXXX";

          StringBuffer batchPayload(TRI_UNKNOWN_MEM_ZONE);

          for (unsigned long i = 0; i < numOperations; ++i) {
            // append boundary
            batchPayload.appendText("--" + boundary + "\r\n");
            // append content-type, this will also begin the body
            batchPayload.appendText(HttpRequest::getPartContentType());

            // everything else (i.e. part request header & body) will get into the body
            const HttpRequest::HttpRequestType type = _operation->type();
            const string url = _operation->url();
            size_t payloadLength = 0;
            const char* payload = _operation->payload(&payloadLength, _offset + _counter++);
            const map<string, string>& headers = _operation->headers();

            // headline, e.g. POST /... HTTP/1.1
            HttpRequest::appendMethod(type, &batchPayload);
            batchPayload.appendText(url + " HTTP/1.1\r\n");
            // extra headers
            for (map<string, string>::const_iterator it = headers.begin(); it != headers.end(); ++it) {
              batchPayload.appendText((*it).first + ": " + (*it).second + "\r\n");
            }
            batchPayload.appendText("\r\n");
            
            // body
            batchPayload.appendText(payload, payloadLength);
            batchPayload.appendText("\r\n");
          }

          // end of MIME
          batchPayload.appendText("--" + boundary + "--\r\n");
          
          map<string, string> batchHeaders;
          batchHeaders["Content-Type"] = HttpRequest::getMultipartContentType() + 
                                         "; boundary=" + boundary;

          Timing timer(Timing::TI_WALLCLOCK);
          SimpleHttpResult* result = _client->request(HttpRequest::HTTP_REQUEST_POST,
                                                      "/_api/batch",
                                                      batchPayload.c_str(),
                                                      batchPayload.length(),
                                                      batchHeaders);
          _time += ((double) timer.time()) / 1000000.0;

          if (result == 0) {
            _operationsCounter->incFailures();
            return;
          }

          if (result->getHttpReturnCode() >= 400) { 
            _operationsCounter->incFailures();
          }
          delete result;
        }

////////////////////////////////////////////////////////////////////////////////
/// @brief execute a single request
////////////////////////////////////////////////////////////////////////////////

        void executeSingleRequest () {
          const HttpRequest::HttpRequestType type = _operation->type();
          const string url = _operation->url();
          size_t payloadLength = 0;
          const char* payload = _operation->payload(&payloadLength, _offset + _counter++);
          const map<string, string>& headers = _operation->headers();

          Timing timer(Timing::TI_WALLCLOCK);
          SimpleHttpResult* result = _client->request(type,
                                                      url,
                                                      payload,
                                                      payloadLength,
                                                      headers);
          _time += ((double) timer.time()) / 1000000.0;

          if (result == 0) {
            _operationsCounter->incFailures();
            return;
          }

          if (result->getHttpReturnCode() >= 400) { 
            _operationsCounter->incFailures();
          }
          delete result;
        }

////////////////////////////////////////////////////////////////////////////////
/// @}
////////////////////////////////////////////////////////////////////////////////

// -----------------------------------------------------------------------------
// --SECTION--                                                    public methods
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @addtogroup V8Client
/// @{
////////////////////////////////////////////////////////////////////////////////

      public:

////////////////////////////////////////////////////////////////////////////////
/// @brief set the threads offset value
////////////////////////////////////////////////////////////////////////////////

        void setOffset (size_t offset) {
          _offset = offset;
        }

////////////////////////////////////////////////////////////////////////////////
/// @brief return the total time accumulated by the thread
////////////////////////////////////////////////////////////////////////////////

        double getTime () const {
          return _time;
        }

////////////////////////////////////////////////////////////////////////////////
/// @}
////////////////////////////////////////////////////////////////////////////////

// -----------------------------------------------------------------------------
// --SECTION--                                                 private variables
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @addtogroup V8Client
/// @{
////////////////////////////////////////////////////////////////////////////////

      private:

////////////////////////////////////////////////////////////////////////////////
/// @brief the operation to benchmark
////////////////////////////////////////////////////////////////////////////////

        BenchmarkOperation* _operation;

////////////////////////////////////////////////////////////////////////////////
/// @brief condition variable
////////////////////////////////////////////////////////////////////////////////

        ConditionVariable* _startCondition;

////////////////////////////////////////////////////////////////////////////////
/// @brief batch size
////////////////////////////////////////////////////////////////////////////////

        const unsigned long _batchSize;

////////////////////////////////////////////////////////////////////////////////
/// @brief benchmark counter
////////////////////////////////////////////////////////////////////////////////

        BenchmarkCounter<unsigned long>* _operationsCounter;

////////////////////////////////////////////////////////////////////////////////
/// @brief endpoint to use
////////////////////////////////////////////////////////////////////////////////

        Endpoint* _endpoint;

////////////////////////////////////////////////////////////////////////////////
/// @brief HTTP username
////////////////////////////////////////////////////////////////////////////////

        const string _username;

////////////////////////////////////////////////////////////////////////////////
/// @brief HTTP password
////////////////////////////////////////////////////////////////////////////////

        const string _password;

////////////////////////////////////////////////////////////////////////////////
/// @brief underlying client
////////////////////////////////////////////////////////////////////////////////

        triagens::httpclient::SimpleClient* _client;

////////////////////////////////////////////////////////////////////////////////
/// @brief connection to the server
////////////////////////////////////////////////////////////////////////////////

        triagens::httpclient::GeneralClientConnection* _connection;

////////////////////////////////////////////////////////////////////////////////
/// @brief thread offset value
////////////////////////////////////////////////////////////////////////////////

        size_t _offset;

////////////////////////////////////////////////////////////////////////////////
/// @brief thread counter value
////////////////////////////////////////////////////////////////////////////////

        size_t _counter;

////////////////////////////////////////////////////////////////////////////////
/// @brief time
////////////////////////////////////////////////////////////////////////////////

        double _time;

    };
  }
}

////////////////////////////////////////////////////////////////////////////////
/// @}
////////////////////////////////////////////////////////////////////////////////

#endif

// -----------------------------------------------------------------------------
// --SECTION--                                                       END-OF-FILE
// -----------------------------------------------------------------------------

// Local Variables:
// mode: outline-minor
// outline-regexp: "^\\(/// @brief\\|/// {@inheritDoc}\\|/// @addtogroup\\|// --SECTION--\\|/// @\\}\\)"
// End:
