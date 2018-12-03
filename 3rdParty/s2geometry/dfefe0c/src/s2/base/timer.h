// Copyright 2018 Google Inc. All Rights Reserved.
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//     http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS-IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.
//

#ifndef S2_BASE_TIMER_H_
#define S2_BASE_TIMER_H_

#include <chrono>

#include "s2/base/integral_types.h"

class CycleTimer {
 public:
  CycleTimer() = default;

  void Start() {
    start_ = Now();
  }

  int64 GetInMs() const {
    using msec = std::chrono::milliseconds;
    return std::chrono::duration_cast<msec>(GetDuration()).count();
  }

 private:
  using Clock = std::chrono::high_resolution_clock;

  static Clock::time_point Now() {
    return Clock::now();
  }

  Clock::duration GetDuration() const {
    return Now() - start_;
  }

  Clock::time_point start_;
};

#endif  // S2_BASE_TIMER_H_
