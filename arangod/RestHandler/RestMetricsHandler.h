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

#ifndef ARANGOD_REST_HANDLER_REST_METRICS_HANDLER_H
#define ARANGOD_REST_HANDLER_REST_METRICS_HANDLER_H 1

#include "RestHandler/RestBaseHandler.h"
#include "RestServer/MetricsFeature.h"

namespace arangodb {
class RestMetricsHandler : public arangodb::RestBaseHandler {
 public:
  RestMetricsHandler(application_features::ApplicationServer&, GeneralRequest*,
                     GeneralResponse*);

  char const* name() const override final { return "RestMetricsHandler"; }
  /// @brief must be on fast lane so that metrics can always be retrieved, 
  /// even from otherwise totally busy servers
  RequestLane lane() const override final { return RequestLane::CLIENT_FAST; }
  RestStatus execute() override;

};



}  // namespace arangodb

#endif
