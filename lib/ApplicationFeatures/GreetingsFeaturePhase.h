////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2014-2022 ArangoDB GmbH, Cologne, Germany
/// Copyright 2004-2014 triAGENS GmbH, Cologne, Germany
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

#pragma once

#include "ApplicationFeaturePhase.h"

namespace arangodb {

class ConfigFeature;
class LoggerFeature;
class RandomFeature;
class ShellColorsFeature;
class VersionFeature;
class GreetingsFeature;

namespace application_features {

class GreetingsFeaturePhase final : public ApplicationFeaturePhase {
 public:
  static constexpr std::string_view name() noexcept { return "GreetingsPhase"; }

  template<typename Server, bool IsClient>
  explicit GreetingsFeaturePhase(Server& server,
                                 std::integral_constant<bool, IsClient>)
      : ApplicationFeaturePhase{server, *this} {
    setOptional(false);

    startsAfter<ConfigFeature, Server>();
    startsAfter<LoggerFeature, Server>();
    startsAfter<RandomFeature, Server>();
    startsAfter<ShellColorsFeature, Server>();
    startsAfter<VersionFeature, Server>();

    if constexpr (!IsClient) {
      // These are server only features
      startsAfter<GreetingsFeature, Server>();
    }
  }
};

}  // namespace application_features
}  // namespace arangodb
