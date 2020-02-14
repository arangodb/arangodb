////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2018 ArangoDB GmbH, Cologne, Germany
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
/// @author Michael Hackstein
////////////////////////////////////////////////////////////////////////////////

#include "GreetingsFeaturePhase.h"

#include "ApplicationFeatures/ConfigFeature.h"
#include "ApplicationFeatures/GreetingsFeature.h"
#include "ApplicationFeatures/ShellColorsFeature.h"
#include "ApplicationFeatures/VersionFeature.h"
#include "Logger/LogBufferFeature.h"
#include "Logger/LoggerFeature.h"
#include "Random/RandomFeature.h"

namespace arangodb {
namespace application_features {

GreetingsFeaturePhase::GreetingsFeaturePhase(ApplicationServer& server, bool isClient)
    : ApplicationFeaturePhase(server, "GreetingsPhase") {
  setOptional(false);

  startsAfter<ConfigFeature>();
  startsAfter<LoggerFeature>();
  startsAfter<RandomFeature>();
  startsAfter<ShellColorsFeature>();
  startsAfter<VersionFeature>();

  if (!isClient) {
    // These are server only features
    startsAfter<GreetingsFeature>();
    startsAfter<LogBufferFeature>();
  }
}

}  // namespace application_features
}  // namespace arangodb
