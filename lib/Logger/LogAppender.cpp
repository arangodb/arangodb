////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2014-2016 ArangoDB GmbH, Cologne, Germany
/// Copyright 2004-2013 triAGENS GmbH, Cologne, Germany
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

#include "LogAppender.h"

#include "ApplicationFeatures/ShellColorsFeature.h"
#include "Basics/MutexLocker.h"
#include "Basics/StringUtils.h"
#include "Logger/LogAppenderFile.h"
#include "Logger/LogAppenderSyslog.h"
#include "Logger/Logger.h"

using namespace arangodb;
using namespace arangodb::basics;

arangodb::Mutex LogAppender::_appendersLock;

std::map<size_t, std::vector<std::shared_ptr<LogAppender>>>
    LogAppender::_topics2appenders;

std::map<std::pair<std::string, std::string>, std::shared_ptr<LogAppender>>
    LogAppender::_definition2appenders;

std::vector<std::function<void(LogMessage*)>> LogAppender::_loggers;

bool LogAppender::_allowStdLogging = true;

void LogAppender::addLogger(std::function<void(LogMessage*)> func) {
  MUTEX_LOCKER(guard, _appendersLock); // to silence TSan
  _loggers.emplace_back(func);
}

void LogAppender::addAppender(std::string const& definition,
                              std::string const& filter) {
  MUTEX_LOCKER(guard, _appendersLock);
  std::pair<std::shared_ptr<LogAppender>, LogTopic*> appender(
      buildAppender(definition, filter));

  if (appender.first == nullptr) {
    return;
  }

  LogTopic* topic = appender.second;
  size_t n = (topic == nullptr) ? LogTopic::MAX_LOG_TOPICS : topic->id();
  if (std::find(_topics2appenders[n].begin(), _topics2appenders[n].end(), appender.first) == _topics2appenders[n].end()) {
    _topics2appenders[n].emplace_back(appender.first);
  }
}

std::pair<std::shared_ptr<LogAppender>, LogTopic*> LogAppender::buildAppender(
    std::string const& definition, std::string const& filter) {
  std::vector<std::string> v = StringUtils::split(definition, '=', '\0');
  std::string topicName;
  std::string output;
  std::string contentFilter;
 
  if (v.size() == 1) {
    output = v[0];
    contentFilter = filter;
  } else if (v.size() == 2) {
    topicName = StringUtils::tolower(v[0]);

    if (topicName.empty()) {
      output = v[0];
      contentFilter = filter;
    } else {
      output = v[1];
    }
  } else {
    LOG_TOPIC(ERR, arangodb::Logger::FIXME) << "strange output definition '" << definition << "' ignored";
    return {nullptr, nullptr};
  }

  LogTopic* topic = nullptr;

  if (!topicName.empty()) {
    topic = LogTopic::lookup(topicName);

    if (topic == nullptr) {
      LOG_TOPIC(ERR, arangodb::Logger::FIXME) << "strange topic '" << topicName
               << "', ignoring whole defintion";
      return {nullptr, nullptr};
    }
  }

  auto key = std::make_pair(output, contentFilter);

#ifdef ARANGODB_ENABLE_SYSLOG
  if (StringUtils::isPrefix(output, "syslog://")) {
    key = std::make_pair("syslog://", "");
  }
#endif

  auto it = _definition2appenders.find(key);

  if (it != _definition2appenders.end()) {
    return {it->second, topic};
  }

#ifdef ARANGODB_ENABLE_SYSLOG
  // first handle syslog-logging
  if (StringUtils::isPrefix(output, "syslog://")) {
    auto s = StringUtils::split(output.substr(9), '/');

    if (s.size() < 1 || s.size() > 2) {
      LOG_TOPIC(ERR, arangodb::Logger::FIXME) << "unknown syslog definition '" << output << "', expecting "
               << "'syslog://facility/identifier'";
      return {nullptr, nullptr};
    }

    std::string identifier = "";

    if (s.size() == 2) {
      identifier = s[1];
    }

    auto result =
        std::make_shared<LogAppenderSyslog>(s[0], identifier, contentFilter);
    _definition2appenders[key] = result;

    return {result, topic};
  }
#endif

  if (output == "+" || output == "-") {
    for (auto const& it : _definition2appenders) {
      if (it.first.first == "+" || it.first.first == "-") {
        // alreay got a logger for stderr/stdout
        return {nullptr, nullptr};
      }
    }
  }

  // everything else must be file-/stream-based logging
  std::shared_ptr<LogAppenderStream> result;
  
  if (output == "+") {
    result.reset(new LogAppenderStderr(contentFilter));
  } else if (output == "-") {
    result.reset(new LogAppenderStdout(contentFilter));
  } else if (StringUtils::isPrefix(output, "file://")) {
    result.reset(new LogAppenderFile(output.substr(7), contentFilter));
  } else {
    LOG_TOPIC(ERR, arangodb::Logger::FIXME) << "unknown output definition '" << output << "'";
    return {nullptr, nullptr};
  }

  try {
    _definition2appenders[key] = result;

    return {result, topic};
  } catch (...) {
    // cannot open file for logging
    return {nullptr, nullptr};
  }
}

void LogAppender::log(LogMessage* message) {
  LogLevel level = message->_level;
  size_t topicId = message->_topicId;
  std::string const& m = message->_message;
  size_t offset = message->_offset;

  MUTEX_LOCKER(guard, _appendersLock);

  // output to appender
  auto output = [&level, &m, &offset](size_t n) -> bool {
    auto const& it = _topics2appenders.find(n);
    bool shown = false;

    if (it != _topics2appenders.end()) {
      auto const& appenders = it->second;

      for (auto const& appender : appenders) {
        if (appender->checkContent(m)) {
          appender->logMessage(level, m, offset);
        }

        shown = true;
      }
    }

    return shown;
  };

  bool shown = false;

  // try to find a specific topic
  if (topicId < LogTopic::MAX_LOG_TOPICS) {
    shown = output(topicId);
  }

  // otherwise use the general topic
  if (!shown) {
    shown = output(LogTopic::MAX_LOG_TOPICS);
  }

  for (auto const& logger : _loggers) {
    logger(message);
  }
}

void LogAppender::shutdown() {
  MUTEX_LOCKER(guard, _appendersLock);

  LogAppenderSyslog::close();
  LogAppenderFile::closeAll();

  _topics2appenders.clear();
  _definition2appenders.clear();
}

void LogAppender::reopen() {
  MUTEX_LOCKER(guard, _appendersLock);

  LogAppenderFile::reopenAll();
}
