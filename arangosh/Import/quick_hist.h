////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2017 ArangoDB GmbH, Cologne, Germany
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

#include <chrono>
#include <ctime>
#include <future>
#include <iomanip>
#include <mutex>
#include <numeric>
#include <vector>

namespace arangodb {
namespace import {
//
// quickly written histogram class for debugging.  Too awkward for
//  production
//
class QuickHistogram {
 private:
  QuickHistogram(QuickHistogram const&) = delete;
  QuickHistogram& operator=(QuickHistogram const&) = delete;

 public:
  QuickHistogram()
  {
    _interval_start=std::chrono::steady_clock::now();
    _measuring_start = _interval_start;
    printf(R"("elapsed","window","n","min","mean","median","95th","99th","99.9th","max","unused1","clock")" "\n");

    _writingLatencies = &_vectors[0];
    _readingLatencies = &_vectors[1];
    _threadRunning.store(true);
    _threadFuture = std::async(std::launch::async, &QuickHistogram::ThreadLoop, this);
  }


  ~QuickHistogram()
  {
    {
      std::unique_lock<std::mutex> lg(_mutex);
      _threadRunning.store(false);
      _condvar.notify_one();
    }
    _threadFuture.wait();
  }


  void post_latency(std::chrono::microseconds latency) {
    {
      std::unique_lock<std::mutex> lg(_mutex);

      _writingLatencies->push_back(latency);
    }
  }

  //////////////////////////////////////////////////////////////////////////////
  /// @brief
  //////////////////////////////////////////////////////////////////////////////
  std::chrono::steady_clock::time_point _measuring_start;
  std::chrono::steady_clock::time_point _interval_start;

  typedef std::vector<std::chrono::microseconds> LatencyVec_t;
  LatencyVec_t _vectors[2];
  LatencyVec_t * _writingLatencies, * _readingLatencies;

  std::mutex _mutex;
  std::condition_variable _condvar;
  std::future<void> _threadFuture;
  std::atomic<bool> _threadRunning;

 protected:
  void ThreadLoop() {
    while(_threadRunning.load()) {
      std::unique_lock<std::mutex> lg(_mutex);

      std::chrono::seconds ten_sec(10);
      _condvar.wait_for(lg, ten_sec);

      LatencyVec_t * temp;
      temp = _writingLatencies;
      _writingLatencies = _readingLatencies;
      _readingLatencies = temp;

      print_interval();
    }

  } // ThreadLoop


  void print_interval(bool force=false) {
    std::chrono::steady_clock::time_point interval_end;
    std::chrono::milliseconds interval_diff, measuring_diff;
    std::chrono::microseconds sum, zero_micros;


    zero_micros = std::chrono::microseconds(0);
    interval_end=std::chrono::steady_clock::now();
    interval_diff=std::chrono::duration_cast<std::chrono::milliseconds>(interval_end - _interval_start);

    {
      // retest within mutex
      interval_end=std::chrono::steady_clock::now();
      interval_diff=std::chrono::duration_cast<std::chrono::milliseconds>(interval_end - _interval_start);
      if (std::chrono::milliseconds(10000) <= interval_diff || force) {
        double fp_measuring, fp_interval;
        size_t num=_readingLatencies->size();

        measuring_diff = std::chrono::duration_cast<std::chrono::milliseconds>(interval_end - _measuring_start);
        fp_measuring = measuring_diff.count() / 1000.0;
        fp_interval = interval_diff.count() / 1000.0;

        std::chrono::microseconds mean, median, per95, per99, per99_9;

        if (0 != num) {
          bool odd(num & 1);
          size_t half(num/2);

          sum = std::accumulate(_readingLatencies->begin(), _readingLatencies->end(), zero_micros);
          mean = sum / num;

          if (1==num) {
            median = _readingLatencies->at(0);
          } else if (odd) {
            median = (_readingLatencies->at(half) + _readingLatencies->at(half +1)) / 2;
          } else {
            median = _readingLatencies->at(half);
          }
        } else {
          sum = zero_micros;
          mean = zero_micros;
          median = zero_micros;
        } // else

        sort(_readingLatencies->begin(), _readingLatencies->end());

        // close but not exact math for percentiles
        per95 = CalcPercentile(*_readingLatencies, 950);
        per99 = CalcPercentile(*_readingLatencies, 990);
        per99_9 = CalcPercentile(*_readingLatencies, 999);

        // timestamp to help match to other logs ...
        auto t = std::time(nullptr);
        auto tm = *std::localtime(&t);

        std::ostringstream oss;
        oss << std::put_time(&tm, "%m-%d-%Y %H:%M:%S");
        auto str = oss.str();

        printf("%.3f,%.3f,%zd,%ld,%ld,%ld,%ld,%ld,%ld,%ld,%d,%s\n",
               fp_measuring, fp_interval, num,
               (0!=num) ? _readingLatencies->at(0).count() : 0,
               mean.count(),median.count(),
               per95.count(), per99.count(), per99_9.count(),
               (0!=num) ? _readingLatencies->at(num-1).count() : 0,
               0, str.c_str());
        _readingLatencies->clear();
        _interval_start=interval_end;
      }
    }
  }

  //
  // calculation taken from http://www.dummies.com/education/math/statistics/how-to-calculate-percentiles-in-statistics
  //  (zero and one size vector calculations not included in that link)
  std::chrono::microseconds CalcPercentile(std::vector<std::chrono::microseconds> &SortedLatencies, int Percentile) {
    size_t index, remainder, next_index;
    std::chrono::microseconds ret_val;

    ret_val=std::chrono::microseconds(0);

    if (0 < SortedLatencies.size()) {
      // useful vector size

      // percentiles are x10 ... so 95% is 950 and 99.9% is 999
      index = SortedLatencies.size() * Percentile;
      remainder = index % 1000;
      index /= 1000;

      // index is supposed to be x'th entry in list. x'th entry is 1 based, vector is 0 based,
      //  then fix range
      if (0 < index) {
        --index;
      } // if
      next_index = ((index+1)<SortedLatencies.size()) ? index+1 : index;

      if (0 == remainder) {
        // whole number index
        std::chrono::microseconds low, high;
        low = SortedLatencies[index];
        high = SortedLatencies[next_index];
        ret_val = (low + high) / 2;
      } else {
        // fractional index, round up ... but we are 0 based, so already one higher
        ret_val = SortedLatencies[next_index];
      } // else
    } else {
      // vector too small
      if (1 == SortedLatencies.size()) {
        ret_val = SortedLatencies[0] / 2;
      } else {
        ret_val=std::chrono::microseconds(0);
      } // else
    } // else

    return ret_val;
  } // CalcPercentile

 private:
};


class QuickHistogramTimer {
public:
  QuickHistogramTimer(QuickHistogram & histo)
    : _histogram(histo) {
    _interval_start=std::chrono::steady_clock::now();
  }

  ~QuickHistogramTimer() {
    std::chrono::microseconds latency;

    latency = std::chrono::duration_cast<std::chrono::microseconds>(std::chrono::steady_clock::now() - _interval_start);
    _histogram.post_latency(latency);
  }

  std::chrono::steady_clock::time_point _interval_start;
  QuickHistogram & _histogram;
}; // QuickHistogramTimer
}
}
#endif
