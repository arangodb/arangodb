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

#pragma once

#include "ApplicationFeaturePhase.h"

#include "ApplicationFeatures/ConfigFeature.h"
#include "ApplicationFeatures/FileSystemFeature.h"
#include "ApplicationFeatures/GreetingsFeature.h"
#include "ApplicationFeatures/ShellColorsFeature.h"
#include "ApplicationFeatures/VersionFeature.h"
#include "Logger/LoggerFeature.h"
#include "Random/RandomFeature.h"

namespace arangodb {

namespace application_features {

class GreetingsFeaturePhase final : public ApplicationFeaturePhase {
 public:
  static constexpr std::string_view name() noexcept { return "GreetingsPhase"; }

  template<typename Server, bool IsClient>
  explicit GreetingsFeaturePhase(Server& server,
                                 std::integral_constant<bool, IsClient>)
      : ApplicationFeaturePhase{server, *this} {
    setOptional(false);

    startsAfter<ConfigFeature>();
    startsAfter<FileSystemFeature>();
    startsAfter<LoggerFeature>();
    startsAfter<RandomFeature>();
    startsAfter<ShellColorsFeature>();
    startsAfter<VersionFeature>();

    if constexpr (!IsClient) {
      // These are server only features
      startsAfter<GreetingsFeature>();
    }
  }
};

}  // namespace application_features
}  // namespace arangodb
