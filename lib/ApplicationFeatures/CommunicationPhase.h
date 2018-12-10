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

#ifndef ARANGODB_APPLICATION_FEATURES_COMM_FEATURE_PHASE_H
#define ARANGODB_APPLICATION_FEATURES_COMM_FEATURE_PHASE_H 1

#include "ApplicationFeaturePhase.h"

namespace arangodb {
namespace application_features {

class CommunicationFeaturePhase : public ApplicationFeaturePhase {
 public:
  explicit CommunicationFeaturePhase(ApplicationServer& server);
  /**
   * @brief decide whether we may freely communicate or not.
   */
  bool getCommAllowed() {
    switch (state()) {
    case ApplicationServer::FeatureState::UNINITIALIZED:
    case ApplicationServer::FeatureState::INITIALIZED:
    case ApplicationServer::FeatureState::VALIDATED:
    case ApplicationServer::FeatureState::PREPARED:
    case ApplicationServer::FeatureState::STARTED:
    case ApplicationServer::FeatureState::STOPPED:
      return true;
    case ApplicationServer::FeatureState::UNPREPARED:
      return false;
    }
    return false;
  }
};

}  // namespace application_features
}  // namespace arangodb

#endif
