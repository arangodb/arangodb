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

#include "V8ShellFeaturePhase.h"

#include "ApplicationFeatures/GreetingsFeaturePhase.h"
#include "ApplicationFeatures/V8PlatformFeature.h"
#include "ApplicationFeatures/V8SecurityFeature.h"
#include "Shell/ConsoleFeature.h"
#include "Shell/V8ShellFeature.h"

namespace arangodb {
namespace application_features {

V8ShellFeaturePhase::V8ShellFeaturePhase(ApplicationServer& server)
    : ApplicationFeaturePhase(server, "V8ShellPhase") {
  setOptional(false);
  startsAfter<GreetingsFeaturePhase>();

  startsAfter<ConsoleFeature>();
  startsAfter<V8ShellFeature>();
  startsAfter<V8PlatformFeature>();
  startsAfter<V8SecurityFeature>();
}

}  // namespace application_features
}  // namespace arangodb
