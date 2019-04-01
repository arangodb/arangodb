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
    : ApplicationFeature(server, "V8Security") {
  setOptional(false);
  startsAfter("V8Platform");
}

void V8SecurityFeature::collectOptions(std::shared_ptr<ProgramOptions> options) {
  options->addSection("javascript", "Configure the Javascript engine");

  options->addOption("--javascript.startup-options-filter",
                     "startup options whose names match this regular "
                     "expression will not be exposed to JavaScript actions",
                     new StringParameter(&_startupOptionsFilter));

  options->addOption("--javascript.environment-variables-filter",
                     "environment variables whose names match this regular "
                     "expression will not be exposed to JavaScript actions",
                     new StringParameter(&_environmentVariablesFilter));

  options->addOption(
      "--javascript.endpoints-filter",
      "endpoints that match this regular expression cannot be connected to via "
      "internal.download() in JavaScript actions",
      new StringParameter(&_endpointsFilter));

  // TODO - update descriptions once mechanics decided
  options->addOption("--javascript.files-white-list-expression",
                     "Files in this re will be accessible - FIX DESCRIPTION",
                     new StringParameter(&_filesWhiteList));

  options->addOption(
      "--javascript.files-black-list-expression",
      "Files in this re will not be accessible - FIX DESCRIPTION",
      new StringParameter(&_filesBlackList));

  options->addOption("--javascript.files-white-list",
                     "Paths to be added to files-white-list-expression",
                     new VectorParameter<StringParameter>(&_filesWhiteListVec));

  options->addOption("--javascript.files-black-list",
                     "Paths to be added to files-black-list-expression",
                     new VectorParameter<StringParameter>(&_filesBlackListVec));
}

void V8SecurityFeature::validateOptions(std::shared_ptr<ProgramOptions> options) {
  // check if the regexes compile properly
  try {
    std::regex(_startupOptionsFilter, std::regex::nosubs | std::regex::ECMAScript);
  } catch (std::exception const& ex) {
    LOG_TOPIC("6e560", FATAL, arangodb::Logger::FIXME)
        << "value for '--javascript.startup-options-filter' is not valid a "
           "regular expression: "
        << ex.what();
    FATAL_ERROR_EXIT();
  }

  try {
    std::regex(_environmentVariablesFilter, std::regex::nosubs | std::regex::ECMAScript);
  } catch (std::exception const& ex) {
    LOG_TOPIC("ef35e", FATAL, arangodb::Logger::FIXME)
        << "value for '--javascript.environment-variables-filter' is not a "
           "valid regular expression: "
        << ex.what();
    FATAL_ERROR_EXIT();
  }

  try {
    std::regex(_endpointsFilter, std::regex::nosubs | std::regex::ECMAScript);
  } catch (std::exception const& ex) {
    LOG_TOPIC("ab7d5", FATAL, arangodb::Logger::FIXME)
        << "value for '--javascript.endpoints-filter' is not a valid regular "
           "expression: "
        << ex.what();
    FATAL_ERROR_EXIT();
  }

  auto convertToRe = [](std::vector<std::string>& files, std::string& target_re) {
    if (!files.empty()) {
      std::stringstream ss;
      std::string last = std::move(files.back());
      files.pop_back();

      // Do we need to check for "()|" in filenames?

      if (!target_re.empty()) {
        ss << "(" << target_re << ")|";
      }

      while (!files.empty()) {
        ss << files.back() << "|";
        files.pop_back();
      }

      ss << last;

      target_re = ss.str();
    }
  };

  convertToRe(_filesWhiteListVec, _filesWhiteList);
  convertToRe(_filesBlackListVec, _filesBlackList);

  try {
    std::regex(_filesWhiteList, std::regex::nosubs | std::regex::ECMAScript);
  } catch (std::exception const& ex) {
    LOG_TOPIC("ab9d5", FATAL, arangodb::Logger::FIXME)
        << "value for '--javascript.files-white-list-expression' is not a "
           "valid regular "
           "expression: "
        << ex.what();
    FATAL_ERROR_EXIT();
  }

  try {
    std::regex(_filesBlackList, std::regex::nosubs | std::regex::ECMAScript);
  } catch (std::exception const& ex) {
    LOG_TOPIC("ab8d5", FATAL, arangodb::Logger::FIXME)
        << "value for '--javascript.files-black-list-expression' is not a "
           "valid regular "
           "expression: "
        << ex.what();
    FATAL_ERROR_EXIT();
  }
}

void V8SecurityFeature::start() {
  // initialize regexes for filtering options. the regexes must have been validated before
  _startupOptionsFilterRegex =
      std::regex(_startupOptionsFilter, std::regex::nosubs | std::regex::ECMAScript);
  _environmentVariablesFilterRegex =
      std::regex(_environmentVariablesFilter, std::regex::nosubs | std::regex::ECMAScript);
  _endpointsFilterRegex =
      std::regex(_endpointsFilter, std::regex::nosubs | std::regex::ECMAScript);
  _filesWhiteListRegex =
      std::regex(_filesWhiteList, std::regex::nosubs | std::regex::ECMAScript);
  _filesBlackListRegex =
      std::regex(_filesBlackList, std::regex::nosubs | std::regex::ECMAScript);
}

bool V8SecurityFeature::canDefineHttpAction(v8::Isolate* isolate) const {
  TRI_GET_GLOBALS();
  // v8g may be a nullptr when we are in arangosh
  return v8g != nullptr && v8g->_securityContext.canDefineHttpAction();
}

bool V8SecurityFeature::shouldExposeStartupOption(v8::Isolate* isolate,
                                                  std::string const& name) const {
  return _startupOptionsFilter.empty() ||
         !std::regex_search(name, _startupOptionsFilterRegex);
}

bool V8SecurityFeature::shouldExposeEnvironmentVariable(v8::Isolate* isolate,
                                                        std::string const& name) const {
  return _environmentVariablesFilter.empty() ||
         !std::regex_search(name, _environmentVariablesFilterRegex);
}

bool V8SecurityFeature::isAllowedToConnectToEndpoint(v8::Isolate* isolate,
                                                     std::string const& name) const {
  TRI_GET_GLOBALS();
  if (v8g != nullptr && v8g->_securityContext.isInternal()) {
    // internal security contexts are allowed to connect to any endpoint
    // this includes connecting to self or to other instances in a cluster
    return true;
  }
  return _endpointsFilter.empty() || !std::regex_search(name, _endpointsFilterRegex);
}

bool V8SecurityFeature::isAllowedToAccessPath(v8::Isolate* isolate,
                                              char const* path, bool read) const {
  // expects 0 terminated utf-8 string
  TRI_ASSERT(path != nullptr);
  return isAllowedToAccessPath(isolate, std::string(path), read);
}

bool V8SecurityFeature::isAllowedToAccessPath(v8::Isolate* isolate,
                                              std::string path, bool write) const {
  // check security context first
  TRI_GET_GLOBALS();
  auto& sec = v8g->_securityContext;
  if ((!write && !sec.canReadFs()) || (write && !sec.canWriteFs())) {
    return false;
  }

  LOG_DEVEL << "@ path in" << path;

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

  LOG_DEVEL << "@ path resolved" << path;
  LOG_DEVEL << "@ white-list expression" << _filesWhiteList;
  LOG_DEVEL << "@ black-list expression" << _filesBlackList;

  if (_filesWhiteList.empty() && _filesBlackList.empty()) {
    return true;
  }

  if (_filesBlackList.empty()) {
    // must be white listed
    return std::regex_search(path, _filesWhiteListRegex);
  }

  if (_filesWhiteList.empty()) {
    // must be white listed
    return !std::regex_search(path, _filesBlackListRegex);
  }

  if (std::regex_search(path, _filesWhiteListRegex)) {
    return true;  // white-list wins - simple implementation
  } else {
    return !std::regex_search(path, _filesBlackListRegex);
  }
}

bool V8SecurityFeature::isAllowedToExecuteJavaScript(v8::Isolate* isolate) const {
  // implement me
  return true;
}
