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

#ifndef ARANGOD_UTILS_FLUSH_THREAD_H
#define ARANGOD_UTILS_FLUSH_THREAD_H 1

#include "Basics/Common.h"
#include "Basics/ConditionVariable.h"
#include "Basics/Thread.h"

namespace arangodb {
class FlushFeature;

class FlushThread final : public Thread {
 private:
  FlushThread(FlushThread const&) = delete;
  FlushThread& operator=(FlushThread const&) = delete;

 public:
  /// flush interval in microseconds
  explicit FlushThread(FlushFeature& feature, uint64_t flushInterval);
  ~FlushThread() { shutdown(); }

 public:
  void beginShutdown() override;

  /// @brief wake up the flush thread
  void wakeup();

 private:
  void run() override;

 private:
  /// @brief condition variable for the thread
  basics::ConditionVariable _condition;

  /// @brief reference to the owning feature
  FlushFeature& _feature;

  /// @brief wait interval for the flusher thread when idle (in microseconds)
  uint64_t const _flushInterval;
};

}  // namespace arangodb

#endif
