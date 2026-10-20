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

#include "ApplicationFeatures/TempFeature.h"

#include "ApplicationFeatures/ApplicationServer.h"
#include "ApplicationFeatures/TempOptionsProvider.h"
#include "Basics/files.h"
#include "ProgramOptions/ProgramOptions.h"

using namespace arangodb::options;

namespace arangodb {

TempFeature::TempFeature(application_features::ApplicationServer& server,
                         std::string const& appname)
    : TempFeature(server, appname, TempFeatureOptions{}) {}

TempFeature::TempFeature(application_features::ApplicationServer& server,
                         std::string const& appname, TempFeatureOptions options)
    : ApplicationFeature{server, *this},
      _options(std::move(options)),
      _appname(appname) {
  setOptional(false);
  startsAfter<application_features::GreetingsFeaturePhase>();
}

void TempFeature::prepare() {
  TRI_SetApplicationName(_appname);
  if (!_options.path.empty()) {
    TRI_SetTempPath(_options.path);
  }
}

}  // namespace arangodb
