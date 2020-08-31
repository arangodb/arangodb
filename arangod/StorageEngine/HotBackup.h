////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2014-2020 ArangoDB GmbH, Cologne, Germany
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
/// @author Kaveh Vahedipour
////////////////////////////////////////////////////////////////////////////////

#ifndef ARANGOD_HOTBACKUP_H
#define ARANGOD_HOTBACKUP_H 1

#include "Basics/Result.h"
#include "Rest/CommonDefines.h"

#include <velocypack/Builder.h>
#include <velocypack/Slice.h>
#include <velocypack/velocypack-aliases.h>

namespace arangodb {
namespace application_features {
class ApplicationServer;
}
class ClusterFeature;

////////////////////////////////////////////////////////////////////////////////
/// @brief HotBackup engine selector operations
////////////////////////////////////////////////////////////////////////////////

enum BACKUP_ENGINE {ROCKSDB, CLUSTER};

class HotBackup {
public:
 explicit HotBackup(application_features::ApplicationServer& server);
 virtual ~HotBackup() = default;

 /**
  * @brief execute storage engine's command with payload and report back
  * @param  command  backup command [create, delete, list, upload, download]
  * @param  payload  JSON payload
  * @param  report   operation's report
  * @return
  */
 arangodb::Result execute(std::string const& command, VPackSlice const payload,
                          VPackBuilder& report);
  
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
  
#ifdef USE_ENTERPRISE
  application_features::ApplicationServer& _server;
#endif
  BACKUP_ENGINE _engine;
  
};// class HotBackup

}

#endif
