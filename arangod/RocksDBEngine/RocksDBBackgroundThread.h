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
/// @author Simon Grätzer
////////////////////////////////////////////////////////////////////////////////

#ifndef ARANGOD_ROCKSDB_ENGINE_BACKGROUND_H
#define ARANGOD_ROCKSDB_ENGINE_BACKGROUND_H 1

#include "Basics/Common.h"
#include "Basics/ConditionVariable.h"
#include "Basics/Thread.h"

namespace arangodb {

class RocksDBEngine;

class RocksDBBackgroundThread final : public Thread {
 public:
  //////////////////////////////////////////////////////////////////////////////
  /// @brief engine pointer
  //////////////////////////////////////////////////////////////////////////////
  RocksDBEngine& _engine;

  //////////////////////////////////////////////////////////////////////////////
  /// @brief interval in which we will run
  //////////////////////////////////////////////////////////////////////////////
  double const _interval;

  //////////////////////////////////////////////////////////////////////////////
  /// @brief condition variable for heartbeat
  //////////////////////////////////////////////////////////////////////////////
  arangodb::basics::ConditionVariable _condition;

  RocksDBBackgroundThread(RocksDBEngine& eng, double interval);
  ~RocksDBBackgroundThread();

  void beginShutdown() override;

 protected:
  void run() override;
};
}  // namespace arangodb

#endif
