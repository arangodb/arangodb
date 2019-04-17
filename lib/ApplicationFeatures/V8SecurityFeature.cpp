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

namespace {

void testRegex(std::string const& whitelist, char const* optionName) {
  try {
    std::regex(whitelist, std::regex::nosubs | std::regex::ECMAScript);
  } catch (std::exception const& ex) {
    LOG_TOPIC("ab6d5", FATAL, arangodb::Logger::FIXME)
        << "value for '--javascript." << optionName << "-whitelist' is not a "
           "valid regular expression: " << ex.what();
    FATAL_ERROR_EXIT();
  }
}

std::string canonicalpath(std::string const& path) {
#ifndef _WIN32
  auto realPath = std::unique_ptr<char, void (*)(void*)>(
    ::realpath(path.c_str(), nullptr), &free);
  if (realPath) {
    return std::string(realPath.get());
  }
  // fallthrough intentional
#endif
  return path;
}

void convertToRegex(std::vector<std::string> const& files, std::string& targetRegex) {
  if (files.empty()) {
    return;
  }

  targetRegex = arangodb::basics::StringUtils::join(files, '|');
}

void convertToRegex(std::unordered_set<std::string> const& files, std::string& targetRegex) {
  // does not delete from the set
  if (files.empty()) {
    return;
  }
  auto last = *files.cbegin();

  std::stringstream ss;
  for (auto fileIt = std::next(files.cbegin()); fileIt != files.cend(); ++fileIt) {
    ss << *fileIt << "|";
  }

  ss << last;
  targetRegex = ss.str();
}

bool checkWhitelist(std::string const& value, bool hasWhitelist,
                            std::regex const& whitelist){
  if (!hasWhitelist) {
    return true;
  }

  return std::regex_search(value, whitelist);
}
}  // namespace

V8SecurityFeature::V8SecurityFeature(application_features::ApplicationServer& server)
    : ApplicationFeature(server, "V8Security"),
      _hardenInternalModule(false),
      _allowProcessControl(false),
      _allowPortTesting(false) {
  setOptional(false);
  startsAfter("Temp");
  startsAfter("V8Platform");
}

void V8SecurityFeature::collectOptions(std::shared_ptr<ProgramOptions> options) {
  options->addSection("javascript", "Configure the Javascript engine");
  options->addOption("--javascript.allow-port-testing", "allow testing of ports from within JavaScript actions",
                     new BooleanParameter(&_allowPortTesting),
                     arangodb::options::makeFlags(arangodb::options::Flags::Hidden))
                    .setIntroducedIn(30500);

  options->addOption("--javascript.allow-external-process-control",
                     "allow execution and control of external processes from within JavaScript actions",
                     new BooleanParameter(&_allowProcessControl),
                     arangodb::options::makeFlags(arangodb::options::Flags::Hidden))
                     .setIntroducedIn(30500);

  options->addOption("--javascript.harden",
                     "disables access to JavaScript functions in the internal module: getPid(), "
                     "processStatistics() andl logLevel()",
                     new BooleanParameter(&_hardenInternalModule))
                     .setIntroducedIn(30500);

  options->addOption("--javascript.startup-options-whitelist",
                     "startup options whose names match this regular "
                     "expression will be whitelisted and exposed to JavaScript",
                     new VectorParameter<StringParameter>(&_startupOptionsWhitelistVec))
                     .setIntroducedIn(30500);

  options->addOption("--javascript.environment-variables-whitelist",
                     "environment variables that will be accessible in JavaScript",
                     new VectorParameter<StringParameter>(&_environmentVariablesWhitelistVec))
                     .setIntroducedIn(30500);

  options->addOption("--javascript.endpoints-whitelist",
                     "endpoints that can be connected to via "
                     "@arangodb/request module in JavaScript actions",
                     new VectorParameter<StringParameter>(&_endpointsWhitelistVec))
                     .setIntroducedIn(30500);

  options->addOption("--javascript.files-whitelist",
                     "filesystem paths that will be accessible from within JavaScript actions",
                     new VectorParameter<StringParameter>(&_filesWhitelistVec))
                     .setIntroducedIn(30500);
}

void V8SecurityFeature::validateOptions(std::shared_ptr<ProgramOptions> options) {
  // check if the regular expressions compile properly

  // startup options
  convertToRegex(_startupOptionsWhitelistVec, _startupOptionsWhitelist);
  testRegex(_startupOptionsWhitelist, "startup-options");

  // environment variables
  convertToRegex(_environmentVariablesWhitelistVec, _environmentVariablesWhitelist);
  testRegex(_environmentVariablesWhitelist, "environment-variables");

  // endpoints
  convertToRegex(_endpointsWhitelistVec, _endpointsWhitelist);
  testRegex(_endpointsWhitelist, "endpoints");

  // file access
  convertToRegex(_filesWhitelistVec, _filesWhitelist);
  testRegex(_filesWhitelist, "files");
}

void V8SecurityFeature::prepare() {
  V8SecurityFeature* v8security =
      application_features::ApplicationServer::getFeature<V8SecurityFeature>(
          "V8Security");

  v8security->addToInternalReadWhitelist(TRI_GetTempPath());
}

void V8SecurityFeature::start() {
  // initialize regexes for filtering options. the regexes must have been validated before
  _startupOptionsWhitelistRegex =
      std::regex(_startupOptionsWhitelist, std::regex::nosubs | std::regex::ECMAScript);

  _environmentVariablesWhitelistRegex =
      std::regex(_environmentVariablesWhitelist, std::regex::nosubs | std::regex::ECMAScript);

  _endpointsWhitelistRegex =
      std::regex(_endpointsWhitelist, std::regex::nosubs | std::regex::ECMAScript);

  _filesWhitelistRegex =
      std::regex(_filesWhitelist, std::regex::nosubs | std::regex::ECMAScript);
}

void V8SecurityFeature::addToInternalReadWhitelist(std::string const& item) {
  // This function is not efficient and we would not need the _readWhitelist
  // to be persistent. But the persistence will help in debugging and
  // there are only a few items expected.
  auto path = "^" + canonicalpath(item) + TRI_DIR_SEPARATOR_STR;
  _readWhitelistSet.emplace(std::move(path));
  _readWhitelist.clear();
  convertToRegex(_readWhitelistSet, _readWhitelist);
  _readWhitelistRegex =
      std::regex(_readWhitelist, std::regex::nosubs | std::regex::ECMAScript);
  LOG_TOPIC("dedb6", DEBUG, Logger::FIXME) << "read-whitelist set to " << _readWhitelist;
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
  return checkWhitelist(name, !_startupOptionsWhitelist.empty(),
                                _startupOptionsWhitelistRegex);
}

bool V8SecurityFeature::shouldExposeEnvironmentVariable(v8::Isolate* isolate,
                                                        std::string const& name) const {
  return checkWhitelist(name, !_environmentVariablesWhitelist.empty(),
                                _environmentVariablesWhitelistRegex);
}

bool V8SecurityFeature::isAllowedToConnectToEndpoint(v8::Isolate* isolate,
                                                     std::string const& name) const {
  TRI_GET_GLOBALS();
  TRI_ASSERT(v8g != nullptr);
  if (v8g->_securityContext.isInternal()) {
    // internal security contexts are allowed to connect to any endpoint
    // this includes connecting to self or to other instances in a cluster
    return true;
  }

  return checkWhitelist(name, !_endpointsWhitelist.empty(), _endpointsWhitelistRegex);
}

bool V8SecurityFeature::isAllowedToAccessPath(v8::Isolate* isolate, std::string const&  path,
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

  bool rv = checkWhitelist(path, !_filesWhitelist.empty(), _filesWhitelistRegex);

  if (rv) {
    return true;
  }

  if (access == FSAccessType::READ && std::regex_search(path, _readWhitelistRegex)) {
    // even in restricted contexts we may read module paths
    return true;
  }

  return rv;
}
