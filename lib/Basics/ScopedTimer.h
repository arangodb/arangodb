////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2014-2017 ArangoDB GmbH, Cologne, Germany
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
/// @author Jan Christoph Uhde
////////////////////////////////////////////////////////////////////////////////

#ifndef ARANGODB_BASICS_SCOPEDTIMER__H
#define ARANGODB_BASICS_SCOPEDTIMER__H 1

#include <lib/Logger/Logger.h>

#include <chrono>
#include <functional>
#include <iomanip>
#include <iostream>
#include <sstream>
#include <utility>
#include <vector>
namespace arangodb {

class ScopedTimer {
 public:  // defines
  using hclock = std::chrono::high_resolution_clock;
  using hrcStrVec = std::vector<std::pair<hclock::time_point, std::string>>;
  using intStrVec = std::vector<std::pair<int, std::string>>;

 private:  // variables
  hrcStrVec _timePoints;
  std::function<void(intStrVec)> _callback;
  bool _enabled;
  bool _addDtorEntry;

 public:  // functions
  ScopedTimer(ScopedTimer const&) = delete;
  ScopedTimer(ScopedTimer&&) = delete;

  ScopedTimer(std::function<void(intStrVec)> callback = &ScopedTimer::print)
      : _callback(callback), _enabled(true), _addDtorEntry(true) {
    _timePoints.reserve(10);  // if you want time more than 10 - add template param?
    _timePoints.emplace_back(hclock::time_point(), "");
    _timePoints.back().first = hclock::now();  // defeats the purpose of emplace a bit :/
  }

  ScopedTimer(std::string const& description,
              std::function<void(intStrVec)> callback = &ScopedTimer::print)
      : _callback(callback), _enabled(true), _addDtorEntry(true) {
    _timePoints.reserve(10);
    _timePoints.emplace_back(hclock::time_point(), description);
    _timePoints.back().first = hclock::now();
  }

  ScopedTimer(std::string&& description,
              std::function<void(intStrVec)> callback = &ScopedTimer::print)
      : _callback(callback), _enabled(true), _addDtorEntry(true) {
    _timePoints.reserve(10);
    _timePoints.emplace_back(hclock::time_point(), std::move(description));
    _timePoints.back().first = hclock::now();
  }

  ~ScopedTimer(void) {
    if (_addDtorEntry) {
      _timePoints.emplace_back(hclock::now(), "dtor");
    }
    if (_enabled) {
      _callback(calculate());
    }
  }

  void addStep(std::string const& str = "") {
    _timePoints.emplace_back(hclock::now(), str);
  }
  void addStep(std::string&& str = "") {
    _timePoints.emplace_back(hclock::now(), std::move(str));
  }

  void disableDtorEntry() { _addDtorEntry = false; }

  void runCallback(bool disable = true) {
    if (disable) {
      _enabled = false;
    }
    return _callback(calculate());
  }

  void set_description(std::string const& description) {
    _timePoints[0].second = description;
  }

  void set_description(std::string&& description) {
    _timePoints[0].second = description;
  }

 private:  // functions
  static long int get_time_diff(hclock::time_point const& t0, hclock::time_point const& t1) {
    return std::chrono::duration_cast<std::chrono::nanoseconds>(t1 - t0).count();
  }

  intStrVec calculate() {
    long int total_time =
        get_time_diff(_timePoints.front().first, _timePoints.back().first);
    intStrVec times;
    times.emplace_back(std::make_pair(total_time, _timePoints.front().second));
    if (_timePoints.size() > 2) {
      for (std::size_t i = 1; i < _timePoints.size(); i++) {
        times.emplace_back(std::make_pair(get_time_diff(_timePoints[i - 1].first,
                                                        _timePoints[i].first),
                                          _timePoints[i].second));
      }
    }
    return times;
  }  // function - calculate

  static std::stringstream toStringStream(intStrVec const& times) {
    std::stringstream ss;
    int width = 15;
    ss << "total   : " << std::setw(width) << times[0].first << " ns - "
       << std::setprecision(8) << std::fixed << times[0].first / 1000.0 << " μs - "
       << std::setprecision(8) << std::fixed << times[0].first / 1000000.0 << " ms - "
       << std::setprecision(8) << std::fixed << times[0].first / 1000000000.0 << " s";

    if (!times[0].second.empty()) {
      ss << " - " << times[0].second;
    }
    ss << std::endl;

    if (times.size() > 1) {
      for (std::size_t i = 1; i < times.size(); i++) {
        ss << "step " << std::setw(3) << i << ": " << std::setw(width)
           << times[i].first << " ns" << std::setprecision(1) << std::fixed
           << " (" << std::setw(5)
           << 100 * static_cast<float>(times[i].first) /
                  static_cast<float>(times[0].first)
           << "%)";
        if (!times[i].second.empty()) {
          ss << " - " << times[i].second;
        }
        ss << std::endl;
      }
    }
    return ss;
  }

  static std::string toString(intStrVec const& times) {
    return toStringStream(times).str();
  }

  static void print(intStrVec const& times) {
    std::string to;
    auto out = toStringStream(times);
    while (std::getline(out, to, '\n')) {
      LOG_TOPIC(ERR, Logger::FIXME) << to;
    }
  }

 public:
  std::string str(bool disable = true) {
    if (disable) {
      _enabled = false;
    }
    return toString(calculate());
  }
};

}  // namespace arangodb
#endif
