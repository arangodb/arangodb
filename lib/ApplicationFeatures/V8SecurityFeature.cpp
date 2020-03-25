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
#include "Basics/FileResultString.h"
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

void testRegexPair(std::string const& whitelist, std::string const& blacklist,
                   char const* optionName) {
  try {
    std::regex(whitelist, std::regex::nosubs | std::regex::ECMAScript);
  } catch (std::exception const& ex) {
    LOG_TOPIC("ab6d5", FATAL, arangodb::Logger::FIXME)
        << "value for '--javascript." << optionName
        << "-whitelist' is not a "
           "valid regular expression: "
        << ex.what();
    FATAL_ERROR_EXIT();
  }

  try {
    std::regex(blacklist, std::regex::nosubs | std::regex::ECMAScript);
  } catch (std::exception const& ex) {
    LOG_TOPIC("ab2d5", FATAL, arangodb::Logger::FIXME)
        << "value for '--javascript." << optionName
        << "-blacklist' is not a "
           "valid regular expression: "
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

struct checkBlackWhiteResult {
  bool result;
  bool white;
  bool black;
};

checkBlackWhiteResult checkBlackAndWhitelist(std::string const& value, bool hasWhitelist,
                            std::regex const& whitelist, bool hasBlacklist,
                            std::regex const& blacklist) {
  if (!hasWhitelist && !hasBlacklist) {
    return {true, false, false};
  }

  if (!hasBlacklist) {
    // only have a whitelist
    bool white = std::regex_search(value, whitelist);
    return {white, white, false};
  }

  if (!hasWhitelist) {
    // only have a blacklist
    bool black = std::regex_search(value, blacklist);
    return {!black, false, black};
  }

  std::smatch white_result{};
  std::smatch black_result{};
  bool white = std::regex_search(value, white_result, whitelist);
  bool black = std::regex_search(value, black_result, blacklist);

  if (white && !black) {
    // we only have a whitelist hit => allow
    return {true, white, black};
  } else if (!white && black) {
    // we only have a blacklist hit => deny
    return {false, white, black};
  } else if (!white && !black) {
    // we have neither a whitelist nor a blacklist hit => deny
    return {false, white, black};
  }

  // longer match or blacklist wins
  bool white_longer_black = white_result[0].length() > black_result[0].length();
  return {white_longer_black, white_longer_black, !white_longer_black};
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
      ->addOption("--javascript.startup-options-whitelist",
                  "startup options whose names match this regular "
                  "expression will be whitelisted and exposed to JavaScript",
                  new VectorParameter<StringParameter>(&_startupOptionsWhitelistVec),
                  arangodb::options::makeFlags(
                  arangodb::options::Flags::DefaultNoComponents,
                  arangodb::options::Flags::OnCoordinator,
                  arangodb::options::Flags::OnSingle))
      .setIntroducedIn(30500);
  options
      ->addOption("--javascript.startup-options-blacklist",
                  "startup options whose names match this regular "
                  "expression will not be exposed (if not whitelisted) to "
                  "JavaScript actions",
                  new VectorParameter<StringParameter>(&_startupOptionsBlacklistVec),
                  arangodb::options::makeFlags(
                  arangodb::options::Flags::DefaultNoComponents,
                  arangodb::options::Flags::OnCoordinator,
                  arangodb::options::Flags::OnSingle))
      .setIntroducedIn(30500);

  options
      ->addOption("--javascript.environment-variables-whitelist",
                  "environment variables that will be accessible in JavaScript",
                  new VectorParameter<StringParameter>(&_environmentVariablesWhitelistVec),
                  arangodb::options::makeFlags(
                  arangodb::options::Flags::DefaultNoComponents,
                  arangodb::options::Flags::OnCoordinator,
                  arangodb::options::Flags::OnSingle))
      .setIntroducedIn(30500);
  options
      ->addOption("--javascript.environment-variables-blacklist",
                  "environment variables that will be inaccessible in "
                  "JavaScript if not whitelisted",
                  new VectorParameter<StringParameter>(&_environmentVariablesBlacklistVec),
                  arangodb::options::makeFlags(
                  arangodb::options::Flags::DefaultNoComponents,
                  arangodb::options::Flags::OnCoordinator,
                  arangodb::options::Flags::OnSingle))
      .setIntroducedIn(30500);

  options
      ->addOption("--javascript.endpoints-whitelist",
                  "endpoints that can be connected to via "
                  "@arangodb/request module in JavaScript actions",
                  new VectorParameter<StringParameter>(&_endpointsWhitelistVec),
                  arangodb::options::makeFlags(
                  arangodb::options::Flags::DefaultNoComponents,
                  arangodb::options::Flags::OnCoordinator,
                  arangodb::options::Flags::OnSingle))
      .setIntroducedIn(30500);
  options
      ->addOption("--javascript.endpoints-blacklist",
                  "endpoints that cannot be connected to via @arangodb/request "
                  "module in "
                  "JavaScript actions if not whitelisted",
                  new VectorParameter<StringParameter>(&_endpointsBlacklistVec),
                  arangodb::options::makeFlags(
                  arangodb::options::Flags::DefaultNoComponents,
                  arangodb::options::Flags::OnCoordinator,
                  arangodb::options::Flags::OnSingle))
      .setIntroducedIn(30500);

  options
      ->addOption("--javascript.files-whitelist",
                  "filesystem paths that will be accessible from within "
                  "JavaScript actions",
                  new VectorParameter<StringParameter>(&_filesWhitelistVec),
                  arangodb::options::makeFlags(
                  arangodb::options::Flags::DefaultNoComponents,
                  arangodb::options::Flags::OnCoordinator,
                  arangodb::options::Flags::OnSingle))
      .setIntroducedIn(30500);
}

void V8SecurityFeature::validateOptions(std::shared_ptr<ProgramOptions> options) {
  // check if the regular expressions compile properly

  // startup options
  convertToSingleExpression(_startupOptionsWhitelistVec, _startupOptionsWhitelist);
  convertToSingleExpression(_startupOptionsBlacklistVec, _startupOptionsBlacklist);
  testRegexPair(_startupOptionsWhitelist, _startupOptionsBlacklist,
                "startup-options");

  // environment variables
  convertToSingleExpression(_environmentVariablesWhitelistVec, _environmentVariablesWhitelist);
  convertToSingleExpression(_environmentVariablesBlacklistVec, _environmentVariablesBlacklist);
  testRegexPair(_environmentVariablesWhitelist, _environmentVariablesBlacklist,
                "environment-variables");

  // endpoints
  convertToSingleExpression(_endpointsWhitelistVec, _endpointsWhitelist);
  convertToSingleExpression(_endpointsBlacklistVec, _endpointsBlacklist);
  testRegexPair(_endpointsWhitelist, _endpointsBlacklist, "endpoints");

  // file access
  convertToSingleExpression(_filesWhitelistVec, _filesWhitelist);
  testRegexPair(_filesWhitelist, "", "files");
}

void V8SecurityFeature::prepare() {
  addToInternalWhitelist(TRI_GetTempPath(), FSAccessType::READ);
  addToInternalWhitelist(TRI_GetTempPath(), FSAccessType::WRITE);
  TRI_ASSERT(!_writeWhitelist.empty());
  TRI_ASSERT(!_readWhitelist.empty());
}

void V8SecurityFeature::start() {
  // initialize regexes for filtering options. the regexes must have been validated before
  _startupOptionsWhitelistRegex =
      std::regex(_startupOptionsWhitelist, std::regex::nosubs | std::regex::ECMAScript);
  _startupOptionsBlacklistRegex =
      std::regex(_startupOptionsBlacklist, std::regex::nosubs | std::regex::ECMAScript);

  _environmentVariablesWhitelistRegex =
      std::regex(_environmentVariablesWhitelist, std::regex::nosubs | std::regex::ECMAScript);
  _environmentVariablesBlacklistRegex =
      std::regex(_environmentVariablesBlacklist, std::regex::nosubs | std::regex::ECMAScript);

  _endpointsWhitelistRegex =
      std::regex(_endpointsWhitelist, std::regex::nosubs | std::regex::ECMAScript);
  _endpointsBlacklistRegex =
      std::regex(_endpointsBlacklist, std::regex::nosubs | std::regex::ECMAScript);

  _filesWhitelistRegex =
      std::regex(_filesWhitelist, std::regex::nosubs | std::regex::ECMAScript);

}

void V8SecurityFeature::dumpAccessLists() const {
  LOG_TOPIC("2cafe", DEBUG, arangodb::Logger::SECURITY)
    << "files whitelisted by user:" << _filesWhitelist
    << ", internal read whitelist:" << _readWhitelist
    << ", internal write whitelist:" << _writeWhitelist
    << ", internal startup options whitelist:" << _startupOptionsWhitelist
    << ", internal startup options blacklist: " << _startupOptionsBlacklist
    << ", internal environment variable whitelist:" << _environmentVariablesWhitelist
    << ", internal environment variables blacklist: " << _environmentVariablesBlacklist
    << ", internal endpoints whitelist:" << _endpointsWhitelist
    << ", internal endpoints blacklist: " << _endpointsBlacklist;
}

void V8SecurityFeature::addToInternalWhitelist(std::string const& inItem, FSAccessType type) {
  // This function is not efficient and we would not need the _readWhitelist
  // to be persistent. But the persistence will help in debugging and
  // there are only a few items expected.
  auto* set = &_readWhitelistSet;
  auto* expression = &_readWhitelist;
  auto* re = &_readWhitelistRegex;

  if (type == FSAccessType::WRITE) {
    set = &_writeWhitelistSet;
    expression = &_writeWhitelist;
    re = &_writeWhitelistRegex;
  }

  auto item =  canonicalpath(inItem);
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
  return checkBlackAndWhitelist(name, !_startupOptionsWhitelist.empty(),
                                _startupOptionsWhitelistRegex,
                                !_startupOptionsBlacklist.empty(),
                                _startupOptionsBlacklistRegex).result;
}

bool V8SecurityFeature::shouldExposeEnvironmentVariable(v8::Isolate* isolate,
                                                        std::string const& name) const {
  return checkBlackAndWhitelist(name, !_environmentVariablesWhitelist.empty(),
                                _environmentVariablesWhitelistRegex,
                                !_environmentVariablesBlacklist.empty(),
                                _environmentVariablesBlacklistRegex).result;
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

  auto endpointResult = checkBlackAndWhitelist(endpoint, !_endpointsWhitelist.empty(), _endpointsWhitelistRegex,
                                !_endpointsBlacklist.empty(), _endpointsBlacklistRegex);

  auto urlResult = checkBlackAndWhitelist(url, !_endpointsWhitelist.empty(), _endpointsWhitelistRegex,
                                !_endpointsBlacklist.empty(), _endpointsBlacklistRegex);

  return endpointResult.result || ( urlResult.result && !endpointResult.black);
}

bool V8SecurityFeature::isAllowedToAccessPath(v8::Isolate* isolate, std::string const& path,
                                              FSAccessType access) const {
  return V8SecurityFeature::isAllowedToAccessPath(isolate, path.c_str(), access);
}

bool V8SecurityFeature::isAllowedToAccessPath(v8::Isolate* isolate, char const* pathPtr,
                                              FSAccessType access) const {
  if (_filesWhitelist.empty()) {
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

  if (access == FSAccessType::READ && std::regex_search(path, _readWhitelistRegex)) {
    // even in restricted contexts we may read module paths
    return true;
  }

  if (access == FSAccessType::WRITE && std::regex_search(path, _writeWhitelistRegex)) {
    // even in restricted contexts we may read module paths
    return true;
  }

  return checkBlackAndWhitelist(path, !_filesWhitelist.empty(), _filesWhitelistRegex,
                                false, _filesWhitelistRegex /*passed to match the signature but not used*/).result;
}
