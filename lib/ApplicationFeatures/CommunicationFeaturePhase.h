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

namespace arangodb {
namespace application_features {

class CommunicationFeaturePhase : public ApplicationFeaturePhase {
 public:
  static constexpr std::string_view name() noexcept {
    return "CommunicationPhase";
  }

  template<typename Server>
  explicit CommunicationFeaturePhase(Server& server)
      : ApplicationFeaturePhase(
            server, Server::template id<CommunicationFeaturePhase>(), name()) {
    setOptional(false);
  }

  /**
   * @brief decide whether we may freely communicate or not.
   */
  bool getCommAllowed() {
    switch (state()) {
      case ApplicationFeature::State::UNINITIALIZED:
      case ApplicationFeature::State::INITIALIZED:
      case ApplicationFeature::State::VALIDATED:
      case ApplicationFeature::State::PREPARED:
      case ApplicationFeature::State::STARTED:
      case ApplicationFeature::State::STOPPED:
        return true;
      case ApplicationFeature::State::UNPREPARED:
        break;
    }

    return false;
  }
};

}  // namespace application_features
}  // namespace arangodb
