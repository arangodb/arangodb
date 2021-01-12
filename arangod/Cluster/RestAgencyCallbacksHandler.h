////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2014-2021 ArangoDB GmbH, Cologne, Germany
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
/// @author Andreas Streichardt
////////////////////////////////////////////////////////////////////////////////

#ifndef CLUSTER_RESTAGENCYCALLACKSHANDLER_H
#define CLUSTER_RESTAGENCYCALLACKSHANDLER_H 1

#include "Basics/Common.h"
#include "RestHandler/RestVocbaseBaseHandler.h"

namespace arangodb {
class AgencyCallbackRegistry;

namespace rest {

////////////////////////////////////////////////////////////////////////////////
/// @brief shard control request handler
////////////////////////////////////////////////////////////////////////////////

class RestAgencyCallbacksHandler : public RestVocbaseBaseHandler {
 public:
  RestAgencyCallbacksHandler(application_features::ApplicationServer& server,
                             GeneralRequest* request, GeneralResponse* response,
                             AgencyCallbackRegistry* agencyCallbackRegistry);

 public:
  char const* name() const override final {
    return "RestAgencyCallbacksHandler";
  }
  RequestLane lane() const override final {
    return RequestLane::AGENCY_INTERNAL;
  }
  RestStatus execute() override;

 private:
  AgencyCallbackRegistry* _agencyCallbackRegistry;
};
}  // namespace rest
}  // namespace arangodb

#endif
