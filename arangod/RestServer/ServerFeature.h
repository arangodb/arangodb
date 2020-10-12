////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2014-2020 ArangoDB GmbH, Cologne, Germany
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

#ifndef REST_SERVER_SERVER_FEATURE_H
#define REST_SERVER_SERVER_FEATURE_H 1

#include "ApplicationFeatures/ApplicationFeature.h"
#include "GeneralServer/OperationMode.h"

namespace arangodb {

namespace rest {

class RestHandlerFactory;
class AsyncJobManager;

}  // namespace rest

class ServerFeature final : public application_features::ApplicationFeature {
 public:
  static std::string operationModeString(OperationMode mode);

  ServerFeature(application_features::ApplicationServer& server, int* result);

  void collectOptions(std::shared_ptr<options::ProgramOptions>) override final;
  void validateOptions(std::shared_ptr<options::ProgramOptions>) override final;
  void prepare() override final;
  void start() override final;
  void stop() override final;
  void beginShutdown() override final;
  bool isStopping() const { return _isStopping; }

  OperationMode operationMode() const { return _operationMode; }

  std::string operationModeString() const {
    return operationModeString(operationMode());
  }

  std::vector<std::string> const& scripts() const { return _scripts; }

  bool isConsoleMode() const {
    return (_operationMode == OperationMode::MODE_CONSOLE);
  }

 private:
  void waitForHeartbeat();

  bool _console = false;
  bool _restServer = true;
  bool _validateUtf8Strings = true;
  bool _isStopping = false;
  std::vector<std::string> _scripts;
  int* _result;
  OperationMode _operationMode;
#if _WIN32
  uint16_t _codePage;
  uint16_t _originalCodePage;
#endif
};

}  // namespace arangodb

#endif
