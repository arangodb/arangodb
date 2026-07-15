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

#include "ApplicationFeatures/ApplicationFeature.h"
#include "ApplicationFeatures/ApplicationFeaturePhase.h"
#include "ApplicationFeatures/ApplicationServer.h"
#include "RestServer/ArangodOptionProviers.h"


namespace arangodb {

// ArangodServer - the main server class for arangod
class ArangodServer : public application_features::ApplicationServer {
 public:
  ArangodServer(
      std::shared_ptr<options::ProgramOptions> options, char const* binaryPath,
      std::string_view binaryName,
      std::shared_ptr<crash_handler::DumpManager> dumpManager,
      std::shared_ptr<crash_handler::DataSourceRegistry> dataSourceRegistry)
      : ApplicationServer(options, binaryPath),
        _programOptions(options),
        _binaryName(binaryName),
        _dumpManager(dumpManager),
        _dataSourceRegistry(dataSourceRegistry) {}

  // Adds all features to the server. Must be called before run().
  // @param ret pointer to return value (used by some features)
  void addFeatures(int* ret);

 protected:
  void collectOptions() final;
  void validateOptions() final;
  // Called by server::run() after collect & validate.
  void addFeaturesWithOptionProvider() final;

 private:
  std::shared_ptr<options::ProgramOptions> _programOptions;
  std::string_view _binaryName;
  std::shared_ptr<crash_handler::DumpManager> _dumpManager;
  std::shared_ptr<crash_handler::DataSourceRegistry> _dataSourceRegistry;
  ArangodOptionProviders _optionProviders;
};

}  // namespace arangodb
