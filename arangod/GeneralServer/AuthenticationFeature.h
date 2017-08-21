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
/// @author Andreas Streichardt <andreas@arangodb.com>
////////////////////////////////////////////////////////////////////////////////

#ifndef APPLICATION_FEATURES_AUTHENTICATION_FEATURE_H
#define APPLICATION_FEATURES_AUTHENTICATION_FEATURE_H 1

#include "ApplicationFeatures/ApplicationFeature.h"
#include "VocBase/AuthInfo.h"

namespace arangodb {
class AuthenticationFeature final
    : public application_features::ApplicationFeature {
 private:
  const size_t _maxSecretLength = 64;

 public:
  explicit AuthenticationFeature(application_features::ApplicationServer*);
  ~AuthenticationFeature();
  static AuthenticationFeature* INSTANCE;

 public:
  void collectOptions(std::shared_ptr<options::ProgramOptions>) override final;
  void validateOptions(std::shared_ptr<options::ProgramOptions>) override final;
  void prepare() override final;
  void start() override final;
  void unprepare() override final;

  bool isActive() const { return _active && isEnabled(); }

 private:
  AuthInfo* _authInfo;
  bool _authenticationUnixSockets;
  bool _authenticationSystemOnly;

  std::string _jwtSecretProgramOption;
  bool _active;

 public:
  bool authenticationUnixSockets() const { return _authenticationUnixSockets; }
  bool authenticationSystemOnly() const { return _authenticationSystemOnly; }
  std::string jwtSecret() { return authInfo()->jwtSecret(); }
  std::string generateNewJwtSecret();
  bool hasUserdefinedJwt() { return !_jwtSecretProgramOption.empty(); }
  void setJwtSecret(std::string const& jwtSecret) {
    authInfo()->setJwtSecret(jwtSecret);
  }
  AuthInfo* authInfo();
  AuthLevel canUseDatabase(std::string const& username,
                           std::string const& dbname);
  AuthLevel canUseCollection(std::string const& username,
                             std::string const& dbname,
                             std::string const& collection);
};
};

#endif
