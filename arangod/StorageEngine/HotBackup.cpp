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
    _engine = BACKUP_ENGINE::CLUSTER;
  } else if (EngineSelectorFeature::isRocksDB()) {
    _engine = BACKUP_ENGINE::ROCKSDB;
  } else {
    _engine = BACKUP_ENGINE::MMFILES;
  }
}


arangodb::Result HotBackup::execute (
  std::string const& command, VPackSlice const payload, VPackBuilder& report) {

  switch (_engine) {
  case BACKUP_ENGINE::ROCKSDB:
    return executeRocksDB(command, payload, report);
  case BACKUP_ENGINE::MMFILES:
    return executeMMFiles(command, payload, report);
  case BACKUP_ENGINE::CLUSTER:
    return executeCoordinator(command, payload, report);
  }

  return arangodb::Result();

}


arangodb::Result HotBackup::executeRocksDB(
  std::string const& command, VPackSlice const payload, VPackBuilder& report) {

  std::shared_ptr<RocksDBHotBackup> operation;
  operation = RocksDBHotBackup::operationFactory(command, payload, report);

  if (operation->valid()) {
    operation->execute();
  } // if

  // if !valid() then !success() already set
  if (!operation->success()) {
    return arangodb::Result(
      operation->restResponseError(), operation->errorMessage());
  } // if

  return arangodb::Result();

}


arangodb::Result HotBackup::executeCoordinator(
  std::string const& command, VPackSlice const payload, VPackBuilder& report) {

  if (command == "create") {
    return hotBackupCoordinator(payload, report);
  } else if (command == "lock") {
    return arangodb::Result(
      TRI_ERROR_NOT_IMPLEMENTED, "backup locks not implemented on coordinators");
  } else if (command == "restore") {
    return hotRestoreCoordinator(payload, report);
  } else if (command == "delete") {
    return deleteHotBakupsOnCoordinator(payload, report);
  } else if (command == "list") {
    return listHotBakupsOnCoordinator(payload, report);
#ifdef USE_ENTERPRISE
  } else if (command == "upload") {
    return uploadBackupsOnCoordinator(payload, report);
  } else if (command == "download") {
    return downloadBackupsOnCoordinator(payload, report);
#endif
  } else {
    return arangodb::Result(
      TRI_ERROR_NOT_IMPLEMENTED, command + " is not implemented on coordinators");
  }

  // We'll never get here
  return arangodb::Result();

}

arangodb::Result HotBackup::executeMMFiles(
  std::string const& command, VPackSlice const payload, VPackBuilder& report) {
  return arangodb::Result(
    TRI_ERROR_NOT_IMPLEMENTED, "hot backup not implemented on MMFiles");
}

}
