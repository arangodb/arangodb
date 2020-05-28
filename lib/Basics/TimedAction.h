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

#ifndef ARANGODB_BASICS_TIMED_ACTION_H
#define ARANGODB_BASICS_TIMED_ACTION_H 1

#include "Basics/Common.h"
#include "Basics/system-functions.h"

namespace arangodb {

class TimedAction {
 public:
  TimedAction(TimedAction const&) = delete;
  TimedAction& operator=(TimedAction const&) = delete;

  TimedAction(std::function<void(double)> const& callback, double threshold)
      : _callback(callback), _threshold(threshold), _start(TRI_microtime()), _done(false) {}

  ~TimedAction() = default;

 public:
  double elapsed() const { return (TRI_microtime() - _start); }
  bool tick() {
    if (!_done) {
      if (elapsed() >= _threshold) {
        _done = true;
        _callback(_threshold);
        return true;
      }
    }
    return false;
  }

 private:
  std::function<void(double)> const _callback;
  double const _threshold;
  double _start;
  bool _done;
};

}  // namespace arangodb

#endif
