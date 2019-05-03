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

#ifndef APPLICATION_FEATURES_GENERAL_SERVER_FEATURE_H
#define APPLICATION_FEATURES_GENERAL_SERVER_FEATURE_H 1

#include "ApplicationFeatures/ApplicationFeature.h"

namespace arangodb {

namespace aql {
class QueryRegistry;
}

namespace traverser {
class TraverserEngineRegistry;
}

namespace rest {
class AsyncJobManager;
class RestHandlerFactory;
class GeneralServer;
}  // namespace rest

class RestServerThread;

class GeneralServerFeature final : public application_features::ApplicationFeature {
 public:
  static rest::RestHandlerFactory* HANDLER_FACTORY;
  static rest::AsyncJobManager* JOB_MANAGER;

 public:
  static double keepAliveTimeout() {
    return GENERAL_SERVER != nullptr ? GENERAL_SERVER->_keepAliveTimeout : 300.0;
  };

  static bool hasProxyCheck() {
    return GENERAL_SERVER != nullptr && GENERAL_SERVER->proxyCheck();
  }

  static std::vector<std::string> getTrustedProxies() {
    if (GENERAL_SERVER == nullptr) {
      return std::vector<std::string>();
    }

    return GENERAL_SERVER->trustedProxies();
  }

  static bool allowMethodOverride() {
    if (GENERAL_SERVER == nullptr) {
      return false;
    }

    return GENERAL_SERVER->_allowMethodOverride;
  }

  static std::vector<std::string> const& accessControlAllowOrigins() {
    static std::vector<std::string> empty;

    if (GENERAL_SERVER == nullptr) {
      return empty;
    }

    return GENERAL_SERVER->_accessControlAllowOrigins;
  }

 private:
  static GeneralServerFeature* GENERAL_SERVER;

 public:
  explicit GeneralServerFeature(application_features::ApplicationServer& server);

  void collectOptions(std::shared_ptr<options::ProgramOptions>) override final;
  void validateOptions(std::shared_ptr<options::ProgramOptions>) override final;
  void prepare() override final;
  void start() override final;
  void stop() override final;
  void unprepare() override final;
 
  bool proxyCheck() const { return _proxyCheck; }
  std::vector<std::string> trustedProxies() const { return _trustedProxies; }
 
 private:
  void buildServers();
  void defineHandlers();

 private:
  double _keepAliveTimeout = 300.0;
  bool _allowMethodOverride;
  bool _proxyCheck;
  std::vector<std::string> _trustedProxies;
  std::vector<std::string> _accessControlAllowOrigins;
  std::unique_ptr<rest::RestHandlerFactory> _handlerFactory;
  std::unique_ptr<rest::AsyncJobManager> _jobManager;
  std::unique_ptr<std::pair<aql::QueryRegistry*, traverser::TraverserEngineRegistry*>> _combinedRegistries;
  std::vector<std::unique_ptr<rest::GeneralServer>> _servers;
  uint64_t _numIoThreads;
};

}  // namespace arangodb

#endif
