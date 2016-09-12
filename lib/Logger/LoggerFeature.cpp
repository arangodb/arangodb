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

#include "LoggerFeature.h"

#include "Basics/StringUtils.h"
#include "Logger/LogAppender.h"
#include "Logger/Logger.h"
#include "ProgramOptions/ProgramOptions.h"
#include "ProgramOptions/Section.h"

using namespace arangodb;
using namespace arangodb::basics;
using namespace arangodb::options;

LoggerFeature::LoggerFeature(application_features::ApplicationServer* server,
                             bool threaded)
    : ApplicationFeature(server, "Logger"),
      _output(),
      _levels(),
      _useLocalTime(false),
      _prefix(""),
      _file(),
      _lineNumber(false),
      _thread(false),
      _performance(false),
      _keepLogRotate(false),
      _foregroundTty(true),
      _forceDirect(false),
      _supervisor(false),
      _backgrounded(false),
      _threaded(threaded) {
  setOptional(false);
  requiresElevatedPrivileges(false);

  startsAfter("Version");
  if (threaded) {
    startsAfter("WorkMonitor");
  }

  _levels.push_back("info");

  // if stdout is not a tty, then the default for _foregroundTty becomes false
  _foregroundTty = (isatty(STDOUT_FILENO) != 0);
}

LoggerFeature::~LoggerFeature() {
  Logger::shutdown(true);
}

void LoggerFeature::collectOptions(std::shared_ptr<ProgramOptions> options) {
  options->addOldOption("log.tty", "log.foreground-tty");
  options->addOldOption("log.content-filter", "");
  options->addOldOption("log.source-filter", "");
  options->addOldOption("log.application", "");
  options->addOldOption("log.facility", "");
  
  options->addHiddenOption("--log", "the global or topic-specific log level",
                           new VectorParameter<StringParameter>(&_levels));

  options->addSection("log", "Configure the logging");

  options->addOption("--log.output,-o", "log destination(s)",
                     new VectorParameter<StringParameter>(&_output));

  options->addOption("--log.level,-l", "the global or topic-specific log level",
                     new VectorParameter<StringParameter>(&_levels));

  options->addOption("--log.use-local-time",
                     "use local timezone instead of UTC",
                     new BooleanParameter(&_useLocalTime));

  options->addHiddenOption("--log.prefix",
                           "prefix log message with this string",
                           new StringParameter(&_prefix));

  options->addHiddenOption("--log.file",
                           "shortcut for '--log.output file://<filename>'",
                           new StringParameter(&_file));

  options->addHiddenOption("--log.line-number",
                           "append line number and file name",
                           new BooleanParameter(&_lineNumber));

  options->addHiddenOption("--log.thread",
                           "show thread identifier in log message",
                           new BooleanParameter(&_thread));

  options->addHiddenOption("--log.performance",
                           "shortcut for '--log.level performance=trace'",
                           new BooleanParameter(&_performance));

  options->addHiddenOption("--log.keep-logrotate",
                           "keep the old log file after receiving a sighup",
                           new BooleanParameter(&_keepLogRotate));

  options->addHiddenOption("--log.foreground-tty",
                           "also log to tty if not backgrounded",
                           new BooleanParameter(&_foregroundTty));

  options->addHiddenOption("--log.force-direct",
                           "do not start a seperate thread for logging",
                           new BooleanParameter(&_forceDirect));
}

void LoggerFeature::loadOptions(
    std::shared_ptr<options::ProgramOptions> options) {
  // for debugging purpose, we set the log levels NOW
  // this might be overwritten latter
  Logger::setLogLevel(_levels);
}

void LoggerFeature::validateOptions(std::shared_ptr<ProgramOptions> options) {
  if (options->processingResult().touched("log.file")) {
    std::string definition;

    if (_file == "+" || _file == "-") {
      definition = _file;
    } else {
      definition = "file://" + _file;
    }

    _output.push_back(definition);
  }

  if (_performance) {
    _levels.push_back("performance=trace");
  }
}

void LoggerFeature::prepare() {
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
  Logger::setKeepLogrotate(_keepLogRotate);

  for (auto definition : _output) {
    if (_supervisor && StringUtils::isPrefix(definition, "file://")) {
      definition += ".supervisor";
    }

    LogAppender::addAppender(definition);
  }

  if (!_backgrounded && _foregroundTty) {
    LogAppender::addTtyAppender();
  }

  if (_forceDirect) {
    Logger::initialize(false);
  } else {
    Logger::initialize(_threaded);
  }
}

void LoggerFeature::unprepare() {
  Logger::flush();
}
