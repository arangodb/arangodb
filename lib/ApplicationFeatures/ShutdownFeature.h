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
/// @author Dr. Frank Celler
////////////////////////////////////////////////////////////////////////////////

#pragma once

#include "ApplicationFeatures/ApplicationFeature.h"

namespace arangodb {
namespace application_features {
class GreetingsFeaturePhase;
}

class LoggerFeature;

class ShutdownFeature final : public application_features::ApplicationFeature {
 public:
  static constexpr std::string_view name() noexcept { return "Shutdown"; }

  template<typename Server>
  ShutdownFeature(Server& server, std::vector<size_t> const& features)
      : ApplicationFeature{server, *this} {
    setOptional(true);
    startsAfter<application_features::GreetingsFeaturePhase, Server>();

    for (auto feature : features) {
      if (feature != Server::template id<LoggerFeature>()) {
        startsAfter(feature);
      }
    }
  }

  void start() override final;
};

}  // namespace arangodb
