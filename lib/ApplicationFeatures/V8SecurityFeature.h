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

#include <memory>
#include <regex>
#include <string>
#include <unordered_set>
#include <vector>

#include "ApplicationFeatures/ApplicationFeature.h"

namespace v8 {
class Isolate;
}

namespace arangodb {
namespace application_features {
class ApplicationServer;
}
namespace options {
class ProgramOptions;
}

enum class FSAccessType{
  READ,
  WRITE
};

class V8SecurityFeature final : public application_features::ApplicationFeature {
 public:
  explicit V8SecurityFeature(application_features::ApplicationServer& server);

  void collectOptions(std::shared_ptr<options::ProgramOptions>) override final;
  void validateOptions(std::shared_ptr<options::ProgramOptions>) override final;
  void prepare() override final;
  void start() override final;
  void dumpAccessLists() const;

  /// @brief tests if in the current security context it is allowed to
  /// start, collect and kill external processes
  bool isAllowedToControlProcesses(v8::Isolate* isolate) const;
  bool isAllowedToTestPorts(v8::Isolate* isolate) const;

  /// @brief tests if in the current security context it is allowed to
  /// execute external binaries
  bool isInternalModuleHardened(v8::Isolate* isolate) const;

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
  bool isAllowedToConnectToEndpoint(v8::Isolate* isolate, std::string const& endpoint, std::string const& url) const;

  /// @brief tests if the path (or path component) shall be accessible for the
  /// calling JavaScript code
  bool isAllowedToAccessPath(v8::Isolate* isolate, std::string const& path, FSAccessType) const;
  bool isAllowedToAccessPath(v8::Isolate* isolate, char const* path, FSAccessType) const;
  bool isInternalContext(v8::Isolate* isolate) const;

  void addToInternalWhitelist(std::string const& item, FSAccessType);

 private:
  bool _hardenInternalModule;
  bool _allowProcessControl;
  bool _allowPortTesting;

  std::string _readWhitelist;
  std::unordered_set<std::string> _readWhitelistSet;
  std::regex _readWhitelistRegex;

  std::string _writeWhitelist;
  std::unordered_set<std::string> _writeWhitelistSet;
  std::regex _writeWhitelistRegex;

  // All the following options have whitelists and blacklists.
  // The whitelist will take precedence over the blacklist
  // Items is the corresponding Vector will be joined with
  // an logical OR to the final expression. That in turn
  // will be compiled into an std::regex.

  std::string _startupOptionsWhitelist;
  std::vector<std::string> _startupOptionsWhitelistVec;
  std::regex _startupOptionsWhitelistRegex;
  std::string _startupOptionsBlacklist;
  std::vector<std::string> _startupOptionsBlacklistVec;
  std::regex _startupOptionsBlacklistRegex;

  /// @brief regular expression string for forbidden IP address/host names
  /// to connect to via JS_Download/internal.download
  std::string _endpointsWhitelist;
  std::vector<std::string> _endpointsWhitelistVec;
  std::regex _endpointsWhitelistRegex;
  std::string _endpointsBlacklist;
  std::vector<std::string> _endpointsBlacklistVec;
  std::regex _endpointsBlacklistRegex;

  /// @brief regular expression string for environment variables filtering
  std::string _environmentVariablesWhitelist;
  std::vector<std::string> _environmentVariablesWhitelistVec;
  std::regex _environmentVariablesWhitelistRegex;
  std::string _environmentVariablesBlacklist;
  std::vector<std::string> _environmentVariablesBlacklistVec;
  std::regex _environmentVariablesBlacklistRegex;

  /// @brief variables for file access
  std::string _filesWhitelist;
  std::vector<std::string> _filesWhitelistVec;
  std::regex _filesWhitelistRegex;
};

}  // namespace arangodb

#endif
