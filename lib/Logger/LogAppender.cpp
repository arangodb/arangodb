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

#include "Basics/MutexLocker.h"
#include "Basics/StringUtils.h"
#include "Basics/shell-colors.h"
#include "Logger/LogAppenderFile.h"
#include "Logger/LogAppenderSyslog.h"
#include "Logger/LogAppenderTty.h"
#include "Logger/Logger.h"

using namespace arangodb;
using namespace arangodb::basics;

arangodb::Mutex LogAppender::_appendersLock;

std::unique_ptr<LogAppender> LogAppender::_ttyAppender(nullptr);

std::map<size_t, std::vector<std::shared_ptr<LogAppender>>>
    LogAppender::_topics2appenders;

std::map<std::pair<std::string, std::string>, std::shared_ptr<LogAppender>>
    LogAppender::_definition2appenders;

std::vector<std::function<void(LogMessage*)>> LogAppender::_loggers;

void LogAppender::addLogger(std::function<void(LogMessage*)> func) {
  _loggers.emplace_back(func);
}

void LogAppender::addAppender(std::string const& definition,
                              std::string const& filter) {
  std::vector<std::string> v = StringUtils::split(definition, '=');
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
    LOG(ERR) << "strange output definition '" << definition << "' ignored";
    return;
  }

  LogTopic* topic = nullptr;

  if (!topicName.empty()) {
    topic = LogTopic::lookup(topicName);

    if (topic == nullptr) {
      LOG(ERR) << "strange topic '" << topicName
               << "', ignoring whole defintion";
      return;
    }
  }

  MUTEX_LOCKER(guard, _appendersLock);
  std::shared_ptr<LogAppender> appender(buildAppender(output, contentFilter));

  if (appender == nullptr) {
    return;
  }

  size_t n = (topic == nullptr) ? LogTopic::MAX_LOG_TOPICS : topic->id();
  _topics2appenders[n].emplace_back(appender);
}

void LogAppender::addTtyAppender() { _ttyAppender.reset(new LogAppenderTty()); }

std::shared_ptr<LogAppender> LogAppender::buildAppender(
    std::string const& output, std::string const& contentFilter) {
  auto key = make_pair(output, contentFilter);

#ifdef ARANGODB_ENABLE_SYSLOG
  if (StringUtils::isPrefix(output, "syslog://")) {
    key = std::make_pair("syslog://", "");
  }
#endif

  auto it = _definition2appenders.find(key);

  if (it != _definition2appenders.end()) {
    return it->second;
  }

#ifdef ARANGODB_ENABLE_SYSLOG
  // first handle syslog-logging
  if (StringUtils::isPrefix(output, "syslog://")) {
    auto s = StringUtils::split(output.substr(9), '/');

    if (s.size() < 1 || s.size() > 2) {
      LOG(ERR) << "unknown syslog definition '" << output << "', expecting "
               << "'syslog://facility/identifier'";
      return nullptr;
    }

    std::string identifier = "";

    if (s.size() == 2) {
      identifier = s[1];
    }

    auto result =
        std::make_shared<LogAppenderSyslog>(s[0], identifier, contentFilter);
    _definition2appenders[key] = result;

    return result;
  }
#endif

  // everything else must be file-based logging
  std::string filename;

  if (output == "-" || output == "+") {
    filename = output;
  } else if (StringUtils::isPrefix(output, "file://")) {
    filename = output.substr(7);
  } else {
    LOG(ERR) << "unknown output definition '" << output << "'";
    return nullptr;
  }

  try {
    auto result = std::make_shared<LogAppenderFile>(filename, contentFilter);
    _definition2appenders[key] = result;

    return result;
  } catch (...) {
    // cannot open file for logging
    return nullptr;
  }
}

void LogAppender::log(LogMessage* message) {
  LogLevel level = message->_level;
  size_t topicId = message->_topicId;
  std::string const& m = message->_message;
  size_t offset = message->_offset;
  bool shownStd = false;

  MUTEX_LOCKER(guard, _appendersLock);

  // output to appender
  auto output = [&level, &m, &offset, &shownStd](size_t n) -> bool {
    auto const& it = _topics2appenders.find(n);
    bool shown = false;

    if (it != _topics2appenders.end()) {
      auto const& appenders = it->second;

      for (auto const& appender : appenders) {
        if (appender->checkContent(m)) {
          bool s = appender->logMessage(level, m, offset);
          shownStd |= s;
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

  if (_ttyAppender != nullptr && !shownStd) {
    _ttyAppender->logMessage(level, m, offset);
    shown = true;
  }

  if (!shown) {
    writeStderr(level, m);
  }

  for (auto const& logger : _loggers) {
    logger(message);
  }
}

void LogAppender::writeStderr(LogLevel level, std::string const& msg) {
  if (level == LogLevel::FATAL || level == LogLevel::ERR) {
    fprintf(stderr, TRI_SHELL_COLOR_RED "%s" TRI_SHELL_COLOR_RESET "\n",
            msg.c_str());
  } else if (level == LogLevel::WARN) {
    fprintf(stderr, TRI_SHELL_COLOR_YELLOW "%s" TRI_SHELL_COLOR_RESET "\n",
            msg.c_str());
  } else {
    fprintf(stderr, "%s\n", msg.c_str());
  }
}

void LogAppender::shutdown() {
  MUTEX_LOCKER(guard, _appendersLock);

  LogAppenderSyslog::close();
  LogAppenderFile::close();

  _topics2appenders.clear();
  _definition2appenders.clear();
  _ttyAppender.reset(nullptr);
}

void LogAppender::reopen() {
  MUTEX_LOCKER(guard, _appendersLock);

  LogAppenderFile::reopen();
}
