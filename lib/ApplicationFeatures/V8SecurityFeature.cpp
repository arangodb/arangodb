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

#include "ApplicationFeatures/V8SecurityFeature.h"

#include "Basics/FileUtils.h"
#include "Basics/MutexLocker.h"
#include "Basics/StringUtils.h"
#include "Basics/files.h"
#include "Basics/tri-strings.h"
#include "Logger/Logger.h"
#include "ProgramOptions/ProgramOptions.h"
#include "ProgramOptions/Section.h"
#include "V8/v8-globals.h"

#include <v8.h>

using namespace arangodb;
using namespace arangodb::basics;
using namespace arangodb::options;

V8SecurityFeature::V8SecurityFeature(application_features::ApplicationServer& server)
    : ApplicationFeature(server, "V8Security"),
      _disableFoxxApi(false),
      _disableFoxxStore(false),
      _denyHardenedApi(false),
      _denyHardenedJavaScript(false),
      _allowExecutionOfBinaries(false),
      _allowPortTesting(false) {
  setOptional(false);
  startsAfter("V8Platform");
}

void V8SecurityFeature::collectOptions(std::shared_ptr<ProgramOptions> options) {
  options->addSection("server", "Server features");
  options->addOption(
      "--server.harden",
      "users will not be able to receive version information or change "
      "the log level via the rest api. Admin users and servers without "
      "authentication will be unaffected.",
      new BooleanParameter(&_denyHardenedApi));


  options->addSection("foxx", "Configure Foxx");
  options->addOption("--foxx.disable-api", "disables Foxx management api",
                     new BooleanParameter(&_disableFoxxApi));
  options->addOption("--foxx.disable-store", "disables Foxx store in web-ui",
                     new BooleanParameter(&_disableFoxxStore));


  options->addSection("javascript", "Configure the Javascript engine");
  options->addOption("--javascript.allow-port-testing", "allow testing of ports",
                     new BooleanParameter(&_allowPortTesting));
  options->addOption("--javascript.allow-external-process-control",
                     "allow execution and control of external binaries.",
                     new BooleanParameter(&_allowExecutionOfBinaries));
  options->addOption("--javascript.harden",
                     "disables javascript funtions: getPid(), "
                     "processStatistics() andl logLevel()",
                     new BooleanParameter(&_denyHardenedJavaScript));

  options->addOption("--javascript.startup-options-white-list",
                     "startup options whose names match this regular "
                     "expression will be whitelisted and exposed to JavaScript",
                     new VectorParameter<StringParameter>(&_startupOptionsWhiteListVec));
  options->addOption("--javascript.startup-options-black-list",
                     "startup options whose names match this regular "
                     "expression will not be exposed (if not whitelisted) to "
                     "JavaScript actions",
                     new VectorParameter<StringParameter>(&_startupOptionsBlackListVec));

  options->addOption("--javascript.environment-variables-white-list",
                     "variables that will be accessible in JavaScript",
                     new VectorParameter<StringParameter>(&_environmentVariablesWhiteListVec));
  options->addOption("--javascript.environment-variables-black-list",
                     "variables that will be inaccessible in JavaScript if not white listed",
                     new VectorParameter<StringParameter>(&_environmentVariablesBlackListVec));

  options->addOption("--javascript.endpoints-white-list",
                     "endpoints that can be connected to via internal.download() in JavaScript actions",
                     new VectorParameter<StringParameter>(&_endpointsWhiteListVec));
  options->addOption("--javascript.endpoints-black-list",
                     "endpoints that cannot be connected to via internal.download() in JavaScript actions if not white listed",
                     new VectorParameter<StringParameter>(&_endpointsBlackListVec));

  options->addOption("--javascript.files-white-list",
                     "paths that will be accessible from within JavaScript in restricted contexts",
                     new VectorParameter<StringParameter>(&_filesWhiteListVec));
  options->addOption("--javascript.files-black-list",
                     "paths that will be inaccessible from within JavaScript in restricted contexts if not white listed",
                     new VectorParameter<StringParameter>(&_filesBlackListVec));
}

namespace {
void convertToRe(std::vector<std::string>& files, std::string& target_re) {
  if (!files.empty()) {
    std::stringstream ss;
    std::string last = std::move(files.back());
    files.pop_back();

    for (auto const& f : files) {
      ss << f << "|";
    }
    ss << last;
    target_re = ss.str();
  }
};

void convertToRe(std::unordered_set<std::string>& files, std::string& target_re) {
  if (!files.empty()) {
    std::stringstream ss;
    std::string last = std::move(*files.begin());
    files.erase(files.begin());

    for (auto const& f : files) {
      ss << f << "|";
    }
    ss << last;
    target_re = ss.str();
  }
};

bool checkBlackAndWhiteList(std::string const& value, bool hasWhiteList,
                            std::regex const& whiteList, bool hasBlacklist,
                            std::regex const& blackList) {
  if (!hasWhiteList && !hasBlacklist) {
    return true;
  }

  if (!hasBlacklist) {
    // must be white listed
    return std::regex_search(value, whiteList);
  }

  if (!hasWhiteList) {
    // must be white listed
    return !std::regex_search(value, blackList);
  }

  if (std::regex_search(value, whiteList)) {
    return true;  // white-list wins - simple implementation
  } else {
    return !std::regex_search(value, blackList);
  }
}
}  // namespace

void V8SecurityFeature::addToInternalReadWhiteList(char const* item) {
  // This function is not efficient and we would not need the _readWhiteList
  // to be persistent. But the persistence will help in debugging and
  // there are only a few items expected.
  TRI_ASSERT(_readWhiteListSet.size() < 3);
  auto path = std::string(item) + "*";
  _readWhiteListSet.emplace(std::move(path));
  _readWhiteList.clear();
  convertToRe(_readWhiteListSet, _readWhiteList);
  _readWhiteListRegex =
      std::regex(_readWhiteList, std::regex::nosubs | std::regex::ECMAScript);
  LOG_TOPIC("de5db6", DEBUG, Logger::FIXME) << _readWhiteList;
}

void V8SecurityFeature::validateOptions(std::shared_ptr<ProgramOptions> options) {
  // check if the regular expressions compile properly

  // startup options
  convertToRe(_startupOptionsWhiteListVec, _startupOptionsWhiteList);
  try {
    std::regex(_startupOptionsWhiteList, std::regex::nosubs | std::regex::ECMAScript);
  } catch (std::exception const& ex) {
    LOG_TOPIC("ab9d5", FATAL, arangodb::Logger::FIXME)
        << "value for '--javascript.startup-options-white-list' is not a "
           "valid regular "
           "expression: "
        << ex.what();
    FATAL_ERROR_EXIT();
  }

  convertToRe(_startupOptionsBlackListVec, _startupOptionsBlackList);
  try {
    std::regex(_startupOptionsBlackList, std::regex::nosubs | std::regex::ECMAScript);
  } catch (std::exception const& ex) {
    LOG_TOPIC("ab8d5", FATAL, arangodb::Logger::FIXME)
        << "value for '--javascript.startup-options-black-list' is not a "
           "valid regular "
           "expression: "
        << ex.what();
    FATAL_ERROR_EXIT();
  }

  // environment variables
  convertToRe(_environmentVariablesWhiteListVec, _environmentVariablesWhiteList);
  try {
    std::regex(_environmentVariablesWhiteList, std::regex::nosubs | std::regex::ECMAScript);
  } catch (std::exception const& ex) {
    LOG_TOPIC("ab9d5", FATAL, arangodb::Logger::FIXME)
        << "value for '--javascript.environment-variables-white-list' is not a "
           "valid regular "
           "expression: "
        << ex.what();
    FATAL_ERROR_EXIT();
  }

  convertToRe(_environmentVariablesBlackListVec, _environmentVariablesBlackList);
  try {
    std::regex(_environmentVariablesBlackList, std::regex::nosubs | std::regex::ECMAScript);
  } catch (std::exception const& ex) {
    LOG_TOPIC("ab8d5", FATAL, arangodb::Logger::FIXME)
        << "value for '--javascript.environment-variables-black-list' is not a "
           "valid regular "
           "expression: "
        << ex.what();
    FATAL_ERROR_EXIT();
  }

  // endpoints
  convertToRe(_endpointsWhiteListVec, _endpointsWhiteList);
  try {
    std::regex(_endpointsWhiteList, std::regex::nosubs | std::regex::ECMAScript);
  } catch (std::exception const& ex) {
    LOG_TOPIC("ab9d5", FATAL, arangodb::Logger::FIXME)
        << "value for '--javascript.endpoints-white-list' is not a "
           "valid regular "
           "expression: "
        << ex.what();
    FATAL_ERROR_EXIT();
  }

  convertToRe(_endpointsBlackListVec, _endpointsBlackList);
  try {
    std::regex(_endpointsBlackList, std::regex::nosubs | std::regex::ECMAScript);
  } catch (std::exception const& ex) {
    LOG_TOPIC("ab8d5", FATAL, arangodb::Logger::FIXME)
        << "value for '--javascript.endpoints-black-list' is not a "
           "valid regular "
           "expression: "
        << ex.what();
    FATAL_ERROR_EXIT();
  }

  // file access
  convertToRe(_filesWhiteListVec, _filesWhiteList);
  try {
    std::regex(_filesWhiteList, std::regex::nosubs | std::regex::ECMAScript);
  } catch (std::exception const& ex) {
    LOG_TOPIC("ab9d5", FATAL, arangodb::Logger::FIXME)
        << "value for '--javascript.files-white-list' is not a "
           "valid regular "
           "expression: "
        << ex.what();
    FATAL_ERROR_EXIT();
  }

  convertToRe(_filesBlackListVec, _filesBlackList);
  try {
    std::regex(_filesBlackList, std::regex::nosubs | std::regex::ECMAScript);
  } catch (std::exception const& ex) {
    LOG_TOPIC("ab8d5", FATAL, arangodb::Logger::FIXME)
        << "value for '--javascript.files-black-list' is not a "
           "valid regular "
           "expression: "
        << ex.what();
    FATAL_ERROR_EXIT();
  }
}

void V8SecurityFeature::start() {
  // initialize regexes for filtering options. the regexes must have been validated before
  _startupOptionsWhiteListRegex =
      std::regex(_startupOptionsWhiteList, std::regex::nosubs | std::regex::ECMAScript);
  _startupOptionsBlackListRegex =
      std::regex(_startupOptionsBlackList, std::regex::nosubs | std::regex::ECMAScript);

  _environmentVariablesWhiteListRegex =
      std::regex(_environmentVariablesWhiteList, std::regex::nosubs | std::regex::ECMAScript);
  _environmentVariablesBlackListRegex =
      std::regex(_environmentVariablesBlackList, std::regex::nosubs | std::regex::ECMAScript);

  _endpointsWhiteListRegex =
      std::regex(_endpointsWhiteList, std::regex::nosubs | std::regex::ECMAScript);
  _endpointsBlackListRegex =
      std::regex(_endpointsBlackList, std::regex::nosubs | std::regex::ECMAScript);

  _filesWhiteListRegex =
      std::regex(_filesWhiteList, std::regex::nosubs | std::regex::ECMAScript);
  _filesBlackListRegex =
      std::regex(_filesBlackList, std::regex::nosubs | std::regex::ECMAScript);
}

bool V8SecurityFeature::isAllowedToExecuteExternalBinaries(v8::Isolate* isolate) const {
  TRI_GET_GLOBALS();
  // v8g may be a nullptr when we are in arangosh
  if (v8g != nullptr) {
    return _allowExecutionOfBinaries || v8g->_securityContext.canExecuteExternalBinaries();
  }
  return _allowExecutionOfBinaries;
}

bool V8SecurityFeature::isAllowedToTestPorts(v8::Isolate* isolate) const {
  return _allowPortTesting;
}

bool V8SecurityFeature::disableFoxxApi(v8::Isolate* isolate) const {
  return _disableFoxxApi;
}

bool V8SecurityFeature::disableFoxxStore(v8::Isolate* isolate) const {
  return _disableFoxxStore;
}

bool V8SecurityFeature::isDeniedHardenedJavaScript(v8::Isolate* isolate) const {
  return _denyHardenedJavaScript;
}

bool V8SecurityFeature::isDeniedHardenedApi(v8::Isolate* isolate) const {
  return _denyHardenedApi;
}

bool V8SecurityFeature::isAllowedToDefineHttpAction(v8::Isolate* isolate) const {
  TRI_GET_GLOBALS();
  // v8g may be a nullptr when we are in arangosh
  return v8g != nullptr && v8g->_securityContext.canDefineHttpAction();
}

bool V8SecurityFeature::shouldExposeStartupOption(v8::Isolate* isolate,
                                                  std::string const& name) const {
  return checkBlackAndWhiteList(name, !_startupOptionsWhiteList.empty(),
                                _startupOptionsWhiteListRegex,
                                !_startupOptionsBlackList.empty(),
                                _startupOptionsBlackListRegex);
}

bool V8SecurityFeature::shouldExposeEnvironmentVariable(v8::Isolate* isolate,
                                                        std::string const& name) const {
  return checkBlackAndWhiteList(name, !_environmentVariablesWhiteList.empty(),
                                _environmentVariablesWhiteListRegex,
                                !_environmentVariablesBlackList.empty(),
                                _environmentVariablesBlackListRegex);
}

bool V8SecurityFeature::isAllowedToConnectToEndpoint(v8::Isolate* isolate,
                                                     std::string const& name) const {
  TRI_GET_GLOBALS();
  if (v8g != nullptr && v8g->_securityContext.isInternal()) {
    // internal security contexts are allowed to connect to any endpoint
    // this includes connecting to self or to other instances in a cluster
    return true;
  }

  return checkBlackAndWhiteList(name, !_endpointsWhiteList.empty(), _endpointsWhiteListRegex,
                                !_endpointsBlackList.empty(), _endpointsBlackListRegex);
}

bool V8SecurityFeature::isAllowedToAccessPath(v8::Isolate* isolate, char const* path,
                                              FSAccessType access) const {
  // expects 0 terminated utf-8 string
  TRI_ASSERT(path != nullptr);

  return isAllowedToAccessPath(isolate, std::string(path), access);
}

bool V8SecurityFeature::isAllowedToAccessPath(v8::Isolate* isolate, std::string path,
                                              FSAccessType access) const {
  // check security context first
  TRI_GET_GLOBALS();

  if (v8g == nullptr) {
    TRI_ASSERT(false);
    return false;
  };

  auto& sec = v8g->_securityContext;
  if ((access == FSAccessType::READ && sec.canReadFs()) ||
      (access == FSAccessType::WRITE && sec.canWriteFs())) {
    return true;  // context may read / write without restrictions
  }

  // remove link
  path = TRI_ResolveSymbolicLink(std::move(path));

  // make absolute
  std::string cwd = FileUtils::currentDirectory().result();
  {
    auto absPath = std::unique_ptr<char, void (*)(char*)>(
        TRI_GetAbsolutePath(path.c_str(), cwd.c_str()), &TRI_FreeString);
    if (absPath) {
      path = std::string(absPath.get());
    }
  }

  if (access == FSAccessType::READ && std::regex_search(path, _readWhiteListRegex)) {
    // even in restricted contexts we may read module paths
    return true;
  }

  return checkBlackAndWhiteList(path, !_filesWhiteList.empty(), _filesWhiteListRegex,
                                !_filesBlackList.empty(), _filesBlackListRegex);
}
