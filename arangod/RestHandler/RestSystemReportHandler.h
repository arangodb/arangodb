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

#ifndef ARANGOD_REST_HANDLER_REST_SYSTEM_REPORT_HANDLER_H
#define ARANGOD_REST_HANDLER_REST_SYSTEM_REPORT_HANDLER_H 1

#include "RestHandler/RestBaseHandler.h"
  
#include <mutex>
#include <unordered_map>

namespace arangodb {
class RestSystemReportHandler : public arangodb::RestBaseHandler {
 public:
  RestSystemReportHandler(
    application_features::ApplicationServer&, GeneralRequest*,
    GeneralResponse*);
  char const* name() const override final { return "RestSystemReportHandler"; }
  RequestLane lane() const override final { return RequestLane::CLIENT_SLOW; }
  RestStatus execute() override;
private:
  bool isAdminUser() const;

  static std::mutex _exclusive;
  std::unordered_map<std::string, std::string> const cmds;

};
}  // namespace arangodb

#endif
