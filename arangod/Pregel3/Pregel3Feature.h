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
/// @author Markus Pfeiffer
////////////////////////////////////////////////////////////////////////////////
#pragma once

#include "ApplicationFeatures/ApplicationFeature.h"
#include "Pregel3/GlobalSettings.h"
#include "Pregel3/Query.h"

#include <cstdint>

namespace arangodb {
class Pregel3Feature final : public application_features::ApplicationFeature {
 public:
  explicit Pregel3Feature(application_features::ApplicationServer& server);
  ~Pregel3Feature() override;

  void collectOptions(std::shared_ptr<options::ProgramOptions>) override final;
  void validateOptions(std::shared_ptr<options::ProgramOptions>) override final;
  void prepare() override;

  void createQuery(pregel3::GraphSpecification const& graph);

 private:
  std::unordered_map<pregel3::QueryId, pregel3::Query> _queries;
  std::shared_ptr<pregel3::GlobalSettings> _settings;
};

}  // namespace arangodb
