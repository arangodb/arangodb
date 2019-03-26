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

#include "Basics/MutexLocker.h"
#include "Basics/StringUtils.h"
#include "Logger/Logger.h"
#include "ProgramOptions/ProgramOptions.h"
#include "ProgramOptions/Section.h"

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
                     "startup options whose names match this regular expression will not be exposed to JavaScript actions",
                     new StringParameter(&_startupOptionsFilter));
  
  options->addOption("--javascript.environment-variables-filter",
                     "environment variables whose names match this regular expression will not be exposed to JavaScript actions",
                     new StringParameter(&_environmentVariablesFilter));

  options->addOption("--javascript.endpoints-filter",
                     "endpoints that match this regular expression cannot be connected to via internal.download() in JavaScript actions",
                     new StringParameter(&_endpointsFilter));
}

void V8SecurityFeature::validateOptions(std::shared_ptr<ProgramOptions> options) {
  // check if the regexes compile properly
  try {
    std::regex(_startupOptionsFilter, std::regex::nosubs | std::regex::ECMAScript);
  } catch (std::exception const& ex) {
    LOG_TOPIC("6e560", FATAL, arangodb::Logger::FIXME)
        << "value for '--javascript.startup-options-filter' is not valid a regular expression: " << ex.what();
    FATAL_ERROR_EXIT();
  }

  try {
    std::regex(_environmentVariablesFilter, std::regex::nosubs | std::regex::ECMAScript);
  } catch (std::exception const& ex) {
    LOG_TOPIC("ef35e", FATAL, arangodb::Logger::FIXME)
        << "value for '--javascript.environment-variables-filter' is not a valid regular expression: " << ex.what();
    FATAL_ERROR_EXIT();
  }

  try {
    std::regex(_endpointsFilter, std::regex::nosubs | std::regex::ECMAScript);
  } catch (std::exception const& ex) {
    LOG_TOPIC("ab7d5", FATAL, arangodb::Logger::FIXME)
        << "value for '--javascript.endpoints-filter' is not a valid regular expression: " << ex.what();
    FATAL_ERROR_EXIT();
  }
}

void V8SecurityFeature::start() {
  // initialize regexes for filtering options. the regexes must have been validated before
  _startupOptionsFilterRegex = std::regex(_startupOptionsFilter, std::regex::nosubs | std::regex::ECMAScript);
  _environmentVariablesFilterRegex = std::regex(_environmentVariablesFilter, std::regex::nosubs | std::regex::ECMAScript);
  _endpointsFilterRegex = std::regex(_endpointsFilter, std::regex::nosubs | std::regex::ECMAScript);
}

bool V8SecurityFeature::shouldExposeStartupOption(v8::Isolate* isolate, std::string const& name) const {
  return _startupOptionsFilter.empty() || !std::regex_search(name, _startupOptionsFilterRegex);
}
  
bool V8SecurityFeature::shouldExposeEnvironmentVariable(v8::Isolate* isolate, std::string const& name) const {
  return _environmentVariablesFilter.empty() || !std::regex_search(name, _environmentVariablesFilterRegex);
}

bool V8SecurityFeature::isAllowedToConnectToEndpoint(v8::Isolate* isolate, std::string const& name) const {
  return _endpointsFilter.empty() || !std::regex_search(name, _endpointsFilterRegex);
}
  
bool V8SecurityFeature::isAllowedToAccessPath(v8::Isolate* isolate, std::string const& path) const {
  // TODO: needs implementation
  return true;
}
