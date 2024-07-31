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

#include "Assertions/Assert.h"
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

#include <absl/strings/str_cat.h>

namespace arangodb::logger {

namespace {
constexpr std::string_view filePrefix("file://");
constexpr std::string_view syslogPrefix("syslog://");
}  // namespace

void Appenders::addGlobalAppender(LogGroup const& group,
                                  std::shared_ptr<LogAppender> appender) {
  WRITE_LOCKER(guard, _appendersLock);
  _groups[group.id()].globalAppenders.emplace_back(std::move(appender));
}

void Appenders::addAppender(LogGroup const& logGroup,
                            std::string const& definition) {
  auto res = parseDefinition(definition);
  if (res.fail()) {
    LOG_TOPIC("658e0", ERR, Logger::FIXME) << res.errorMessage();
    return;
  }

  auto& config = res.get();
  TRI_ASSERT(res->type != Type::kUnknown);

  auto key =
      config.type == Type::kSyslog ? std::string(syslogPrefix) : config.output;

  std::shared_ptr<LogAppender> appender;

  WRITE_LOCKER(guard, _appendersLock);

  auto& group = _groups[logGroup.id()];

  auto& definitionsMap = group.definition2appenders;
  if (auto it = definitionsMap.find(key); it != definitionsMap.end()) {
    // found an existing appender
    appender = it->second;
  } else {
    // build a new appender from the definition
    appender = buildAppender(logGroup, config);
    if (appender == nullptr) {
      // cannot create appender, for whatever reason
      return;
    }
    definitionsMap.emplace(key, appender);
  }

  TRI_ASSERT(appender != nullptr);

  auto& topicsMap = group.topics2appenders;
  size_t n = (config.topic == nullptr) ? LogTopic::GLOBAL_LOG_TOPIC
                                       : config.topic->id();
  if (std::find(topicsMap[n].begin(), topicsMap[n].end(), appender) ==
      topicsMap[n].end()) {
    topicsMap[n].emplace_back(appender);
  }
}

std::shared_ptr<LogAppender> Appenders::buildAppender(
    LogGroup const& group, AppenderConfig const& config) {
  switch (config.type) {
    case Type::kFile:
      return LogAppenderFileFactory::getFileAppender(
          config.output.substr(filePrefix.size()));
    case Type::kStderr:
      TRI_ASSERT(config.output == "+");
      if (!_groups[group.id()].definition2appenders.contains("+")) {
        return std::make_shared<LogAppenderStderr>();
      }
      // already got a logger for stderr
      break;
    case Type::kStdout:
      TRI_ASSERT(config.output == "-");
      if (!_groups[group.id()].definition2appenders.contains("-")) {
        return std::make_shared<LogAppenderStdout>();
      }
      // already got a logger for stdout
      break;
    case Type::kSyslog:
#ifdef ARANGODB_ENABLE_SYSLOG
    {
      auto s = basics::StringUtils::split(
          config.output.substr(syslogPrefix.size()), '/');
      TRI_ASSERT(s.size() == 1 || s.size() == 2);

      std::string identifier;
      if (s.size() == 2) {
        identifier = s[1];
      }

      return std::make_shared<LogAppenderSyslog>(s[0], identifier);
    }
#endif
    case Type::kUnknown:
      TRI_ASSERT(false);
  }
  return nullptr;
}

void Appenders::logGlobal(LogGroup const& group, LogMessage const& message) {
  READ_LOCKER(guard, _appendersLock);

  try {
    auto& appenders = _groups.at(group.id()).globalAppenders;

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
    auto& topicsMap = _groups.at(group.id()).topics2appenders;
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

    if (topicId < LogTopic::GLOBAL_LOG_TOPIC) {
      shown = output(group, message, topicId);
    }

    // otherwise use the general topic appender
    if (!shown) {
      output(group, message, LogTopic::GLOBAL_LOG_TOPIC);
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

  for (auto& g : _groups) {
    g.globalAppenders.clear();
    g.topics2appenders.clear();
    g.definition2appenders.clear();
  }
}

void Appenders::reopen() {
  WRITE_LOCKER(guard, _appendersLock);

  LogAppenderFileFactory::reopenAll();
}

ResultT<Appenders::AppenderConfig> Appenders::parseDefinition(
    std::string const& definition) {
  AppenderConfig result;

  // split into parts and do some basic validation
  std::vector<std::string> v = basics::StringUtils::split(definition, '=');
  std::string topicName;

  if (v.size() == 1) {
    result.output = v[0];
  } else if (v.size() == 2) {
    topicName = basics::StringUtils::tolower(v[0]);

    if (topicName.empty()) {
      result.output = v[0];
    } else {
      result.output = v[1];
    }
  } else {
    return Result(
        TRI_ERROR_BAD_PARAMETER,
        absl::StrCat("strange output definition '", definition, "' ignored"));
  }

  if (!topicName.empty()) {
    result.topic = LogTopic::lookup(topicName);

    if (result.topic == nullptr) {
      return Result(TRI_ERROR_BAD_PARAMETER,
                    absl::StrCat("strange topic '", topicName,
                                 "', ignoring whole defintion"));
    }
  }

  if (result.output == "+") {
    result.type = Type::kStderr;
  } else if (result.output == "-") {
    result.type = Type::kStdout;
#ifdef ARANGODB_ENABLE_SYSLOG
  } else if (result.output.starts_with(syslogPrefix)) {
    auto s = basics::StringUtils::split(
        result.output.substr(syslogPrefix.size()), '/');

    if (s.size() < 1 || s.size() > 2) {
      return Result(
          TRI_ERROR_BAD_PARAMETER,
          absl::StrCat("unknown syslog definition '", result.output,
                       "', expecting 'syslog://facility/identifier'"));
    }
    result.type = Type::kSyslog;
#endif
  } else if (result.output.starts_with(filePrefix)) {
    result.type = Type::kFile;
  } else {
    return Result(
        TRI_ERROR_BAD_PARAMETER,
        absl::StrCat("unknown output definition '", result.output, "'"));
  }

  return result;
}

bool Appenders::haveAppenders(LogGroup const& logGroup, size_t topicId) {
  // It might be preferable if we could avoid the lock here, but ATM this is
  // not possible. If this actually causes performance issues we have to think
  // about other solutions.
  READ_LOCKER(guard, _appendersLock);
  auto& group = _groups.at(logGroup.id());
  try {
    auto const& appenders = group.topics2appenders;
    auto haveTopicAppenders = [&appenders](size_t topicId) {
      auto it = appenders.find(topicId);
      return it != appenders.end() && !it->second.empty();
    };
    return haveTopicAppenders(topicId) ||
           haveTopicAppenders(LogTopic::GLOBAL_LOG_TOPIC) ||
           !group.globalAppenders.empty();
  } catch (std::out_of_range const&) {
    // no topic 2 appenders entry for this group.
    TRI_ASSERT(false) << "no topic 2 appender match for group "
                      << logGroup.id();
    // This should never happen, however if it does we should not crash
    // but we also cannot log anything, as we are the logger.
    return false;
  }
}

}  // namespace arangodb::logger
