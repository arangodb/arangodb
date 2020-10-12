////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2014-2020 ArangoDB GmbH, Cologne, Germany
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
/// @author Matthew Von-Maszewski
////////////////////////////////////////////////////////////////////////////////

#ifndef ARANGODB_IMPORT_QUICK_HIST_H
#define ARANGODB_IMPORT_QUICK_HIST_H 1

#include <algorithm>
#include <chrono>
#include <ctime>
#include <future>
#include <iomanip>
#include <mutex>
#include <numeric>
#include <vector>

#include "Basics/ConditionLocker.h"
#include "Basics/ConditionVariable.h"
#include "Basics/Thread.h"
#include "Logger/LogMacros.h"
#include "Logger/Logger.h"
#include "Logger/LoggerStream.h"

namespace arangodb {
namespace import {
//
// quickly written histogram class for debugging
//
class QuickHistogram : public arangodb::Thread {
 private:
  QuickHistogram(QuickHistogram const&) = delete;
  QuickHistogram& operator=(QuickHistogram const&) = delete;

 public:
  explicit QuickHistogram(application_features::ApplicationServer& server)
      : Thread(server, "QuickHistogram"),
        _writingLatencies(nullptr),
        _readingLatencies(nullptr),
        _objectsWriting(0),
        _objectsReading(0),
        _threadRunning(false) {}

  ~QuickHistogram() { shutdown(); }

  void beginShutdown() override {
    _threadRunning = false;
    Thread::beginShutdown();

    // wake up the thread that may be waiting in run()
    CONDITION_LOCKER(guard, _condvar);
    guard.broadcast();
  }

  void postLatency(std::chrono::microseconds latency, uint64_t objects = 1) {
    if (_threadRunning.load()) {
      CONDITION_LOCKER(guard, _condvar);

      // initial case has this called in a destructor ... block exception
      try {
        _writingLatencies->push_back(latency);
      } catch (...) {
        LOG_TOPIC("08a26", ERR, arangodb::Logger::FIXME)
            << "QuickHistogram::postLatency() had exception doing a push_back "
               "(out of ram)";
      }
      _objectsWriting += objects;
    }
  }

 protected:
  void run() override {
    _intervalStart = std::chrono::steady_clock::now();
    _measuringStart = _intervalStart;
    LOG_TOPIC("f206c", INFO, arangodb::Logger::FIXME)
        << R"("elapsed","window","n","min","mean","median","95th","99th","99.9th","max","objects","clock")";

    _writingLatencies = &_vectors[0];
    _readingLatencies = &_vectors[1];
    _threadRunning.store(true);

    while (_threadRunning.load()) {
      CONDITION_LOCKER(guard, _condvar);

      std::chrono::seconds tenSec(10);
      _condvar.wait(tenSec);

      LatencyVec_t* temp;
      temp = _writingLatencies;
      _writingLatencies = _readingLatencies;
      _readingLatencies = temp;
      _objectsReading = _objectsWriting.load();
      _objectsWriting = 0;

      printInterval(!_threadRunning.load());
    }
  }  // run

  //////////////////////////////////////////////////////////////////////////////
  /// @brief
  //////////////////////////////////////////////////////////////////////////////
  std::chrono::steady_clock::time_point _measuringStart;
  std::chrono::steady_clock::time_point _intervalStart;

  typedef std::vector<std::chrono::microseconds> LatencyVec_t;
  LatencyVec_t _vectors[2];
  LatencyVec_t *_writingLatencies, *_readingLatencies;
  std::atomic<uint64_t> _objectsWriting, _objectsReading;

  basics::ConditionVariable _condvar;
  std::atomic<bool> _threadRunning;

 protected:
  void printInterval(bool force = false) {
    std::chrono::steady_clock::time_point intervalEnd;
    std::chrono::milliseconds intervalDiff, measuringDiff;
    std::chrono::microseconds zeroMicros = std::chrono::microseconds(0);
    intervalEnd = std::chrono::steady_clock::now();
    intervalDiff =
        std::chrono::duration_cast<std::chrono::milliseconds>(intervalEnd - _intervalStart);

    {
      // retest within mutex
      intervalEnd = std::chrono::steady_clock::now();
      intervalDiff = std::chrono::duration_cast<std::chrono::milliseconds>(
          intervalEnd - _intervalStart);
      if (std::chrono::milliseconds(10000) <= intervalDiff || force) {
        double fp_measuring, fp_interval;
        size_t num = _readingLatencies->size();

        measuringDiff = std::chrono::duration_cast<std::chrono::milliseconds>(
            intervalEnd - _measuringStart);
        fp_measuring = measuringDiff.count() / 1000.0;
        fp_interval = intervalDiff.count() / 1000.0;

        std::chrono::microseconds mean, median, per95, per99, per99_9;

        if (0 != num) {
          bool odd(num & 1);
          size_t half(num / 2);

          std::chrono::microseconds sum =
              std::accumulate(_readingLatencies->begin(), _readingLatencies->end(), zeroMicros);
          mean = sum / num;

          if (1 == num) {
            median = _readingLatencies->at(0);
          } else if (odd) {
            median = (_readingLatencies->at(half) + _readingLatencies->at(half + 1)) / 2;
          } else {
            median = _readingLatencies->at(half);
          }
        } else {
          mean = zeroMicros;
          median = zeroMicros;
        }  // else

        std::sort(_readingLatencies->begin(), _readingLatencies->end());

        // close but not exact math for percentiles
        per95 = calcPercentile(*_readingLatencies, 950);
        per99 = calcPercentile(*_readingLatencies, 990);
        per99_9 = calcPercentile(*_readingLatencies, 999);

        // timestamp to help match to other logs ...
        auto t = std::time(nullptr);
        auto tm = *std::localtime(&t);

        // new age string buffering & formatting
        std::ostringstream oss;
        oss << std::put_time(&tm, "%m-%d-%Y %H:%M:%S");

        LOG_TOPIC("8a76c",INFO, arangodb::Logger::FIXME) << Logger::FIXED(fp_measuring,3) << ","
                                                 << Logger::FIXED(fp_interval,3) << ","
                                                 << num << ","
                                                 << ((0 != num) ? _readingLatencies->at(0).count() : 0) << ","
                                                 << mean.count() << "," << median.count() << ","
                                                 << per95.count() << "," << per99.count() << ","
                                                 << per99_9.count() << ","
                                                 << ((0 != num) ? _readingLatencies->at(num - 1).count() : 0) << ","
                                                 << _objectsReading.load() << "," << oss.str();

        _readingLatencies->clear();
        _intervalStart = intervalEnd;
      }
    }
  }

  //
  // calculation taken from
  // http://www.dummies.com/education/math/statistics/how-to-calculate-percentiles-in-statistics
  //  (zero and one size vector calculations not included in that link)
  std::chrono::microseconds calcPercentile(std::vector<std::chrono::microseconds>& SortedLatencies,
                                           int Percentile) {
    std::chrono::microseconds retVal = std::chrono::microseconds(0);

    if (0 < SortedLatencies.size()) {
      // useful vector size

      // percentiles are x10 ... so 95% is 950 and 99.9% is 999
      size_t index = SortedLatencies.size() * Percentile;
      size_t remainder = index % 1000;
      index /= 1000;

      // index is supposed to be x'th entry in list. x'th entry is 1 based,
      // vector is 0 based,
      //  then fix range
      if (0 < index) {
        --index;
      }  // if
      size_t nextIndex = ((index + 1) < SortedLatencies.size()) ? index + 1 : index;

      if (0 == remainder) {
        // whole number index
        std::chrono::microseconds low, high;
        low = SortedLatencies[index];
        high = SortedLatencies[nextIndex];
        retVal = (low + high) / 2;
      } else {
        // fractional index, round up ... but we are 0 based, so already one
        // higher
        retVal = SortedLatencies[nextIndex];
      }  // else
    } else {
      // vector too small
      if (1 == SortedLatencies.size()) {
        retVal = SortedLatencies[0] / 2;
      } else {
        retVal = std::chrono::microseconds(0);
      }  // else
    }    // else

    return retVal;
  }  // calcPercentile
};

class QuickHistogramTimer {
 public:
  explicit QuickHistogramTimer(QuickHistogram& histo)
    : _intervalStart(std::chrono::steady_clock::now()), _histogram(histo),
    _objects(1) {}

  explicit QuickHistogramTimer(QuickHistogram& histo, uint64_t objects)
    : _intervalStart(std::chrono::steady_clock::now()), _histogram(histo),
    _objects(objects) {}

  ~QuickHistogramTimer() {
    std::chrono::microseconds latency;

    latency = std::chrono::duration_cast<std::chrono::microseconds>(
        std::chrono::steady_clock::now() - _intervalStart);
    _histogram.postLatency(latency, _objects);
  }

  std::chrono::steady_clock::time_point _intervalStart;
  QuickHistogram& _histogram;
  uint64_t _objects;
};  // QuickHistogramTimer
}  // namespace import
}  // namespace arangodb
#endif
