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

#ifndef ARANGOD_WAL_REMOVER_THREAD_H
#define ARANGOD_WAL_REMOVER_THREAD_H 1

#include "Basics/Common.h"
#include "Basics/ConditionVariable.h"
#include "Basics/Thread.h"

namespace arangodb {
namespace wal {

class LogfileManager;

class RemoverThread final : public Thread {
  RemoverThread(RemoverThread const&) = delete;
  RemoverThread& operator=(RemoverThread const&) = delete;

 public:
  explicit RemoverThread(LogfileManager*);
  ~RemoverThread() { shutdown(); }

 public:
  void beginShutdown() override final;

 protected:
  void run() override;

 private:
  /// @brief the logfile manager
  LogfileManager* _logfileManager;

  /// @brief condition variable for the collector thread
  basics::ConditionVariable _condition;

  /// @brief wait interval for the collector thread when idle
  static uint64_t const Interval;
};
}
}

#endif
