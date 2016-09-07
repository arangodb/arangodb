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
/// @author Dr. Frank Celler
////////////////////////////////////////////////////////////////////////////////

#include "LogTopic.h"

#include "Basics/MutexLocker.h"
#include "Logger/Logger.h"

using namespace arangodb;

namespace {
// std::atomic_uint_fast16_t NEXT_TOPIC_ID(0);
std::atomic<uint16_t> NEXT_TOPIC_ID(0);
}

Mutex LogTopic::_namesLock;
std::map<std::string, LogTopic*> LogTopic::_names;

LogTopic Logger::AGENCY("agency", LogLevel::INFO);
LogTopic Logger::AGENCYCOMM("agencycomm", LogLevel::INFO);
LogTopic Logger::CLUSTER("cluster", LogLevel::INFO);
LogTopic Logger::COLLECTOR("collector");
LogTopic Logger::COMMUNICATION("communication", LogLevel::INFO);
LogTopic Logger::COMPACTOR("compactor");
LogTopic Logger::CONFIG("config");
LogTopic Logger::DATAFILES("datafiles", LogLevel::INFO);
LogTopic Logger::HEARTBEAT("heartbeat", LogLevel::INFO);
LogTopic Logger::MMAP("mmap");
LogTopic Logger::PERFORMANCE("performance", LogLevel::FATAL);  // suppress
LogTopic Logger::QUERIES("queries", LogLevel::INFO);
LogTopic Logger::REPLICATION("replication", LogLevel::INFO);
LogTopic Logger::REQUESTS("requests", LogLevel::FATAL);  // suppress
LogTopic Logger::STARTUP("startup", LogLevel::INFO);
LogTopic Logger::THREADS("threads", LogLevel::WARN);
LogTopic Logger::V8("v8", LogLevel::WARN);

std::vector<std::pair<std::string, LogLevel>> LogTopic::logLevelTopics() {
  std::vector<std::pair<std::string, LogLevel>> levels;

  MUTEX_LOCKER(guard, _namesLock);

  for (auto const& topic : _names) {
    levels.emplace_back(std::make_pair(topic.first, topic.second->level()));
  }

  return levels;
}

void LogTopic::setLogLevel(std::string const& name, LogLevel level) {
  MUTEX_LOCKER(guard, _namesLock);

  auto it = _names.find(name);

  if (it == _names.end()) {
    LOG(ERR) << "strange topic '" << name << "'";
    return;
  }

  auto topic = it->second;

  if (topic != nullptr) {
    topic->setLogLevel(level);
  }
}

LogTopic* LogTopic::lookup(std::string const& name) {
  MUTEX_LOCKER(guard, _namesLock);

  auto it = _names.find(name);

  if (it == _names.end()) {
    return nullptr;
  }

  return it->second;
}

LogTopic::LogTopic(std::string const& name)
    : LogTopic(name, LogLevel::DEFAULT) {}

LogTopic::LogTopic(std::string const& name, LogLevel level)
    : _id(NEXT_TOPIC_ID.fetch_add(1, std::memory_order_seq_cst)),
      _name(name),
      _level(level) {
  try {
    MUTEX_LOCKER(guard, _namesLock);
    _names[name] = this;
  } catch (...) {
  }
}
