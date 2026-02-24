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

#include "ApplicationFeatures/ApplicationFeature.h"
#include "GeneralServer/OperationMode.h"
#include "RestServer/ServerFeatureOptions.h"

namespace arangodb {

namespace rest {

class RestHandlerFactory;
class AsyncJobManager;

}  // namespace rest

class ServerFeature final : public application_features::ApplicationFeature {
 public:
  static constexpr std::string_view name() noexcept { return "Server"; }

  static std::string operationModeString(OperationMode mode);

  ServerFeature(application_features::ApplicationServer& server, int* result);

  void collectOptions(std::shared_ptr<options::ProgramOptions>) override final;
  void validateOptions(std::shared_ptr<options::ProgramOptions>) override final;
  void prepare() override final;
  void start() override final;
  void beginShutdown() override final;
  bool isStopping() const { return _isStopping; }

  OperationMode operationMode() const { return _operationMode; }

  std::string operationModeString() const {
    return operationModeString(operationMode());
  }

  std::vector<std::string> const& scripts() const { return _options.scripts; }

  bool isConsoleMode() const {
    return (_operationMode == OperationMode::MODE_CONSOLE);
  }

 private:
  void waitForHeartbeat();

  ServerFeatureOptions _options;
  bool _isStopping = false;
  int* _result;
  OperationMode _operationMode;
};

}  // namespace arangodb
