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
class RestHandlerFactory;
class HttpServer;
}

class RestServerThread;

class RestServerFeature final
    : public application_features::ApplicationFeature {
 public:
  static rest::RestHandlerFactory* HANDLER_FACTORY;
  static rest::AsyncJobManager* JOB_MANAGER;
  
  static bool authenticationEnabled() {
    return REST_SERVER != nullptr && REST_SERVER->authentication();
  }
  
  static bool hasProxyCheck() {
    return REST_SERVER != nullptr && REST_SERVER->proxyCheck();
  }
  
  static std::vector<std::string> getTrustedProxies() {
    if (REST_SERVER == nullptr) {
      return std::vector<std::string>();
    }
    return REST_SERVER->trustedProxies();
  }

 private:
  static RestServerFeature* REST_SERVER;

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
  bool _allowMethodOverride;
  bool _authentication;
  bool _authenticationUnixSockets;
  bool _authenticationSystemOnly;
  bool _proxyCheck;
  std::vector<std::string> _trustedProxies;
  std::vector<std::string> _accessControlAllowOrigins;

 public:
  bool authentication() const { return _authentication; }
  bool authenticationUnixSockets() const { return _authenticationUnixSockets; }
  bool authenticationSystemOnly() const { return _authenticationSystemOnly; }
  bool proxyCheck() const { return _proxyCheck; }
  std::vector<std::string> trustedProxies() const { return _trustedProxies; }

 private:
  void buildServers();
  void defineHandlers();

 private:
  std::unique_ptr<rest::RestHandlerFactory> _handlerFactory;
  std::unique_ptr<rest::AsyncJobManager> _jobManager;
  std::vector<rest::HttpServer*> _servers;
};
}

#endif
