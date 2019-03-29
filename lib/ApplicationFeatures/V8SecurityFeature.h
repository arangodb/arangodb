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
/// @author Jan Steemann
////////////////////////////////////////////////////////////////////////////////

#ifndef ARANGODB_APPLICATION_FEATURES_V8SECURITY_FEATURE_H
#define ARANGODB_APPLICATION_FEATURES_V8SECURITY_FEATURE_H 1

#include "ApplicationFeatures/ApplicationFeature.h"
#include <regex>

namespace v8 {
class Isolate;
}

namespace arangodb {

class V8SecurityFeature final : public application_features::ApplicationFeature {
 public:
  explicit V8SecurityFeature(application_features::ApplicationServer& server);

  void collectOptions(std::shared_ptr<options::ProgramOptions>) override final;
  void validateOptions(std::shared_ptr<options::ProgramOptions>) override final;
  void start() override final;

  /// @brief tests if in the current security context it is allowed to define
  /// additional HTTP REST actions
  /// must only be called in arangod!
  bool canDefineHttpAction(v8::Isolate* isolate) const;

  /// @brief tests if the value of the startup option should be exposed to end
  /// users via JavaScript actions. will use _startupOptionsFilter*
  bool shouldExposeStartupOption(v8::Isolate* isolate, std::string const& name) const;

  /// @brief tests if the value of the environment variable should be exposed to end
  /// users via JavaScript actions. will use _environmentVariablesFilter*
  bool shouldExposeEnvironmentVariable(v8::Isolate* isolate, std::string const& name) const;

  /// @brief tests if the IP address or domain/host name given should be accessible
  /// via the JS_Download (internal.download) function in JavaScript actions
  /// the endpoint is passed in via protocol (e.g. tcp://, ssl://, unix://) and port
  /// number (if applicable)
  bool isAllowedToConnectToEndpoint(v8::Isolate* isolate, std::string const& endpoint) const;

  /// @brief tests if the path (or path component) shall be accessible for the
  /// calling JavaScript code
  bool isAllowedToAccessPath(v8::Isolate* isolate, std::string path, bool write = false) const;
  bool isAllowedToAccessPath(v8::Isolate* isolate, char const* path, bool write = false) const;

 private:
  /// @brief regular expression string for startup options filtering
  std::string _startupOptionsFilter;
  /// @brief regular expression generated from _startupOptionsFilter
  std::regex _startupOptionsFilterRegex;
  /// @brief regular expression string for forbidden IP address/host names
  /// to connect to via JS_Download/internal.download
  std::string _endpointsFilter;

  /// @brief regular expression string for environment variables filtering
  std::string _environmentVariablesFilter;
  /// @brief regular expression generated from _environmentVariablesFilter
  std::regex _environmentVariablesFilterRegex;
  /// @brief regular expression generated from _endpointsFilter
  std::regex _endpointsFilterRegex;

  /// @brief
  std::string _filesWhiteList;
  /// @brief
  std::string _filesBlackList;
  /// @brief regular expression for pathWhiteList
  std::regex _filesWhiteListRegex;
  /// @brief regular expression for pathBlackList
  std::regex _filesBlackListRegex;
};

}  // namespace arangodb

#endif
