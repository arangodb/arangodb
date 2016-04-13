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

#ifndef APPLICATION_FEATURES_REST_SERVER_FEATURE_H
#define APPLICATION_FEATURES_REST_SERVER_FEATURE_H 1

#include "ApplicationFeatures/ApplicationFeature.h"

#include "Actions/RestActionHandler.h"

namespace arangodb {
namespace rest {
class AsyncJobManager;
class HttpHandlerFactory;
class HttpServer;
}

class RestServerThread;

class RestServerFeature final
    : public application_features::ApplicationFeature {
 public:
  RestServerFeature(application_features::ApplicationServer*,
                    std::string const&);

 public:
  void collectOptions(std::shared_ptr<options::ProgramOptions>) override final;
  void validateOptions(std::shared_ptr<options::ProgramOptions>) override final;
  void prepare() override final;
  void start() override final;
  void stop() override final;

 private:
  double _keepAliveTimeout;
  std::string const _authenticationRealm;
  int32_t _defaultApiCompatibility;
  bool _allowMethodOverride;

 public:
#warning TODO
  bool authentication() const { return false; }
  bool authenticationUnixSockets() const { return false; }
  bool authenticationSystemOnly() const { return false; }

 private:
  void buildServers();
  void defineHandlers();

 private:
  std::unique_ptr<rest::HttpHandlerFactory> _handlerFactory;
  std::unique_ptr<rest::AsyncJobManager> _jobManager;
  std::vector<rest::HttpServer*> _servers;
  RestActionHandler::action_options_t _httpOptions;
};
}

#endif
