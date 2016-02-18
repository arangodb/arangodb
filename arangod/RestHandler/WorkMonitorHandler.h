////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2016-2016 ArangoDB GmbH, Cologne, Germany
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
/// @author Dr. Frank Celler
////////////////////////////////////////////////////////////////////////////////

#ifndef ARANGOD_REST_HANDLER_WORK_MONITOR_HANDLER_H
#define ARANGOD_REST_HANDLER_WORK_MONITOR_HANDLER_H 1

#include "RestHandler/RestBaseHandler.h"

namespace arangodb {

////////////////////////////////////////////////////////////////////////////////
/// @brief version request handler
////////////////////////////////////////////////////////////////////////////////

class WorkMonitorHandler : public arangodb::RestBaseHandler {
 public:

  explicit WorkMonitorHandler(arangodb::rest::GeneralRequest*);

 public:
  bool isDirect() const override;

  status_t execute() override;
};
}

#endif
