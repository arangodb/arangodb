////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2014-2023 ArangoDB GmbH, Cologne, Germany
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
/// @author Daniel Larkin-York
////////////////////////////////////////////////////////////////////////////////

#pragma once

#include "Basics/Common.h"
#include "Basics/Result.h"
#include "RestServer/arangod.h"
#include "StorageEngine/StorageEngine.h"

#include <rocksdb/types.h>

#include <atomic>

namespace rocksdb {

class TransactionDB;
}  // namespace rocksdb

namespace arangodb {

class RocksDBRecoveryManager final : public ArangodFeature {
 public:
  static constexpr std::string_view name() { return "RocksDBRecoveryManager"; }

  explicit RocksDBRecoveryManager(Server& server);

  void start() override;

  void runRecovery();

  RecoveryState recoveryState() const noexcept;

  // current recovery sequence number
  rocksdb::SequenceNumber recoverySequenceNumber() const noexcept;

 private:
  Result parseRocksWAL();

  std::atomic<rocksdb::SequenceNumber> _currentSequenceNumber;
  std::atomic<RecoveryState> _recoveryState;
};

}  // namespace arangodb
