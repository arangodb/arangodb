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
////////////////////////////////////////////////////////////////////////////////

#ifndef ARANGOD_REST_HANDLER_HOTBACKUP_HANDLER_H
#define ARANGOD_REST_HANDLER_HOTBACKUP_HANDLER_H 1

#include "Basics/Common.h"
#include "RestHandler/RestBaseHandler.h"
#include "RocksDBEngine/RocksDBHotBackup.h"

namespace arangodb {

////////////////////////////////////////////////////////////////////////////////
/// @brief job control request handler
////////////////////////////////////////////////////////////////////////////////

class RestHotBackupHandler : public RestBaseHandler {
 public:
  RestHotBackupHandler(GeneralRequest*, GeneralResponse*);

 public:
  char const* name() const override final { return "RestHotBackupHandler"; }
  RequestLane lane() const override final { return RequestLane::CLIENT_SLOW; }
  RestStatus execute() override;

 protected:
  arangodb::Result  verifyPermitted();

  arangodb::Result parseHotBackupParams(
    RequestType const, std::vector<std::string> const&, VPackSlice& slice);
};
}  // namespace arangodb

#endif
