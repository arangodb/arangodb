////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2014-2024 ArangoDB GmbH, Cologne, Germany
/// Copyright 2004-2014 triAGENS GmbH, Cologne, Germany
///
/// Licensed under the Business Source License 1.1 (the "License");
/// you may not use this file except in compliance with the License.
/// You may obtain a copy of the License at
///
///     https://github.com/arangodb/arangodb/blob/devel/LICENSE
///
/// Unless required by applicable law or agreed to in writing, software
/// distributed under the License is distributed on an "AS IS" BASIS,
/// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
/// See the License for the specific language governing permissions and
/// limitations under the License.
///
/// Copyright holder is ArangoDB GmbH, Cologne, Germany
///
/// @author Simon Grätzer
////////////////////////////////////////////////////////////////////////////////

#pragma once

#include "RestHandler/RestBaseHandler.h"

namespace arangodb {
class RestClusterHandler : public arangodb::RestBaseHandler {
 public:
  RestClusterHandler(ArangodServer&, GeneralRequest*, GeneralResponse*);

 public:
  virtual char const* name() const override { return "RestClusterHandler"; }
  RequestLane lane() const override final { return RequestLane::CLIENT_FAST; }
  RestStatus execute() override;

 private:
  /// _api/cluster/endpoints
  void handleCommandEndpoints();

  /// _api/cluster/agency-dump
  void handleAgencyDump();

  /// _api/cluster/agency-cache
  void handleAgencyCache();

  /// _api/cluster/cluster-info
  void handleClusterInfo();

  bool isAdmin();
  void handleCI_doesDatabaseExist(std::vector<std::string> const& suffixes);
  void handleCI_databases();
  void handleCI_flush();
  void handleCI_getCollectionInfo(std::vector<std::string> const& suffixes);
  void handleCI_getCollectionInfoCurrent(
      std::vector<std::string> const& suffixes);
  void handleCI_getResponsibleServer(std::vector<std::string> const& suffixes);
  void handleCI_getResponsibleServers();
  void handleCI_getResponsibleShard(std::vector<std::string> const& suffixes);
  void handleCI_getServerEndpoint(std::vector<std::string> const& suffixes);
  void handleCI_getServerName(std::vector<std::string> const& suffixes);
  void handleCI_getDBServers();
  void handleCI_getCoordinators();
  void handleCI_uniqid(std::vector<std::string> const& suffixes);
  void handleCI_getAnalyzersRevision(std::vector<std::string> const& suffixes);
  void handleCI_waitForPlanVersion(std::vector<std::string> const& suffixes);
};
}  // namespace arangodb
