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

#ifndef ARANGOD_HOTBACKUP_COMMON_H
#define ARANGOD_HOTBACKUP_COMMON_H 1

#include "Cluster/ResultT.h"

#include <velocypack/Builder.h>
#include <velocypack/Slice.h>
#include <velocypack/velocypack-aliases.h>

#include "Basics/VelocyPackHelper.h"
#include "Cluster/ServerState.h"

namespace arangodb {

constexpr char const* BAD_PARAMS_CREATE = "backup payload must be an object "
  "defining optional string attribute 'label' and/or optional floating point "
  "parameter 'timeout' in seconds" ;

////////////////////////////////////////////////////////////////////////////////
/// @brief Struct containing meta data for backups
////////////////////////////////////////////////////////////////////////////////

struct BackupMeta {
  std::string _id;
  std::string _version;
  std::string _datetime;
  size_t _sizeInBytes;
  size_t _nrFiles;
  unsigned int _nrDBServers;
  std::string _serverId;
  bool _potentiallyInconsistent;

  static constexpr const char *ID = "id";
  static constexpr const char *VERSION = "version";
  static constexpr const char *DATETIME = "datetime";
  static constexpr const char *SIZEINBYTES = "sizeInBytes";
  static constexpr const char *NRFILES = "nrFiles";
  static constexpr const char *NRDBSERVERS = "nrDBServers";
  static constexpr const char *SERVERID = "serverId";
  static constexpr const char *POTENTIALLYINCONSISTENT = "potentiallyInconsistent";

  void toVelocyPack(VPackBuilder &builder) const {
    {
      VPackObjectBuilder ob(&builder);
      builder.add(ID, VPackValue(_id));
      builder.add(VERSION, VPackValue(_version));
      builder.add(DATETIME, VPackValue(_datetime));
      builder.add(SIZEINBYTES, VPackValue(_sizeInBytes));
      builder.add(NRFILES, VPackValue(_nrFiles));
      builder.add(NRDBSERVERS, VPackValue(_nrDBServers));
      if (ServerState::instance()->isDBServer()) {
        builder.add(SERVERID, VPackValue(_serverId));
      }
      builder.add(POTENTIALLYINCONSISTENT, VPackValue(_potentiallyInconsistent));
    }
  }

  static ResultT<BackupMeta> fromSlice(VPackSlice const& slice) {
    try {
      BackupMeta meta;
      meta._id = slice.get(ID).copyString();
      meta._version  = slice.get(VERSION).copyString();
      meta._datetime = slice.get(DATETIME).copyString();
      meta._sizeInBytes = basics::VelocyPackHelper::getNumericValue<size_t>(
          slice, SIZEINBYTES, 0);
      meta._nrFiles = basics::VelocyPackHelper::getNumericValue<size_t>(
          slice, NRFILES, 0);
      meta._nrDBServers = basics::VelocyPackHelper::getNumericValue<unsigned int>(
          slice, NRDBSERVERS, 1);
      meta._serverId = basics::VelocyPackHelper::getStringValue(slice, SERVERID, "");
      meta._potentiallyInconsistent = basics::VelocyPackHelper::getBooleanValue(slice, POTENTIALLYINCONSISTENT, false);
      return meta;
    } catch (std::exception const& e) {
      return ResultT<BackupMeta>::error(TRI_ERROR_BAD_PARAMETER, e.what());
    }
  }

  BackupMeta(std::string const& id, std::string const& version, std::string const& datetime, size_t sizeInBytes, size_t nrFiles, unsigned int nrDBServers, std::string const& serverId, bool potentiallyInconsistent) :
    _id(id), _version(version), _datetime(datetime),
    _sizeInBytes(sizeInBytes), _nrFiles(nrFiles), _nrDBServers(nrDBServers),
    _serverId(serverId), _potentiallyInconsistent(potentiallyInconsistent) {}

private:
  BackupMeta() {}
};

//std::ostream& operator<<(std::ostream& os, const BackupMeta& bm);

}

#endif
