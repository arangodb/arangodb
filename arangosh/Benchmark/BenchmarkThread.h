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
/// @author Jan Steemann
////////////////////////////////////////////////////////////////////////////////

#ifndef ARANGODB_BENCHMARK_BENCHMARK_THREAD_H
#define ARANGODB_BENCHMARK_BENCHMARK_THREAD_H 1

#include <cmath>

#include "Basics/Common.h"

#include "Basics/ConditionLocker.h"
#include "Basics/ConditionVariable.h"
#include "Basics/Exceptions.h"
#include "Basics/Thread.h"
#include "Basics/application-exit.h"
#include "Basics/hashes.h"
#include "Basics/system-functions.h"
#include "Benchmark/BenchmarkCounter.h"
#include "Benchmark/BenchmarkOperation.h"
#include "Logger/LogMacros.h"
#include "Logger/Logger.h"
#include "Logger/LoggerStream.h"
#include "Rest/HttpRequest.h"
#include "Rest/HttpResponse.h"
#include "Shell/ClientFeature.h"
#include "SimpleHttpClient/GeneralClientConnection.h"
#include "SimpleHttpClient/SimpleHttpClient.h"
#include "SimpleHttpClient/SimpleHttpResult.h"

namespace arangodb {
namespace arangobench {

class BenchmarkThread : public arangodb::Thread {
 public:
  BenchmarkThread(application_features::ApplicationServer& server,
                  BenchmarkOperation* operation, basics::ConditionVariable* condition,
                  void (*callback)(), int threadNumber, uint64_t const batchSize,
                  BenchmarkCounter<uint64_t>* operationsCounter,
                  ClientFeature& client, bool keepAlive, bool async, bool verbose,
                  double histogramIntervalSize, uint64_t histogramNumIntervals )
      : Thread(server, "BenchmarkThread"),
        _operation(operation),
        _startCondition(condition),
        _callback(callback),
        _threadNumber(threadNumber),
        _batchSize(batchSize),
        _warningCount(0),
        _operationsCounter(operationsCounter),
        _client(client),
        _headers(),
        _databaseName(client.databaseName()),
        _username(client.username()),
        _password(client.password()),
        _keepAlive(keepAlive),
        _async(async),
        _httpClient(nullptr),
        _offset(0),
        _counter(0),
        _time(0.0),
        _errorHeader(basics::StringUtils::tolower(StaticStrings::Errors)),
        _verbose(verbose),
        _minTime(-1.0),
        _maxTime(0.0),
        _avgTime(0.0),
        _histogramNumIntervals(histogramNumIntervals),
        _histogramIntervalSize(histogramIntervalSize),
        _histogramScope(histogramIntervalSize * histogramNumIntervals),
        _histogram(histogramNumIntervals, 0) { }

  ~BenchmarkThread() { shutdown(); }
  //////////////////////////////////////////////////////////////////////////////
  /// @brief add one time
  //////////////////////////////////////////////////////////////////////////////

  void oneTime(double time) {
    if (_minTime == -1.0 || time < _minTime) {
      _minTime = time;
    }
  
    if (time > _maxTime) {
      _maxTime = time;
    }

    _avgTime = ((_avgTime * _counter) + time) / (_counter + 1);

    if (_histogramScope == 0.0) {
      _histogramScope = time * 20;
      _histogramIntervalSize = _histogramScope / _histogramNumIntervals;
    }

    uint64_t bucket = static_cast<uint64_t>(lround(time / _histogramIntervalSize));
    if (bucket >= _histogramNumIntervals) {
      bucket = _histogramNumIntervals - 1;
    }
    _histogram[bucket] ++;
  }

  void aggregateValues(double& minTime, double& maxTime, double& avgTime, uint64_t& counter) {
      if (minTime == -1.0 || minTime < _minTime) {
        minTime = _minTime;
      }
      if (_maxTime > maxTime) {
        maxTime = _maxTime;
      }
      if (counter == 0) {
        avgTime = _avgTime;
        counter = _counter;
      } else {
        avgTime = ((avgTime * counter) +
                   (_avgTime * _counter)) /
          (counter + _counter);
        counter += _counter;
      }
  }
  
  std::vector<double> getPercentiles(std::vector<double> const& which, double& histogramIntervalSize) {
    std::vector<double> res(which.size(), 0.0);
    std::vector<size_t> counts(which.size());
    size_t i = 0;
    histogramIntervalSize = _histogramIntervalSize;
    while (i < which.size()) {
      counts[i] = static_cast<size_t>(lround(_counter * which[i] / 100));
      i++;
    }
    i = 0;
    size_t nextCount = counts[i];
    size_t count = 0;
    size_t vecPos = 0;
    while (vecPos < _histogramNumIntervals && i < which.size()) {
      count += _histogram[vecPos];
      if (count >= nextCount) {
        res[i] = _histogramIntervalSize * vecPos;
        i++;
        if (i >= which.size()) {
          return res;
        }
        nextCount = counts[i];
      }
      vecPos ++;
    }
    return res;
  }

 protected:
  //////////////////////////////////////////////////////////////////////////////
  /// @brief the thread program
  //////////////////////////////////////////////////////////////////////////////

  void run() override {
    try {
      _httpClient = _client.createHttpClient();
    } catch (...) {
      LOG_TOPIC("b69d7", FATAL, arangodb::Logger::FIXME)
          << "cannot create server connection, giving up!";
      FATAL_ERROR_EXIT();
    }

    _httpClient->params().setLocationRewriter(this, &rewriteLocation);

    _httpClient->params().setUserNamePassword("/", _username, _password);
    _httpClient->params().setKeepAlive(_keepAlive);

    // test the connection
    httpclient::SimpleHttpResult* result =
        _httpClient->request(rest::RequestType::GET, "/_api/version", nullptr, 0, _headers);

    if (!result || !result->isComplete()) {
      if (result) {
        delete result;
      }

      LOG_TOPIC("5cda7", FATAL, arangodb::Logger::FIXME)
          << "could not connect to server";
      FATAL_ERROR_EXIT();
    }

    delete result;

    // if we're the first thread, set up the test
    if (_threadNumber == 0) {
      if (!_operation->setUp(_httpClient.get())) {
        LOG_TOPIC("528b6", FATAL, arangodb::Logger::FIXME)
            << "could not set up the test";
        FATAL_ERROR_EXIT();
      }
    }

    if (_async) {
      _headers["x-arango-async"] = "true";
    }

    _callback();

    // wait for start condition to be broadcasted
    {
      // cppcheck-suppress redundantPointerOp
      CONDITION_LOCKER(guard, (*_startCondition));
      guard.wait();
    }

    while (!isStopping()) {
      uint64_t numOps = _operationsCounter->next(_batchSize);

      if (numOps == 0) {
        break;
      }

      if (_batchSize < 1) {
        executeSingleRequest();
      } else {
        try {
          executeBatchRequest(numOps);
        } catch (arangodb::basics::Exception const& ex) {
          LOG_TOPIC("bd1d1", FATAL, arangodb::Logger::FIXME)
              << "Caught exception during test execution: " << ex.code() << " "
              << ex.what();
          FATAL_ERROR_EXIT();
        } catch (std::bad_alloc const&) {
          LOG_TOPIC("29451", FATAL, arangodb::Logger::FIXME)
              << "Caught OOM exception during test execution!";
          FATAL_ERROR_EXIT();
        } catch (std::exception const& ex) {
          LOG_TOPIC("793e3", FATAL, arangodb::Logger::FIXME)
              << "Caught STD exception during test execution: " << ex.what();
          FATAL_ERROR_EXIT();
        } catch (...) {
          LOG_TOPIC("c1d6d", FATAL, arangodb::Logger::FIXME)
              << "Caught unknown exception during test execution!";
          FATAL_ERROR_EXIT();
        }
      }

      _operationsCounter->done(_batchSize > 0 ? _batchSize : 1);
    }
  }

 private:
  //////////////////////////////////////////////////////////////////////////////
  /// @brief request location rewriter (injects database name)
  //////////////////////////////////////////////////////////////////////////////

  static std::string rewriteLocation(void* data, std::string const& location) {
    auto t = static_cast<arangobench::BenchmarkThread*>(data);

    TRI_ASSERT(t != nullptr);

    if (location.compare(0, 5, "/_db/") == 0) {
      // location already contains /_db/
      return location;
    }

    if (location[0] == '/') {
      return std::string("/_db/" + t->_databaseName + location);
    }
    return std::string("/_db/" + t->_databaseName + "/" + location);
  }

  //////////////////////////////////////////////////////////////////////////////
  /// @brief execute a batch request with numOperations parts
  //////////////////////////////////////////////////////////////////////////////

  void executeBatchRequest(const uint64_t numOperations) {
    static char const boundary[] = "XXXarangobench-benchmarkXXX";
    size_t blen = strlen(boundary);

    basics::StringBuffer batchPayload(true);
    int ret = batchPayload.reserve(numOperations * 1024);
    if (ret != TRI_ERROR_NO_ERROR) {
      LOG_TOPIC("bd98d", FATAL, arangodb::Logger::FIXME)
          << "Failed to reserve " << numOperations * 1024 << " bytes for "
          << numOperations << " batch operations: " << ret;
      FATAL_ERROR_EXIT();
    }
    for (uint64_t i = 0; i < numOperations; ++i) {
      // append boundary
      batchPayload.appendText(TRI_CHAR_LENGTH_PAIR("--"));
      batchPayload.appendText(boundary, blen);
      batchPayload.appendText(TRI_CHAR_LENGTH_PAIR("\r\n"));
      // append content-type, this will also begin the body
      batchPayload.appendText(TRI_CHAR_LENGTH_PAIR("Content-Type: "));
      batchPayload.appendText(StaticStrings::BatchContentType);
      batchPayload.appendText(TRI_CHAR_LENGTH_PAIR("\r\n\r\n"));

      // everything else (i.e. part request header & body) will get into the
      // body
      size_t const threadCounter = _counter++;
      size_t const globalCounter = _offset + threadCounter;
      std::string const url = _operation->url(_threadNumber, threadCounter, globalCounter);
      size_t payloadLength = 0;
      bool mustFree = false;
      char const* payload = _operation->payload(&payloadLength, _threadNumber,
                                                threadCounter, globalCounter, &mustFree);
      const rest::RequestType type =
          _operation->type(_threadNumber, threadCounter, globalCounter);
      if (url.empty()) {
        LOG_TOPIC("65da1", WARN, arangodb::Logger::FIXME) << "URL is empty!";
      }

      // headline, e.g. POST /... HTTP/1.1
      HttpRequest::appendMethod(type, &batchPayload);
      batchPayload.appendText(url);
      batchPayload.appendText(TRI_CHAR_LENGTH_PAIR(" HTTP/1.1\r\n"));
      batchPayload.appendText(TRI_CHAR_LENGTH_PAIR("\r\n"));

      // body
      batchPayload.appendText(payload, payloadLength);
      batchPayload.appendText(TRI_CHAR_LENGTH_PAIR("\r\n"));

      if (mustFree) {
        TRI_Free((void*)payload);
      }
    }

    // end of MIME
    batchPayload.appendText(TRI_CHAR_LENGTH_PAIR("--"));
    batchPayload.appendText(boundary, blen);
    batchPayload.appendText(TRI_CHAR_LENGTH_PAIR("--\r\n"));

    _headers[StaticStrings::ContentTypeHeader] =
        StaticStrings::MultiPartContentType + "; boundary=" + boundary;

    double start = TRI_microtime();
    httpclient::SimpleHttpResult* result =
        _httpClient->request(rest::RequestType::POST, "/_api/batch",
                             batchPayload.c_str(), batchPayload.length(), _headers);
    double delta = TRI_microtime() - start;
    oneTime(delta);
    _time += delta;

    if (result == nullptr || !result->isComplete()) {
      if (result != nullptr) {
        _operationsCounter->incIncompleteFailures(numOperations);
      }
      _operationsCounter->incFailures(numOperations);
      if (result != nullptr) {
        delete result;
      }
      _warningCount++;
      if (_warningCount < MaxWarnings) {
        LOG_TOPIC("04e82", WARN, arangodb::Logger::FIXME)
            << "batch operation failed because server did not reply";
      }
      return;
    }

    if (result->wasHttpError()) {
      _operationsCounter->incFailures(numOperations);

      _warningCount++;
      if (_warningCount < MaxWarnings) {
        LOG_TOPIC("948a3", WARN, arangodb::Logger::FIXME)
            << "batch operation failed with HTTP code " << result->getHttpReturnCode()
            << " - " << result->getHttpReturnMessage() << " ";
#ifdef ARANGODB_ENABLE_MAINTAINER_MODE
        LOG_TOPIC("ea82d", WARN, arangodb::Logger::FIXME)
            << "We tried to send this:\n "
            << std::string(batchPayload.c_str(), batchPayload.length());
#endif
      } else if (_warningCount == MaxWarnings) {
        LOG_TOPIC("3ac4b", WARN, arangodb::Logger::FIXME) << "...more warnings...";
      }
    } else {
      auto const& headers = result->getHeaderFields();
      auto it = headers.find(_errorHeader);

      if (it != headers.end()) {
        uint32_t errorCount = basics::StringUtils::uint32((*it).second);

        if (errorCount > 0) {
          _operationsCounter->incFailures(errorCount);
          _warningCount++;
          if (_warningCount < MaxWarnings) {
            LOG_TOPIC("b1db5", WARN, arangodb::Logger::FIXME)
                << "Server side warning count: " << errorCount;
            if (_verbose) {
              LOG_TOPIC("890f9", WARN, arangodb::Logger::FIXME)
                  << "Server reply: " << result->getBody().c_str();
#ifdef ARANGODB_ENABLE_MAINTAINER_MODE
              LOG_TOPIC("4a645", WARN, arangodb::Logger::FIXME)
                  << "We tried to send this:\n "
                  << std::string(batchPayload.c_str(), batchPayload.length());
#endif
            }
          }
        }
      }
    }
    delete result;
  }

  //////////////////////////////////////////////////////////////////////////////
  /// @brief execute a single request
  //////////////////////////////////////////////////////////////////////////////

  void executeSingleRequest() {
    size_t const threadCounter = _counter++;
    size_t const globalCounter = _offset + threadCounter;
    rest::RequestType const type =
        _operation->type(_threadNumber, threadCounter, globalCounter);
    std::string const url = _operation->url(_threadNumber, threadCounter, globalCounter);
    size_t payloadLength = 0;
    bool mustFree = false;

    // std::cout << "thread number #" << _threadNumber << ", threadCounter " <<
    // threadCounter << ", globalCounter " << globalCounter << "\n";
    char const* payload = _operation->payload(&payloadLength, _threadNumber,
                                              threadCounter, globalCounter, &mustFree);

    double start = TRI_microtime();
    httpclient::SimpleHttpResult* result =
        _httpClient->request(type, url, payload, payloadLength, _headers);
    double delta = TRI_microtime() - start;
    oneTime(delta);
    _time += delta;

    if (mustFree) {
      TRI_Free((void*)payload);
    }

    if (result == nullptr || !result->isComplete()) {
      _operationsCounter->incFailures(1);
      if (result != nullptr) {
        _operationsCounter->incIncompleteFailures(1);
        delete result;
      }
      _warningCount++;
      if (_warningCount < MaxWarnings) {
        LOG_TOPIC("f5982", WARN, arangodb::Logger::FIXME)
            << "single operation failed because server did not reply";
      }
      return;
    }

    if (result->wasHttpError()) {
      _operationsCounter->incFailures(1);

      _warningCount++;
      if (_warningCount < MaxWarnings) {
        LOG_TOPIC("fb835", WARN, arangodb::Logger::FIXME)
            << "request for URL '" << url << "' failed with HTTP code "
            << result->getHttpReturnCode() << ": "
            << std::string(result->getBody().c_str(), result->getBody().length());
      } else if (_warningCount == MaxWarnings) {
        LOG_TOPIC("6daf1", WARN, arangodb::Logger::FIXME) << "...more warnings...";
      }
    }
    delete result;
  }

 public:
  //////////////////////////////////////////////////////////////////////////////
  /// @brief set the threads offset value
  //////////////////////////////////////////////////////////////////////////////

  void setOffset(size_t offset) { _offset = offset; }

  //////////////////////////////////////////////////////////////////////////////
  /// @brief return the total time accumulated by the thread
  //////////////////////////////////////////////////////////////////////////////

  double getTime() const { return _time; }

 private:
  //////////////////////////////////////////////////////////////////////////////
  /// @brief the operation to benchmark
  //////////////////////////////////////////////////////////////////////////////

  arangobench::BenchmarkOperation* _operation;

  //////////////////////////////////////////////////////////////////////////////
  /// @brief condition variable
  //////////////////////////////////////////////////////////////////////////////

  basics::ConditionVariable* _startCondition;

  //////////////////////////////////////////////////////////////////////////////
  /// @brief start callback function
  //////////////////////////////////////////////////////////////////////////////

  void (*_callback)();

  //////////////////////////////////////////////////////////////////////////////
  /// @brief our thread number
  //////////////////////////////////////////////////////////////////////////////

  int _threadNumber;

  //////////////////////////////////////////////////////////////////////////////
  /// @brief batch size
  //////////////////////////////////////////////////////////////////////////////

  uint64_t const _batchSize;

  //////////////////////////////////////////////////////////////////////////////
  /// @brief warning counter
  //////////////////////////////////////////////////////////////////////////////

  int _warningCount;

  //////////////////////////////////////////////////////////////////////////////
  /// @brief benchmark counter
  //////////////////////////////////////////////////////////////////////////////

  arangobench::BenchmarkCounter<uint64_t>* _operationsCounter;

  //////////////////////////////////////////////////////////////////////////////
  /// @brief client feature
  //////////////////////////////////////////////////////////////////////////////

  ClientFeature& _client;

  //////////////////////////////////////////////////////////////////////////////
  /// @brief extra request headers
  //////////////////////////////////////////////////////////////////////////////

  std::unordered_map<std::string, std::string> _headers;

  //////////////////////////////////////////////////////////////////////////////
  /// @brief database name
  //////////////////////////////////////////////////////////////////////////////

  std::string const _databaseName;

  //////////////////////////////////////////////////////////////////////////////
  /// @brief HTTP username
  //////////////////////////////////////////////////////////////////////////////

  std::string const _username;

  //////////////////////////////////////////////////////////////////////////////
  /// @brief HTTP password
  //////////////////////////////////////////////////////////////////////////////

  std::string const _password;

  //////////////////////////////////////////////////////////////////////////////
  /// @brief use HTTP keep-alive
  //////////////////////////////////////////////////////////////////////////////

  bool _keepAlive;

  //////////////////////////////////////////////////////////////////////////////
  /// @brief send async requests
  //////////////////////////////////////////////////////////////////////////////

  bool _async;

  //////////////////////////////////////////////////////////////////////////////
  /// @brief underlying http client
  //////////////////////////////////////////////////////////////////////////////

  std::unique_ptr<arangodb::httpclient::SimpleHttpClient> _httpClient;

  //////////////////////////////////////////////////////////////////////////////
  /// @brief thread offset value
  //////////////////////////////////////////////////////////////////////////////

  uint64_t _offset;

public:

  //////////////////////////////////////////////////////////////////////////////
  /// @brief thread counter value
  //////////////////////////////////////////////////////////////////////////////

  uint64_t _counter;

  //////////////////////////////////////////////////////////////////////////////
  /// @brief time
  //////////////////////////////////////////////////////////////////////////////

  double _time;

  //////////////////////////////////////////////////////////////////////////////
  /// @brief lower-case error header we look for
  //////////////////////////////////////////////////////////////////////////////

  std::string _errorHeader;

  //////////////////////////////////////////////////////////////////////////////
  /// @brief maximum number of warnings to be displayed per thread
  //////////////////////////////////////////////////////////////////////////////

  static int const MaxWarnings = 5;

  //////////////////////////////////////////////////////////////////////////////
  /// @brief output replies if error count in http relpy > 0
  //////////////////////////////////////////////////////////////////////////////

  bool _verbose;

  //////////////////////////////////////////////////////////////////////////////
  /// @brief the number of operations done
  //////////////////////////////////////////////////////////////////////////////

  double _minTime;

  //////////////////////////////////////////////////////////////////////////////
  /// @brief the number of operations done
  //////////////////////////////////////////////////////////////////////////////

  double _maxTime;

  //////////////////////////////////////////////////////////////////////////////
  /// @brief the number of operations done
  //////////////////////////////////////////////////////////////////////////////

  double _avgTime;

  uint64_t _histogramNumIntervals;
  double _histogramIntervalSize;
  double _histogramScope;
  std::vector<size_t> _histogram;
};
}  // namespace arangobench
}  // namespace arangodb

#endif
