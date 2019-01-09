////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2014-2017 ArangoDB GmbH, Cologne, Germany
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

#ifndef ARANGOD_ROCKSDB_ENGINE_ROCKSDB_RECOVERY_MANAGER_H
#define ARANGOD_ROCKSDB_ENGINE_ROCKSDB_RECOVERY_MANAGER_H 1

#include <rocksdb/types.h>
#include "ApplicationFeatures/ApplicationFeature.h"
#include "Basics/Common.h"

namespace rocksdb {

class TransactionDB;
}  // namespace rocksdb

namespace arangodb {

class RocksDBRecoveryManager final : public application_features::ApplicationFeature {
 public:
  explicit RocksDBRecoveryManager(application_features::ApplicationServer& server);

  static std::string featureName() { return "RocksDBRecoveryManager"; }
  static RocksDBRecoveryManager* instance();

  void start() override;

  void runRecovery();
  bool inRecovery() const {
    return _inRecovery.load(std::memory_order_acquire);
  }

 private:
  Result parseRocksWAL();

 protected:
  //////////////////////////////////////////////////////////////////////////////
  /// @brief rocksdb instance
  //////////////////////////////////////////////////////////////////////////////
  rocksdb::TransactionDB* _db;

  std::atomic<bool> _inRecovery;
};

}  // namespace arangodb

#endif
