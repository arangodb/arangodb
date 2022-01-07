////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2014-2022 ArangoDB GmbH, Cologne, Germany
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

#include <iostream>
#include <thread>

#include "AutoTuneThread.h"
#include "ImportFeature.h"
#include "ImportHelper.h"

#include "Basics/ConditionLocker.h"
#include "Logger/LogMacros.h"
#include "Logger/Logger.h"
#include "Logger/LoggerStream.h"

using namespace arangodb;
using namespace arangodb::import;

////////////////////////////////////////////////////////////////////////////////
// Goals:
//  1. compute current one second throughput of import
//  2. spread byte count of one second throughput across sender threads
//  3. create "space" between sender execution to give server time for other
//  activities
//
// The code collects the total count of bytes absorbed for ten seconds, then
// averages
//  that amount with the total from the previous 10 seconds.  The per second per
//  thread pace is therefore average divided by the thread count divided by 10.
//
// The pace starts "slow", 1 megabyte per second.  Each recalculation of pace
// adds
//  a 20% growth factor above the actual calculation from average bytes
//  consumed.
//
// The pacing code also notices when threads are completing quickly.  It will
// release
//  a new thread early in such cases to again encourage rate growth.
//
////////////////////////////////////////////////////////////////////////////////

AutoTuneThread::AutoTuneThread(application_features::ApplicationServer& server,
                               ImportHelper& importHelper)
    : Thread(server, "AutoTuneThread"),
      _importHelper(importHelper),
      _nextSend(std::chrono::steady_clock::now()),
      _pace(std::chrono::milliseconds(1000 / importHelper.getThreadCount())) {}

AutoTuneThread::~AutoTuneThread() { shutdown(); }

void AutoTuneThread::beginShutdown() {
  Thread::beginShutdown();

  // wake up the thread that may be waiting in run()
  CONDITION_LOCKER(guard, _condition);
  guard.broadcast();
}

void AutoTuneThread::run() {
  constexpr uint64_t period = 2;  // seconds

  while (!isStopping()) {
    {
      CONDITION_LOCKER(guard, _condition);
      guard.wait(std::chrono::seconds(period));
    }
    if (!isStopping()) {
      // getMaxUploadSize() is per thread
      uint64_t currentMax = _importHelper.getMaxUploadSize();
      currentMax *= _importHelper.getThreadCount();
      uint64_t periodActual = _importHelper.rotatePeriodByteCount();
      uint64_t newMax;

      // is currentMax way too big
      if (periodActual < currentMax && period < periodActual) {
        newMax = periodActual / period;
      } else if (periodActual <= period) {
        newMax = currentMax / period;
      } else {
        newMax = (currentMax + periodActual / period) / 2;
      }

      // grow number slowly (25%)
      newMax += newMax / 4;

      // make "per thread"
      newMax /= _importHelper.getThreadCount();

      // notes in Import mention an internal limit of 768MBytes
      if ((arangodb::import::ImportHelper::MaxBatchSize) < newMax) {
        newMax = arangodb::import::ImportHelper::MaxBatchSize;
      }

      LOG_TOPIC("e815e", DEBUG, arangodb::Logger::FIXME)
          << "current: " << currentMax << ", period: " << periodActual
          << ", new: " << newMax;

      _importHelper.setMaxUploadSize(newMax);
    }
  }
}

void AutoTuneThread::paceSends() {
  auto now = std::chrono::steady_clock::now();
  bool nextReset = false;

  // has _nextSend time_point already passed?
  //  if so, move to next increment of _pace to force wait
  while (_nextSend <= now) {
    _nextSend += _pace;
    nextReset = true;
  }

  std::this_thread::sleep_until(_nextSend);

  // if the previous send thread thread was found really quickly,
  //  assume arangodb is absorbing data faster than current rate.
  //  try doubling rate by halfing pace time for subsequent send.
  if (!nextReset && _pace / 2 < _nextSend - now) {
    _nextSend = _nextSend + _pace / 2;
  } else {
    _nextSend = _nextSend + _pace;
  }

}  // AutoTuneThread::paceSends
