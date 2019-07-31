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

}

#endif
