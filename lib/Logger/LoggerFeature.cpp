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
////////////////////////////////////////////////////////////////////////////////

#include "LoggerFeature.h"

#include "Basics/operating-system.h"

#ifdef ARANGODB_HAVE_GETGRGID
#include <grp.h>
#endif

#include "ApplicationFeatures/ApplicationServer.h"
#include "Basics/Thread.h"
#include "Logger/LogTimeFormat.h"
#include "Logger/Logger.h"

using namespace arangodb::basics;
using namespace arangodb::options;

// Please leave this code in for the next time we have to debug fuerte.
// change to `#if 1` in order to make fuerte logging work.
#if 0
void LogHackWriter(std::string_view msg) { LOG_DEVEL << msg; }
#endif

namespace arangodb {

LoggerFeature::LoggerFeature(application_features::ApplicationServer& server,
                             bool threaded, LoggerOptions loggerOpts,
                             LogApiOptions logApiOpts)
    : ApplicationFeature(server, *this),
      _loggerOpts(std::move(loggerOpts)),
      _logApiOpts(std::move(logApiOpts)),
      _threaded(threaded) {
  startsAfter<ShellColorsFeature>();
  startsAfter<VersionFeature>();
  setOptional(false);
}

LoggerFeature::LoggerFeature(application_features::ApplicationServer& server,
                             bool threaded)
    : LoggerFeature(server, threaded, LoggerOptions{}, LogApiOptions{}) {}

LoggerFeature::LoggerFeature(application_features::ApplicationServer& server,
                             std::type_index registration, bool threaded,
                             LoggerOptions loggerOpts)
    : ApplicationFeature(server, registration, name()),
      _loggerOpts(std::move(loggerOpts)),
      _logApiOpts(LogApiOptions{}),
      _threaded(threaded) {
  // note: we use the _threaded option to determine whether we are arangod
  // (_threaded = true) or one of the client tools (_threaded = false). in
  // the latter case we disable some options for the Logger, which only make
  // sense when we are running in server mode
  setOptional(false);
}

LoggerFeature::~LoggerFeature() { Logger::shutdown(); }

void LoggerFeature::prepare() {
  // set maximum length for each log entry
  Logger::defaultLogGroup().maxLogEntryLength(
      std::max<uint32_t>(256, _loggerOpts.maxEntryLength));

  Logger::setLogLevel(_loggerOpts.levels);
  Logger::setLogStructuredParamsOnServerStart(_loggerOpts.structuredLogParams);
  Logger::setShowIds(_loggerOpts.showIds);
  Logger::setShowRole(_loggerOpts.showRole);
  Logger::setUseColor(_loggerOpts.useColor);
  Logger::setTimeFormat(
      LogTimeFormats::formatFromName(_loggerOpts.timeFormatString));
  Logger::setUseControlEscaped(_loggerOpts.useControlEscaped);
  Logger::setUseUnicodeEscaped(_loggerOpts.useUnicodeEscaped);
  Logger::setEscaping();
  Logger::setShowLineNumber(_loggerOpts.lineNumber);
  Logger::setShortenFilenames(_loggerOpts.shortenFilenames);
  Logger::setShowProcessIdentifier(_loggerOpts.processId);
  Logger::setShowThreadIdentifier(_loggerOpts.threadId);
  Logger::setShowThreadName(_loggerOpts.threadName);
  Logger::setOutputPrefix(_loggerOpts.prefix);
  Logger::setHostname(_loggerOpts.hostname);
  Logger::setKeepLogrotate(_loggerOpts.keepLogRotate);
  Logger::setLogRequestParameters(_loggerOpts.logRequestParameters);
  Logger::setUseJson(_loggerOpts.useJson);

  bool shouldLogToStd = false;
  for (auto const& definition : _loggerOpts.output) {
    if (_supervisor && definition.starts_with("file://")) {
      Logger::addAppender(Logger::defaultLogGroup(),
                          definition + ".supervisor");
    } else {
      Logger::addAppender(Logger::defaultLogGroup(), definition);
      if (shouldLogToStd == false) {
        shouldLogToStd = definition == "+" || definition == "-";
      }
    }
  }

  // if the user defines `--log.output=+`(stderr) explicitly in an environment
  // with a terminal this code will add also an appender to stdout, leading to 2
  // logline per log this will ensure that its only logging once to
  // std(err/out). If the double log line is still desired it is still possible
  // to do it via chain arguments:
  // `--log.output=+ --log.output=-`
  if (_loggerOpts.foregroundTty && !shouldLogToStd) {
    Logger::addAppender(Logger::defaultLogGroup(), "-");
  }

  if (_loggerOpts.forceDirect || _supervisor) {
    Logger::initialize(false, _loggerOpts.maxQueuedLogMessages);
  } else {
    Logger::initialize(_threaded, _loggerOpts.maxQueuedLogMessages);
  }
}

void LoggerFeature::unprepare() { Logger::flush(); }

}  // namespace arangodb
