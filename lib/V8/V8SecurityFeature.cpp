////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2014-2024 ArangoDB GmbH, Cologne, Germany
/// Copyright 2004-2014 triAGENS GmbH, Cologne, Germany
///
/// Licensed under the Business Source License 1.1 (the "License");
/// you may not use this file except in compliance with the License.
/// You may obtain a copy of the License at
///
///     https://github.com/arangodb/arangodb/blob/devel/LICENSE
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

#include <absl/strings/str_cat.h>

#include "V8/V8SecurityFeature.h"

#include "ApplicationFeatures/ApplicationServer.h"
#include "Assertions/ProdAssert.h"
#include "Basics/ArangoGlobalContext.h"
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

auto disjunctionOfRegexes(std::vector<std::string> values) -> std::string {
  return "(" + arangodb::basics::StringUtils::join(values, '|') + ")";
}

auto disjunctionOfRegexes(std::unordered_set<std::string> values)
    -> std::string {
  auto ss = std::stringstream();

  ss << "(" << *values.cbegin();
  for (auto it = std::next(values.cbegin()); it != values.cend(); ++it) {
    ss << "|" << *it;
  }
  ss << ")";
  return ss.str();
}

auto tryStringToRegex(std::string regexString, std::string optionName,
                      std::string listName) -> std::regex {
  try {
    return std::regex(regexString, std::regex::nosubs | std::regex::ECMAScript);
  } catch (std::exception const& ex) {
    LOG_TOPIC("ab2d5", FATAL, arangodb::Logger::FIXME)
        << "value for '--javascript." << optionName << "-" << listName
        << "' is not a valid regular expression: " << ex.what();
    FATAL_ERROR_EXIT();
  }
}

auto optionToRegex(std::vector<std::string> values, std::string optionName,
                   std::string listName) -> std::optional<std::regex> {
  if (values.empty()) {
    return std::nullopt;
  }
  return tryStringToRegex(disjunctionOfRegexes(values), optionName, listName);
}

std::string canonicalpath(std::string const& path) {
  auto realPath = std::unique_ptr<char, void (*)(void*)>(
      ::realpath(path.c_str(), nullptr), &free);
  if (realPath) {
    return std::string(realPath.get());
  }
  // fallthrough intentional
  // TODO: that's cool, mind telling me *why*
  return path;
}

// TODO: Fix this function, probably use std::filesystem::path
auto canonicalPath(std::string path) -> std::string {
  std::string result = TRI_ResolveSymbolicLink(path);

  // make absolute
  std::string cwd = FileUtils::currentDirectory().result();
  path = TRI_GetAbsolutePath(result, cwd);

  // TODO check this function, it seems odd
  result = canonicalpath(std::move(path));
  if (basics::FileUtils::isDirectory(path)) {
    result += TRI_DIR_SEPARATOR_STR;
  }
  return result;
}

}  // namespace

void V8SecurityFeature::collectOptions(
    std::shared_ptr<ProgramOptions> options) {
  options->addSection("javascript", "JavaScript engine and execution");
  options->addOption(
      "--javascript.allow-port-testing",
      "Allow the testing of ports from within JavaScript actions.",
      new BooleanParameter(&_options.allowPortTesting),
      arangodb::options::makeFlags(
          arangodb::options::Flags::DefaultNoComponents,
          arangodb::options::Flags::OnCoordinator,
          arangodb::options::Flags::OnSingle,
          arangodb::options::Flags::Uncommon));

  options->addOption(
      "--javascript.allow-external-process-control",
      "Allow the execution and control of external processes from "
      "within JavaScript actions.",
      new BooleanParameter(&_options.allowProcessControl),
      arangodb::options::makeFlags(
          arangodb::options::Flags::DefaultNoComponents,
          arangodb::options::Flags::OnCoordinator,
          arangodb::options::Flags::OnSingle,
          arangodb::options::Flags::Uncommon));

  options->addOption("--javascript.harden",
                     "Disable access to JavaScript functions in the internal "
                     "module: getPid() and logLevel().",
                     new BooleanParameter(&_options.hardenInternalModule),
                     arangodb::options::makeFlags(
                         arangodb::options::Flags::DefaultNoComponents,
                         arangodb::options::Flags::OnCoordinator,
                         arangodb::options::Flags::OnSingle));

  options->addOption(
      "--javascript.startup-options-allowlist",
      "Startup options whose names match this regular "
      "expression are allowed and exposed to JavaScript.",
      new VectorParameter<StringParameter>(&_options.startupOptionsAllowList),
      arangodb::options::makeFlags(
          arangodb::options::Flags::DefaultNoComponents,
          arangodb::options::Flags::OnCoordinator,
          arangodb::options::Flags::OnSingle));

  options->addOption(
      "--javascript.startup-options-denylist",
      "Startup options whose names match this regular "
      "expression are not exposed (if not in the allowlist) to "
      "JavaScript actions.",
      new VectorParameter<StringParameter>(&_options.startupOptionsDenyList),
      arangodb::options::makeFlags(
          arangodb::options::Flags::DefaultNoComponents,
          arangodb::options::Flags::OnCoordinator,
          arangodb::options::Flags::OnSingle));

  options->addOption("--javascript.environment-variables-allowlist",
                     "Environment variables that are accessible in JavaScript.",
                     new VectorParameter<StringParameter>(
                         &_options.environmentVariablesAllowList),
                     arangodb::options::makeFlags(
                         arangodb::options::Flags::DefaultNoComponents,
                         arangodb::options::Flags::OnCoordinator,
                         arangodb::options::Flags::OnSingle));

  options->addOption("--javascript.environment-variables-denylist",
                     "Environment variables that are inaccessible in "
                     "JavaScript (if not in the allowlist).",
                     new VectorParameter<StringParameter>(
                         &_options.environmentVariablesDenyList),
                     arangodb::options::makeFlags(
                         arangodb::options::Flags::DefaultNoComponents,
                         arangodb::options::Flags::OnCoordinator,
                         arangodb::options::Flags::OnSingle));

  options->addOption(
      "--javascript.endpoints-allowlist",
      "Endpoints that can be connected to via the "
      "`@arangodb/request` module in JavaScript actions.",
      new VectorParameter<StringParameter>(&_options.endpointsAllowList),
      arangodb::options::makeFlags(
          arangodb::options::Flags::DefaultNoComponents,
          arangodb::options::Flags::OnCoordinator,
          arangodb::options::Flags::OnSingle));

  options->addOption(
      "--javascript.endpoints-denylist",
      "Endpoints that cannot be connected to via the "
      "`@arangodb/request` module in JavaScript actions "
      "(if not in the allowlist).",
      new VectorParameter<StringParameter>(&_options.endpointsDenyList),
      arangodb::options::makeFlags(
          arangodb::options::Flags::DefaultNoComponents,
          arangodb::options::Flags::OnCoordinator,
          arangodb::options::Flags::OnSingle));

  options->addOption(
      "--javascript.files-allowlist",
      "Filesystem paths that are accessible from within "
      "JavaScript actions.",
      new VectorParameter<StringParameter>(&_options.filesAllowList),
      arangodb::options::makeFlags(
          arangodb::options::Flags::DefaultNoComponents,
          arangodb::options::Flags::OnCoordinator,
          arangodb::options::Flags::OnSingle));

  options->addOldOption("--javascript.startup-options-whitelist",
                        "--javascript.startup-options-allowlist");
  options->addOldOption("--javascript.startup-options-blacklist",
                        "--javascript.startup-options-denylist");
  options->addOldOption("--javascript.environment-variables-whitelist",
                        "--javascript.environment-variables-allowlist");
  options->addOldOption("--javascript.environment-variables-blacklist",
                        "--javascript.environment-variables-denylist");
  options->addOldOption("--javascript.endpoints-whitelist",
                        "--javascript.endpoints-allowlist");
  options->addOldOption("--javascript.endpoints-blacklist",
                        "--javascript.endpoints-denylist");
  options->addOldOption("--javascript.files-whitelist",
                        "--javascript.files-allowlist");
}

void V8SecurityFeature::validateOptions(
    std::shared_ptr<ProgramOptions> /*options*/) {
  {
    if (_strictness == AllowListStrictness::NONSTRICT &&
        _options.startupOptionsAllowList.empty()) {
      _options.startupOptionsAllowList.emplace_back(".*");
    }

    // startup options
    auto denyRegex = optionToRegex(_options.startupOptionsDenyList,
                                   "startup-options", "deny");
    auto allowRegex = optionToRegex(_options.startupOptionsAllowList,
                                    "startup-options", "allow");
    _startupOptions = DenyAllow(denyRegex, allowRegex);
  }

  {
    if (_strictness == AllowListStrictness::NONSTRICT &&
        _options.environmentVariablesAllowList.empty()) {
      _options.environmentVariablesAllowList.emplace_back(".*");
    }

    // environment variables
    auto denyRegex = optionToRegex(_options.environmentVariablesDenyList,
                                   "environment-variables", "deny");
    auto allowRegex = optionToRegex(_options.environmentVariablesAllowList,
                                    "environment-variables", "allow");
    _environmentVariables = DenyAllow(denyRegex, allowRegex);
  }

  {
    if (_strictness == AllowListStrictness::NONSTRICT &&
        _options.endpointsAllowList.empty()) {
      _options.endpointsAllowList.emplace_back(".*");
    }

    // endpoints
    auto denyRegex =
        optionToRegex(_options.endpointsDenyList, "endpoints", "deny");
    auto allowRegex =
        optionToRegex(_options.endpointsAllowList, "endpoints", "allow");
    _endpoints = DenyAllow(denyRegex, allowRegex);
  }

  {
    if (_strictness == AllowListStrictness::NONSTRICT &&
        _options.filesAllowList.empty()) {
      _options.filesAllowList.emplace_back(".*");
    }

    // file access (a denylist for file access does not exist (yet))
    auto denyRegex = std::nullopt;
    auto allowRegex = optionToRegex(_options.filesAllowList, "files", "allow");

    _files = DenyAllow(denyRegex, allowRegex);
  }
}

void V8SecurityFeature::prepare() {
  addToInternalAllowList(TRI_GetTempPath(), FSAccessType::READ);
  addToInternalAllowList(TRI_GetTempPath(), FSAccessType::WRITE);
  TRI_ASSERT(!_writeAllowList.empty());
  TRI_ASSERT(!_readAllowList.empty());
}

void V8SecurityFeature::start() {}

void V8SecurityFeature::dumpAccessLists() const {
  LOG_TOPIC("2cafe", DEBUG, arangodb::Logger::SECURITY)
      << "files allowed by user:" << _options.filesAllowList
      << ", internal read allow list:" << _readAllowList
      << ", internal write allow list:" << _writeAllowList
      << ", internal startup options allow list:"
      << _options.startupOptionsAllowList
      << ", internal startup options deny list: "
      << _options.startupOptionsDenyList
      << ", internal environment variable allow list:"
      << _options.environmentVariablesAllowList
      << ", internal environment variables deny list: "
      << _options.environmentVariablesDenyList
      << ", internal endpoints allow list:" << _options.endpointsAllowList
      << ", internal endpoints deny list: " << _options.endpointsDenyList;
}

// TODO: this is still very ugly
void V8SecurityFeature::addToInternalAllowList(std::string const& inItem,
                                               FSAccessType type) {
  auto [set, expression, re] = std::invoke([this, &type]() {
    switch (type) {
      case FSAccessType::READ: {
        return std::tuple(&_readAllowListSet, &_readAllowList,
                          &_readAllowRegex);
      }
      case FSAccessType::WRITE: {
        return std::tuple(&_writeAllowListSet, &_writeAllowList,
                          &_writeAllowRegex);
      }
    }
  });

  auto item = canonicalpath(inItem);
  if ((item.length() > 0) &&
      (item[item.length() - 1] != TRI_DIR_SEPARATOR_CHAR)) {
    item += TRI_DIR_SEPARATOR_STR;
  }
  auto path = "^" + arangodb::basics::StringUtils::escapeRegexParams(item);
  set->emplace(std::move(path));
  expression->clear();
  *expression = disjunctionOfRegexes(*set);

  try {
    *re = std::regex(*expression, std::regex::nosubs | std::regex::ECMAScript);
  } catch (std::exception const& ex) {
    throw std::invalid_argument(ex.what() + std::string(" '") + *expression +
                                "'");
  }
}

bool V8SecurityFeature::isAllowedToControlProcesses() const {
  return _options.allowProcessControl;
}

bool V8SecurityFeature::isAllowedToControlProcesses(
    v8::Isolate* isolate) const {
  TRI_GET_GLOBALS();
  TRI_ASSERT(v8g != nullptr);
  return _options.allowProcessControl &&
         v8g->_securityContext.canControlProcesses();
}

bool V8SecurityFeature::isAllowedToTestPorts(v8::Isolate* /*isolate*/) const {
  return _options.allowPortTesting;
}

bool V8SecurityFeature::isInternalModuleHardened(
    v8::Isolate* /*isolate*/) const {
  return _options.hardenInternalModule;
}

bool V8SecurityFeature::isAllowedToDefineHttpAction(
    v8::Isolate* isolate) const {
  TRI_GET_GLOBALS();
  TRI_ASSERT(v8g != nullptr);
  return v8g->_securityContext.canDefineHttpAction();
}

bool V8SecurityFeature::isInternalContext(v8::Isolate* isolate) const {
  TRI_GET_GLOBALS();
  TRI_ASSERT(v8g != nullptr);
  return v8g->_securityContext.isInternal();
}

bool V8SecurityFeature::isAdminScriptContext(v8::Isolate* isolate) const {
  TRI_GET_GLOBALS();
  TRI_ASSERT(v8g != nullptr);
  return v8g->_securityContext.isAdminScript() ||
         v8g->_securityContext.isRestAdminScript();
}

bool V8SecurityFeature::shouldExposeStartupOption(
    v8::Isolate* /*isolate*/, std::string const& name) const {
  switch (_startupOptions.check(name)) {
    case DenyAllowResult::ALLOWED:
      return true;
    case DenyAllowResult::DENIED:
      return false;
    default:
      ADB_PROD_CRASH();
  }
}

bool V8SecurityFeature::shouldExposeEnvironmentVariable(
    v8::Isolate* /*isolate*/, std::string const& name) const {
  switch (_environmentVariables.check(name)) {
    case DenyAllowResult::ALLOWED:
      return true;
    case DenyAllowResult::DENIED:
      return false;
    default:
      ADB_PROD_CRASH();
  }
}

bool V8SecurityFeature::isAllowedToConnectToEndpoint(
    v8::Isolate* isolate, std::string const& endpoint,
    std::string const& url) const {
  TRI_GET_GLOBALS();
  TRI_ASSERT(v8g != nullptr);
  if (v8g->_securityContext.isInternal()) {
    // internal security contexts are allowed to connect to any endpoint
    // this includes connecting to self or to other instances in a cluster
    return true;
  }

  auto endpointCheck = _endpoints.check(endpoint);
  auto urlCheck = _endpoints.check(url);

  return (endpointCheck == DenyAllowResult::ALLOWED) &&
         (urlCheck == DenyAllowResult::ALLOWED);
  // TODO: try to reconstruct the intent of this code.
  // whats an endpoint, whats an url?
#if 0
  auto endpointResult = checkAllowAndDenyList(
      endpoint, !_endpointsAllowList.empty(), _endpointsAllowListRegex,
      !_endpointsDenyList.empty(), _endpointsDenyListRegex);

  auto urlResult = checkAllowAndDenyList(
      url, !_endpointsAllowList.empty(), _endpointsAllowListRegex,
      !_endpointsDenyList.empty(), _endpointsDenyListRegex);

  return endpointResult.result || (urlResult.result && !endpointResult.deny);
  endpointCheck::ALLOWED || (urlCheck::ALLOWED && endpointCheck::ALLOWED)
    (ec::ALLOWED || url::ALLOWED) && (ec::ALLOWED)
#endif
}

bool V8SecurityFeature::isAllowedToAccessPath(v8::Isolate* isolate,
                                              std::string const& path,
                                              FSAccessType access) const {
  return V8SecurityFeature::isAllowedToAccessPath(isolate, path.c_str(),
                                                  access);
}

bool V8SecurityFeature::isAllowedToAccessPath(v8::Isolate* isolate,
                                              char const* pathPtr,
                                              FSAccessType access) const {
  // TODO: check what this means?
  // check security context first
  TRI_GET_GLOBALS();
  TRI_ASSERT(v8g != nullptr);

  auto const& sec = v8g->_securityContext;
  if ((access == FSAccessType::READ && sec.canReadFs()) ||
      (access == FSAccessType::WRITE && sec.canWriteFs())) {
    return true;  // context may read / write without restrictions
  }

  auto canonicalisedPath = canonicalPath(pathPtr);

  // TODO: check that this covers intent (and what the intent is, for that
  // matter)
  if (_readAllowRegex.has_value()) {
    if (access == FSAccessType::READ &&
        std::regex_search(canonicalisedPath, _readAllowRegex.value())) {
      return true;
    }
  }

  if (_writeAllowRegex.has_value()) {
    if (access == FSAccessType::WRITE &&
        std::regex_search(canonicalisedPath, _writeAllowRegex.value())) {
      return true;
    }
  }

  switch (_files.check(canonicalisedPath)) {
    case DenyAllowResult::ALLOWED: {
      return true;
    }
    case DenyAllowResult::DENIED: {
      return false;
    }
    default:
      ADB_PROD_CRASH();
  }
}
