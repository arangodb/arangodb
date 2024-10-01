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

#include "Shell/arangosh.h"

#include <memory>
#include <optional>
#include <string>
#include <vector>

namespace arangodb {

class TelemetricsHandler;

namespace velocypack {
class Builder;
}

class ShellFeature final : public ArangoshFeature {
 public:
  static constexpr std::string_view name() noexcept { return "Shell"; }

  ShellFeature(Server& server, int* result);

  ~ShellFeature();

  void collectOptions(std::shared_ptr<options::ProgramOptions>) override;
  void validateOptions(
      std::shared_ptr<options::ProgramOptions> options) override;
  void start() override;
  void beginShutdown() override;
  void stop() override;

#ifdef ARANGODB_ENABLE_MAINTAINER_MODE
  void getTelemetricsInfo(velocypack::Builder& builder);
  velocypack::Builder sendTelemetricsToEndpoint(std::string const& url);
#endif
#ifdef ARANGODB_ENABLE_FAILURE_TESTS
  void disableAutomaticallySendTelemetricsToEndpoint() {
    this->_automaticallySendTelemetricsToEndpoint = false;
  }
#endif

  void setExitCode(int code) { *_result = code; }

  void startTelemetrics();
  void restartTelemetrics();

 private:
  std::vector<std::string> _executeScripts;
  std::vector<std::string> _executeStrings;
  std::vector<std::string> _checkSyntaxFiles;
  std::vector<std::string> _unitTests;

 public:
  enum class RunMode {
    INTERACTIVE,
    EXECUTE_SCRIPT,
    EXECUTE_STRING,
    CHECK_SYNTAX,
    UNIT_TESTS
  };

 private:
  int* _result;
  RunMode _runMode;
  std::vector<std::string> _positionals;
  std::string _unitTestFilter;
  std::vector<std::string> _scriptParameters;
  std::unique_ptr<TelemetricsHandler> _telemetricsHandler;
#ifdef ARANGODB_ENABLE_FAILURE_TESTS
  bool _automaticallySendTelemetricsToEndpoint{true};
  std::vector<std::string> _failurePoints;
#endif
};

}  // namespace arangodb
