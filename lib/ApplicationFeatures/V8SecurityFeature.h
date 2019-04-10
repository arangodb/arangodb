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

#include <regex>
#include "ApplicationFeatures/ApplicationFeature.h"

namespace v8 {
class Isolate;
}

namespace arangodb {

enum class FSAccessType{
  READ,
  WRITE
};

class V8SecurityFeature final : public application_features::ApplicationFeature {
 public:
  explicit V8SecurityFeature(application_features::ApplicationServer& server);

  void collectOptions(std::shared_ptr<options::ProgramOptions>) override final;
  void validateOptions(std::shared_ptr<options::ProgramOptions>) override final;
  void start() override final;

  /// @brief tests if in the current security context it is allowed to
  /// execute external binaries
  bool isAllowedToExecuteExternalBinaries(v8::Isolate* isolate) const;
  bool isAllowedToTestPorts(v8::Isolate* isolate) const;

  /// @brief tests if in the current security context it is allowed to
  /// execute external binaries
  bool isDeniedHardenedJavaScript(v8::Isolate* isolate) const;
  bool isDeniedHardenedApi(v8::Isolate* isolate) const;

  /// @brief tests if in the current security context it is allowed to define
  /// additional HTTP REST actions
  /// must only be called in arangod!
  bool isAllowedToDefineHttpAction(v8::Isolate* isolate) const;

  /// @brief tests if the value of the startup option should be exposed to end
  /// users via JavaScript actions. will use _startupOptionsFilter*
  bool shouldExposeStartupOption(v8::Isolate* isolate, std::string const& name) const;

  /// @brief tests if the value of the environment variable should be exposed to
  /// end users via JavaScript actions. will use _environmentVariablesFilter*
  bool shouldExposeEnvironmentVariable(v8::Isolate* isolate, std::string const& name) const;

  /// @brief tests if the IP address or domain/host name given should be
  /// accessible via the JS_Download (internal.download) function in JavaScript
  /// actions the endpoint is passed in via protocol (e.g. tcp://, ssl://,
  /// unix://) and port number (if applicable)
  bool isAllowedToConnectToEndpoint(v8::Isolate* isolate, std::string const& endpoint) const;

  /// @brief tests if the path (or path component) shall be accessible for the
  /// calling JavaScript code
  bool isAllowedToAccessPath(v8::Isolate* isolate, std::string path, FSAccessType access) const;
  bool isAllowedToAccessPath(v8::Isolate* isolate, char const* path, FSAccessType access) const;
  bool disableFoxxApi(v8::Isolate* isolate) const;
  bool disableFoxxStore(v8::Isolate* isolate) const;
  bool isInternalContext(v8::Isolate* isolate) const;

  void addToInternalReadWhiteList(char const* item);

 private:
  bool _disableFoxxApi;
  bool _disableFoxxStore;
  bool _denyHardenedApi;
  bool _denyHardenedJavaScript;
  bool _allowExecutionOfBinaries;
  bool _allowPortTesting;

  std::string _readWhiteList;
  std::unordered_set<std::string> _readWhiteListSet;
  std::regex _readWhiteListRegex;


  // All the following options have white and black lists.
  // The white-list will take precedence over the black list
  // Items is the corresponding Vector will be joined with
  // an logical OR to the final expression. That in turn
  // will be compile into an std::regex.

  std::string _startupOptionsWhiteList;
  std::vector<std::string> _startupOptionsWhiteListVec;
  std::regex _startupOptionsWhiteListRegex;
  std::string _startupOptionsBlackList;
  std::vector<std::string> _startupOptionsBlackListVec;
  std::regex _startupOptionsBlackListRegex;

  /// @brief regular expression string for forbidden IP address/host names
  /// to connect to via JS_Download/internal.download
  std::string _endpointsWhiteList;
  std::vector<std::string> _endpointsWhiteListVec;
  std::regex _endpointsWhiteListRegex;
  std::string _endpointsBlackList;
  std::vector<std::string> _endpointsBlackListVec;
  std::regex _endpointsBlackListRegex;

  /// @brief regular expression string for environment variables filtering
  std::string _environmentVariablesWhiteList;
  std::vector<std::string> _environmentVariablesWhiteListVec;
  std::regex _environmentVariablesWhiteListRegex;
  std::string _environmentVariablesBlackList;
  std::vector<std::string> _environmentVariablesBlackListVec;
  std::regex _environmentVariablesBlackListRegex;

  /// @brief variables for file access
  std::string _filesWhiteList;
  std::vector<std::string> _filesWhiteListVec;
  std::regex _filesWhiteListRegex;
  std::string _filesBlackList;
  std::vector<std::string> _filesBlackListVec;
  std::regex _filesBlackListRegex;
};

}  // namespace arangodb

#endif
