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
#include "Rest/CommonDefines.h"

#include <velocypack/Builder.h>
#include <velocypack/Slice.h>
#include <velocypack/velocypack-aliases.h>

namespace arangodb {

////////////////////////////////////////////////////////////////////////////////
/// @brief HotBackup engine selector operations
////////////////////////////////////////////////////////////////////////////////

enum BACKUP_ENGINE {ROCKSDB, MMFILES, CLUSTER};

class HotBackup {
public:

  HotBackup() = delete;
  HotBackup();
  virtual ~HotBackup() = default;

  /**
   * @brief select engine and lock transactions
   * @param  body  rest handling body
   */
  arangodb::Result lock(VPackSlice const payload);

  /**
   * @brief select engine and create backup
   * @param  payload  rest handling payload
   */
  arangodb::Result create(VPackSlice const payload);

  /**
   * @brief select engine and delete specific backup
   * @param  payload  rest handling payload
   */
  arangodb::Result del(VPackSlice const payload);

  /**
   * @brief upload backup to remote
   * @param  payload  rest handling payload
   */
  arangodb::Result upload(VPackSlice const payload);

  /**
   * @brief download backup from
   * @param  payload  rest handling payload
   */
  arangodb::Result download(VPackSlice const payload);

private:

  BACKUP_ENGINE _engine;
  
};// class HotBackup

}
