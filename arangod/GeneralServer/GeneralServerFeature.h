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

#include <openssl/ssl.h>

#include "Actions/RestActionHandler.h"
#include "VocBase/AuthInfo.h"

namespace arangodb {
namespace rest {
class AsyncJobManager;
class RestHandlerFactory;
class GeneralServer;
}

class RestServerThread;

class GeneralServerFeature final
    : public application_features::ApplicationFeature {
 public:
  typedef int (*verification_callback_fptr)(int, X509_STORE_CTX*);

 public:
  static rest::RestHandlerFactory* HANDLER_FACTORY;
  static rest::AsyncJobManager* JOB_MANAGER;
  static AuthInfo AUTH_INFO;

 public:
  static double keepAliveTimeout() {
    return GENERAL_SERVER != nullptr ? GENERAL_SERVER->_keepAliveTimeout
                                     : 300.0;
  };

  static int verificationMode() {
    return GENERAL_SERVER != nullptr ? GENERAL_SERVER->_verificationMode
                                     : SSL_VERIFY_NONE;
  };

  static verification_callback_fptr verificationCallback() {
    return GENERAL_SERVER != nullptr ? GENERAL_SERVER->_verificationCallback
                                     : nullptr;
  };

  static bool authenticationEnabled() {
    return GENERAL_SERVER != nullptr && GENERAL_SERVER->_authentication;
  }

  static bool hasProxyCheck() {
    return GENERAL_SERVER != nullptr && GENERAL_SERVER->proxyCheck();
  }

  static std::vector<std::string> getTrustedProxies() {
    if (GENERAL_SERVER == nullptr) {
      return std::vector<std::string>();
    }

    return GENERAL_SERVER->trustedProxies();
  }

  static std::string getJwtSecret() {
    if (GENERAL_SERVER == nullptr) {
      return std::string();
    }

    return GENERAL_SERVER->jwtSecret();
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
  static const size_t _maxSecretLength = 64;

 public:
  explicit GeneralServerFeature(application_features::ApplicationServer*);

 public:
  void collectOptions(std::shared_ptr<options::ProgramOptions>) override final;
  void validateOptions(std::shared_ptr<options::ProgramOptions>) override final;
  void prepare() override final;
  void start() override final;
  void stop() override final;
  void unprepare() override final;

 public:
  void setVerificationMode(int mode) { _verificationMode = mode; }
  void setVerificationCallback(int (*func)(int, X509_STORE_CTX*)) {
    _verificationCallback = func;
  }

 private:
  double _keepAliveTimeout = 300.0;
  bool _allowMethodOverride;
  bool _authentication;
  bool _authenticationUnixSockets;
  bool _authenticationSystemOnly;

  bool _proxyCheck;
  std::vector<std::string> _trustedProxies;
  std::vector<std::string> _accessControlAllowOrigins;

  std::string _jwtSecret;
  int _verificationMode;
  verification_callback_fptr _verificationCallback;

 public:
  bool authentication() const { return _authentication; }
  bool authenticationUnixSockets() const { return _authenticationUnixSockets; }
  bool authenticationSystemOnly() const { return _authenticationSystemOnly; }
  bool proxyCheck() const { return _proxyCheck; }
  std::vector<std::string> trustedProxies() const { return _trustedProxies; }
  std::string jwtSecret() const { return _jwtSecret; }
  void generateNewJwtSecret();
  void setJwtSecret(std::string const& jwtSecret) { _jwtSecret = jwtSecret; }

 private:
  void buildServers();
  void defineHandlers();

 private:
  std::unique_ptr<rest::RestHandlerFactory> _handlerFactory;
  std::unique_ptr<rest::AsyncJobManager> _jobManager;
  std::vector<rest::GeneralServer*> _servers;
};
}

#endif
