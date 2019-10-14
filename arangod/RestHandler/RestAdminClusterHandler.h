////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2014-2016 ArangoDB GmbH, Cologne, Germany
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
////////////////////////////////////////////////////////////////////////////////

#ifndef ARANGOD_REST_HANDLER_REST_ADMIN_CLUSTER_HANDLER_H
#define ARANGOD_REST_HANDLER_REST_ADMIN_CLUSTER_HANDLER_H 1

#include "RestHandler/RestVocbaseBaseHandler.h"

namespace arangodb {

class RestAdminClusterHandler : public RestVocbaseBaseHandler {
 public:
  RestAdminClusterHandler(application_features::ApplicationServer&, GeneralRequest*, GeneralResponse*);
  ~RestAdminClusterHandler();

 public:
  RestStatus execute() override;
  char const* name() const override final { return "RestAdminClusterHandler"; }
  RequestLane lane() const override final { return RequestLane::CLIENT_SLOW; }

 private:
  static std::string const Health;
  static std::string const NumberOfServers;
  static std::string const Maintenance;

  RestStatus handleHealth();
  RestStatus handleNumberOfServers();
  RestStatus handleMaintenance();

  RestStatus handlePutMaintenance(bool state);
  RestStatus handleGetMaintenance();

  RestStatus handleGetNumberOfServers();
  RestStatus handlePutNumberOfServers();


private:
  RestStatus handleSingleServerJob(std::string const& job, std::string const& server);


  typedef std::chrono::steady_clock clock;
  typedef futures::Future<futures::Unit> futureVoid;

  futureVoid waitForSupervisionState(bool state, clock::time_point startTime = clock::time_point());



};
}  // namespace arangodb

#endif
