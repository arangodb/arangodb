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
/// @author Andrey Abramov
////////////////////////////////////////////////////////////////////////////////

#pragma once

#include "ApplicationFeatures/ApplicationFeature.h"
#include "ApplicationFeatures/ApplicationFeaturePhase.h"
#include "ApplicationFeatures/ApplicationServer.h"

namespace arangodb {

// ArangodServer - the main server class for arangod
class ArangodServer : public application_features::ApplicationServer {
 public:
  ArangodServer(std::shared_ptr<options::ProgramOptions> options,
                char const* binaryPath)
      : ApplicationServer(std::move(options), binaryPath) {}

  // Adds all features to the server. Must be called before run().
  // @param ret pointer to return value (used by some features)
  // @param binaryName name of the binary (used by some features)
  void addFeatures(
      int* ret, std::string_view binaryName,
      std::shared_ptr<crash_handler::DumpManager> dumpManager,
      std::shared_ptr<crash_handler::DataSourceRegistry> dataSourceRegistry);
};

}  // namespace arangodb
