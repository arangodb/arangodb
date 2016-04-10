////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2016 ArangoDB GmbH, Cologne, Germany
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

#include "Actions/RestActionHandler.h"
#include "Rest/OperationMode.h"

namespace arangodb {
namespace rest {
class HttpHandlerFactory;
class AsyncJobManager;
}

class ConsoleThread;

class ServerFeature final : public application_features::ApplicationFeature {
 public:
  ServerFeature(application_features::ApplicationServer* server,
                std::string const&, int*);

 public:
  void collectOptions(std::shared_ptr<options::ProgramOptions>) override;
  void validateOptions(std::shared_ptr<options::ProgramOptions>) override;
  void prepare() override;
  void start() override;
  void beginShutdown() override;
  void stop() override;

 public:
  rest::HttpHandlerFactory* httpHandlerFactory() {
    return _handlerFactory.get();
  }
  rest::AsyncJobManager* jobManager() { return _jobManager; }

 private:
  int32_t _defaultApiCompatibility;
  bool _allowMethodOverride;
  bool _console;
  bool _restServer;
  bool _authentication;
  std::vector<std::string> _unittests;
  std::vector<std::string> _scripts;

 private:
  void startConsole();
  void stopConsole();
  void buildHandlerFactory();
  void defineHandlers();

 private:
  std::string const _authenticationRealm;
  int* _result;
  std::unique_ptr<rest::HttpHandlerFactory> _handlerFactory;
  rest::AsyncJobManager* _jobManager;
  RestActionHandler::action_options_t _httpOptions;
  OperationMode _operationMode;
  std::unique_ptr<ConsoleThread> _consoleThread;
};
}

#endif
