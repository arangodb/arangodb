////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2019 ArangoDB GmbH, Cologne, Germany
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
/// @author Kaveh Vahedipour
////////////////////////////////////////////////////////////////////////////////

#include "HotBackup.h"

#include "Cluster/ServerState.h"
#include "Cluster/ClusterMethods.h"
#include "RocksDBEngine/RocksDBHotBackup.h"
#include "StorageEngine/EngineSelectorFeature.h"

namespace arangodb {

HotBackup::HotBackup() {
  if (ServerState::instance()->isCoordinator()) {
    _engine = HotBackup::CULSTER;
  } else if (EngineSelectorFeature::isRocksDB()) {
    _engine = HotBackup::ROCKSDB;
  } else {
    _engine = HotBackup::MMFILES;
  }
}

arangodb::Result HotBackup::rocksDB    std::shared_ptr<RocksDBHotBackup> operation(parseHotBackupParams(type, suffixes));


arangodb::Result HotBackup::lock(VPackSlice const payload) {
  switch (_engine) {
  case HotBackup::ROCKSDB:
    LOG_DEBUG(DEBUG, Logger::HOTBACKUP) << "hotbackup lock RocksDB";
    return rocksDB(payload)
      break;
  case HotBackup::MMFILES:
    return arangodb::Result(
      TRI_ERROR_NOT_IMPLEMENTED, "hotback feature not implemented on MMFiles engine");
    break;
  case HotBackup::CLUSTER:
    return arangodb::Result(
      TRI_ERROR_NOT_IMPLEMENTED, "hotback locks not implemented on coordinators");
    break;    
  }

}

