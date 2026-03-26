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
/// @author Dr. Frank Celler
////////////////////////////////////////////////////////////////////////////////

#pragma once

#include <span>
#include <typeindex>

#include "ApplicationFeatures/ApplicationFeature.h"
#include "RestServer/CheckVersionFeatureOptions.h"

struct TRI_vocbase_t;

namespace arangodb {

class CheckVersionFeature final
    : public application_features::ApplicationFeature {
 public:
  static constexpr std::string_view name() noexcept { return "CheckVersion"; }

  explicit CheckVersionFeature(
      application_features::ApplicationServer& server, int* result,
      std::span<const std::type_index> nonServerFeatures);

  void collectOptions(std::shared_ptr<options::ProgramOptions>) override final;
  void validateOptions(std::shared_ptr<options::ProgramOptions>) override final;
  void start() override final;

 private:
  void checkVersion();

  CheckVersionFeatureOptions _options;
  int* _result;
  std::span<const std::type_index> _nonServerFeatures;
};

}  // namespace arangodb
