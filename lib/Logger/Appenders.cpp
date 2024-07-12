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

#include "Appenders.h"

#include "Basics/StringUtils.h"
#include "Basics/ReadLocker.h"
#include "Basics/WriteLocker.h"
#include "Logger/LogAppender.h"
#include "Logger/LogAppenderFile.h"
#include "Logger/LogAppenderStdStream.h"
#include "Logger/LogAppenderSyslog.h"
#include "Logger/LogMacros.h"
#include "Logger/LogMessage.h"
#include "Logger/Logger.h"

namespace arangodb::logger {

namespace {
constexpr std::string_view filePrefix("file://");
constexpr std::string_view syslogPrefix("syslog://");
}  // namespace

void Appenders::addGlobalAppender(LogGroup const& group,
                                  std::shared_ptr<LogAppender> appender) {
  WRITE_LOCKER(guard, _appendersLock);
  _globalAppenders[group.id()].emplace_back(std::move(appender));
}

void Appenders::addAppender(LogGroup const& group,
                            std::string const& definition) {
  std::string topicName;
  std::string output;
  LogTopic* topic = nullptr;
  Result res = parseDefinition(definition, topicName, output, topic);

  if (res.fail()) {
    LOG_TOPIC("658e0", ERR, Logger::FIXME) << res.errorMessage();
    return;
  }

  auto key = output;

#ifdef ARANGODB_ENABLE_SYSLOG
  if (output.starts_with(syslogPrefix)) {
    key = syslogPrefix;
  }
#endif

  std::shared_ptr<LogAppender> appender;

  WRITE_LOCKER(guard, _appendersLock);

  auto& definitionsMap = _definition2appenders[group.id()];

  auto it = definitionsMap.find(key);

  if (it != definitionsMap.end()) {
    // found an existing appender
    appender = it->second;
  } else {
    // build a new appender from the definition
    appender = buildAppender(group, output);
    if (appender == nullptr) {
      // cannot create appender, for whatever reason
      return;
    }

    definitionsMap[key] = appender;
  }

  TRI_ASSERT(appender != nullptr);

  auto& topicsMap = _topics2appenders[group.id()];
  size_t n = (topic == nullptr) ? LogTopic::MAX_LOG_TOPICS : topic->id();
  if (std::find(topicsMap[n].begin(), topicsMap[n].end(), appender) ==
      topicsMap[n].end()) {
    topicsMap[n].emplace_back(appender);
  }
}

std::shared_ptr<LogAppender> Appenders::buildAppender(
    LogGroup const& group, std::string const& output) {
#ifdef ARANGODB_ENABLE_SYSLOG
  // first handle syslog-logging
  if (output.starts_with(syslogPrefix)) {
    auto s =
        basics::StringUtils::split(output.substr(syslogPrefix.size()), '/');
    TRI_ASSERT(s.size() == 1 || s.size() == 2);

    std::string identifier;

    if (s.size() == 2) {
      identifier = s[1];
    }

    return std::make_shared<LogAppenderSyslog>(s[0], identifier);
  }
#endif

  if (output == "+" || output == "-") {
    for (auto const& it : _definition2appenders[group.id()]) {
      if (it.first == "+" || it.first == "-") {
        // already got a logger for stderr/stdout
        return nullptr;
      }
    }
  }

  // everything else must be file-/stream-based logging
  std::shared_ptr<LogAppenderStream> result;

  if (output == "+") {
    result = std::make_shared<LogAppenderStderr>();
  } else if (output == "-") {
    result = std::make_shared<LogAppenderStdout>();
  } else if (output.starts_with(filePrefix)) {
    result = LogAppenderFileFactory::getFileAppender(
        output.substr(filePrefix.size()));
  }

  return result;
}

void Appenders::logGlobal(LogGroup const& group, LogMessage const& message) {
  READ_LOCKER(guard, _appendersLock);

  try {
    auto& appenders = _globalAppenders.at(group.id());

    // append to global appenders first
    for (auto const& appender : appenders) {
      appender->logMessageGuarded(message);
    }
  } catch (std::out_of_range const&) {
    // no global appender for this group
    TRI_ASSERT(false) << "no global appender for group " << group.id();
    // This should never happen, however if it does we should not crash
    // but we also cannot log anything, as we are the logger.
  }
}

void Appenders::log(LogGroup const& group, LogMessage const& message) {
  // output to appenders
  READ_LOCKER(guard, _appendersLock);
  try {
    auto& topicsMap = _topics2appenders.at(group.id());
    auto output = [&topicsMap](LogGroup const& group, LogMessage const& message,
                               size_t n) -> bool {
      bool shown = false;

      auto const& it = topicsMap.find(n);
      if (it != topicsMap.end() && !it->second.empty()) {
        auto const& appenders = it->second;

        for (auto const& appender : appenders) {
          appender->logMessageGuarded(message);
        }
        shown = true;
      }

      return shown;
    };

    bool shown = false;

    // try to find a topic-specific appender
    size_t topicId = message._topicId;

    if (topicId < LogTopic::MAX_LOG_TOPICS) {
      shown = output(group, message, topicId);
    }

    // otherwise use the general topic appender
    if (!shown) {
      output(group, message, LogTopic::MAX_LOG_TOPICS);
    }
  } catch (std::out_of_range const&) {
    // no topic 2 appenders entry for this group.
    TRI_ASSERT(false) << "no topic 2 appender match for group " << group.id();
    // This should never happen, however if it does we should not crash
    // but we also cannot log anything, as we are the logger.
  }
}

void Appenders::shutdown() {
  WRITE_LOCKER(guard, _appendersLock);

#ifdef ARANGODB_ENABLE_SYSLOG
  LogAppenderSyslog::close();
#endif
  LogAppenderFileFactory::closeAll();

  for (std::size_t i = 0; i < LogGroup::Count; ++i) {
    _globalAppenders[i].clear();
    _topics2appenders[i].clear();
    _definition2appenders[i].clear();
  }
}

void Appenders::reopen() {
  WRITE_LOCKER(guard, _appendersLock);

  LogAppenderFileFactory::reopenAll();
}

Result Appenders::parseDefinition(std::string const& definition,
                                  std::string& topicName, std::string& output,
                                  LogTopic*& topic) {
  topicName.clear();
  output.clear();
  topic = nullptr;

  // split into parts and do some basic validation
  std::vector<std::string> v = basics::StringUtils::split(definition, '=');

  if (v.size() == 1) {
    output = v[0];
  } else if (v.size() == 2) {
    topicName = basics::StringUtils::tolower(v[0]);

    if (topicName.empty()) {
      output = v[0];
    } else {
      output = v[1];
    }
  } else {
    return Result(
        TRI_ERROR_BAD_PARAMETER,
        std::string("strange output definition '") + definition + "' ignored");
  }

  if (!topicName.empty()) {
    topic = LogTopic::lookup(topicName);

    if (topic == nullptr) {
      return Result(TRI_ERROR_BAD_PARAMETER, std::string("strange topic '") +
                                                 topicName +
                                                 "', ignoring whole defintion");
    }
  }

  bool handled = false;
#ifdef ARANGODB_ENABLE_SYSLOG
  if (output.starts_with(syslogPrefix)) {
    handled = true;
    auto s =
        basics::StringUtils::split(output.substr(syslogPrefix.size()), '/');

    if (s.size() < 1 || s.size() > 2) {
      return Result(TRI_ERROR_BAD_PARAMETER,
                    std::string("unknown syslog definition '") + output +
                        "', expecting 'syslog://facility/identifier'");
    }
  }
#endif

  if (!handled) {
    // not yet handled. must be a file-based logger now.
    if (output != "+" && output != "-" && !output.starts_with(filePrefix)) {
      return Result(TRI_ERROR_BAD_PARAMETER,
                    std::string("unknown output definition '") + output + "'");
    }
  }

  return Result();
}

bool Appenders::haveAppenders(LogGroup const& group, size_t topicId) {
  // It might be preferable if we could avoid the lock here, but ATM this is not
  // possible. If this actually causes performance issues we have to think about
  // other solutions.
  READ_LOCKER(guard, _appendersLock);
  try {
    auto const& appenders = _topics2appenders.at(group.id());
    auto haveTopicAppenders = [&appenders](size_t topicId) {
      auto it = appenders.find(topicId);
      return it != appenders.end() && !it->second.empty();
    };
    return haveTopicAppenders(topicId) ||
           haveTopicAppenders(LogTopic::MAX_LOG_TOPICS) ||
           !_globalAppenders.at(group.id()).empty();
  } catch (std::out_of_range const&) {
    // no topic 2 appenders entry for this group.
    TRI_ASSERT(false) << "no topic 2 appender match for group " << group.id();
    // This should never happen, however if it does we should not crash
    // but we also cannot log anything, as we are the logger.
    return false;
  }
}

}  // namespace arangodb::logger
