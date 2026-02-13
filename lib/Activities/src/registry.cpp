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

#include "Activities/registry.h"
#include "Activities/activity.h"

< < < < < < < <
    HEAD : lib / Activities / src / registry.cpp namespace arangodb::activities{
      == == == ==
#include "ApplicationFeatures/ApplicationFeaturePhase.h"
#include "ApplicationFeatures/ApplicationServer.h"
      >>>>>>>> origin / devel : arangod / FeaturePhases /
                   FoxxFeaturePhase.h

                       Registry::ScopedCurrentlyExecutingActivity::
                           ScopedCurrentlyExecutingActivity(
                               ActivityId activity) noexcept {
                               _oldExecutingActivity =
                                   Registry::currentlyExecutingActivity();
Registry::setCurrentlyExecutingActivity(activity);
}

Registry::ScopedCurrentlyExecutingActivity::
    ~ScopedCurrentlyExecutingActivity() {
  Registry::setCurrentlyExecutingActivity(_oldExecutingActivity);
}

< < < < < < < < HEAD : lib / Activities / src / registry.cpp
}  // namespace arangodb::activities
== == == ==
    explicit FoxxFeaturePhase(application_features::ApplicationServer& server);
}
;

}  // namespace application_features
}  // namespace arangodb
>>>>>>>> origin / devel : arangod / FeaturePhases / FoxxFeaturePhase.h
