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
/// @author Dr. Frank Celler
////////////////////////////////////////////////////////////////////////////////

#include "Basics/operating-system.h"

#ifdef ARANGODB_HAVE_GETGRGID
#include <grp.h>
#endif

#ifdef TRI_HAVE_UNISTD_H
#include <unistd.h>
#endif

#include "LoggerFeature.h"

#include "ApplicationFeatures/ApplicationServer.h"
#include "Basics/FileUtils.h"
#include "Basics/NumberUtils.h"
#include "Basics/StringUtils.h"
#include "Basics/Thread.h"
#include "Basics/application-exit.h"
#include "Basics/error.h"
#include "Basics/voc-errors.h"
#include "Logger/LogAppenderFile.h"
#include "Logger/LogMacros.h"
#include "Logger/LogTimeFormat.h"
#include "Logger/Logger.h"
#include "Logger/LoggerStream.h"
#include "ProgramOptions/Option.h"
#include "ProgramOptions/Parameters.h"
#include "ProgramOptions/ProgramOptions.h"

using namespace arangodb::basics;
using namespace arangodb::options;

// Please leave this code in for the next time we have to debug fuerte.
// change to `#if 1` in order to make fuerte logging work.
#if 0
void LogHackWriter(std::string_view msg) { LOG_DEVEL << msg; }
#endif

namespace arangodb {

LoggerFeature::LoggerFeature(application_features::ApplicationServer& server,
                             size_t registration, bool threaded)
    : ApplicationFeature(server, registration, name()),
      _timeFormatString(LogTimeFormats::defaultFormatName()),
      _threaded(threaded) {
  // note: we use the _threaded option to determine whether we are arangod
  // (_threaded = true) or one of the client tools (_threaded = false). in
  // the latter case we disable some options for the Logger, which only make
  // sense when we are running in server mode
  setOptional(false);

  _levels.push_back("info");

  // if stdout is a tty, then the default for _foregroundTty becomes true
  _foregroundTty = (isatty(STDOUT_FILENO) == 1);
}

LoggerFeature::~LoggerFeature() { Logger::shutdown(); }

void LoggerFeature::collectOptions(std::shared_ptr<ProgramOptions> options) {
  options->addOldOption("log.tty", "log.foreground-tty");
  options->addOldOption("log.escape", "log.escape-control-chars");

  options
      ->addOption("--log",
                  "Set the topic-specific log level, using `--log level` "
                  "for the general topic or `--log topic=level` for the "
                  "specified topic (can be specified multiple times). "
                  "Available log levels: fatal, error, warning, info, debug, "
                  "trace.",
                  new VectorParameter<StringParameter>(&_levels),
                  arangodb::options::makeDefaultFlags(
                      arangodb::options::Flags::Uncommon))
      .setDeprecatedIn(30500);

  options->addSection("log", "logging");

  options->addOption(
      "--log.color", "Use colors for TTY logging.",
      new BooleanParameter(&_useColor),
      arangodb::options::makeDefaultFlags(arangodb::options::Flags::Dynamic));

  options
      ->addOption("--log.escape-control-chars",
                  "Escape control characters in log messages.",
                  new BooleanParameter(&_useControlEscaped))
      .setIntroducedIn(30900)
      .setLongDescription(R"(This option applies to the control characters,
that have hex codes below `\x20`, and also the character `DEL` with hex code
`\x7f`.

If you set this option to `false`, control characters are retained when they
have a visible representation, and replaced with a space character in case they
do not have a visible representation. For example, the control character `\n`
is visible, so a `\n` is displayed in the log. Contrary, the control character
`BEL` is not visible, so a space is displayed instead.

If you set this option to `true`, the hex code for the character is displayed,
for example, the `BEL` character is displayed as `\x07`.

The default value for this option is `true` to ensure compatibility with
previous versions.

A side effect of turning off the escaping is that it reduces the CPU overhead
for the logging. However, this is only noticeable if logging is set to a very
verbose level (e.g. `debug` or `trace`).)");

  options
      ->addOption("--log.escape-unicode-chars",
                  "Escape Unicode characters in log messages.",
                  new BooleanParameter(&_useUnicodeEscaped))
      .setIntroducedIn(30900)
      .setLongDescription(R"(If you set this option to `false`, Unicode
characters are retained and written to the log as-is. For example, `犬` is
logged as `犬`.

If you set this options to `true`, any Unicode characters are escaped, and the
hex codes for all Unicode characters are logged instead. For example, `犬` is
logged as `\u72AC`.

The default value for this option is set to `false` for compatibility with
previous versions.

A side effect of turning off the escaping is that it reduces the CPU overhead
for the logging. However, this is only noticeable if logging is set to a very
verbose level (e.g. `debug` or `trace`).)");

  options
      ->addOption("--log.structured-param",
                  "Toggle the usage of the log category parameter in "
                  "structured log messages.",
                  new VectorParameter<StringParameter>(&_structuredLogParams))
      .setIntroducedIn(31000)
      .setLongDescription(R"(Some log messages can be displayed together with
additional information in a structured form. The following parameters are
available:

- `database`: The name of the database.
- `username`: The name of the user.
- `queryid`: The ID of the AQL query (on DB-Servers only).
- `url`: The endpoint path.

The format to enable or disable a parameter is `<parameter>=<bool>`, or
`<parameter>` to enable it. You can specify the option multiple times to
configure multiple parameters:

`arangod --log.structured-param database=true --log.structured-param url
--log.structured-param username=false`

You can adjust the parameter settings at runtime using the
`/_admin/log/structured` HTTP API.)");

  options
      ->addOption("--log.output,-o",
                  "Log destination(s), e.g. "
                  "file:///path/to/file"
                  " (any occurrence of $PID is replaced with the process ID).",
                  new VectorParameter<StringParameter>(&_output))
      .setLongDescription(R"(This option allows you to direct the global or
per-topic log messages to different outputs. The output definition can be one
of the following:

- `-` for stdin
- `+` for stderr
- `syslog://<syslog-facility>`
- `syslog://<syslog-facility>/<application-name>`
- `file://<relative-or-absolute-path>`

To set up a per-topic output configuration, use
`--log.output <topic>=<definition>`:

`--log.output queries=file://queries.log`

The above example logs query-related messages to the file `queries.log`.

You can specify the option multiple times in order to configure the output
for different log topics:

`--log.level queries=trace --log.output queries=file:///queries.log
--log.level requests=info --log.output requests=file:///requests.log`

The above example logs all query-related messages to the file `queries.log`
and HTTP requests with a level of `info` or higher to the file `requests.log`.

Any occurrence of `$PID` in the log output value is replaced at runtime with
the actual process ID. This enables logging to process-specific files:

`--log.output 'file://arangod.log.$PID'`

Note that dollar sign may need extra escaping when specified on a
command-line such as Bash.

If you specify `--log.file-mode <octalvalue>`, then any newly created log
file uses `octalvalue` as file mode. Please note that the `umask` value is
applied as well.

If you specify `--log.file-group <name>`, then any newly created log file tries
to use `<name>` as the group name. Note that you have to be a member of that
group. Otherwise, the group ownership is not changed.

The old `--log.file` option is still available for convenience. It is a
shortcut for the more general option `--log.output file://filename`.

The old `--log.requests-file` option is still available. It is a shortcut for
the more general option `--log.output requests=file://...`.)");

  std::vector<std::string> topicsVector;
  auto const& levels = Logger::logLevelTopics();
  for (auto const& level : levels) {
    topicsVector.emplace_back(level.first);
  }
  std::string topicsJoined = StringUtils::join(topicsVector, ", ");

  options
      ->addOption("--log.level,-l",
                  "Set the topic-specific log level, using `--log.level level` "
                  "for the general topic or `--log.level topic=level` for the "
                  "specified topic (can be specified multiple times).\n"
                  "Available log levels: fatal, error, warning, info, debug, "
                  "trace.\n"
                  "Available log topics: all, " +
                      topicsJoined + ".",
                  new VectorParameter<StringParameter>(&_levels))
      .setLongDescription(R"(ArangoDB's log output is grouped by topics.
`--log.level` can be specified multiple times at startup, for as many topics as
needed. The log verbosity and output files can be adjusted per log topic.

```
arangod --log.level all=warning --log.level queries=trace --log.level startup=trace
```

This sets a global log level of `warning` and two topic-specific levels
(`trace` for queries and `info` for startup). Note that `--log.level warning`
does not set a log level globally for all existing topics, but only the
`general` topic. Use the pseudo-topic `all` to set a global log level.

The same in a configuration file:

```
[log]
level = all=warning
level = queries=trace
level = startup=trace
```

The available log levels are:

- `fatal`: Only log fatal errors.
- `error`: Only log errors.
- `warning`: Only log warnings and errors.
- `info`: Log information messages, warnings, and errors.
- `debug`: Log debug and information messages, warnings, and errors.
- `trace`: Logs trace, debug, and information messages, warnings, and errors.

Note that the `debug` and `trace` levels are very verbose.

Some relevant log topics available in ArangoDB 3 are:

- `agency`: Information about the cluster Agency.
- `performance`: Performance-related messages.
- `queries`: Executed AQL queries, slow queries.
- `replication`: Replication-related information.
- `requests`: HTTP requests.
- `startup`: Information about server startup and shutdown.
- `threads`: Information about threads.

You can adjust the log levels at runtime via the `PUT /_admin/log/level`
HTTP API endpoint.

**Audit logging** (Enterprise Edition): The server logs all audit events by
default. Low priority events, such as statistics operations, are logged with the
`debug` log level. To keep such events from cluttering the log, set the
appropriate log topics to the `info` log level.)");

  options
      ->addOption("--log.max-entry-length",
                  "The maximum length of a log entry (in bytes).",
                  new UInt32Parameter(&_maxEntryLength))
      .setLongDescription(R"(**Note**: This option does not include audit log
messages. See `--audit.max-entry-length` instead.

Any log messages longer than the specified value are truncated and the suffix
`...` is added to them.

The purpose of this option is to shorten long log messages in case there is not
a lot of space for log files, and to keep rogue log messages from overusing
resources.

The default value is 128 MB, which is very high and should effectively mean
downwards-compatibility with previous arangod versions, which did not restrict
the maximum size of log messages.)");

  options
      ->addOption("--log.use-local-time",
                  "Use the local timezone instead of UTC.",
                  new BooleanParameter(&_useLocalTime),
                  arangodb::options::makeDefaultFlags(
                      arangodb::options::Flags::Uncommon))
      .setDeprecatedIn(30500)
      .setLongDescription(R"(This option is deprecated.
Use `--log.time-format local-datestring` instead.)");

  options
      ->addOption("--log.use-microtime",
                  "Use Unix timestamps in seconds with microsecond precision.",
                  new BooleanParameter(&_useMicrotime),
                  arangodb::options::makeDefaultFlags(
                      arangodb::options::Flags::Uncommon))
      .setDeprecatedIn(30500)
      .setLongDescription(R"(This option is deprecated.
Use `--log.time-format timestamp-micros` instead.)");

  options
      ->addOption(
          "--log.time-format", "The time format to use in logs.",
          new DiscreteValuesParameter<StringParameter>(
              &_timeFormatString, LogTimeFormats::getAvailableFormatNames()))
      .setLongDescription(R"(Overview over the different options:

Format                  | Example                  | Description
:-----------------------|:------------------------ |:-----------
`timestamp`             | 1553766923000            | Unix timestamps, in seconds
`timestamp-millis`      | 1553766923000.123        | Unix timestamps, in seconds, with millisecond precision
`timestamp-micros`      | 1553766923000.123456     | Unix timestamps, in seconds, with microsecond precision
`uptime`                | 987654                   | seconds since server start
`uptime-millis`         | 987654.123               | seconds since server start, with millisecond precision
`uptime-micros`         | 987654.123456            | seconds since server start, with microsecond precision
`utc-datestring`        | 2019-03-28T09:55:23Z     | UTC-based date and time in format YYYY-MM-DDTHH:MM:SSZ 
`utc-datestring-millis` | 2019-03-28T09:55:23.123Z | like `utc-datestring`, but with millisecond precision
`local-datestring`      | 2019-03-28T10:55:23      | local date and time in format YYYY-MM-DDTHH:MM:SS)");

  options
      ->addOption("--log.ids", "Log unique message IDs.",
                  new BooleanParameter(&_showIds))
      .setLongDescription(R"(Each log invocation in the ArangoDB source code
contains a unique log ID, which can be used to quickly find the location in the
source code that produced a specific log message.

Log IDs are printed as 5-digit hexadecimal identifiers in square brackets
between the log level and the log topic:

`2020-06-22T21:16:48Z [39028] INFO [144fe] {general} using storage engine
'rocksdb'` (where `144fe` is the log ID).)");

  options
      ->addOption("--log.role", "Log the server role.",
                  new BooleanParameter(&_showRole))
      .setLongDescription(R"(If you set this option to `true`, log messages
contains a single character with the server's role. The roles are:

- `U`: Undefined / unclear (used at startup)
- `S`: Single server
- `C`: Coordinator
- `P`: Primary / DB-Server
- `A`: Agent)");

  options->addOption(
      "--log.file-mode",
      "mode to use for new log file, umask will be applied as well",
      new StringParameter(&_fileMode));

  if (_threaded) {
    // this option only makes sense for arangod, not for arangosh etc.
    options
        ->addOption("--log.api-enabled",
                    "Whether the log API is enabled (true) or not (false), or "
                    "only enabled for superuser JWT (jwt).",
                    new StringParameter(&_apiSwitch))
        .setLongDescription(R"(Credentials are not written to log files.
Nevertheless, some logged data might be sensitive depending on the context of
the deployment. For example, if request logging is switched on, user requests
and corresponding data might end up in log files. Therefore, a certain care
with log files is recommended.

Since the database server offers an API to control logging and query logging
data, this API has to be secured properly. By default, the API is accessible
for admin users (administrative access to the `_system` database). However,
you can lock this down further.

The possible values for this option are:

 - `true`: The `/_admin/log` API is accessible for admin users.
 - `jwt`: The `/_admin/log` API is accessible for the superuser only
   (authentication with JWT token and empty username).
 - `false`: The `/_admin/log` API is not accessible at all.)");
  }

  options
      ->addOption("--log.use-json-format",
                  "Use JSON as output format for logging.",
                  new BooleanParameter(&_useJson))
      .setIntroducedIn(30800)
      .setLongDescription(R"(You can use this option to switch the log output
to the JSON format. Each log message then produces a separate line with
JSON-encoded log data, which can be consumed by other applications.

The object attributes produced for each log message are:

| Key        | Value      |
|:-----------|:-----------|
| `time`     | date/time of log message, in format specified by `--log.time-format`
| `prefix`   | only emitted if `--log.prefix` is set
| `pid`      | process id, only emitted if `--log.process` is set
| `tid`      | thread id, only emitted if `--log.thread` is set
| `thread`   | thread name, only emitted if `--log.thread-name` is set
| `role`     | server role (1 character), only emitted if `--log.role` is set
| `level`    | log level (e.g. `"WARN"`, `"INFO"`)
| `file`     | source file name of log message, only emitted if `--log.line-number` is set
| `line`     | source file line of log message, only emitted if `--log.line-number` is set 
| `function` | source file function name, only emitted if `--log.line-number` is set
| `topic`    | log topic name
| `id`       | log id (5 digit hexadecimal string), only emitted if `--log.ids` is set
| `hostname` | hostname if `--log.hostname` is set
| `message`  | the actual log message payload)");

#ifdef ARANGODB_HAVE_SETGID
  options->addOption(
      "--log.file-group",
      "group to use for new log file, user must be a member of this group",
      new StringParameter(&_fileGroup));
#endif

  options
      ->addOption("--log.prefix", "Prefix log message with this string.",
                  new StringParameter(&_prefix),
                  arangodb::options::makeDefaultFlags(
                      arangodb::options::Flags::Uncommon))
      .setLongDescription(R"(Example: `arangod ... --log.prefix "-->"`

`2020-07-23T09:46:03Z --> [17493] INFO ...`)");

  options->addOption(
      "--log.file", "shortcut for '--log.output file://<filename>'",
      new StringParameter(&_file),
      arangodb::options::makeDefaultFlags(arangodb::options::Flags::Uncommon));

  options->addOption(
      "--log.line-number",
      "Include the function name, file name, and line number of the source "
      "code that issues the log message. Format: `[func@FileName.cpp:123]`",
      new BooleanParameter(&_lineNumber),
      arangodb::options::makeDefaultFlags(arangodb::options::Flags::Uncommon));

  options->addOption(
      "--log.shorten-filenames",
      "shorten filenames in log output (use with --log.line-number)",
      new BooleanParameter(&_shortenFilenames),
      arangodb::options::makeDefaultFlags(arangodb::options::Flags::Uncommon));

  options
      ->addOption("--log.hostname",
                  "The hostname to use in log message. Leave empty for none, "
                  "use \"auto\" to automatically determine a hostname.",
                  new StringParameter(&_hostname))
      .setIntroducedIn(30800)
      .setLongDescription(R"(You can specify a hostname to be logged at the
beginning of each log message (for regular logging) or inside the `hostname`
attribute (for JSON-based logging).

The default value is an empty string, meaning no hostnames is logged.
If you set this option to `auto`, the hostname is automatically determined.)");

  options
      ->addOption("--log.process",
                  "Show the process identifier (PID) in log messages.",
                  new BooleanParameter(&_processId),
                  arangodb::options::makeDefaultFlags(
                      arangodb::options::Flags::Uncommon))
      .setIntroducedIn(30800);

  options->addOption(
      "--log.thread", "Show the thread identifier in log messages.",
      new BooleanParameter(&_threadId),
      arangodb::options::makeDefaultFlags(arangodb::options::Flags::Uncommon));

  options->addOption(
      "--log.thread-name", "Show thread name in log messages.",
      new BooleanParameter(&_threadName),
      arangodb::options::makeDefaultFlags(arangodb::options::Flags::Uncommon));

  options
      ->addOption("--log.performance",
                  "Shortcut for `--log.level performance=trace`.",
                  new BooleanParameter(&_performance),
                  arangodb::options::makeDefaultFlags(
                      arangodb::options::Flags::Uncommon))
      .setDeprecatedIn(30500);

  if (_threaded) {
    // this option only makes sense for arangod, not for arangosh etc.
    options->addOption("--log.keep-logrotate",
                       "Keep the old log file after receiving a SIGHUP.",
                       new BooleanParameter(&_keepLogRotate),
                       arangodb::options::makeDefaultFlags(
                           arangodb::options::Flags::Uncommon));
  }

  options->addOption(
      "--log.foreground-tty", "Also log to TTY if backgrounded.",
      new BooleanParameter(&_foregroundTty),
      arangodb::options::makeDefaultFlags(arangodb::options::Flags::Uncommon,
                                          arangodb::options::Flags::Dynamic));

  options
      ->addOption("--log.force-direct",
                  "Do not start a separate thread for logging.",
                  new BooleanParameter(&_forceDirect),
                  arangodb::options::makeDefaultFlags(
                      arangodb::options::Flags::Uncommon))
      .setLongDescription(R"(You can use this option to disable logging in an
extra logging thread. If set to `true`, any log messages are immediately
printed in the thread that triggered the log message. This is non-optimal for
performance but can aid debugging. If set to `false`, log messages are handed
off to an extra logging thread, which asynchronously writes the log messages.)");

  options
      ->addOption(
          "--log.max-queued-entries",
          "Upper limit of log entries that are queued in a background thread.",
          new UInt32Parameter(&_maxQueuedLogMessages),
          arangodb::options::makeDefaultFlags(
              arangodb::options::Flags::Uncommon))
      .setIntroducedIn(31012)
      .setIntroducedIn(31105)
      .setIntroducedIn(31200)
      .setLongDescription(R"(Log entries are pushed on a queue for asynchronous
writing unless you enable the `--log.force-direct` startup option. If you use a
slow log output (e.g. syslog), the queue might grow and eventually overflow.

You can configure the upper bound of the queue with this option. If the queue is
full, log entries are written synchronously until the queue has space again.)");

  options->addOption(
      "--log.request-parameters",
      "include full URLs and HTTP request parameters in trace logs",
      new BooleanParameter(&_logRequestParameters),
      arangodb::options::makeDefaultFlags(arangodb::options::Flags::Uncommon));

  options->addObsoleteOption("log.content-filter", "", true);
  options->addObsoleteOption("log.source-filter", "", true);
  options->addObsoleteOption("log.application", "", true);
  options->addObsoleteOption("log.facility", "", true);
}

void LoggerFeature::loadOptions(std::shared_ptr<options::ProgramOptions>,
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

  if (options->processingResult().touched("log.time-format") &&
      (options->processingResult().touched("log.use-microtime") ||
       options->processingResult().touched("log.use-local-time"))) {
    LOG_TOPIC("c3f28", FATAL, arangodb::Logger::FIXME)
        << "cannot combine `--log.time-format` with either "
           "`--log.use-microtime` or `--log.use-local-time`";
    FATAL_ERROR_EXIT();
  }

  // convert the deprecated options into the new timeformat
  if (options->processingResult().touched("log.use-local-time")) {
    _timeFormatString = "local-datestring";
    // the following call ensures the string is actually valid.
    // if not valid, the following call will throw an exception and
    // abort the startup
    LogTimeFormats::formatFromName(_timeFormatString);
  } else if (options->processingResult().touched("log.use-microtime")) {
    _timeFormatString = "timestamp-micros";
    // the following call ensures the string is actually valid.
    // if not valid, the following call will throw an exception and
    // abort the startup
    LogTimeFormats::formatFromName(_timeFormatString);
  }

  if (_apiSwitch == "true" || _apiSwitch == "on" || _apiSwitch == "On") {
    _apiEnabled = true;
    _apiSwitch = "true";
  } else if (_apiSwitch == "jwt" || _apiSwitch == "JWT") {
    _apiEnabled = true;
    _apiSwitch = "jwt";
  } else {
    _apiEnabled = false;
    _apiSwitch = "false";
  }

  if (!_fileMode.empty()) {
    try {
      int result = std::stoi(_fileMode, nullptr, 8);
      LogAppenderFileFactory::setFileMode(result);
    } catch (...) {
      LOG_TOPIC("797c2", FATAL, arangodb::Logger::FIXME)
          << "expecting an octal number for log.file-mode, got '" << _fileMode
          << "'";
      FATAL_ERROR_EXIT();
    }
  }

#ifdef ARANGODB_HAVE_SETGID
  if (!_fileGroup.empty()) {
    bool valid = false;
    int gidNumber = NumberUtils::atoi_positive<int>(
        _fileGroup.data(), _fileGroup.data() + _fileGroup.size(), valid);

    if (valid && gidNumber >= 0) {
#ifdef ARANGODB_HAVE_GETGRGID
      std::optional<gid_t> gid = FileUtils::findGroup(_fileGroup);
      if (!gid) {
        LOG_TOPIC("174c2", FATAL, arangodb::Logger::FIXME)
            << "unknown numeric gid '" << _fileGroup << "'";
        FATAL_ERROR_EXIT();
      }
#endif
    } else {
#ifdef ARANGODB_HAVE_GETGRNAM
      std::optional<gid_t> gid = FileUtils::findGroup(_fileGroup);
      if (gid) {
        gidNumber = gid.value();
      } else {
        TRI_set_errno(TRI_ERROR_SYS_ERROR);
        LOG_TOPIC("11a2c", FATAL, arangodb::Logger::FIXME)
            << "cannot convert groupname '" << _fileGroup
            << "' to numeric gid: " << TRI_last_error();
        FATAL_ERROR_EXIT();
      }
#else
      LOG_TOPIC("1c96f", FATAL, arangodb::Logger::FIXME)
          << "cannot convert groupname '" << _fileGroup << "' to numeric gid";
      FATAL_ERROR_EXIT();
#endif
    }

    LogAppenderFileFactory::setFileGroup(gidNumber);
  }
#endif

  // replace $PID with current process id in filenames
  for (auto& output : _output) {
    output = StringUtils::replace(output, "$PID",
                                  std::to_string(Thread::currentProcessId()));
  }
}

void LoggerFeature::prepare() {
  // set maximum length for each log entry
  Logger::defaultLogGroup().maxLogEntryLength(
      std::max<uint32_t>(256, _maxEntryLength));

  Logger::setLogLevel(_levels);
  Logger::setLogStructuredParamsOnServerStart(_structuredLogParams);
  Logger::setShowIds(_showIds);
  Logger::setShowRole(_showRole);
  Logger::setUseColor(_useColor);
  Logger::setTimeFormat(LogTimeFormats::formatFromName(_timeFormatString));
  Logger::setUseControlEscaped(_useControlEscaped);
  Logger::setUseUnicodeEscaped(_useUnicodeEscaped);
  Logger::setEscaping();
  Logger::setShowLineNumber(_lineNumber);
  Logger::setShortenFilenames(_shortenFilenames);
  Logger::setShowProcessIdentifier(_processId);
  Logger::setShowThreadIdentifier(_threadId);
  Logger::setShowThreadName(_threadName);
  Logger::setOutputPrefix(_prefix);
  Logger::setHostname(_hostname);
  Logger::setKeepLogrotate(_keepLogRotate);
  Logger::setLogRequestParameters(_logRequestParameters);
  Logger::setUseJson(_useJson);

  for (auto const& definition : _output) {
    if (_supervisor && definition.starts_with("file://")) {
      Logger::addAppender(Logger::defaultLogGroup(),
                          definition + ".supervisor");
    } else {
      Logger::addAppender(Logger::defaultLogGroup(), definition);
    }
  }

  if (_foregroundTty) {
    Logger::addAppender(Logger::defaultLogGroup(), "-");
  }

  if (_forceDirect || _supervisor) {
    Logger::initialize(false, _maxQueuedLogMessages);
  } else {
    Logger::initialize(_threaded, _maxQueuedLogMessages);
  }
}

void LoggerFeature::unprepare() { Logger::flush(); }

}  // namespace arangodb
