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

#pragma once

#include "ApplicationFeatures/ApplicationFeature.h"
#include "Shell/ShellFeatureOptions.h"

#include <memory>
#include <optional>
#include <string>
#include <vector>

namespace arangodb {

class ShellFeature final : public application_features::ApplicationFeature {
 public:
  static constexpr std::string_view name() noexcept { return "Shell"; }

  ShellFeature(application_features::ApplicationServer& server, int* result,
               ShellFeatureOptions options);
  ShellFeature(application_features::ApplicationServer& server, int* result);

  ~ShellFeature();

  void collectOptions(std::shared_ptr<options::ProgramOptions>) override;
  void validateOptions(
      std::shared_ptr<options::ProgramOptions> options) override;
  void start() override;

  void setExitCode(int code) { *_result = code; }

 public:
  enum class RunMode {
    INTERACTIVE,
    EXECUTE_SCRIPT,
    EXECUTE_STRING,
    CHECK_SYNTAX,
    UNIT_TESTS
  };

 private:
  ShellFeatureOptions _options;
  int* _result;
  RunMode _runMode;
  std::vector<std::string> _positionals;
};

}  // namespace arangodb
