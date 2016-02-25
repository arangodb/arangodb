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
/// @author Dr. Frank Celler
////////////////////////////////////////////////////////////////////////////////

#include "ApplicationFeatures/LoggerFeature.h"

#include "ProgramOptions2/ProgramOptions.h"
#include "ProgramOptions2/Section.h"

using namespace arangodb;
using namespace arangodb::options;

LoggerFeature::LoggerFeature(application_features::ApplicationServer* server)
    : ApplicationFeature(server, "LoggerFeature"),
      _output(),
      _level("info"),
      _useLocalTime(false),
      _lineNumber(false),
      _thread(false) {
  setOptional(false);
  requiresElevatedPrivileges(false);
}

void LoggerFeature::collectOptions(std::shared_ptr<ProgramOptions> options) {
  options->addSection(
      Section("log", "Configure the logging", "logging options", false, false));

  options->addOption("--log.output,-o", "log destination(s)",
                     new VectorParameter<StringParameter>(&_output));

  options->addOption("--log.level,-l", "the global or topic-specific log level",
                     new StringParameter(&_level));

  options->addOption("--log.use-local-time",
                     "use local timezone instead of UTC",
                     new BooleanParameter(&_useLocalTime));

  options->addSection(Section("log-hidden", "Configure the logging",
                              "hidden logging options", true, false));

  options->addOption("--log.line-number", "append line number and file name",
                     new BooleanParameter(&_lineNumber));

  options->addOption("--log.thread", "append a thread identifier",
                     new BooleanParameter(&_thread));
}
