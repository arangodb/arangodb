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
/// @author Michael Hackstein
////////////////////////////////////////////////////////////////////////////////

#include "V8FeaturePhase.h"

#include "Actions/ActionFeature.h"
#include "FeaturePhases/ClusterFeaturePhase.h"
#include "GeneralServer/ServerSecurityFeature.h"
#include "V8/V8PlatformFeature.h"
#include "V8/V8SecurityFeature.h"
#include "V8Server/V8DealerFeature.h"

namespace arangodb::application_features {

V8FeaturePhase::V8FeaturePhase(application_features::ApplicationServer& server)
    : ApplicationFeaturePhase{server, *this} {
  setOptional(false);
  startsAfter<ClusterFeaturePhase>();

  startsAfter<ActionFeature>();
  startsAfter<ServerSecurityFeature>();
  startsAfter<V8DealerFeature>();
  startsAfter<V8PlatformFeature>();
  startsAfter<V8SecurityFeature>();
}

}  // namespace arangodb::application_features
