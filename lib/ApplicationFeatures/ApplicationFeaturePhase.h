////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2014-2021 ArangoDB GmbH, Cologne, Germany
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

#ifndef ARANGODB_APPLICATION_FEATURES_APPLICATION_FEATURE_PHASE_H
#define ARANGODB_APPLICATION_FEATURES_APPLICATION_FEATURE_PHASE_H 1

#include <memory>
#include <string>

#include "ApplicationFeatures/ApplicationFeature.h"

namespace arangodb {
namespace options {
class ProgramOptions;
}
namespace application_features {
class ApplicationServer;

class ApplicationFeaturePhase : public ApplicationFeature {
  friend class ApplicationServer;

 public:
  explicit ApplicationFeaturePhase(ApplicationServer& server, std::string const& name);

  // validate options of this phase
  void validateOptions(std::shared_ptr<options::ProgramOptions>) override;

  // prepare the phase
  void prepare() override;

  // start the phase
  void start() override;

  // notify the phase about a shutdown request
  void beginShutdown() override;

  // Start stopping the phase
  void stop() override;

  // Start shut down the phase
  void unprepare() override;
};

}  // namespace application_features
}  // namespace arangodb

#endif
