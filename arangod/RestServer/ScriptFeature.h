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
/// @author Dr. Frank Celler
////////////////////////////////////////////////////////////////////////////////

#ifndef APPLICATION_FEATURES_SCRIPT_FEATURE_H
#define APPLICATION_FEATURES_SCRIPT_FEATURE_H 1

#include "ApplicationFeatures/ApplicationFeature.h"
#include "GeneralServer/OperationMode.h"

namespace arangodb {

class ScriptFeature final : public application_features::ApplicationFeature {
 public:
  explicit ScriptFeature(application_features::ApplicationServer& server, int* result);

  void collectOptions(std::shared_ptr<options::ProgramOptions>) override final;
  void start() override final;

 private:
  std::vector<std::string> _scriptParameters;

  int runScript(std::vector<std::string> const& scripts);

  int* _result;
};

}  // namespace arangodb

#endif