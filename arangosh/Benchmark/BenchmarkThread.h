////////////////////////////////////////////////////////////////////////////////
/// @brief benchmark thread
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
/// @author Jan Steemann
/// @author Copyright 2014, ArangoDB GmbH, Cologne, Germany
/// @author Copyright 2012-2013, triAGENS GmbH, Cologne, Germany
////////////////////////////////////////////////////////////////////////////////

#ifndef ARANGODB_BENCHMARK_BENCHMARK_THREAD_H
#define ARANGODB_BENCHMARK_BENCHMARK_THREAD_H 1

#include "Basics/Common.h"

#include "Basics/hashes.h"

#include "Basics/ConditionLocker.h"
#include "Basics/ConditionVariable.h"
#include "Basics/Thread.h"
#include "Basics/logging.h"
#include "Rest/HttpResponse.h"
#include "SimpleHttpClient/SimpleHttpClient.h"
#include "SimpleHttpClient/GeneralClientConnection.h"
#include "Benchmark/BenchmarkCounter.h"
#include "Benchmark/BenchmarkOperation.h"

namespace triagens {
  namespace arangob {

// -----------------------------------------------------------------------------
// --SECTION--                                             class BenchmarkThread
// -----------------------------------------------------------------------------

// -----------------------------------------------------------------------------
// --SECTION--                                        constructors / destructors
// -----------------------------------------------------------------------------

    class BenchmarkThread : public triagens::basics::Thread {

      public:

////////////////////////////////////////////////////////////////////////////////
/// @brief construct the benchmark thread
////////////////////////////////////////////////////////////////////////////////

        BenchmarkThread (BenchmarkOperation* operation,
                         basics::ConditionVariable* condition,
                         void (*callback) (),
                         int threadNumber,
                         const unsigned long batchSize,
                         BenchmarkCounter<unsigned long>* operationsCounter,
                         rest::Endpoint* endpoint,
                         const std::string& databaseName,
                         const std::string& username,
                         const std::string& password,
                         double requestTimeout,
                         double connectTimeout,
                         uint32_t sslProtocol,
                         bool keepAlive,
                         bool async,
                         bool verbose)
          : Thread("arangob"),
            _operation(operation),
            _startCondition(condition),
            _callback(callback),
            _threadNumber(threadNumber),
            _batchSize(batchSize),
            _warningCount(0),
            _operationsCounter(operationsCounter),
            _endpoint(endpoint),
            _headers(),
            _databaseName(databaseName),
            _username(username),
            _password(password),
            _requestTimeout(requestTimeout),
            _connectTimeout(connectTimeout),
            _sslProtocol(sslProtocol),
            _keepAlive(keepAlive),
            _async(async),
            _client(nullptr),
            _connection(nullptr),
            _offset(0),
            _counter(0),
            _time(0.0),
            _verbose(verbose) {

          _errorHeader = basics::StringUtils::tolower(rest::HttpResponse::BatchErrorHeader);
        }

////////////////////////////////////////////////////////////////////////////////
/// @brief destroy the benchmark thread
////////////////////////////////////////////////////////////////////////////////

        ~BenchmarkThread () {
          delete _client;
          delete _connection;
        }

// -----------------------------------------------------------------------------
// --SECTION--                                         virtual protected methods
// -----------------------------------------------------------------------------

      protected:

////////////////////////////////////////////////////////////////////////////////
/// @brief the thread program
////////////////////////////////////////////////////////////////////////////////

        void run () {
          _connection = httpclient::GeneralClientConnection::factory(_endpoint, _requestTimeout, _connectTimeout, 3, _sslProtocol);

          if (_connection == nullptr) {
            LOG_FATAL_AND_EXIT("out of memory");
          }

          _client = new httpclient::SimpleHttpClient(_connection, _requestTimeout, true);

          if (_client == nullptr) {
            LOG_FATAL_AND_EXIT("out of memory");
          }

          _client->setLocationRewriter(this, &rewriteLocation);

          _client->setUserNamePassword("/", _username, _password);
          _client->setKeepAlive(_keepAlive);

          // test the connection
          httpclient::SimpleHttpResult* result = _client->request(rest::HttpRequest::HTTP_REQUEST_GET,
                                                      "/_api/version",
                                                      nullptr,
                                                      0,
                                                      _headers);

          if (! result || ! result->isComplete()) {
            if (result) {
              delete result;
            }

            LOG_FATAL_AND_EXIT("could not connect to server");
          }

          delete result;

          // if we're the first thread, set up the test
          if (_threadNumber == 0) {
            if (! _operation->setUp(_client)) {
              LOG_FATAL_AND_EXIT("could not set up the test");
            }
          }

          if (_async) {
            _headers["x-arango-async"] = "true";
          }

          _callback();

          // wait for start condition to be broadcasted
          {
            basics::ConditionLocker guard(_startCondition);
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
              try {
                executeBatchRequest(numOps);
              }
              catch (triagens::basics::Exception const& ex) {
                LOG_FATAL_AND_EXIT("Caught exception during test execution: %d %s",
                                   ex.code(), ex.what());
              }
              catch (std::bad_alloc const&) {
                LOG_FATAL_AND_EXIT("Caught OOM exception during test execution!");
              }
              catch (std::exception const& ex) {
                LOG_FATAL_AND_EXIT("Caught STD exception during test execution: %s", ex.what());
              }
              catch (...) {
                LOG_FATAL_AND_EXIT("Caught unknown exception during test execution!");
              }
            }
            _operationsCounter->done(_batchSize > 0 ? _batchSize : 1);
          }
        }

// -----------------------------------------------------------------------------
// --SECTION--                                                   private methods
// -----------------------------------------------------------------------------

      private:

////////////////////////////////////////////////////////////////////////////////
/// @brief request location rewriter (injects database name)
////////////////////////////////////////////////////////////////////////////////

        static std::string rewriteLocation (void* data, const std::string& location) {
          auto t = static_cast<arangob::BenchmarkThread*>(data);

          TRI_ASSERT(t != nullptr);

          if (location.substr(0, 5) == "/_db/") {
            // location already contains /_db/
            return location;
          }

          if (location[0] == '/') {
            return std::string("/_db/" + t->_databaseName + location);
          }
          else {
            return std::string("/_db/" + t->_databaseName + "/" + location);
          }
        }

////////////////////////////////////////////////////////////////////////////////
/// @brief execute a batch request with numOperations parts
////////////////////////////////////////////////////////////////////////////////

        void executeBatchRequest (const unsigned long numOperations) {
          static const char boundary[] = "XXXarangob-benchmarkXXX";
          size_t blen = strlen(boundary);

          basics::StringBuffer batchPayload(TRI_UNKNOWN_MEM_ZONE);
          int ret = batchPayload.reserve(numOperations * 1024);
          if (ret != TRI_ERROR_NO_ERROR) {
            LOG_FATAL_AND_EXIT("Failed to reserve %lu bytes for %lu batch operations: %d",
                               numOperations * 1024,
                               numOperations,
                               ret);
          }
          for (unsigned long i = 0; i < numOperations; ++i) {
            // append boundary
            batchPayload.appendText("--", 2);
            batchPayload.appendText(boundary, blen);
            batchPayload.appendText("\r\n", 2);
            // append content-type, this will also begin the body
            batchPayload.appendText("Content-Type: ", 14);
            batchPayload.appendText(rest::HttpRequest::BatchContentType);
            batchPayload.appendText("\r\n\r\n", 4);

            // everything else (i.e. part request header & body) will get into the body
            const size_t threadCounter = _counter++;
            const size_t globalCounter = _offset + threadCounter;
            const std::string url = _operation->url(_threadNumber, threadCounter, globalCounter);
            size_t payloadLength = 0;
            bool mustFree = false;
            const char* payload = _operation->payload(&payloadLength, _threadNumber, threadCounter, globalCounter, &mustFree);
            const rest::HttpRequest::HttpRequestType type = _operation->type(_threadNumber, threadCounter, globalCounter);
            if (url.empty()) {
              LOG_WARNING("URL is empty!");
            }

            // headline, e.g. POST /... HTTP/1.1
            rest::HttpRequest::appendMethod(type, &batchPayload);
            size_t oldLength = batchPayload.length();
            batchPayload.appendText(url);
            if (batchPayload.length() == oldLength) {
              LOG_WARNING("URL nonempty empty, but nothing appended!");
            }              
            batchPayload.appendText(" HTTP/1.1\r\n", 11);
            batchPayload.appendText("\r\n", 2);

            // body
            batchPayload.appendText(payload, payloadLength);
            batchPayload.appendText("\r\n", 2);

            if (mustFree) {
              TRI_Free(TRI_UNKNOWN_MEM_ZONE, (void*) payload);
            }
          }

          // end of MIME
          batchPayload.appendText("--", 2);
          batchPayload.appendText(boundary, blen);
          batchPayload.appendText("--\r\n", 2);

          _headers.erase("Content-Type");
          _headers["Content-Type"] = rest::HttpRequest::MultiPartContentType +
                                     "; boundary=" + boundary;

          double start = TRI_microtime();
          httpclient::SimpleHttpResult* result = _client->request(rest::HttpRequest::HTTP_REQUEST_POST,
                                                      "/_api/batch",
                                                      batchPayload.c_str(),
                                                      batchPayload.length(),
                                                      _headers);
          _time += TRI_microtime() - start;

          if (result == nullptr || ! result->isComplete()) {
            if (result != nullptr){
              _operationsCounter->incIncompleteFailures(numOperations);
            }
            _operationsCounter->incFailures(numOperations);
            if (result != nullptr) {
              delete result;
            }
            _warningCount++;
            if (_warningCount < MaxWarnings) {
              LOG_WARNING("batch operation failed because server did not reply");
            }
            return;
          }

          if (result->wasHttpError()) {
            _operationsCounter->incFailures(numOperations);

            _warningCount++;
            if (_warningCount < MaxWarnings) {
              LOG_WARNING("batch operation failed with HTTP code %d - %s ",
                          (int) result->getHttpReturnCode(),
                          result->getHttpReturnMessage().c_str());
#ifdef TRI_ENABLE_MAINTAINER_MODE
              LOG_WARNING("We tried to send this size:\n %llu", (unsigned long long) batchPayload.length());
              LOG_WARNING("We tried to send this:\n %s", batchPayload.c_str());
#endif
            }
            else if (_warningCount == MaxWarnings) {
              LOG_WARNING("...more warnings...");
            }
          }
          else {
            auto const& headers = result->getHeaderFields();
            auto it = headers.find(_errorHeader);

            if (it != headers.end()) {
              uint32_t errorCount =  basics::StringUtils::uint32((*it).second);

              if (errorCount > 0) {
                _operationsCounter->incFailures(errorCount);
                _warningCount++;
                if (_warningCount < MaxWarnings) {
                  LOG_WARNING("Server side warning count: %u", errorCount);
                  if (_verbose) {
                    LOG_WARNING("Server reply: %s", result->getBody().c_str());
#ifdef TRI_ENABLE_MAINTAINER_MODE
                    LOG_WARNING("We tried to send this size:\n %llu", (unsigned long long) batchPayload.length());
                    LOG_WARNING("We tried to send this:\n %s", batchPayload.c_str());
#endif
                  }
                }
              }
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
          const rest::HttpRequest::HttpRequestType type = _operation->type(_threadNumber, threadCounter, globalCounter);
          const std::string url = _operation->url(_threadNumber, threadCounter, globalCounter);
          size_t payloadLength = 0;
          bool mustFree = false;

          // std::cout << "thread number #" << _threadNumber << ", threadCounter " << threadCounter << ", globalCounter " << globalCounter << "\n";
          const char* payload = _operation->payload(&payloadLength, _threadNumber, threadCounter, globalCounter, &mustFree);

          double start = TRI_microtime();
          httpclient::SimpleHttpResult* result = _client->request(type,
                                                      url,
                                                      payload,
                                                      payloadLength,
                                                      _headers);
          _time += TRI_microtime() - start;

          if (mustFree) {
            TRI_Free(TRI_UNKNOWN_MEM_ZONE, (void*) payload);
          }

          if (result == nullptr || ! result->isComplete()) {
            _operationsCounter->incFailures(1);
            if (result != nullptr) {
              _operationsCounter->incIncompleteFailures(1);
              delete result;
            }
            _warningCount++;
            if (_warningCount < MaxWarnings) {
              LOG_WARNING("batch operation failed because server did not reply");
            }
            return;
          }

          if (result->wasHttpError()) {
            _operationsCounter->incFailures(1);

            _warningCount++;
            if (_warningCount < MaxWarnings) {
              LOG_WARNING("request for URL '%s' failed with HTTP code %d", url.c_str(), (int) result->getHttpReturnCode());
            }
            else if (_warningCount == MaxWarnings) {
              LOG_WARNING("...more warnings...");
            }
          }
          delete result;
        }

// -----------------------------------------------------------------------------
// --SECTION--                                                    public methods
// -----------------------------------------------------------------------------

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

// -----------------------------------------------------------------------------
// --SECTION--                                                 private variables
// -----------------------------------------------------------------------------

      private:

////////////////////////////////////////////////////////////////////////////////
/// @brief the operation to benchmark
////////////////////////////////////////////////////////////////////////////////

        arangob::BenchmarkOperation* _operation;

////////////////////////////////////////////////////////////////////////////////
/// @brief condition variable
////////////////////////////////////////////////////////////////////////////////

        basics::ConditionVariable* _startCondition;

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

        unsigned long const _batchSize;

////////////////////////////////////////////////////////////////////////////////
/// @brief warning counter
////////////////////////////////////////////////////////////////////////////////

        int _warningCount;

////////////////////////////////////////////////////////////////////////////////
/// @brief benchmark counter
////////////////////////////////////////////////////////////////////////////////

        arangob::BenchmarkCounter<unsigned long>* _operationsCounter;

////////////////////////////////////////////////////////////////////////////////
/// @brief endpoint to use
////////////////////////////////////////////////////////////////////////////////

        rest::Endpoint* _endpoint;

////////////////////////////////////////////////////////////////////////////////
/// @brief extra request headers
////////////////////////////////////////////////////////////////////////////////

        std::map<std::string, std::string> _headers;

////////////////////////////////////////////////////////////////////////////////
/// @brief database name
////////////////////////////////////////////////////////////////////////////////

        std::string const _databaseName;

////////////////////////////////////////////////////////////////////////////////
/// @brief HTTP username
////////////////////////////////////////////////////////////////////////////////

        std::string const _username;

////////////////////////////////////////////////////////////////////////////////
/// @brief HTTP password
////////////////////////////////////////////////////////////////////////////////

        std::string const _password;

////////////////////////////////////////////////////////////////////////////////
/// @brief the request timeout (in s)
////////////////////////////////////////////////////////////////////////////////

        double _requestTimeout;

////////////////////////////////////////////////////////////////////////////////
/// @brief the connection timeout (in s)
////////////////////////////////////////////////////////////////////////////////

        double _connectTimeout;

////////////////////////////////////////////////////////////////////////////////
/// @brief ssl protocol
////////////////////////////////////////////////////////////////////////////////

        uint32_t _sslProtocol;

////////////////////////////////////////////////////////////////////////////////
/// @brief use HTTP keep-alive
////////////////////////////////////////////////////////////////////////////////

        bool _keepAlive;

////////////////////////////////////////////////////////////////////////////////
/// @brief send async requests
////////////////////////////////////////////////////////////////////////////////

        bool _async;

////////////////////////////////////////////////////////////////////////////////
/// @brief underlying client
////////////////////////////////////////////////////////////////////////////////

        triagens::httpclient::SimpleHttpClient* _client;

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

        std::string _errorHeader;

////////////////////////////////////////////////////////////////////////////////
/// @brief maximum number of warnings to be displayed per thread
////////////////////////////////////////////////////////////////////////////////

        static const int MaxWarnings = 5;

////////////////////////////////////////////////////////////////////////////////
/// @brief output replies if error count in http relpy > 0
////////////////////////////////////////////////////////////////////////////////

        bool _verbose;

    };
  }
}

#endif

// -----------------------------------------------------------------------------
// --SECTION--                                                       END-OF-FILE
// -----------------------------------------------------------------------------

// Local Variables:
// mode: outline-minor
// outline-regexp: "/// @brief\\|/// {@inheritDoc}\\|/// @page\\|// --SECTION--\\|/// @\\}"
// End:
