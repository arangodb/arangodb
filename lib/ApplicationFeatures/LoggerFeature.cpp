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

#include "Logger/Logger.h"
#include "ProgramOptions2/ProgramOptions.h"
#include "ProgramOptions2/Section.h"

using namespace arangodb;
using namespace arangodb::options;

LoggerFeature::LoggerFeature(application_features::ApplicationServer* server)
    : ApplicationFeature(server, "LoggerFeature"),
      _output(),
      _levels(),
      _useLocalTime(false),
      _prefix(""),
      _file(),
      _lineNumber(false),
      _thread(false),
      _performance(false),
      _daemon(false),
      _backgrounded(false),
      _threaded(false) {
  _levels.push_back("info");
  setOptional(false);
  requiresElevatedPrivileges(false);
}

void LoggerFeature::collectOptions(std::shared_ptr<ProgramOptions> options) {
  LOG_TOPIC(TRACE, Logger::STARTUP) << name() << "::collectOptions";

  options->addSection("log", "Configure the logging");

  options->addOption("--log.output,-o", "log destination(s)",
                     new VectorParameter<StringParameter>(&_output));

  options->addOption("--log.level,-l", "the global or topic-specific log level",
                     new VectorParameter<StringParameter>(&_levels));

  options->addOption("--log.use-local-time",
                     "use local timezone instead of UTC",
                     new BooleanParameter(&_useLocalTime));

  options->addOption("--log.prefix", "prefix log message with this string",
                     new StringParameter(&_prefix));

  options->addHiddenOption(
      "--log.prefix", "adds a prefix in case multiple instances are running",
      new StringParameter(&_prefix));

  options->addOption("--log", "the global or topic-specific log level",
                     new VectorParameter<StringParameter>(&_levels));

  options->addHiddenOption("--log.file",
                           "shortcut for '--log.output file://<filename>'",
                           new StringParameter(&_file));

  options->addHiddenOption("--log.line-number",
                           "append line number and file name",
                           new BooleanParameter(&_lineNumber));

  options->addHiddenOption("--log.thread", "append a thread identifier",
                           new BooleanParameter(&_thread));

  options->addHiddenOption("--log.performance",
                           "shortcut for '--log.level requests=trace'",
                           new BooleanParameter(&_performance));
}

void LoggerFeature::loadOptions(
    std::shared_ptr<options::ProgramOptions> options) {
  LOG_TOPIC(TRACE, Logger::STARTUP) << name() << "::loadOptions";

  // for debugging purpose, we set the log levels NOW
  // this might be overwritten latter
  Logger::initialize(false);
  Logger::setLogLevel(_levels);
}

void LoggerFeature::validateOptions(std::shared_ptr<ProgramOptions> options) {
  LOG_TOPIC(TRACE, Logger::STARTUP) << name() << "::validateOptions";

  if (options->processingResult().touched("log.file")) {
    std::string definition;

    if (_file == "+" || _file == "-") {
      definition = _file;
    } else if (_daemon) {
      definition = "file://" + _file + ".daemon";
    } else {
      definition = "file://" + _file;
    }

    _output.push_back(definition);
  }

  if (_performance) {
    _levels.push_back("requests=trace");
  }

  if (!_backgrounded && isatty(STDIN_FILENO) != 0) {
    _output.push_back("*");
  }
}

void LoggerFeature::prepare() {
  LOG_TOPIC(TRACE, Logger::STARTUP) << name() << "::prepare";

#if _WIN32
  if (!TRI_InitWindowsEventLog()) {
    std::cerr << "failed to init event log" << std::endl;
    FATAL_ERROR_EXIT();
  }
#endif

  Logger::setLogLevel(_levels);
  Logger::setUseLocalTime(_useLocalTime);
  Logger::setShowLineNumber(_lineNumber);
  Logger::setShowThreadIdentifier(_thread);
  Logger::setOutputPrefix(_prefix);
}

void LoggerFeature::start() {
  LOG_TOPIC(TRACE, Logger::STARTUP) << name() << "::start";

  if (_threaded) {
    Logger::flush();
    Logger::shutdown(false);
    Logger::initialize(_threaded);
  }
}

void LoggerFeature::stop() {
  LOG_TOPIC(TRACE, Logger::STARTUP) << name() << "::stop";

  Logger::flush();
  Logger::shutdown(true);
}
