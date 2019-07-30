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

#ifndef ARANGOD_HOTBACKUP_H
#define ARANGOD_HOTBACKUP_H 1

#include "Basics/Result.h"
#include "Cluster/ResultT.h"
#include "Rest/CommonDefines.h"

#include <velocypack/Builder.h>
#include <velocypack/Slice.h>
#include <velocypack/velocypack-aliases.h>

namespace arangodb {

////////////////////////////////////////////////////////////////////////////////
/// @brief Struct containing meta data for backups
////////////////////////////////////////////////////////////////////////////////

struct BackupMeta {
  std::string _id;
  std::string _version;
  std::string _datetime;

  static constexpr const char *ID = "id";
  static constexpr const char *VERSION = "version";
  static constexpr const char *DATETIME = "datetime";

  void toVelocyPack(VPackBuilder &builder) const {
    {
      VPackObjectBuilder ob(&builder);
      builder.add(ID, VPackValue(_id));
      builder.add(VERSION, VPackValue(_version));
      builder.add(DATETIME, VPackValue(_datetime));
    }
  }

  static ResultT<BackupMeta> fromSlice(VPackSlice const& slice) {
    try {
      BackupMeta meta;
      meta._id = slice.get(ID).copyString();
      meta._version  = slice.get(VERSION).copyString();
      meta._datetime = slice.get(DATETIME).copyString();
      return meta;
    } catch (std::exception const& e) {
      return ResultT<BackupMeta>::error(TRI_ERROR_BAD_PARAMETER, e.what());
    }
  }

  BackupMeta(std::string const& id, std::string const& version, std::string const& datetime) :
    _id(id), _version(version), _datetime(datetime) {}

private:
  BackupMeta() {}
};

//std::ostream& operator<<(std::ostream& os, const BackupMeta& bm);

////////////////////////////////////////////////////////////////////////////////
/// @brief HotBackup engine selector operations
////////////////////////////////////////////////////////////////////////////////

enum BACKUP_ENGINE {ROCKSDB, MMFILES, CLUSTER};

class HotBackup {
public:

  HotBackup();
  virtual ~HotBackup() = default;

  /**
   * @brief execute storage engine's command with payload and report back
   * @param  command  backup command [create, delete, list, upload, download]
   * @param  payload  JSON payload
   * @param  report   operation's report
   * @return 
   */
  arangodb::Result execute (
    std::string const& command, VPackSlice const payload, VPackBuilder& report);
  
private:

  /**
   * @brief select engine and lock transactions
   * @param  body  rest handling body
   */
  arangodb::Result executeRocksDB(
    std::string const& command, VPackSlice const payload, VPackBuilder& report);

  /**
   * @brief select engine and create backup
   * @param  payload  rest handling payload
   */
  arangodb::Result executeCoordinator(
    std::string const& command, VPackSlice const payload, VPackBuilder& report);
  
  /**
   * @brief select engine and create backup
   * @param  payload  rest handling payload
   */
  arangodb::Result executeMMFiles(
    std::string const& command, VPackSlice const payload, VPackBuilder& report);
  
  BACKUP_ENGINE _engine;
  
};// class HotBackup

}

#endif
