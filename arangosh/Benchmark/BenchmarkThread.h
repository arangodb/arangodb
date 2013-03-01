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

#ifndef TRIAGENS_BENCHMARK_BENCHMARK_THREAD_H
#define TRIAGENS_BENCHMARK_BENCHMARK_THREAD_H 1

#include "Basics/Common.h"

#include "BasicsC/hashes.h"

#include "Basics/ConditionLocker.h"
#include "Basics/ConditionVariable.h"
#include "Basics/Thread.h"
#include "Rest/HttpResponse.h"
#include "SimpleHttpClient/SimpleClient.h"
#include "SimpleHttpClient/SimpleHttpClient.h"
#include "SimpleHttpClient/GeneralClientConnection.h"
#include "Benchmark/BenchmarkCounter.h"
#include "Benchmark/BenchmarkOperation.h"

using namespace std;
using namespace triagens::basics;
using namespace triagens::httpclient;
using namespace triagens::rest;

namespace triagens {
  namespace arangob {
  
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
                         void (*callback) (),
                         int threadNumber, 
                         const unsigned long batchSize,
                         BenchmarkCounter<unsigned long>* operationsCounter,
                         Endpoint* endpoint, 
                         const string& username, 
                         const string& password) 
          : Thread("arangob"), 
            _operation(operation),
            _startCondition(condition),
            _callback(callback),
            _threadNumber(threadNumber),
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
            
          _errorHeader = StringUtils::tolower(HttpResponse::getBatchErrorHeader());
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

          _connection = GeneralClientConnection::factory(_endpoint, 10.0, 10.0, 3);
          if (_connection == 0) {
            cerr << "out of memory" << endl;
            exit(EXIT_FAILURE);
          }

          _client = new SimpleHttpClient(_connection, 10.0, true);
          _client->setUserNamePassword("/", _username, _password);

          // test the connection
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


          // if we're the first thread, wipe the existing collection
          if (_threadNumber == 0 && _operation->useCollection()) {
            result = _client->request(HttpRequest::HTTP_REQUEST_DELETE,
                                      "/_api/collection/" + _operation->collectionName(),
                                      "",
                                      0, 
                                      headerFields); 

            if (result == 0 || (result->getHttpReturnCode() != 200 && result->getHttpReturnCode() != 404)) {
              cerr << "could not wipe existing collection " << _operation->collectionName() << endl;
              exit(EXIT_FAILURE);
            }
            else {
              delete result;
            }
            
            // now create the collection
            string payload = "{\"name\":\"" + _operation->collectionName() + "\"}";
            result = _client->request(HttpRequest::HTTP_REQUEST_POST,
                                      "/_api/collection",
                                      payload.c_str(),
                                      payload.size(), 
                                      headerFields); 

            if (result == 0 || (result->getHttpReturnCode() != 200 && result->getHttpReturnCode() != 201)) {
              cerr << "could not create collection " << _operation->collectionName() << endl;
              exit(EXIT_FAILURE);
            }
            else {
              delete result;
            }
          }

          _callback();
 
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
          static const string boundary = "XXXarangob-benchmarkXXX";

          StringBuffer batchPayload(TRI_UNKNOWN_MEM_ZONE);

          for (unsigned long i = 0; i < numOperations; ++i) {
            // append boundary
            batchPayload.appendText("--" + boundary + "\r\n");
            // append content-type, this will also begin the body
            batchPayload.appendText("Content-Type: ", 14);
            batchPayload.appendText(HttpRequest::getPartContentType());
            batchPayload.appendText("\r\n\r\n", 4);
            
            // everything else (i.e. part request header & body) will get into the body
            const size_t threadCounter = _counter++;
            const size_t globalCounter = _offset + threadCounter;
            const string url = _operation->url(_threadNumber, threadCounter, globalCounter);
            size_t payloadLength = 0;
            bool mustFree = false;
            const char* payload = _operation->payload(&payloadLength, _threadNumber, threadCounter, globalCounter, &mustFree);
            const map<string, string>& headers = _operation->headers();
            const HttpRequest::HttpRequestType type = _operation->type(_threadNumber, threadCounter, globalCounter);
          
            // headline, e.g. POST /... HTTP/1.1
            HttpRequest::appendMethod(type, &batchPayload);
            batchPayload.appendText(url + " HTTP/1.1\r\n");
            // extra headers
            for (map<string, string>::const_iterator it = headers.begin(); it != headers.end(); ++it) {
              batchPayload.appendText((*it).first + ": " + (*it).second + "\r\n");
            }
            batchPayload.appendText("\r\n", 2);
            
            // body
            batchPayload.appendText(payload, payloadLength);
            batchPayload.appendText("\r\n", 2);

            if (mustFree) {
              TRI_Free(TRI_UNKNOWN_MEM_ZONE, (void*) payload);
            }
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
            _operationsCounter->incFailures(numOperations);
            return;
          }

          if (result->getHttpReturnCode() >= 400) { 
            _operationsCounter->incFailures(numOperations);
          }
          else {
            const std::map<string, string>& headers = result->getHeaderFields();
            map<string, string>::const_iterator it = headers.find(_errorHeader);

            if (it != headers.end()) {
              size_t errorCount = (size_t) StringUtils::uint32((*it).second);
              _operationsCounter->incFailures(errorCount);
            }
          }
          delete result;
        }

////////////////////////////////////////////////////////////////////////////////
/// @brief execute a single request
////////////////////////////////////////////////////////////////////////////////

        void executeSingleRequest () {
          const size_t threadCounter = _counter++;
          const size_t globalCounter = _offset + threadCounter;
          const HttpRequest::HttpRequestType type = _operation->type(_threadNumber, threadCounter, globalCounter);
          const string url = _operation->url(_threadNumber, threadCounter, globalCounter);
          size_t payloadLength = 0;
          bool mustFree = false;
          const char* payload = _operation->payload(&payloadLength, _threadNumber, threadCounter, globalCounter, &mustFree);
          const map<string, string>& headers = _operation->headers();

          Timing timer(Timing::TI_WALLCLOCK);
          SimpleHttpResult* result = _client->request(type,
                                                      url,
                                                      payload,
                                                      payloadLength,
                                                      headers);
          _time += ((double) timer.time()) / 1000000.0;
            
          if (mustFree) {
            TRI_Free(TRI_UNKNOWN_MEM_ZONE, (void*) payload);
          }

          if (result == 0) {
            _operationsCounter->incFailures(1);
            return;
          }

          if (result->getHttpReturnCode() >= 400) { 
            _operationsCounter->incFailures(1);
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
/// @brief start callback function
////////////////////////////////////////////////////////////////////////////////

        void (*_callback) ();

////////////////////////////////////////////////////////////////////////////////
/// @brief our thread number
////////////////////////////////////////////////////////////////////////////////

        int _threadNumber;

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

////////////////////////////////////////////////////////////////////////////////
/// @brief lower-case error header we look for
////////////////////////////////////////////////////////////////////////////////

        string _errorHeader;

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
