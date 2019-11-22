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
/// @author Ewout Prangsma
////////////////////////////////////////////////////////////////////////////////
#pragma once

#ifndef ARANGO_CXX_DRIVER_WAITGROUP
#define ARANGO_CXX_DRIVER_WAITGROUP

#include <chrono>
#include <condition_variable>
#include <mutex>

namespace arangodb { namespace fuerte { inline namespace v1 {

// WaitGroup is used to wait on a number of events.
// Typical usage:
//
// WaitGroup wg;
// wg.add();
// start_your_asynchronous_event(
//    []() { /* some callback*/ wg.done(); });
// wg.wait();
class WaitGroup {
 public:
  WaitGroup() : _counter(0) {}
  ~WaitGroup() = default;

  // Prevent copying
  WaitGroup(WaitGroup const& other) = delete;
  WaitGroup& operator=(WaitGroup const& other) = delete;

  // add an event to wait for.
  // call done() when the event has finished.
  void add(unsigned int increment = 1) {
    assert(increment > 0);
    std::unique_lock<std::mutex> lock(_mutex);
    _counter += increment;
  }

  // call when the event is done.
  // when the counter reached 0, wait() is unblocked.
  void done() {
    std::unique_lock<std::mutex> lock(_mutex);
    _counter--;
    if (_counter == 0) {
      // lock.unlock();
      _conditionVar.notify_all();
    }
  }

  // wait for all events to have finished.
  // If no events have been added, this will return immediately.
  void wait() {
    std::unique_lock<std::mutex> lock(_mutex);
    if (_counter == 0) {
      return;
    }
    _conditionVar.wait(lock, [&] { return (_counter == 0); });
  }

  // wait for all events to have finished or a timeout occurs.
  // If no events have been added, this will return immediately.
  // Returns true when all events have finished, false on timeout.
  template <class Rep, class Period>
  bool wait_for(const std::chrono::duration<Rep, Period>& rel_time) {
    std::unique_lock<std::mutex> lock(_mutex);
    if (_counter == 0) {
      return true;
    }
    return _conditionVar.wait_for(lock, rel_time,
                                  [&] { return (_counter == 0); });
  }

 private:
  int _counter;
  std::mutex _mutex;
  std::condition_variable _conditionVar;
};

// WaitGroupDone is a helper to call WaitGroup.done when falling out of scope.
// Typical usage:
//
// WaitGroup wg;
// wg.add();
// start_your_asynchronous_event(
//    []() {
//      WaitGroupDone done(wg);
//      /* do your callback work which is allowed to fail */
//    });
// wg.wait();
class WaitGroupDone {
 public:
  WaitGroupDone(WaitGroup& wg) : _wg(wg) {}
  ~WaitGroupDone() noexcept { _wg.done(); }

  // Prevent copying
  WaitGroupDone(WaitGroupDone const& other) = delete;
  WaitGroupDone& operator=(WaitGroupDone const& other) = delete;

 private:
  WaitGroup& _wg;
};
}}}  // namespace arangodb::fuerte::v1
#endif
