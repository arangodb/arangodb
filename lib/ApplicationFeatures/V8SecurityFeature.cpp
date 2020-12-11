////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2014-2020 ArangoDB GmbH, Cologne, Germany
/// Copyright 2004-2014 triAGENS GmbH, Cologne, Germany
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

#include <stdlib.h>
#include <algorithm>
#include <cstdint>
#include <exception>
#include <iterator>
#include <map>
#include <sstream>
#include <stdexcept>
#include <utility>

#include "ApplicationFeatures/V8SecurityFeature.h"

#include "ApplicationFeatures/ApplicationServer.h"
#include "ApplicationFeatures/TempFeature.h"
#include "ApplicationFeatures/V8PlatformFeature.h"
#include "Basics/FileUtils.h"
#include "Basics/StringUtils.h"
#include "Basics/application-exit.h"
#include "Basics/debugging.h"
#include "Basics/files.h"
#include "Basics/operating-system.h"
#include "Logger/LogMacros.h"
#include "Logger/Logger.h"
#include "Logger/LoggerStream.h"
#include "ProgramOptions/Option.h"
#include "ProgramOptions/Parameters.h"
#include "ProgramOptions/ProgramOptions.h"
#include "V8/JavaScriptSecurityContext.h"
#include "V8/v8-globals.h"

using namespace arangodb;
using namespace arangodb::basics;
using namespace arangodb::options;

namespace {

void testRegexPair(std::string const& allowList, 
                   std::string const& denyList,
                   char const* optionName) {
  try {
    std::regex(allowList, std::regex::nosubs | std::regex::ECMAScript);
  } catch (std::exception const& ex) {
    LOG_TOPIC("ab6d5", FATAL, arangodb::Logger::FIXME)
        << "value for '--javascript." << optionName
        << "-allowlist' is not a valid regular expression: "
        << ex.what();
    FATAL_ERROR_EXIT();
  }

  try {
    std::regex(denyList, std::regex::nosubs | std::regex::ECMAScript);
  } catch (std::exception const& ex) {
    LOG_TOPIC("ab2d5", FATAL, arangodb::Logger::FIXME)
        << "value for '--javascript." << optionName
        << "-denylist' is not a valid regular expression: "
        << ex.what();
    FATAL_ERROR_EXIT();
  }
}

std::string canonicalpath(std::string const& path) {
#ifndef _WIN32
  auto realPath =
      std::unique_ptr<char, void (*)(void*)>(::realpath(path.c_str(), nullptr), &free);
  if (realPath) {
    return std::string(realPath.get());
  }
  // fallthrough intentional
#endif
  return path;
}

void convertToSingleExpression(std::vector<std::string> const& values, std::string& targetRegex) {
  if (values.empty()) {
    return;
  }

  targetRegex = "(" + arangodb::basics::StringUtils::join(values, '|') + ")";
}

void convertToSingleExpression(std::unordered_set<std::string> const& values,
                               std::string& targetRegex) {
  // does not delete from the set
  if (values.empty()) {
    return;
  }
  auto last = *values.cbegin();

  std::stringstream ss;
  ss << "(";
  for (auto it = std::next(values.cbegin()); it != values.cend(); ++it) {
    ss << *it << "|";
  }

  ss << last;
  ss << ")";
  targetRegex = ss.str();
}

struct checkAllowDenyResult {
  bool result;
  bool allow;
  bool deny;
};

checkAllowDenyResult checkAllowAndDenyList(std::string const& value, 
                                           bool hasAllowList, std::regex const& allowList, 
                                           bool hasDenyList,  std::regex const& denyList) {
  if (!hasAllowList && !hasDenyList) {
    return {true, false, false};
  }

  if (!hasDenyList) {
    // only have an allow list
    bool allow = std::regex_search(value, allowList);
    return {allow, allow, false};
  }

  if (!hasAllowList) {
    // only have a deny list
    bool deny = std::regex_search(value, denyList);
    return {!deny, false, deny};
  }

  std::smatch allowResult{};
  std::smatch denyResult{};
  bool allow = std::regex_search(value, allowResult, allowList);
  bool deny = std::regex_search(value, denyResult, denyList);

  if (allow && !deny) {
    // we only have an allow list hit => allow
    return {true, allow, deny};
  } else if (!allow && deny) {
    // we only have a deny list hit => deny
    return {false, allow, deny};
  } else if (!allow && !deny) {
    // we have neither an allow list nor a deny list hit => deny
    return {false, allow, deny};
  }

  // longer match or deny list wins
  bool allowLongerDeny = allowResult[0].length() > denyResult[0].length();
  return {allowLongerDeny, allowLongerDeny, !allowLongerDeny};
}
}  // namespace

V8SecurityFeature::V8SecurityFeature(application_features::ApplicationServer& server)
    : ApplicationFeature(server, "V8Security"),
      _hardenInternalModule(false),
      _allowProcessControl(false),
      _allowPortTesting(false) {
  setOptional(false);
  startsAfter<TempFeature>();
  startsAfter<V8PlatformFeature>();
}

void V8SecurityFeature::collectOptions(std::shared_ptr<ProgramOptions> options) {
  options->addSection("javascript", "Configure the JavaScript engine");
  options
      ->addOption("--javascript.allow-port-testing",
                  "allow testing of ports from within JavaScript actions",
                  new BooleanParameter(&_allowPortTesting),
                  arangodb::options::makeFlags(
                  arangodb::options::Flags::DefaultNoComponents,
                  arangodb::options::Flags::OnCoordinator,
                  arangodb::options::Flags::OnSingle,
                  arangodb::options::Flags::Hidden))
      .setIntroducedIn(30500);

  options
      ->addOption("--javascript.allow-external-process-control",
                  "allow execution and control of external processes from "
                  "within JavaScript actions",
                  new BooleanParameter(&_allowProcessControl),
                  arangodb::options::makeFlags(
                  arangodb::options::Flags::DefaultNoComponents,
                  arangodb::options::Flags::OnCoordinator,
                  arangodb::options::Flags::OnSingle,
                  arangodb::options::Flags::Hidden))
      .setIntroducedIn(30500);

  options
      ->addOption("--javascript.harden",
                  "disables access to JavaScript functions in the internal "
                  "module: getPid() and logLevel()",
                  new BooleanParameter(&_hardenInternalModule),
                  arangodb::options::makeFlags(
                  arangodb::options::Flags::DefaultNoComponents,
                  arangodb::options::Flags::OnCoordinator,
                  arangodb::options::Flags::OnSingle))
      .setIntroducedIn(30500);

  options
      ->addOption("--javascript.startup-options-allowlist",
                  "startup options whose names match this regular "
                  "expression will be allowed and exposed to JavaScript",
                  new VectorParameter<StringParameter>(&_startupOptionsAllowListVec),
                  arangodb::options::makeFlags(
                  arangodb::options::Flags::DefaultNoComponents,
                  arangodb::options::Flags::OnCoordinator,
                  arangodb::options::Flags::OnSingle))
      .setIntroducedIn(30500);

  options
      ->addOption("--javascript.startup-options-denylist",
                  "startup options whose names match this regular "
                  "expression will not be exposed (if not in allowlist) to "
                  "JavaScript actions",
                  new VectorParameter<StringParameter>(&_startupOptionsDenyListVec),
                  arangodb::options::makeFlags(
                  arangodb::options::Flags::DefaultNoComponents,
                  arangodb::options::Flags::OnCoordinator,
                  arangodb::options::Flags::OnSingle))
      .setIntroducedIn(30500);
  
  options
      ->addOption("--javascript.environment-variables-allowlist",
                  "environment variables that will be accessible in JavaScript",
                  new VectorParameter<StringParameter>(&_environmentVariablesAllowListVec),
                  arangodb::options::makeFlags(
                  arangodb::options::Flags::DefaultNoComponents,
                  arangodb::options::Flags::OnCoordinator,
                  arangodb::options::Flags::OnSingle))
      .setIntroducedIn(30500);
  
  options
      ->addOption("--javascript.environment-variables-denylist",
                  "environment variables that will be inaccessible in "
                  "JavaScript (if not in allowlist)",
                  new VectorParameter<StringParameter>(&_environmentVariablesDenyListVec),
                  arangodb::options::makeFlags(
                  arangodb::options::Flags::DefaultNoComponents,
                  arangodb::options::Flags::OnCoordinator,
                  arangodb::options::Flags::OnSingle))
      .setIntroducedIn(30500);
  
  options
      ->addOption("--javascript.endpoints-allowlist",
                  "endpoints that can be connected to via "
                  "@arangodb/request module in JavaScript actions",
                  new VectorParameter<StringParameter>(&_endpointsAllowListVec),
                  arangodb::options::makeFlags(
                  arangodb::options::Flags::DefaultNoComponents,
                  arangodb::options::Flags::OnCoordinator,
                  arangodb::options::Flags::OnSingle))
      .setIntroducedIn(30500);

  options
      ->addOption("--javascript.endpoints-denylist",
                  "endpoints that cannot be connected to via @arangodb/request "
                  "module in JavaScript actions (if not in allowlist)",
                  new VectorParameter<StringParameter>(&_endpointsDenyListVec),
                  arangodb::options::makeFlags(
                  arangodb::options::Flags::DefaultNoComponents,
                  arangodb::options::Flags::OnCoordinator,
                  arangodb::options::Flags::OnSingle))
      .setIntroducedIn(30500);
  
  options
      ->addOption("--javascript.files-allowlist",
                  "filesystem paths that will be accessible from within "
                  "JavaScript actions",
                  new VectorParameter<StringParameter>(&_filesAllowListVec),
                  arangodb::options::makeFlags(
                  arangodb::options::Flags::DefaultNoComponents,
                  arangodb::options::Flags::OnCoordinator,
                  arangodb::options::Flags::OnSingle))
      .setIntroducedIn(30500);
  
  options->addOldOption("--javascript.startup-options-whitelist", "--javascript.startup-options-allowlist");
  options->addOldOption("--javascript.startup-options-blacklist", "--javascript.startup-options-denylist");
  options->addOldOption("--javascript.environment-variables-whitelist", "--javascript.environment-variables-allowlist");
  options->addOldOption("--javascript.environment-variables-blacklist", "--javascript.environment-variables-denylist");
  options->addOldOption("--javascript.endpoints-whitelist", "--javascript.endpoints-allowlist");
  options->addOldOption("--javascript.endpoints-blacklist", "--javascript.endpoints-denylist");
  options->addOldOption("--javascript.files-whitelist", "--javascript.files-allowlist");
}

void V8SecurityFeature::validateOptions(std::shared_ptr<ProgramOptions> options) {
  // check if the regular expressions compile properly

  // startup options
  convertToSingleExpression(_startupOptionsAllowListVec, _startupOptionsAllowList);
  convertToSingleExpression(_startupOptionsDenyListVec, _startupOptionsDenyList);
  testRegexPair(_startupOptionsAllowList, _startupOptionsDenyList,
                "startup-options");

  // environment variables
  convertToSingleExpression(_environmentVariablesAllowListVec, _environmentVariablesAllowList);
  convertToSingleExpression(_environmentVariablesDenyListVec, _environmentVariablesDenyList);
  testRegexPair(_environmentVariablesAllowList, _environmentVariablesDenyList,
                "environment-variables");

  // endpoints
  convertToSingleExpression(_endpointsAllowListVec, _endpointsAllowList);
  convertToSingleExpression(_endpointsDenyListVec, _endpointsDenyList);
  testRegexPair(_endpointsAllowList, _endpointsDenyList, "endpoints");

  // file access
  convertToSingleExpression(_filesAllowListVec, _filesAllowList);
  testRegexPair(_filesAllowList, "", "files");
}

void V8SecurityFeature::prepare() {
  addToInternalAllowList(TRI_GetTempPath(), FSAccessType::READ);
  addToInternalAllowList(TRI_GetTempPath(), FSAccessType::WRITE);
  TRI_ASSERT(!_writeAllowList.empty());
  TRI_ASSERT(!_readAllowList.empty());
}

void V8SecurityFeature::start() {
  // initialize regexes for filtering options. the regexes must have been validated before
  _startupOptionsAllowListRegex =
      std::regex(_startupOptionsAllowList, std::regex::nosubs | std::regex::ECMAScript);
  _startupOptionsDenyListRegex =
      std::regex(_startupOptionsDenyList, std::regex::nosubs | std::regex::ECMAScript);

  _environmentVariablesAllowListRegex =
      std::regex(_environmentVariablesAllowList, std::regex::nosubs | std::regex::ECMAScript);
  _environmentVariablesDenyListRegex =
      std::regex(_environmentVariablesDenyList, std::regex::nosubs | std::regex::ECMAScript);

  _endpointsAllowListRegex =
      std::regex(_endpointsAllowList, std::regex::nosubs | std::regex::ECMAScript);
  _endpointsDenyListRegex =
      std::regex(_endpointsDenyList, std::regex::nosubs | std::regex::ECMAScript);

  _filesAllowListRegex =
      std::regex(_filesAllowList, std::regex::nosubs | std::regex::ECMAScript);

}

void V8SecurityFeature::dumpAccessLists() const {
  LOG_TOPIC("2cafe", DEBUG, arangodb::Logger::SECURITY)
    << "files allowed by user:" << _filesAllowList
    << ", internal read allow list:" << _readAllowList
    << ", internal write allow list:" << _writeAllowList
    << ", internal startup options allow list:" << _startupOptionsAllowList
    << ", internal startup options deny list: " << _startupOptionsDenyList
    << ", internal environment variable allow list:" << _environmentVariablesAllowList
    << ", internal environment variables deny list: " << _environmentVariablesDenyList
    << ", internal endpoints allow list:" << _endpointsAllowList
    << ", internal endpoints deny list: " << _endpointsDenyList;
}

void V8SecurityFeature::addToInternalAllowList(std::string const& inItem, FSAccessType type) {
  // This function is not efficient and we would not need the _readAllowList
  // to be persistent. But the persistence will help in debugging and
  // there are only a few items expected.
  auto* set = &_readAllowListSet;
  auto* expression = &_readAllowList;
  auto* re = &_readAllowListRegex;

  if (type == FSAccessType::WRITE) {
    set = &_writeAllowListSet;
    expression = &_writeAllowList;
    re = &_writeAllowListRegex;
  }

  auto item = canonicalpath(inItem);
  if ((item.length() > 0) &&
      (item[item.length() - 1] != TRI_DIR_SEPARATOR_CHAR)) {
    item += TRI_DIR_SEPARATOR_STR;
  }
  auto path = "^" + arangodb::basics::StringUtils::escapeRegexParams(item);
  set->emplace(std::move(path));
  expression->clear();
  convertToSingleExpression(*set, *expression);
  try {
    *re = std::regex(*expression, std::regex::nosubs | std::regex::ECMAScript);
  } catch (std::exception const& ex) {
    throw std::invalid_argument(ex.what() + std::string(" '") + *expression + "'");
  }
}

bool V8SecurityFeature::isAllowedToControlProcesses(v8::Isolate* isolate) const {
  TRI_GET_GLOBALS();
  TRI_ASSERT(v8g != nullptr);
  return _allowProcessControl && v8g->_securityContext.canControlProcesses();
}

bool V8SecurityFeature::isAllowedToTestPorts(v8::Isolate* isolate) const {
  return _allowPortTesting;
}

bool V8SecurityFeature::isInternalModuleHardened(v8::Isolate* isolate) const {
  return _hardenInternalModule;
}

bool V8SecurityFeature::isAllowedToDefineHttpAction(v8::Isolate* isolate) const {
  TRI_GET_GLOBALS();
  TRI_ASSERT(v8g != nullptr);
  return v8g->_securityContext.canDefineHttpAction();
}

bool V8SecurityFeature::isInternalContext(v8::Isolate* isolate) const {
  TRI_GET_GLOBALS();
  TRI_ASSERT(v8g != nullptr);
  return v8g->_securityContext.isInternal();
}

bool V8SecurityFeature::shouldExposeStartupOption(v8::Isolate* isolate,
                                                  std::string const& name) const {
  return checkAllowAndDenyList(name, !_startupOptionsAllowList.empty(),
                               _startupOptionsAllowListRegex,
                               !_startupOptionsDenyList.empty(),
                               _startupOptionsDenyListRegex).result;
}

bool V8SecurityFeature::shouldExposeEnvironmentVariable(v8::Isolate* isolate,
                                                        std::string const& name) const {
  return checkAllowAndDenyList(name, !_environmentVariablesAllowList.empty(),
                               _environmentVariablesAllowListRegex,
                               !_environmentVariablesDenyList.empty(),
                               _environmentVariablesDenyListRegex).result;
}

bool V8SecurityFeature::isAllowedToConnectToEndpoint(v8::Isolate* isolate,
                                                     std::string const& endpoint,
                                                     std::string const& url) const {
  TRI_GET_GLOBALS();
  TRI_ASSERT(v8g != nullptr);
  if (v8g->_securityContext.isInternal()) {
    // internal security contexts are allowed to connect to any endpoint
    // this includes connecting to self or to other instances in a cluster
    return true;
  }

  auto endpointResult = checkAllowAndDenyList(endpoint, !_endpointsAllowList.empty(), _endpointsAllowListRegex,
                                              !_endpointsDenyList.empty(), _endpointsDenyListRegex);

  auto urlResult = checkAllowAndDenyList(url, !_endpointsAllowList.empty(), _endpointsAllowListRegex,
                                         !_endpointsDenyList.empty(), _endpointsDenyListRegex);

  return endpointResult.result || ( urlResult.result && !endpointResult.deny);
}

bool V8SecurityFeature::isAllowedToAccessPath(v8::Isolate* isolate, std::string const& path,
                                              FSAccessType access) const {
  return V8SecurityFeature::isAllowedToAccessPath(isolate, path.c_str(), access);
}

bool V8SecurityFeature::isAllowedToAccessPath(v8::Isolate* isolate, char const* pathPtr,
                                              FSAccessType access) const {
  if (_filesAllowList.empty()) {
    return true;
  }

  // check security context first
  TRI_GET_GLOBALS();
  TRI_ASSERT(v8g != nullptr);

  auto const& sec = v8g->_securityContext;
  if ((access == FSAccessType::READ && sec.canReadFs()) ||
      (access == FSAccessType::WRITE && sec.canWriteFs())) {
    return true;  // context may read / write without restrictions
  }

  std::string path = TRI_ResolveSymbolicLink(pathPtr);

  // make absolute
  std::string cwd = FileUtils::currentDirectory().result();
  path = TRI_GetAbsolutePath(path, cwd);

  path = canonicalpath(std::move(path));
  if (basics::FileUtils::isDirectory(path)) {
    path += TRI_DIR_SEPARATOR_STR;
  }

  if (access == FSAccessType::READ && std::regex_search(path, _readAllowListRegex)) {
    // even in restricted contexts we may read module paths
    return true;
  }

  if (access == FSAccessType::WRITE && std::regex_search(path, _writeAllowListRegex)) {
    // even in restricted contexts we may read module paths
    return true;
  }

  return checkAllowAndDenyList(path, !_filesAllowList.empty(), _filesAllowListRegex,
                               false, _filesAllowListRegex /*passed to match the signature but not used*/).result;
}
