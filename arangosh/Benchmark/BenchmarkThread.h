////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2014-2021 ArangoDB GmbH, Cologne, Germany
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

#pragma once

#include <cmath>
#include <memory>
#include <string>
#include <vector>

#include "Basics/Common.h"

#include "Basics/ConditionLocker.h"
#include "Basics/ConditionVariable.h"
#include "Basics/Exceptions.h"
#include "Basics/StaticStrings.h"
#include "Basics/Thread.h"
#include "Basics/application-exit.h"
#include "Basics/debugging.h"
#include "Basics/system-functions.h"
#include "Benchmark/BenchmarkCounter.h"
#include "Benchmark/BenchmarkOperation.h"
#include "Benchmark/BenchmarkStats.h"
#include "Logger/LogMacros.h"
#include "Logger/Logger.h"
#include "Logger/LoggerStream.h"
#include "Rest/HttpRequest.h"
#include "Shell/ClientFeature.h"
#include "SimpleHttpClient/GeneralClientConnection.h"
#include "SimpleHttpClient/SimpleHttpClient.h"
#include "SimpleHttpClient/SimpleHttpResult.h"

#include <velocypack/Builder.h>
#include <velocypack/Dumper.h>
#include <velocypack/Sink.h>

namespace arangodb {
namespace arangobench {

class BenchmarkThread : public arangodb::Thread {
 public:
  BenchmarkThread(application_features::ApplicationServer& server,
                  BenchmarkOperation* operation,
                  basics::ConditionVariable* condition, void (*callback)(),
                  size_t threadNumber, uint64_t const batchSize,
                  BenchmarkCounter<uint64_t>* operationsCounter,
                  ClientFeature& client, bool keepAlive, bool async,
                  double histogramIntervalSize, uint64_t histogramNumIntervals,
                  bool generateHistogram)
      : Thread(server, "BenchmarkThread"),
        _operation(operation),
        _startCondition(condition),
        _callback(callback),
        _threadNumber(threadNumber),
        _batchSize(batchSize),
        _warningCount(0),
        _operationsCounter(operationsCounter),
        _client(client),
        _databaseName(client.databaseName()),
        _username(client.username()),
        _password(client.password()),
        _keepAlive(keepAlive),
        _async(async),
        _useVelocyPack(_batchSize == 0),
        _generateHistogram(generateHistogram),
        _httpClient(nullptr),
        _offset(0),
        _counter(0),
        _histogramNumIntervals(histogramNumIntervals),
        _histogramIntervalSize(histogramIntervalSize),
        _histogramScope(histogramIntervalSize * histogramNumIntervals),
        _histogram(histogramNumIntervals, 0) {}

  ~BenchmarkThread() { shutdown(); }

  void trackTime(double time) {
    _stats.track(time);

    if (_generateHistogram) {
      if (_histogramScope == 0.0) {
        _histogramScope = time * 20;
        _histogramIntervalSize = _histogramScope / _histogramNumIntervals;
      }

      uint64_t bucket =
          static_cast<uint64_t>(lround(time / _histogramIntervalSize));
      if (bucket >= _histogramNumIntervals) {
        bucket = _histogramNumIntervals - 1;
      }

      ++_histogram[bucket];
    }
  }

  std::vector<double> getPercentiles(std::vector<double> const& which,
                                     double& histogramIntervalSize) {
    std::vector<double> res(which.size(), 0.0);
    std::vector<size_t> counts(which.size());
    size_t i = 0;
    histogramIntervalSize = _histogramIntervalSize;
    uint64_t divisor = std::max(std::uint64_t(1), _batchSize);
    while (i < which.size()) {
      counts[i] =
          static_cast<size_t>(lround(_counter * which[i] / divisor / 100.0));
      i++;
    }
    i = 0;
    size_t nextCount = counts[i];
    size_t count = 0;
    size_t vecPos = 0;
    while (vecPos < _histogramNumIntervals && i < which.size()) {
      count += _histogram[vecPos];
      while (count >= nextCount) {
        res[i] = _histogramIntervalSize * vecPos;
        i++;
        if (i >= which.size()) {
          return res;
        }
        nextCount = counts[i];
      }
      vecPos++;
    }
    return res;
  }

  /// @brief set the thread's offset value
  void setOffset(size_t offset) { _offset = offset; }

  // return a copy of the thread's stats
  BenchmarkStats stats() const { return _stats; }

 protected:
  void run() override {
    try {
      _httpClient = _client.createHttpClient(_threadNumber);
    } catch (...) {
      LOG_TOPIC("b69d7", FATAL, arangodb::Logger::BENCH)
          << "cannot create server connection, giving up!";
      FATAL_ERROR_EXIT();
    }

    _httpClient->params().setLocationRewriter(this, &rewriteLocation);

    _httpClient->params().setUserNamePassword("/", _username, _password);
    _httpClient->params().setKeepAlive(_keepAlive);

    // test the connection
    std::unique_ptr<httpclient::SimpleHttpResult> result(_httpClient->request(
        rest::RequestType::GET, "/_api/version", nullptr, 0, _headers));

    if (!result || !result->isComplete()) {
      LOG_TOPIC("5cda7", FATAL, arangodb::Logger::BENCH)
          << "could not connect to server";
      FATAL_ERROR_EXIT();
    }

    // if we're the first thread, set up the test
    if (_threadNumber == 0) {
      if (!_operation->setUp(_httpClient.get())) {
        LOG_TOPIC("528b6", FATAL, arangodb::Logger::BENCH)
            << "could not set up the test";
        FATAL_ERROR_EXIT();
      }
    }

    if (_async) {
      _headers[StaticStrings::Async] = "true";
    }

    if (_useVelocyPack) {
      _headers[StaticStrings::ContentTypeHeader] = StaticStrings::MimeTypeVPack;
      _headers[StaticStrings::Accept] = StaticStrings::MimeTypeVPack;
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

      try {
        if (_batchSize < 1) {
          executeSingleRequest();
        } else {
          executeBatchRequest(numOps);
        }
      } catch (std::bad_alloc const&) {
        LOG_TOPIC("29451", FATAL, arangodb::Logger::BENCH)
            << "Caught OOM exception during test execution!";
        FATAL_ERROR_EXIT();
      } catch (std::exception const& ex) {
        LOG_TOPIC("793e3", FATAL, arangodb::Logger::BENCH)
            << "Caught STD exception during test execution: " << ex.what();
        FATAL_ERROR_EXIT();
      } catch (...) {
        LOG_TOPIC("c1d6d", FATAL, arangodb::Logger::BENCH)
            << "Caught unknown exception during test execution!";
        FATAL_ERROR_EXIT();
      }

      _operationsCounter->done(_batchSize > 0 ? _batchSize : 1);
    }
  }

 private:
  /// @brief request location rewriter (injects database name)
  static std::string rewriteLocation(void* data, std::string const& location) {
    auto t = static_cast<arangobench::BenchmarkThread*>(data);

    TRI_ASSERT(t != nullptr);

    if (location.compare(0, 5, "/_db/") == 0) {
      // location already contains /_db/
      return location;
    }

    if (location[0] == '/') {
      return std::string("/_db/" +
                         basics::StringUtils::urlEncode(t->_databaseName) +
                         location);
    }
    return std::string("/_db/" +
                       basics::StringUtils::urlEncode(t->_databaseName) + "/" +
                       location);
  }

  /// @brief execute a batch request with numOperations parts
  void executeBatchRequest(uint64_t numOperations) {
    TRI_ASSERT(!_useVelocyPack);

    static char const boundary[] = "XXXarangobench-benchmarkXXX";
    size_t blen = strlen(boundary);

    _payloadBuffer.clear();
    for (uint64_t i = 0; i < numOperations; ++i) {
      // append boundary
      _payloadBuffer.append(TRI_CHAR_LENGTH_PAIR("--"));
      _payloadBuffer.append(boundary, blen);
      _payloadBuffer.append(TRI_CHAR_LENGTH_PAIR("\r\n"));
      // append content-type, this will also begin the body
      _payloadBuffer.append(TRI_CHAR_LENGTH_PAIR("Content-Type: "));
      _payloadBuffer.append(StaticStrings::BatchContentType);
      _payloadBuffer.append(TRI_CHAR_LENGTH_PAIR("\r\n\r\n"));

      // everything else (i.e. part request header & body) will get into the
      // body
      size_t const threadCounter = _counter++;
      size_t const globalCounter = _offset + threadCounter;

      _requestData.clear();
      _operation->buildRequest(_threadNumber, threadCounter, globalCounter,
                               _requestData);

      // headline, e.g. POST /... HTTP/1.1
      _payloadBuffer.append(HttpRequest::translateMethod(_requestData.type));
      _payloadBuffer.push_back(' ');
      _payloadBuffer.append(_requestData.url);
      _payloadBuffer.append(TRI_CHAR_LENGTH_PAIR(" HTTP/1.1\r\n\r\n"));
      velocypack::Slice payloadSlice = _requestData.payload.slice();
      if (!payloadSlice.isNone()) {
        velocypack::StringSink sink(&_payloadBuffer);
        velocypack::Dumper dumper(&sink);
        dumper.dump(payloadSlice);
        _payloadBuffer.append(TRI_CHAR_LENGTH_PAIR("\r\n"));
      }
    }

    // end of MIME
    _payloadBuffer.append(TRI_CHAR_LENGTH_PAIR("--"));
    _payloadBuffer.append(boundary, blen);
    _payloadBuffer.append(TRI_CHAR_LENGTH_PAIR("--\r\n"));

    _headers[StaticStrings::ContentTypeHeader] =
        StaticStrings::MultiPartContentType + "; boundary=" + boundary;

    double start = TRI_microtime();
    std::unique_ptr<httpclient::SimpleHttpResult> result(_httpClient->request(
        rest::RequestType::POST, "/_api/batch", _payloadBuffer.data(),
        _payloadBuffer.size(), _headers));
    double delta = TRI_microtime() - start;
    trackTime(delta);
    processResponse(result.get(), /*batch*/ true, numOperations);

    _httpClient->recycleResult(std::move(result));
  }

  /// @brief execute a single request
  void executeSingleRequest() {
    size_t const threadCounter = _counter++;
    size_t const globalCounter = _offset + threadCounter;

    _requestData.clear();
    _operation->buildRequest(_threadNumber, threadCounter, globalCounter,
                             _requestData);

    velocypack::Slice payloadSlice = _requestData.payload.slice();
    char const* p = nullptr;
    size_t length = 0;

    if (!payloadSlice.isNone()) {
      if (_useVelocyPack) {
        // send as raw velocypack
        p = payloadSlice.startAs<char const>();
        length = payloadSlice.byteSize();
      } else {
        // send as stringified JSON
        // dump JSON string into reusable payload string
        _payloadBuffer.clear();
        velocypack::StringSink sink(&_payloadBuffer);
        velocypack::Dumper dumper(&sink);
        dumper.dump(payloadSlice);
        p = _payloadBuffer.data();
        length = _payloadBuffer.size();
      }
    }

    TRI_ASSERT(p != nullptr || length == 0);

    double start = TRI_microtime();
    std::unique_ptr<httpclient::SimpleHttpResult> result(_httpClient->request(
        _requestData.type, _requestData.url, p, length, _headers));
    double delta = TRI_microtime() - start;
    trackTime(delta);
    processResponse(result.get(), /*batch*/ false, 1);

    _httpClient->recycleResult(std::move(result));
  }

  void processResponse(httpclient::SimpleHttpResult const* result, bool batch,
                       uint64_t numOperations) {
    char const* type = (batch ? "batch" : "single");
    TRI_ASSERT(numOperations > 0);

    if (result != nullptr && result->isComplete() && !result->wasHttpError()) {
      if (batch) {
        // for batch requests we have to check the error header in addition
        auto const& headers = result->getHeaderFields();
        if (auto it = headers.find(StaticStrings::Errors);
            it != headers.end()) {
          uint32_t errorCount = basics::StringUtils::uint32((*it).second);
          if (errorCount > 0) {
            _operationsCounter->incFailures(errorCount);
            if (++_warningCount < maxWarnings) {
              LOG_TOPIC("b1db5", WARN, arangodb::Logger::BENCH)
                  << type
                  << " operation sServer side warning count: " << errorCount;
            }
          }
        }
      }

      return;
    }

    _operationsCounter->incFailures(numOperations);
    if (result != nullptr && !result->isComplete()) {
      _operationsCounter->incIncompleteFailures(numOperations);
    }
    if (++_warningCount < maxWarnings) {
      if (result != nullptr && result->wasHttpError()) {
        LOG_TOPIC("fb835", WARN, arangodb::Logger::BENCH)
            << type << " request for URL '" << _requestData.url
            << "' failed with HTTP code " << result->getHttpReturnCode() << ": "
            << std::string(result->getBody().c_str(),
                           result->getBody().length());
      } else {
        LOG_TOPIC("f5982", WARN, arangodb::Logger::BENCH)
            << type << " operation failed because server did not reply";
      }
    } else if (_warningCount == maxWarnings) {
      LOG_TOPIC("6daf1", WARN, arangodb::Logger::BENCH)
          << "...more warnings...";
    }
  }

 private:
  /// @brief the request builder with HTTP request values
  /// (will be recycled for each request)
  BenchmarkOperation::RequestData _requestData;

  /// @brief temporary buffer for stringified JSON values or
  /// for batch requests
  /// (will be recycled for each request)
  std::string _payloadBuffer;

  /// @brief the operation to benchmark
  arangobench::BenchmarkOperation* _operation;

  /// @brief condition variable
  basics::ConditionVariable* _startCondition;

  /// @brief start callback function
  void (*_callback)();

  /// @brief our thread number
  size_t _threadNumber;

  /// @brief batch size
  uint64_t const _batchSize;

  /// @brief warning counter
  int _warningCount;

  /// @brief benchmark counter
  arangobench::BenchmarkCounter<uint64_t>* _operationsCounter;

  /// @brief client feature
  ClientFeature& _client;

  /// @brief extra request headers
  std::unordered_map<std::string, std::string> _headers;

  /// @brief database name
  std::string const _databaseName;

  /// @brief HTTP username
  std::string const _username;

  /// @brief HTTP password
  std::string const _password;

  /// @brief use HTTP keep-alive
  bool _keepAlive;

  /// @brief send async requests
  bool const _async;

  /// @brief send velocypack-encoded data
  bool const _useVelocyPack;

  /// @brief show the histogram or not
  bool const _generateHistogram;

  /// @brief underlying http client
  std::unique_ptr<arangodb::httpclient::SimpleHttpClient> _httpClient;

  /// @brief thread offset value
  uint64_t _offset;

  /// @brief statistics for the thread
  BenchmarkStats _stats;

 public:
  /// @brief thread counter value
  uint64_t _counter;

  /// @brief maximum number of warnings to be displayed per thread
  static constexpr int maxWarnings = 5;

  uint64_t _histogramNumIntervals;
  double _histogramIntervalSize;
  double _histogramScope;
  std::vector<size_t> _histogram;
};
}  // namespace arangobench
}  // namespace arangodb
