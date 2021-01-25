////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2014-2020 ArangoDB GmbH, Cologne, Germany
/// Copyright 2004-2014 triAGENS GmbH, Cologne, Germany
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
/// @author Achim Brandt
/// @author Dr. Frank Celler
////////////////////////////////////////////////////////////////////////////////

#include <algorithm>
#include <type_traits>

#include "LogAppender.h"

#include "ApplicationFeatures/ShellColorsFeature.h"
#include "Basics/Mutex.h"
#include "Basics/MutexLocker.h"
#include "Basics/StringUtils.h"
#include "Basics/operating-system.h"
#include "Logger/LogAppenderFile.h"
#include "Logger/LogAppenderSyslog.h"
#include "Logger/LogGroup.h"
#include "Logger/LogMacros.h"
#include "Logger/LogTopic.h"
#include "Logger/Logger.h"
#include "Logger/LoggerStream.h"

using namespace arangodb;
using namespace arangodb::basics;

arangodb::Mutex LogAppender::_appendersLock;

std::array<std::vector<std::shared_ptr<LogAppender>>, LogGroup::Count> LogAppender::_globalAppenders;

std::array<std::map<size_t, std::vector<std::shared_ptr<LogAppender>>>, LogGroup::Count> LogAppender::_topics2appenders;

std::array<std::map<std::string, std::shared_ptr<LogAppender>>, LogGroup::Count> LogAppender::_definition2appenders;

bool LogAppender::_allowStdLogging = true;

void LogAppender::addGlobalAppender(LogGroup const& group,
                                    std::shared_ptr<LogAppender> appender) {
  MUTEX_LOCKER(guard, _appendersLock);
  _globalAppenders[group.id()].emplace_back(std::move(appender));
}

void LogAppender::addAppender(LogGroup const& group, std::string const& definition) {
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
  if (StringUtils::isPrefix(output, "syslog://")) {
    key = "syslog://";
  }
#endif

  std::shared_ptr<LogAppender> appender;

  auto& definitionsMap = _definition2appenders[group.id()];

  MUTEX_LOCKER(guard, _appendersLock);

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

std::shared_ptr<LogAppender> LogAppender::buildAppender(LogGroup const& group,
                                                        std::string const& output) {
#ifdef ARANGODB_ENABLE_SYSLOG
  // first handle syslog-logging
  if (StringUtils::isPrefix(output, "syslog://")) {
    auto s = StringUtils::split(output.substr(9), '/');
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
        // alreay got a logger for stderr/stdout
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
  } else if (StringUtils::isPrefix(output, "file://")) {
    result = std::make_shared<LogAppenderFile>(output.substr(7));
  }
  
  return result;
}

void LogAppender::logGlobal(LogGroup const& group, LogMessage const& message) {
  MUTEX_LOCKER(guard, _appendersLock);

  auto& appenders = _globalAppenders[group.id()];

  // append to global appenders first
  for (auto const& appender : appenders) {
    appender->logMessage(message);
  }
}

void LogAppender::log(LogGroup const& group, LogMessage const& message) {
  // output to appenders
  auto& topicsMap = _topics2appenders[group.id()];
  auto output = [&topicsMap](LogGroup const& group, LogMessage const& message,
                             size_t n) -> bool {
    bool shown = false;

    auto const& it = topicsMap.find(n);
    if (it != topicsMap.end() && !it->second.empty()) {
      auto const& appenders = it->second;

      for (auto const& appender : appenders) {
        appender->logMessage(message);
      }
      shown = true;
    }

    return shown;
  };

  bool shown = false;

  // try to find a topic-specific appender
  size_t topicId = message._topicId;
  
  MUTEX_LOCKER(guard, _appendersLock);
 
  if (topicId < LogTopic::MAX_LOG_TOPICS) {
    shown = output(group, message, topicId);
  }

  // otherwise use the general topic appender
  if (!shown) {
    output(group, message, LogTopic::MAX_LOG_TOPICS);
  }
}

void LogAppender::shutdown() {
  MUTEX_LOCKER(guard, _appendersLock);

#ifdef ARANGODB_ENABLE_SYSLOG
  LogAppenderSyslog::close();
#endif
  LogAppenderFile::closeAll();

  for (std::size_t i = 0; i < LogGroup::Count; ++i) {
    _globalAppenders[i].clear();
    _topics2appenders[i].clear();
    _definition2appenders[i].clear();
  }
}

void LogAppender::reopen() {
  MUTEX_LOCKER(guard, _appendersLock);

  LogAppenderFile::reopenAll();
}

Result LogAppender::parseDefinition(std::string const& definition, 
                                    std::string& topicName,
                                    std::string& output,
                                    LogTopic*& topic) {
  topicName.clear();
  output.clear();
  topic = nullptr;

  // split into parts and do some basic validation
  std::vector<std::string> v = StringUtils::split(definition, '=');

  if (v.size() == 1) {
    output = v[0];
  } else if (v.size() == 2) {
    topicName = StringUtils::tolower(v[0]);

    if (topicName.empty()) {
      output = v[0];
    } else {
      output = v[1];
    }
  } else {
    return Result(TRI_ERROR_BAD_PARAMETER, std::string("strange output definition '") + definition + "' ignored");
  }

  if (!topicName.empty()) {
    topic = LogTopic::lookup(topicName);

    if (topic == nullptr) {
      return Result(TRI_ERROR_BAD_PARAMETER, std::string("strange topic '") + topicName + "', ignoring whole defintion");
    }
  }

  bool handled = false;
#ifdef ARANGODB_ENABLE_SYSLOG
  if (StringUtils::isPrefix(output, "syslog://")) {
    handled = true;
    auto s = StringUtils::split(output.substr(9), '/');

    if (s.size() < 1 || s.size() > 2) {
      return Result(TRI_ERROR_BAD_PARAMETER, std::string("unknown syslog definition '") + output + "', expecting 'syslog://facility/identifier'");
    }
  }
#endif

  if (!handled) {
    // not yet handled. must be a file-based logger now.
    if (output != "+" && output != "-" && !StringUtils::isPrefix(output, "file://")) {
      return Result(TRI_ERROR_BAD_PARAMETER, std::string("unknown output definition '") + output + "'");
    }
  }
 
  return Result();
}

bool LogAppender::haveAppenders(LogGroup const& group, size_t topicId) {
  return !_topics2appenders[group.id()][topicId].empty() ||
         !_topics2appenders[group.id()][LogTopic::MAX_LOG_TOPICS].empty() ||
         !_globalAppenders[group.id()].empty();
}
