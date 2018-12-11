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

#if _WIN32
#include <iostream>
#endif

using namespace arangodb::basics;
using namespace arangodb::options;

namespace arangodb {

LoggerFeature::LoggerFeature(
    application_features::ApplicationServer& server,
    bool threaded
)
    : ApplicationFeature(server, "Logger"),
      _threaded(threaded) {
  setOptional(false);

  startsAfter("ShellColors");
  startsAfter("Version");

  _levels.push_back("info");

  // if stdout is a tty, then the default for _foregroundTty becomes true
  _foregroundTty = (isatty(STDOUT_FILENO) == 1);
}

LoggerFeature::~LoggerFeature() {
  Logger::shutdown();
}

void LoggerFeature::collectOptions(std::shared_ptr<ProgramOptions> options) {
  options->addOldOption("log.tty", "log.foreground-tty");
  options->addOldOption("log.content-filter", "");
  options->addOldOption("log.source-filter", "");
  options->addOldOption("log.application", "");
  options->addOldOption("log.facility", "");

  options->addOption("--log", "the global or topic-specific log level",
                     new VectorParameter<StringParameter>(&_levels),
                     arangodb::options::makeFlags(arangodb::options::Flags::Hidden));

  options->addSection("log", "Configure the logging");

  options->addOption("--log.color", "use colors for TTY logging",
                     new BooleanParameter(&_useColor));

  options->addOption("--log.escape", "escape characters when logging",
                     new BooleanParameter(&_useEscaped));

  options->addOption("--log.output,-o", "log destination(s)",
                     new VectorParameter<StringParameter>(&_output));

  options->addOption("--log.level,-l", "the global or topic-specific log level",
                     new VectorParameter<StringParameter>(&_levels));

  options->addOption("--log.use-local-time",
                     "use local timezone instead of UTC",
                     new BooleanParameter(&_useLocalTime));

  options->addOption("--log.use-microtime",
                     "use microtime instead",
                     new BooleanParameter(&_useMicrotime));

  options->addOption("--log.role",
                     "log server role",
                     new BooleanParameter(&_showRole));

  options->addOption("--log.prefix",
                     "prefix log message with this string",
                     new StringParameter(&_prefix),
                     arangodb::options::makeFlags(arangodb::options::Flags::Hidden));

  options->addOption("--log.file",
                     "shortcut for '--log.output file://<filename>'",
                     new StringParameter(&_file),
                     arangodb::options::makeFlags(arangodb::options::Flags::Hidden));

  options->addOption("--log.line-number",
                     "append line number and file name",
                     new BooleanParameter(&_lineNumber),
                     arangodb::options::makeFlags(arangodb::options::Flags::Hidden));

  options->addOption("--log.shorten-filenames",
                     "shorten filenames in log output (use with --log.line-number)",
                     new BooleanParameter(&_shortenFilenames),
                     arangodb::options::makeFlags(arangodb::options::Flags::Hidden));

  options->addOption("--log.thread",
                     "show thread identifier in log message",
                     new BooleanParameter(&_threadId),
                     arangodb::options::makeFlags(arangodb::options::Flags::Hidden));

  options->addOption("--log.thread-name",
                     "show thread name in log message",
                     new BooleanParameter(&_threadName),
                     arangodb::options::makeFlags(arangodb::options::Flags::Hidden));

  options->addOption("--log.performance",
                     "shortcut for '--log.level performance=trace'",
                     new BooleanParameter(&_performance),
                     arangodb::options::makeFlags(arangodb::options::Flags::Hidden));

  options->addOption("--log.keep-logrotate",
                     "keep the old log file after receiving a sighup",
                     new BooleanParameter(&_keepLogRotate),
                     arangodb::options::makeFlags(arangodb::options::Flags::Hidden));

  options->addOption("--log.foreground-tty",
                     "also log to tty if backgrounded",
                     new BooleanParameter(&_foregroundTty),
                     arangodb::options::makeFlags(arangodb::options::Flags::Hidden));

  options->addOption("--log.force-direct",
                     "do not start a seperate thread for logging",
                     new BooleanParameter(&_forceDirect),
                     arangodb::options::makeFlags(arangodb::options::Flags::Hidden));

  options->addOption("--log.request-parameters",
                     "include full URLs and HTTP request parameters in trace logs",
                     new BooleanParameter(&_logRequestParameters),
                     arangodb::options::makeFlags(arangodb::options::Flags::Hidden));
}

void LoggerFeature::loadOptions(
    std::shared_ptr<options::ProgramOptions>,
    char const* binaryPath) {
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
  Logger::setShowRole(_showRole);
  Logger::setUseColor(_useColor);
  Logger::setUseLocalTime(_useLocalTime);
  Logger::setUseMicrotime(_useMicrotime);
  Logger::setUseEscaped(_useEscaped);
  Logger::setShowLineNumber(_lineNumber);
  Logger::setShortenFilenames(_shortenFilenames);
  Logger::setShowThreadIdentifier(_threadId);
  Logger::setShowThreadName(_threadName);
  Logger::setOutputPrefix(_prefix);
  Logger::setKeepLogrotate(_keepLogRotate);
  Logger::setLogRequestParameters(_logRequestParameters);

  for (auto const& definition : _output) {
    if (_supervisor && StringUtils::isPrefix(definition, "file://")) {
      LogAppender::addAppender(definition + ".supervisor");
    } else {
      LogAppender::addAppender(definition);
    }
  }

  if (_foregroundTty) {
    LogAppender::addAppender("-");
  }

  if (_forceDirect || _supervisor) {
    Logger::initialize(false);
  } else {
    Logger::initialize(_threaded);
  }
}

void LoggerFeature::unprepare() {
  Logger::flush();
}

} // arangodb
