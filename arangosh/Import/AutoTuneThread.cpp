////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2018 ArangoDB GmbH, Cologne, Germany
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

#include <iostream>

#include "AutoTuneThread.h"
#include "Basics/ConditionLocker.h"
#include "ImportHelper.h"

using namespace arangodb;
using namespace arangodb::import;

AutoTuneThread::AutoTuneThread(ImportHelper & importHelper)
    : Thread("AutoTuneThread"),
      _importHelper(importHelper) {}

AutoTuneThread::~AutoTuneThread() {
  shutdown();
}

void AutoTuneThread::beginShutdown() {
  Thread::beginShutdown();

  // wake up the thread that may be waiting in run()
  CONDITION_LOCKER(guard, _condition);
  guard.broadcast();
}


void AutoTuneThread::run() {
//  uint64_t total_time(0), total_bytes(0);

  while (!isStopping()) {
    {
      CONDITION_LOCKER(guard, _condition);
      guard.wait(std::chrono::seconds(10));
    }
    if (!isStopping()) {
#if 1
      // getMaxUploadSize() is per thread
      uint64_t current_max = _importHelper.getMaxUploadSize();
      current_max *= _importHelper.getThreadCount();
      uint64_t ten_second_actual = _importHelper.rotatePeriodByteCount();
      uint64_t new_max = current_max;

      // is current_max way too big
      if (ten_second_actual < current_max && 10 < ten_second_actual) {
        new_max = ten_second_actual / 10;
      } else if ( ten_second_actual <= 10 ) {
        new_max = current_max / 10;
      } else {
        new_max = (current_max + ten_second_actual / 10) / 2;
      }

      // grow number slowly if possible (10%)
//      new_max += new_max/10;
      new_max += new_max/5;  //(20% growth)

      // make "per thread"
      new_max /= _importHelper.getThreadCount();

      // notes in Import mention an internal limit of 768MBytes
      if ((768 * 1024 * 1024) < new_max) {
        new_max = 768 * 1024 * 1024;
      }

      std::cout << "Current: " << current_max
                << ", ten_sec: " << ten_second_actual
                << ", new_max: " << new_max
                << std::endl;
#else
      total_time +=10;
      total_bytes += _importHelper.rotatePeriodByteCount();

      uint64_t new_max = total_bytes / total_time / _importHelper.getThreadCount();

      new_max += new_max/10;

      std::cout << "Total sec: " << total_time
                << ", total byte: " << total_bytes
                << ", new_max: " << new_max
                << std::endl;
#endif
      _importHelper.setMaxUploadSize(new_max);
    }
  }
}
