////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2014-2024 ArangoDB GmbH, Cologne, Germany
/// Copyright 2004-2014 triAGENS GmbH, Cologne, Germany
///
/// Licensed under the Business Source License 1.1 (the "License");
/// you may not use this file except in compliance with the License.
/// You may obtain a copy of the License at
///
///     https://github.com/arangodb/arangodb/blob/devel/LICENSE
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
#include <shared_mutex>

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
#include "SimpleHttpClient/HttpResponseChecker.h"
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
                  size_t threadNumber,
                  BenchmarkCounter<uint64_t>* operationsCounter,
                  ClientFeature& client, bool keepAlive, bool async,
                  double histogramIntervalSize, uint64_t histogramNumIntervals,
                  bool generateHistogram)
      : Thread(server, "BenchmarkThread"),
        _operation(operation),
        _startCondition(condition),
        _callback(callback),
        _threadNumber(threadNumber),
        _warningCount(0),
        _operationsCounter(operationsCounter),
        _client(client),
        _keepAlive(keepAlive),
        _async(async),
        _useVelocyPack(true),
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
    std::lock_guard lock{_mutex};
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
    std::shared_lock lock{_mutex};
    histogramIntervalSize = _histogramIntervalSize;
    while (i < which.size()) {
      counts[i] = static_cast<size_t>(lround(_counter * which[i] / 100.0));
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
  BenchmarkStats stats() const {
    std::shared_lock lock{_mutex};
    return _stats;
  }

 protected:
  void run() override {
    try {
      _httpClient = _client.createHttpClient(_threadNumber);
    } catch (...) {
      LOG_TOPIC("b69d7", FATAL, arangodb::Logger::BENCH)
          << "cannot create server connection, giving up!";
      FATAL_ERROR_EXIT();
    }

    _httpClient->params().setKeepAlive(_keepAlive);

    // test the connection
    std::unique_ptr<httpclient::SimpleHttpResult> result(_httpClient->request(
        rest::RequestType::GET, "/_api/version", nullptr, 0, _headers));
    auto check = arangodb::HttpResponseChecker::check(
        _httpClient->getErrorMessage(), result.get());
    if (check.fail()) {
      LOG_TOPIC("5cda7", FATAL, arangodb::Logger::BENCH)
          << check.errorMessage();
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
      std::unique_lock guard{_startCondition->mutex};
      _startCondition->cv.wait(guard);
    }

    while (!isStopping()) {
      uint64_t numOps = _operationsCounter->next(0);

      if (numOps == 0) {
        break;
      }

      try {
        executeRequest();
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

      _operationsCounter->done(1);
    }
  }

 private:
  /// @brief execute a single request
  void executeRequest() {
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
    processResponse(result.get());

    _httpClient->recycleResult(std::move(result));
  }

  void processResponse(httpclient::SimpleHttpResult const* result) {
    auto check = arangodb::HttpResponseChecker::check(
        _httpClient->getErrorMessage(), result);
    if (check.ok()) {
      return;
    }

    _operationsCounter->incFailures(1);
    if (result != nullptr && !result->isComplete()) {
      _operationsCounter->incIncompleteFailures(1);
    }
    if (++_warningCount < maxWarnings) {
      if (check.fail()) {
        LOG_TOPIC("fb835", WARN, arangodb::Logger::BENCH)
            << "Request for URL '" << _requestData.url
            << "': " << check.errorMessage();
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

  /// @brief temporary buffer for stringified JSON values
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

  /// @brief warning counter
  int _warningCount;

  /// @brief benchmark counter
  arangobench::BenchmarkCounter<uint64_t>* _operationsCounter;

  /// @brief client feature
  ClientFeature& _client;

  /// @brief extra request headers
  std::unordered_map<std::string, std::string> _headers;

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

  mutable std::shared_mutex _mutex;
};
}  // namespace arangobench
}  // namespace arangodb
