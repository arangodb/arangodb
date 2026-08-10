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
////////////////////////////////////////////////////////////////////////////////

#pragma once

#include "ApplicationFeatures/OptionProvidingServer.h"
#include "ApplicationFeatures/ApplicationFeaturePhase.h"
#include "ApplicationFeatures/ApplicationServer.h"
#include "RestServer/ArangodOptionProviders.h"

namespace arangodb {

// ArangodServer - the main server class for arangod
class ArangodServer : public OptionProvidingServer<ArangodOptionProviders> {
 public:
  ArangodServer(
      std::shared_ptr<options::ProgramOptions> options, char const* binaryPath,
      std::string binaryName, int* ret,
      std::shared_ptr<crash_handler::DumpManager> dumpManager,
      std::shared_ptr<crash_handler::DataSourceRegistry> dataSourceRegistry)
      : OptionProvidingServer<ArangodOptionProviders>(
            options, binaryPath, std::move(binaryName), ret),
        _dumpManager(dumpManager),
        _dataSourceRegistry(dataSourceRegistry) {}

  // Adds all features to the server. Must be called before run().
  void addFeatures();

 protected:
  // Called by server::run() after collect & validate.
  void addFeaturesWithOptionProvider() final;

  void processOptions() override final;

 private:
  static ServerState::RoleEnum resolveRole(ClusterOptions const& clusterOptions,
                                           AgencyOptions const& agencyOptions);

  std::shared_ptr<crash_handler::DumpManager> _dumpManager;
  std::shared_ptr<crash_handler::DataSourceRegistry> _dataSourceRegistry;
};

}  // namespace arangodb
