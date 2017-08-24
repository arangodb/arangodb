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

#ifdef USE_ENTERPRISE
#include "Enterprise/Audit/AuditFeature.h"
#endif

using namespace arangodb;

namespace {
std::atomic<uint16_t> NEXT_TOPIC_ID(0);
}

Mutex LogTopic::_namesLock;
std::map<std::string, LogTopic*> LogTopic::_names;

LogTopic Logger::AGENCY("agency", LogLevel::INFO);
LogTopic Logger::AGENCYCOMM("agencycomm", LogLevel::INFO);
LogTopic Logger::AUTHENTICATION("authentication");
LogTopic Logger::AUTHORIZATION("authorization");
LogTopic Logger::CACHE("cache", LogLevel::INFO);
LogTopic Logger::CLUSTER("cluster", LogLevel::INFO);
LogTopic Logger::COLLECTOR("collector");
LogTopic Logger::COMMUNICATION("communication", LogLevel::INFO);
LogTopic Logger::COMPACTOR("compactor");
LogTopic Logger::CONFIG("config");
LogTopic Logger::DATAFILES("datafiles", LogLevel::INFO);
LogTopic Logger::DEVEL("development", LogLevel::FATAL);
LogTopic Logger::ENGINES("engines", LogLevel::INFO);
LogTopic Logger::FIXME("general", LogLevel::INFO);
LogTopic Logger::GRAPHS("graphs", LogLevel::INFO);
LogTopic Logger::HEARTBEAT("heartbeat", LogLevel::INFO);
LogTopic Logger::MEMORY("memory", LogLevel::WARN);
LogTopic Logger::MMAP("mmap");
LogTopic Logger::PERFORMANCE("performance", LogLevel::WARN);
LogTopic Logger::PREGEL("pregel", LogLevel::INFO);
LogTopic Logger::QUERIES("queries", LogLevel::INFO);
LogTopic Logger::REPLICATION("replication", LogLevel::INFO);
LogTopic Logger::REQUESTS("requests", LogLevel::FATAL);  // suppress
LogTopic Logger::ROCKSDB("rocksdb", LogLevel::WARN);
LogTopic Logger::SSL("ssl", LogLevel::WARN);
LogTopic Logger::STARTUP("startup", LogLevel::INFO);
LogTopic Logger::SUPERVISION("supervision", LogLevel::INFO);
LogTopic Logger::SYSCALL("syscall", LogLevel::INFO);
LogTopic Logger::THREADS("threads", LogLevel::WARN);
LogTopic Logger::TRANSACTIONS("trx", LogLevel::WARN);
LogTopic Logger::V8("v8", LogLevel::WARN);
LogTopic Logger::VIEWS("views", LogLevel::FATAL);

#ifdef USE_ENTERPRISE
LogTopic AuditFeature::AUDIT_AUTHENTICATION("audit-authentication", LogLevel::INFO);
LogTopic AuditFeature::AUDIT_DATABASE("audit-database", LogLevel::INFO);
LogTopic AuditFeature::AUDIT_COLLECTION("audit-collection", LogLevel::INFO);
LogTopic AuditFeature::AUDIT_VIEW("audit-view", LogLevel::INFO);
LogTopic AuditFeature::AUDIT_DOCUMENT("audit-documentation", LogLevel::INFO);
LogTopic AuditFeature::AUDIT_SERVICE("audit-service", LogLevel::INFO);
#endif

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
    LOG_TOPIC(ERR, arangodb::Logger::FIXME) << "strange topic '" << name << "'";
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

std::string LogTopic::lookup(size_t topicId) {
  MUTEX_LOCKER(guard, _namesLock);

  for (auto const& it : _names) {
    if (it.second->_id == topicId) {
      return it.second->_name;
    }
  }

  return std::string("UNKNOWN");
}

LogTopic::LogTopic(std::string const& name)
    : LogTopic(name, LogLevel::DEFAULT) {}

LogTopic::LogTopic(std::string const& name, LogLevel level)
    : _id(NEXT_TOPIC_ID.fetch_add(1, std::memory_order_seq_cst)),
      _name(name),
      _level(level) {
  if (name != "fixme" && name != "general") {
    // "fixme" is a remainder from ArangoDB < 3.2, when it was
    // allowed to log messages without a topic. From 3.2 onwards,
    // logging is always topic-based, and all previously topicless
    // log invocations now use the log topic "fixme".
    _displayName = std::string("{") + name + "} ";
  }

  try {
    MUTEX_LOCKER(guard, _namesLock);
    _names[name] = this;
  } catch (...) {
  }
}
