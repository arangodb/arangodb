////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2014-2024 ArangoDB GmbH, Cologne, Germany
/// Copyright 2004-2014 triAGENS GmbH, Cologne, Germany
///
/// Licensed under the Business Source License 1.1 (the "License");
/// you may not use this file except in compliance with the License.
/// You may obtain a copy of the License at
///
///     https://github.com/arangodb/arangodb/blob/devel/LICENSE
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
/// @author Daniel Larkin-York
////////////////////////////////////////////////////////////////////////////////

#pragma once

#include "ApplicationFeatures/ApplicationFeature.h"
#include "Basics/Result.h"
#include "RocksDBEngine/IRecoveryState.h"
#include "StorageEngine/StorageEngine.h"

#include <atomic>
#include <rocksdb/types.h>

namespace arangodb {

class RocksDBEngine;
struct IDatabaseProvider;
struct IRecoveryCallback;

class RocksDBRecoveryManager final
    : public application_features::ApplicationFeature,
      public IRecoveryState {
 public:
  static constexpr std::string_view name() { return "RocksDBRecoveryManager"; }

  explicit RocksDBRecoveryManager(
      application_features::ApplicationServer& server,
      IDatabaseProvider& dbProvider, IRecoveryCallback& recoveryCallback);

  // must be called before start()
  void attachEngine(RocksDBEngine& engine) noexcept;

  RecoveryState recoveryState() const noexcept override;
  TRI_voc_tick_t recoveryTick() const noexcept override;

  void start() override;

 private:
  Result parseRocksWAL();

  RocksDBEngine* _engine{nullptr};
  IDatabaseProvider& _dbProvider;
  IRecoveryCallback& _recoveryCallback;
  // release-stores synchronize with acquire reads in recoveryState()
  std::atomic<RecoveryState> _recoveryState{RecoveryState::BEFORE};
  // relaxed writes become visible after the DONE release-store above
  std::atomic<rocksdb::SequenceNumber> _currentSequenceNumber{0};
};

}  // namespace arangodb
